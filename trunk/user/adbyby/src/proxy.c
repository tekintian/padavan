#include "proxy.h"
#include "utils.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

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
    
    // 检查URL中的广告关键词
    const char* ad_patterns[] = {
        "/ad.", "/ads.", "/advert", "/advertisement", "/banner", "/popup",
        "doubleclick", "googlesyndication", "googleads", "facebook.com/tr",
        "analytics.google.com", "amazon-adsystem", "taboola", "outbrain",
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
    // 简单实现：返回一个通用的代理响应
    if (!request || !response) return 0;
    
    memset(response, 0, sizeof(http_response_t));
    response->status_code = HTTP_OK;
    strcpy(response->status_text, "OK");
    strcpy(response->content_type, "text/html");
    
    const char* proxy_msg = 
        "<!DOCTYPE html>"
        "<html><head><title>代理转发</title></head>"
        "<body><h1>AdByBy-Open 代理</h1>"
        "<p>请求已转发，此功能待完善</p>"
        "<p>URL: ";
    
    strcpy(response->body, proxy_msg);
    strcat(response->body, request->url);
    strcat(response->body, "</p></body></html>");
    
    response->content_length = strlen(response->body);
    
    return 1;
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