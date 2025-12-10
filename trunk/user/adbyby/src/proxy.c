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
#include <sys/time.h>
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
        } else if (strncasecmp(line, "Content-Length:", 15) == 0) {
            request->content_length = atoi(line + 15);
        }
        line = strtok(NULL, "\r\n");
    }
    
    // 请求体解析已移除 - 状态页面不需要body，节省内存
    // 注：对于需要body的POST请求，可在需要时重新添加此功能
    
    free(data);
    return 1;
}

int is_blocked_request(const http_request_t* request) {
    if (!request) return 0;
    
    // 首先使用规则管理器进行匹配（这样会统计命中次数）
    if (rule_manager && rule_manager_is_blocked(rule_manager, request->url, request->host)) {
        log_message(LOG_DEBUG, "Blocked by rule manager: %s", request->url);
        return 1;
    }
    
    // 检查URL中的广告关键词（国内环境优化）- 作为后备匹配
    const char* ad_patterns[] = {
        // 通用广告路径
        "/ad.", "/ads.", "/advert", "/advertisement", "/banner", "/popup",
        
        // 国际广告平台
        "doubleclick", "googlesyndication", "googleads", "facebook.com/tr",
        "analytics.google.com", "amazon-adsystem", "taboola", "outbrain",
        
        // 国内广告平台
        "tanx.com", "pangolin-sdk", "gdt.qq.com", "e.qq.com", "ad.qq.com",
        "allyes.com", "admaster.com.cn", "miaozhen.com", "mediav.com", "iads.cn", "ads.pinduoduo.com",
        
        // 统计分析
        "hm.baidu.com", "log.baidu.com", "analytics.tencent.com", "log.mi.com",
        
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