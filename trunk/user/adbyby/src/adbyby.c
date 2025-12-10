#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include "proxy.h"
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "utils.h"
#include "proxy.h"
#include "rules.h"
#include "adhook_config.h"

#define DEFAULT_PORT 8118
#define MAX_CLIENTS 50    // 路由器资源优化：减少最大客户端数
#define BUFFER_SIZE 2048  // 路由器优化：减小缓冲区大小

static int running = 1;
rule_manager_t* rule_manager = NULL;
static adhook_config_t config;

// 处理HTTP请求 - 轻量级版本（节省路由器资源）
void handle_client_request(int client_fd) {
    // 设置较短超时，快速响应
    struct timeval timeout;
    timeout.tv_sec = 3;  // 3秒超时足够
    timeout.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    
    // 使用栈分配（节省路由器堆内存）
    char buffer[512];   // 状态页面请求很小，512字节绰绰有余
    
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received <= 0) {
        close(client_fd);
        return;
    }
    
    buffer[bytes_received] = '\0';
    
    if (config.debug_mode) {
        log_message(LOG_DEBUG, "Request received: %.*s", bytes_received < 100 ? bytes_received : 100, buffer);
    }
    
    http_request_t request;
    if (!parse_http_request(buffer, &request)) {
        // 简化的错误响应
        const char* error_response = 
            "HTTP/1.1 400 Bad Request\r\n"
            "Connection: close\r\n"
            "\r\n";
        write(client_fd, error_response, strlen(error_response));
        close(client_fd);
        return;
    }
    
    // 对于状态页面，直接构建响应（避免DNS解析和网络连接）
    if (strcmp(request.url, "/") == 0 || strlen(request.url) == 0) {
        // 获取真实统计数据
        int total_rules = 0, enabled_rules = 0, total_hits = 0;
        if (rule_manager) {
            rule_manager_get_stats(rule_manager, &total_rules, &enabled_rules, &total_hits);
        }
        
        // 直接构建状态页面HTML（优化内存使用）
        char status_html[2048];  // 优化：状态页面HTML实际约1.5KB
        int html_len = snprintf(status_html, sizeof(status_html),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Connection: close\r\n"
            "Cache-Control: no-cache, no-store, must-revalidate\r\n"
            "Pragma: no-cache\r\n"
            "Expires: 0\r\n"
            "\r\n"
            "<!DOCTYPE html>"
            "<html><head>"
            "<title>AdByBy-Open 状态</title>"
            "<meta charset='utf-8'>"
            "<style>"
            "body { font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }"
            ".container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }"
            ".header { text-align: center; color: #2c3e50; border-bottom: 2px solid #3498db; padding-bottom: 15px; }"
            ".status { display: flex; justify-content: space-around; margin: 20px 0; }"
            ".status-item { text-align: center; padding: 15px; background: #ecf0f1; border-radius: 6px; flex: 1; margin: 0 5px; }"
            ".status-item h3 { color: #27ae60; margin: 0 0 8px 0; }"
            ".footer { text-align: center; margin-top: 20px; color: #7f8c8d; font-size: 12px; }"
            ".running { color: #27ae60; font-weight: bold; }"
            "</style>"
            "</head><body>"
            "<div class='container'>"
            "<div class='header'>"
            "<h1>🛡️ AdByBy-Open</h1>"
            "<p class='running'>✅ 服务运行中</p>"
            "</div>"
            
            "<div class='status'>"
            "<div class='status-item'>"
            "<h3>🌐 代理状态</h3>"
            "<p>端口: 8118</p>"
            "<p>状态: 运行中</p>"
            "</div>"
            "<div class='status-item'>"
            "<h3>📊 过滤统计</h3>"
            "<p>规则: %d条</p>"
            "<p>命中: %d次</p>"
            "</div>"
            "<div class='status-item'>"
            "<h3>⚙️ 系统</h3>"
            "<p>架构: MIPS</p>"
            "<p>版本: v1.0</p>"
            "</div>"
            "</div>"

            "<div class='footer'>"
            "<p>🔒 AdByBy-Open - 开源广告过滤解决方案  |  <a href='https://dev.tekin.cn' target='_blank'>软件定制开发</a>咨询QQ:932256355</p>"
            "<p><a href='javascript:location.reload()'>🔄 刷新状态</a></p>"
            "</div>"
            "</div>"
            "</body></html>",
            total_rules, total_hits);
        
        // 发送响应
        int sent = 0;
        while (sent < html_len) {
            int result = write(client_fd, status_html + sent, html_len - sent);
            if (result <= 0) break;
            sent += result;
        }
    } else {
        // 检查是否为广告请求
        if (is_blocked_request(&request)) {
            // 发送屏蔽响应
            send_block_response(client_fd, &request);
            log_message(LOG_INFO, "Blocked: %s", request.url);
        } else {
            // 非广告请求，返回简单的代理响应
            // 注意：这里应该实现真正的代理转发逻辑，但为了简化演示，返回501
            const char* not_implemented = 
                "HTTP/1.1 501 Not Implemented\r\n"
                "Content-Type: text/html\r\n"
                "Connection: close\r\n"
                "\r\n"
                "<!DOCTYPE html><html><head><title>501 Not Implemented</title></head>"
                "<body><h1>Proxy Not Implemented</h1><p>This is an ad filter, not a general proxy.</p></body></html>";
            write(client_fd, not_implemented, strlen(not_implemented));
        }
    }
    
    // 确保关闭连接
    close(client_fd);
}

