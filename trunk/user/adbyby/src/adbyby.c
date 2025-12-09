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
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "utils.h"
#include "proxy.h"
#include "rules.h"
#include "adhook_config.h"

#define DEFAULT_PORT 8118
#define MAX_CLIENTS 100
#define BUFFER_SIZE 4096

static int running = 1;
static rule_manager_t* rule_manager = NULL;
static adhook_config_t config;

// 处理HTTP请求
void handle_client_request(int client_fd) {
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received <= 0) {
        close(client_fd);
        return;
    }
    
    buffer[bytes_received] = '\0';
    
    if (config.debug_mode) {
        log_message(LOG_DEBUG, "Request received:\n%s", buffer);
    }
    
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
    
    // 使用规则管理器检查是否应该屏蔽
    if (rule_manager_is_blocked(rule_manager, request.url, request.host)) {
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
    int opt;
    int daemon_mode = 1;
    char rules_file[256] = "/tmp/adbyby/data/rules.txt";
    char config_file[256] = "/tmp/adbyby/adhook.ini";
    int show_stats_only = 0;
    
    // 初始化配置
    adhook_config_init(&config);
    
    // 尝试加载配置文件
    adhook_config_load(&config, config_file);
    
    // 解析命令行参数
    while ((opt = getopt(argc, argv, "p:dr:hsh")) != -1) {
        switch (opt) {
            case 'p':
                config.listen_port = atoi(optarg);
                break;
            case 'd':
                config.debug_mode = 1;
                break;
            case 'r':
                strncpy(rules_file, optarg, sizeof(rules_file) - 1);
                break;
            case 'h':
                show_help();
                return 0;
            case 's':
                show_stats_only = 1;
                break;
            default:
                show_help();
                return 1;
        }
    }
    
    // 检查是否以守护进程模式运行
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-daemon") == 0) {
            daemon_mode = 0;
            break;
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
    
    // 主循环
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
        
        // 处理请求
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