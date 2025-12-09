#include "proxy.h"
#include "utils.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

int parse_http_request(const char* request_data, http_request_t* request) {
    if (!request_data || !request) {
        return 0;
    }
    
    memset(request, 0, sizeof(http_request_t));
    
    char* data = strdup(request_data);
    char* line = strtok(data, "\r\n");
    
    if (!line) {
        free(data);
        return 0;
    }
    
    // 解析请求行
    if (sscanf(line, "%15s %2047s %15s", request->method, request->url, request->version) != 3) {
        free(data);
        return 0;
    }
    
    // 解析头部
    char* headers_start = line + strlen(line) + 2;
    strcpy(request->headers, headers_start);
    
    // 解析各个头部字段
    line = strtok(NULL, "\r\n");
    while (line && strlen(line) > 0) {
        if (strncasecmp(line, "Host:", 5) == 0) {
            sscanf(line, "Host: %255s", request->host);
            char* colon = strchr(request->host, ':');
            if (colon) {
                *colon = '\0';
                request->port = atoi(colon + 1);
            } else {
                request->port = (strncmp(request->url, "https:", 6) == 0) ? 443 : 80;
            }
        } else if (strncasecmp(line, "User-Agent:", 11) == 0) {
            sscanf(line, "User-Agent: %255[^\r\n]", request->user_agent);
        } else if (strncasecmp(line, "Referer:", 8) == 0) {
            sscanf(line, "Referer: %511[^\r\n]", request->referer);
        } else if (strncasecmp(line, "Content-Type:", 13) == 0) {
            sscanf(line, "Content-Type: %127[^\r\n]", request->content_type);
        } else if (strncasecmp(line, "Content-Length:", 15) == 0) {
            request->content_length = atoi(line + 15);
        }
        line = strtok(NULL, "\r\n");
    }
    
    // 解析请求体
    if (request->content_length > 0) {
        char* body_start = strstr(request_data, "\r\n\r\n");
        if (body_start) {
            body_start += 4;
            int body_len = strlen(body_start);
            if (body_len > 0) {
                strncpy(request->body, body_start, sizeof(request->body) - 1);
                request->body[sizeof(request->body) - 1] = '\0';
            }
        }
    }
    
    free(data);
    return 1;
}

int is_blocked_request(const http_request_t* request) {
    if (!request) return 0;
    
    // 检查URL中的广告关键词（国内环境优化）
    const char* ad_patterns[] = {
        // 通用广告路径
        "/ad.", "/ads.", "/advert", "/advertisement", "/banner", "/popup",
        
        // 国际广告平台
        "doubleclick", "googlesyndication", "googleads", "facebook.com/tr",
        "analytics.google.com", "amazon-adsystem", "taboola", "outbrain",
        
        // 国内广告平台
        "tanx.com", "pangolin-sdk", "gdt.qq.com", "e.qq.com", "ad.qq.com",
        "allyes.com", "admaster.com.cn", "miaozhen.com", "mediav.com", "iads.cn",
        
        // 统计分析
        "hm.baidu.com", "tongji.baidu.com", "cnzz.com", "51.la",
        
        // 短视频广告
        "douyin.com/ad", "kuaishou.com/ad", "toutiao.com/ad",
        
        NULL
    };
    
    for (int i = 0; ad_patterns[i]; i++) {
        if (strstr(request->url, ad_patterns[i])) {
            log_message(LOG_DEBUG, "Blocked URL pattern: %s in %s", ad_patterns[i], request->url);
            return 1;
        }
    }
    
    // 检查Host是否为广告域名
    url_info_t url_info;
    if (parse_url(request->url, &url_info)) {
        if (is_ad_domain(url_info.host)) {
            log_message(LOG_DEBUG, "Blocked ad domain: %s", url_info.host);
            return 1;
        }
    }
    
    if (strlen(request->host) > 0 && is_ad_domain(request->host)) {
        log_message(LOG_DEBUG, "Blocked ad host: %s", request->host);
        return 1;
    }
    
    return 0;
}

