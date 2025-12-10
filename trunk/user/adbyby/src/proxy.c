#include "proxy.h"
#include "utils.h"
#include "rules.h"

// 外部声明规则管理器（在adbyby.c中定义）
extern rule_manager_t* rule_manager;
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
    strncpy(request->headers, headers_start, sizeof(request->headers) - 1);
    request->headers[sizeof(request->headers) - 1] = '\0';
    
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
        "hm.baidu.com", "cnzz.com", "51.la",
        
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
    // 极简屏蔽页面，节省路由器资源
    (void)request; // 避免未使用参数警告
    const char* simple_block = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<!DOCTYPE html>"
        "<html><head><title>Blocked</title></head>"
        "<body><h1>🚫 Ad Blocked</h1></body></html>";
    
    write(client_fd, simple_block, strlen(simple_block));
}

int forward_request(const http_request_t* request, http_response_t* response) {
    if (!request || !response) return 0;
    
    memset(response, 0, sizeof(http_response_t));
    
    // 检查是否为根路径请求，如果是则返回状态页面
    if (strcmp(request->url, "/") == 0 || strlen(request->url) == 0) {
        response->status_code = HTTP_OK;
        strncpy(response->status_text, "OK", sizeof(response->status_text) - 1);
        strncpy(response->content_type, "text/html", sizeof(response->content_type) - 1);
        
        // 获取真实统计数据
        int total_rules = 0, enabled_rules = 0, total_hits = 0;
        if (rule_manager) {
            rule_manager_get_stats(rule_manager, &total_rules, &enabled_rules, &total_hits);
        }
        
        char stats_buffer[256];
        snprintf(stats_buffer, sizeof(stats_buffer), 
            "<p>内置规则: %d条</p>"
            "<p>过滤命中: <span id='hits'>%d</span>次</p>", 
            total_rules, total_hits);
        
        // 构建包含真实统计数据的HTML响应
        strcpy(response->body, 
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
            "<p class='running'>✅ 服务正在运行  |  <a href='https://dev.tekin.cn' target='_blank'>软件定制开发</a>咨询QQ:932256355</p>"
            "</div>"
            
            "<div class='status'>"
            "<div class='status-item'>"
            "<h3>🌐 代理状态</h3>"
            "<p>端口: 8118</p>"
            "<p>状态: <span class='running'>运行中</span></p>"
            "</div>"
            "<div class='status-item'>"
            "<h3>📊 过滤统计</h3>");
        
        // 添加动态统计数据
        strcat(response->body, stats_buffer);
        
        strcat(response->body,
            "</div>"
            "<div class='status-item'>"
            "<h3>⚙️ 系统信息</h3>"
            "<p>版本: AdByBy-Open v1.0</p>"
            "<p>架构: MIPS</p>"
            "</div>"
            "</div>"

            "<div class='info-section'>"
            "<h3>🔧 请求信息</h3>"
            "<p><strong>当前请求URL:</strong> ");
        
        // 添加请求URL和时间
        strcat(response->body, request->url);
        strcat(response->body, "</p>"
            "<p><strong>请求时间:</strong> <span id='timestamp'></span></p>"
            "</div>"
            
            "<div class='footer'>"
            "<p>🔒 AdByBy-Open - 开源广告过滤解决方案 | 保护您的隐私，提升浏览体验</p>"
            "<p>如遇问题，请检查路由器管理界面或查看系统日志. 技术支持QQ:932256355</p>"
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
        "Cache-Control: no-cache, no-store, must-revalidate\r\n"
        "Pragma: no-cache\r\n"
        "Expires: 0\r\n"
        "\r\n",
        response->status_code,
        response->status_text,
        response->content_type,
        response->content_length);
    
    // 确保完整发送头部
    int sent = 0;
    while (sent < header_len) {
        int result = write(client_fd, headers + sent, header_len - sent);
        if (result <= 0) break;
        sent += result;
    }
    
    if (response->content_length > 0 && strlen(response->body) > 0) {
        sent = 0;
        int body_len = response->content_length;
        while (sent < body_len) {
            int result = write(client_fd, response->body + sent, body_len - sent);
            if (result <= 0) break;
            sent += result;
        }
    }
    
    // 确保数据发送完成
    fsync(client_fd);
}

void handle_client(int client_fd) {
    // 设置3秒超时
    struct timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    char buffer[1024];  // 小缓冲区，栈分配
    
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received <= 0) {
        close(client_fd);
        return;
    }
    
    buffer[bytes_received] = '\0';
    
    // 简单检查是否为根路径请求
    if (strstr(buffer, "GET / HTTP") != NULL) {
        // 返回简单的状态页面
        const char* status_page = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "Cache-Control: no-cache\r\n"
            "\r\n"
            "<!DOCTYPE html>"
            "<html><head><title>AdByBy Status</title>"
            "<meta charset='utf-8'></head>"
            "<body>"
            "<h2>🛡️ AdByBy Status</h2>"
            "<p>✅ Service: Running</p>"
            "<p>🌐 Port: 8118</p>"
            "<p>⏰ " __DATE__ " " __TIME__ "</p>"
            "<p><a href='javascript:location.reload()'>🔄 Refresh</a></p>"
            "</body></html>";
        
        write(client_fd, status_page, strlen(status_page));
    } else {
        // 简单的404响应
        const char* not_found = 
            "HTTP/1.1 404 Not Found\r\n"
            "Connection: close\r\n"
            "\r\n";
        write(client_fd, not_found, strlen(not_found));
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