// 信号处理
void signal_handler(int sig) {
    log_message(LOG_INFO, "Received signal %d, shutting down...", sig);
    running = 0;
}

// 创建PID文件
int create_pid_file() {
    FILE* pidfile = fopen("/var/run/adbyby.pid", "w");
    if (pidfile) {
        fprintf(pidfile, "%d", getpid());
        fclose(pidfile);
        return 1;
    }
    return 0;
}

// 显示帮助信息
void show_help() {
    printf("AdByBy-Open v1.0 - Open Source Ad Filter\n");
    printf("Usage: adbyby [options]\n");
    printf("Options:\n");
    printf("  -p PORT     Listen on port (default: 8118)\n");
    printf("  -d          Enable debug mode\n");
    printf("  -r FILE     Load rules from file\n");
    printf("  --no-daemon Run in foreground\n");
    printf("  -h          Show this help\n");
    printf("  -s          Show statistics\n");
}

// 显示统计信息
void show_statistics() {
    if (rule_manager) {
        rule_manager_print_stats(rule_manager);
    } else {
        printf("Rule manager not initialized\n");
    }
}

int main(int argc, char* argv[]) {
    // 移除未使用的变量
    // int opt;
    int daemon_mode = 1;
    char rules_file[256] = "/tmp/adbyby/data/rules.txt";
    char config_file[256] = "/tmp/adbyby/adhook.ini";
    int show_stats_only = 0;
    
    // 初始化配置
    adhook_config_init(&config);
    
    // 检查是否以守护进程模式运行（处理长选项）
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-daemon") == 0) {
            daemon_mode = 0;
            break;
        }
    }
    
    // 尝试加载配置文件
    adhook_config_load(&config, config_file);
    
    // 手动解析命令行参数（支持长选项）
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-daemon") == 0) {
            daemon_mode = 0;
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            config.listen_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0) {
            config.debug_mode = 1;
        } else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            strncpy(rules_file, argv[++i], sizeof(rules_file) - 1);
        } else if (strcmp(argv[i], "-h") == 0) {
            show_help();
            return 0;
        } else if (strcmp(argv[i], "-s") == 0) {
            show_stats_only = 1;
        }
    }
    
    // 初始化规则管理器
    rule_manager = rule_manager_create(rules_file);
    if (!rule_manager) {
        log_message(LOG_ERROR, "Failed to create rule manager");
        return 1;
    }
    
    // 如果只是显示统计信息
    if (show_stats_only) {
        show_statistics();
        rule_manager_destroy(rule_manager);
        return 0;
    }
    
    int total_rules, enabled_rules, total_hits;
    rule_manager_get_stats(rule_manager, &total_rules, &enabled_rules, &total_hits);
    log_message(LOG_INFO, "Rule manager initialized: %d total rules, %d enabled", total_rules, enabled_rules);
    
    // 设置信号处理
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    
    // 创建PID文件
    create_pid_file();
    
    // 如果是守护进程模式，fork到后台
    if (daemon_mode) {
        if (fork() > 0) {
            exit(0); // 父进程退出
        }
        setsid(); // 创建新的会话
    }
    
    // 初始化代理服务器
    int server_fd = init_proxy(config.listen_port);
    if (server_fd < 0) {
        log_message(LOG_ERROR, "Failed to initialize proxy server");
        rule_manager_destroy(rule_manager);
        return 1;
    }
    
    log_message(LOG_INFO, "AdByBy-Open started on port %d", config.listen_port);
    
    // 主循环 - 轻量级单线程处理（适合路由器环境）
    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            log_message(LOG_ERROR, "Accept failed: %s", strerror(errno));
            break;
        }
        
        if (config.debug_mode) {
            log_message(LOG_DEBUG, "Connection from %s:%d", 
                   inet_ntoa(client_addr.sin_addr), 
                   ntohs(client_addr.sin_port));
        }
        
        // 单线程处理（节省路由器资源）
        handle_client_request(client_fd);
    }
    
    // 清理
    close(server_fd);
    unlink("/var/run/adbyby.pid");
    
    // 显示最终统计
    rule_manager_get_stats(rule_manager, &total_rules, &enabled_rules, &total_hits);
    log_message(LOG_INFO, "Final stats: %d total blocks", total_hits);
    
    rule_manager_destroy(rule_manager);
    log_message(LOG_INFO, "AdByBy-Open stopped");
    
    return 0;
}