void send_block_response(int client_fd, const http_request_t* request) {
    const char* block_html = 
        "<!DOCTYPE html>"
        "<html><head>"
        "<title>广告已屏蔽</title>"
        "<meta charset='utf-8'>"
        "<style>"
        "body{font-family:Arial,sans-serif;background:#f5f5f5;margin:0;padding:50px;}"
        ".container{max-width:600px;margin:0 auto;background:white;padding:30px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}"
        ".blocked-icon{font-size:48px;color:#e74c3c;margin-bottom:20px;}"
        ".title{color:#2c3e50;font-size:24px;margin-bottom:10px;}"
        ".message{color:#7f8c8d;font-size:16px;}"
        ".details{margin-top:20px;padding:15px;background:#f8f9fa;border-radius:4px;font-family:monospace;font-size:12px;color:#555;}"
        "</style></head><body>"
        "<div class='container'>"
        "<div class='blocked-icon'>🚫</div>"
        "<div class='title'>广告已被屏蔽</div>"
        "<div class='message'>此广告被 AdByBy-Open 自动拦截，保护您的网络安全</div>";
    
    if (request && strlen(request->url) > 0) {
        char details[512];
        snprintf(details, sizeof(details), 
            "<div class='details'>被屏蔽的URL: %s<br>Host: %s</div>", 
            request->url, request->host);
        block_html = realloc((char*)block_html, strlen(block_html) + strlen(details) + 100);
        strcat((char*)block_html, details);
    }
    
    strcat((char*)block_html, "</div></body></html>");
    
    char response[4096];
    int html_len = strlen(block_html);
    
    int header_len = snprintf(response, sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Server: AdByBy-Open/1.0\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-cache\r\n"
        "\r\n",
        html_len);
    
    write(client_fd, response, header_len);
    write(client_fd, block_html, html_len);
    
    log_message(LOG_INFO, "Sent block page for URL: %s", request ? request->url : "unknown");
}

