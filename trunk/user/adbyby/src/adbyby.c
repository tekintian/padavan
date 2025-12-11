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

// 处理HTTP请求 - 极简版本（移除状态页面，专注广告过滤）
void handle_client_request(int client_fd) {
    // 极短超时，快速处理
    struct timeval timeout;
    timeout.tv_sec = 2;  // 2秒超时
    timeout.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    // 最小缓冲区，只读取HTTP头部
    char buffer[256];   // 足够读取GET请求行
    
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received <= 0) {
        close(client_fd);
        return;
    }
    
    buffer[bytes_received] = '\0';
    
    // 极简解析：只提取GET请求的URL
    char url[128] = {0};
    if (sscanf(buffer, "GET %127s", url) != 1) {
        close(client_fd);
        return;
    }
    
    if (config.debug_mode) {
        log_message(LOG_DEBUG, "Request: %s", url);
    }
    
    // 极简广告检测 - 统一使用规则管理器（避免重复逻辑）
    int is_ad = rule_manager && rule_manager_is_blocked(rule_manager, url, "");
    
    const char* response;
    int response_len;
    
    if (is_ad) {
        // 极简屏蔽响应
        response = "HTTP/1.1 200 OK\r\n"
                   "Content-Type: text/html\r\n"
                   "Connection: close\r\n"
                   "Cache-Control: no-store\r\n"
                   "\r\n"
                   "<!-- blocked -->";
        response_len = strlen(response);
        log_message(LOG_DEBUG, "Blocked: %s", url);
    } else {
        // 连接重置 - 让客户端直接连接目标服务器
        // 这样避免了复杂的代理逻辑，更稳定
        response = "HTTP/1.1 302 Found\r\n"
                   "Location: about:blank\r\n"
                   "Connection: close\r\n"
                   "\r\n";
        response_len = strlen(response);
    }
    
    // 发送响应并立即关闭连接
    write(client_fd, response, response_len);
    close(client_fd);
}

// 信号处理
void signal_handler(int sig) {
    log_message(LOG_INFO, "Received signal %d, shutting down...", sig);
    running = 0;
}

// 创建PID文件
int create_pid_file() {
    // 尝试多个可能的PID文件位置
    const char* pid_paths[] = {
        "/var/run/adbyby.pid",
        "/tmp/adbyby.pid",
        "/tmp/adbyby/adbyby.pid"
    };
    
    for (int i = 0; i < 3; i++) {
        FILE* pidfile = fopen(pid_paths[i], "w");
        if (pidfile) {
            fprintf(pidfile, "%d", getpid());
            fclose(pidfile);
            log_message(LOG_INFO, "PID file created: %s", pid_paths[i]);
            return 1;
        }
    }
    
    log_message(LOG_ERROR, "Failed to create PID file in any location");
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
    
    // 如果是守护进程模式，fork到后台
    if (daemon_mode) {
        if (fork() > 0) {
            exit(0); // 父进程退出
        }
        setsid(); // 创建新的会话
        // 在子进程中创建PID文件（确保PID正确）
        create_pid_file();
    } else {
        // 非守护进程模式也创建PID文件
        create_pid_file();
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
    
    // 清理所有可能的PID文件
    unlink("/var/run/adbyby.pid");
    unlink("/tmp/adbyby.pid");
    unlink("/tmp/adbyby/adbyby.pid");
    
    // 显示最终统计
    rule_manager_get_stats(rule_manager, &total_rules, &enabled_rules, &total_hits);
    log_message(LOG_INFO, "Final stats: %d total blocks", total_hits);
    
    rule_manager_destroy(rule_manager);
    log_message(LOG_INFO, "AdByBy-Open stopped");
    
    return 0;
}