int forward_request(const http_request_t* request, http_response_t* response) {
    if (!request || !response) return 0;
    
    memset(response, 0, sizeof(http_response_t));
    
    // 检查是否为根路径请求，如果是则返回状态页面
    if (strcmp(request->url, "/") == 0 || strlen(request->url) == 0) {
        response->status_code = HTTP_OK;
        strcpy(response->status_text, "OK");
        strcpy(response->content_type, "text/html");
        
        const char* proxy_msg = 
            "<!DOCTYPE html>"
            "<html><head>"
            "<title>AdByBy-Open 状态</title>"
            "<meta charset='utf-8'>"
            "<style>"
            "body { font-family: Arial, sans-serif; margin: 40px; background-color: #f5f5f5; }"
            ".container { max-width: 800px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
            ".header { text-align: center; color: #2c3e50; border-bottom: 2px solid #3498db; padding-bottom: 20px; }"
            ".status { display: flex; justify-content: space-around; margin: 30px 0; }"
            ".status-item { text-align: center; padding: 20px; background: #ecf0f1; border-radius: 8px; flex: 1; margin: 0 10px; }"
            ".status-item h3 { color: #27ae60; margin: 0 0 10px 0; }"
            ".info-section { margin: 20px 0; padding: 15px; background: #f8f9fa; border-left: 4px solid #3498db; }"
            ".footer { text-align: center; margin-top: 30px; color: #7f8c8d; font-size: 14px; }"
            ".running { color: #27ae60; }"
            ".stopped { color: #e74c3c; }"
            "</style>"
            "</head><body>"
            "<div class='container'>"
            "<div class='header'>"
            "<h1>🛡️ AdByBy-Open 广告过滤代理</h1>"
            "<p class='running'>✅ 服务正在运行</p>"
            "</div>"
            
            "<div class='status'>"
            "<div class='status-item'>"
            "<h3>🌐 代理状态</h3>"
            "<p>端口: 8118</p>"
            "<p>状态: <span class='running'>运行中</span></p>"
            "</div>"
            "<div class='status-item'>"
            "<h3>📊 过滤统计</h3>"
            "<p>内置规则: 48条</p>"
            "<p>过滤命中: <span id='hits'>0</span>次</p>"
            "</div>"
            "<div class='status-item'>"
            "<h3>⚙️ 系统信息</h3>"
            "<p>版本: AdByBy-Open v1.0</p>"
            "<p>架构: MIPS</p>"
            "</div>"
            "</div>"
            
            "<div class='info-section'>"
            "<h3>📋 功能说明</h3>"
            "<ul>"
            "<li><strong>透明代理</strong>: 自动过滤HTTP请求中的广告</li>"
            "<li><strong>DNS过滤</strong>: 阻止广告域名解析</li>"
            "<li><strong>规则更新</strong>: 支持在线更新过滤规则</li>"
            "<li><strong>自定义规则</strong>: 支持用户自定义过滤规则</li>"
            "</ul>"
            "</div>"
            
            "<div class='info-section'>"
            "<h3>🔧 请求信息</h3>"
            "<p><strong>当前请求URL:</strong> ";
        
        strcpy(response->body, proxy_msg);
        strcat(response->body, request->url);
        strcat(response->body, "</p>"
            "<p><strong>请求时间:</strong> <span id='timestamp'></span></p>"
            "</div>"
            
            "<div class='footer'>"
            "<p>🔒 AdByBy-Open - 开源广告过滤解决方案 | 保护您的隐私，提升浏览体验</p>"
            "<p>如遇问题，请检查路由器管理界面或查看系统日志</p>"
            "</div>"
            "</div>"
            
            "<script>"
            "document.getElementById('timestamp').textContent = new Date().toLocaleString('zh-CN');"
            "</script>"
            "</body></html>");
        
        response->content_length = strlen(response->body);
        return 1;
    }
    
    // 对于其他请求，实现真正的HTTP转发
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        log_message(LOG_ERROR, "Failed to create server socket");
        return 0;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(request->port);
    
    // 解析目标主机
    struct hostent* host_entry = gethostbyname(request->host);
    if (!host_entry) {
        log_message(LOG_ERROR, "Failed to resolve host: %s", request->host);
        close(server_fd);
        return 0;
    }
    
    memcpy(&server_addr.sin_addr, host_entry->h_addr, host_entry->h_length);
    
    // 连接到目标服务器
    if (connect(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_message(LOG_ERROR, "Failed to connect to %s:%d", request->host, request->port);
        close(server_fd);
        return 0;
    }
    
    // 构建完整的HTTP请求
    char full_request[8192];
    int request_len = snprintf(full_request, sizeof(full_request),
        "%s %s %s\r\n"
        "Host: %s\r\n"
        "User-Agent: %s\r\n"
        "Connection: close\r\n",
        request->method, request->url, request->version,
        request->host, request->user_agent);
    
    // 添加其他头部
    if (strlen(request->headers) > 0) {
        strncat(full_request + request_len, request->headers, sizeof(full_request) - request_len - 1);
        request_len = strlen(full_request);
    }
    
    // 添加头部结束标记
    strcat(full_request + request_len, "\r\n");
    request_len = strlen(full_request);
    
    // 发送请求到目标服务器
    if (send(server_fd, full_request, request_len, 0) < 0) {
        log_message(LOG_ERROR, "Failed to send request to server");
        close(server_fd);
        return 0;
    }
    
    // 接收响应
    char response_buffer[32768];
    int total_received = 0;
    int bytes_received;
    
    while ((bytes_received = recv(server_fd, response_buffer + total_received, 
                                  (int)(sizeof(response_buffer) - total_received - 1), 0)) > 0) {
        total_received += bytes_received;
        if (total_received >= (int)(sizeof(response_buffer) - 1)) {
            break;
        }
    }
    
    response_buffer[total_received] = '\0';
    close(server_fd);
    
    if (total_received == 0) {
        log_message(LOG_ERROR, "No response received from server");
        return 0;
    }
    
    // 解析响应
    char* headers_end = strstr(response_buffer, "\r\n\r\n");
    if (!headers_end) {
        log_message(LOG_ERROR, "Invalid HTTP response format");
        return 0;
    }
    
    int headers_length = headers_end - response_buffer + 4;
    char* body_start = headers_end + 4;
    int body_length = total_received - headers_length;
    
    // 解析状态行
    char* first_line = strtok(response_buffer, "\r\n");
    if (first_line && sscanf(first_line, "HTTP/%*f %d %255[^\r\n]", 
                            &response->status_code, response->status_text) == 2) {
        // 解析头部
        char* line = strtok(NULL, "\r\n");
        while (line && strlen(line) > 0) {
            if (strncasecmp(line, "Content-Type:", 13) == 0) {
                sscanf(line + 13, " %127[^\r\n]", response->content_type);
            } else if (strncasecmp(line, "Content-Length:", 15) == 0) {
                response->content_length = atoi(line + 15);
            }
            line = strtok(NULL, "\r\n");
        }
        
        // 复制响应体
        if (body_length > 0) {
            int copy_length = (body_length < (int)(sizeof(response->body) - 1)) ? 
                             body_length : (int)(sizeof(response->body) - 1);
            memcpy(response->body, body_start, copy_length);
            response->body[copy_length] = '\0';
        }
        
        log_message(LOG_INFO, "Forwarded request: %s %s - Status: %d", 
                   request->method, request->url, response->status_code);
        return 1;
    }
    
    return 0;
}

void send_response(int client_fd, const http_response_t* response) {
    if (!response) return;
    
    char headers[4096];
    int header_len = snprintf(headers, sizeof(headers),
        "HTTP/1.1 %d %s\r\n"
        "Server: AdByBy-Open/1.0\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n",
        response->status_code,
        response->status_text,
        response->content_type,
        response->content_length);
    
    write(client_fd, headers, header_len);
    
    if (response->content_length > 0 && strlen(response->body) > 0) {
        write(client_fd, response->body, response->content_length);
    }
}

void handle_client(int client_fd) {
    char buffer[16384];
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received <= 0) {
        close(client_fd);
        return;
    }
    
    buffer[bytes_received] = '\0';
    log_message(LOG_DEBUG, "Received request:\n%s", buffer);
    
    http_request_t request;
    if (!parse_http_request(buffer, &request)) {
        const char* error_response = 
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 11\r\n"
            "Connection: close\r\n"
            "\r\n"
            "Bad Request";
        write(client_fd, error_response, strlen(error_response));
        close(client_fd);
        return;
    }
    
    // 检查是否应该屏蔽
    if (is_blocked_request(&request)) {
        send_block_response(client_fd, &request);
    } else {
        // 转发请求（简化实现）
        http_response_t response;
        if (forward_request(&request, &response)) {
            send_response(client_fd, &response);
        } else {
            const char* error_response = 
                "HTTP/1.1 500 Internal Server Error\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 21\r\n"
                "Connection: close\r\n"
                "\r\n"
                "Internal Server Error";
            write(client_fd, error_response, strlen(error_response));
        }
    }
    
    close(client_fd);
}

int init_proxy(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        log_message(LOG_ERROR, "Failed to create socket");
        return -1;
    }
    
    int opt_val = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_message(LOG_ERROR, "Failed to bind to port %d", port);
        close(server_fd);
        return -1;
    }
    
    if (listen(server_fd, 100) < 0) {
        log_message(LOG_ERROR, "Failed to listen on port %d", port);
        close(server_fd);
        return -1;
    }
    
    log_message(LOG_INFO, "Proxy server started on port %d", port);
    return server_fd;
}