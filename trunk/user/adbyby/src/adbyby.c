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

static volatile int running = 1;  // 添加volatile确保信号可见性
static int server_fd = -1;         // 全局server_fd，用于信号处理
rule_manager_t* rule_manager = NULL;
static adhook_config_t config;

// 处理HTTP请求 - 改进版本，增强稳定性
void handle_client_request(int client_fd) {
    // 设置较短但合理的超时
    struct timeval timeout;
    timeout.tv_sec = 5;  // 5秒超时（稍微增加以确保处理完整）
    timeout.tv_usec = 0;
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    // 增大缓冲区以处理更完整的HTTP头部
    char buffer[1024];   // 增加缓冲区大小
    
    int bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received <= 0) {
        if (bytes_received < 0) {
            log_message(LOG_DEBUG, "Recv error: %s", strerror(errno));
        }
        close(client_fd);
        return;
    }
    
    buffer[bytes_received] = '\0';
    
    // 更安全的HTTP请求解析
    char url[512] = {0};  // 增加URL缓冲区大小
    char method[16] = {0};
    
    // 使用更安全的解析方式
    if (sscanf(buffer, "%15s %511s", method, url) != 2) {
        log_message(LOG_DEBUG, "Invalid HTTP request");
        close(client_fd);
        return;
    }
    
    // 只处理GET和HEAD请求（更安全）
    if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) {
        const char* not_allowed = "HTTP/1.1 405 Method Not Allowed\r\n"
                                  "Connection: close\r\n"
                                  "\r\n";
        write(client_fd, not_allowed, strlen(not_allowed));
        close(client_fd);
        return;
    }
    
    if (config.debug_mode) {
        log_message(LOG_DEBUG, "Request: %s %s", method, url);
    }
    
    // 安全的广告检测
    int is_ad = 0;
    if (rule_manager && url[0] != '\0') {
        // 提取主机名用于更精确的匹配
        char host[256] = {0};
        char* host_start = strstr(buffer, "Host:");
        if (host_start) {
            host_start += 5; // 跳过"Host:"
            while (*host_start == ' ' || *host_start == '\t') host_start++;
            char* host_end = strchr(host_start, '\r');
            if (!host_end) host_end = strchr(host_start, '\n');
            if (host_end) {
                int host_len = host_end - host_start;
                if (host_len > 0 && host_len < (int)sizeof(host)) {
                    strncpy(host, host_start, host_len);
                    host[host_len] = '\0';
                }
            }
        }
        
        is_ad = rule_manager_is_blocked(rule_manager, url, host);
    }
    
    const char* response;
    int response_len;
    
    if (is_ad) {
        // 改进的屏蔽响应
        response = "HTTP/1.1 200 OK\r\n"
                   "Content-Type: text/html\r\n"
                   "Connection: close\r\n"
                   "Cache-Control: no-store, no-cache\r\n"
                   "Pragma: no-cache\r\n"
                   "\r\n"
                   "<!-- adbyby-blocked -->";
        response_len = strlen(response);
        log_message(LOG_DEBUG, "Blocked: %s", url);
    } else {
        // 允许通过的响应
        response = "HTTP/1.1 302 Found\r\n"
                   "Location: about:blank\r\n"
                   "Connection: close\r\n"
                   "\r\n";
        response_len = strlen(response);
    }
    
    // 安全的响应发送
    ssize_t sent = write(client_fd, response, response_len);
    if (sent != response_len) {
        log_message(LOG_DEBUG, "Incomplete response sent: %zd/%d", sent, response_len);
    }
    
    close(client_fd);
}

// 清理PID文件的函数
void cleanup_pid_files() {
    unlink("/var/run/adbyby.pid");
    unlink("/tmp/adbyby.pid");
    unlink("/tmp/adbyby/adbyby.pid");
    log_message(LOG_INFO, "PID files cleaned up");
}

// 信号处理
void signal_handler(int sig) {
    log_message(LOG_INFO, "Received signal %d, shutting down...", sig);
    running = 0;
    
    // 立即关闭监听socket，强制释放端口
    if (server_fd >= 0) {
        close(server_fd);
        server_fd = -1;
        log_message(LOG_INFO, "Server socket closed immediately");
    }
    
    // 立即清理PID文件，避免健康检查误判
    cleanup_pid_files();
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
    
    // 获取统计信息
    int init_total_rules, init_enabled_rules, init_total_hits;
    rule_manager_get_stats(rule_manager, &init_total_rules, &init_enabled_rules, &init_total_hits);
    log_message(LOG_INFO, "Rule manager initialized: %d total rules, %d enabled", init_total_rules, init_enabled_rules);
    
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
    server_fd = init_proxy(config.listen_port);
    if (server_fd < 0) {
        log_message(LOG_ERROR, "Failed to initialize proxy server");
        rule_manager_destroy(rule_manager);
        return 1;
    }
    
    log_message(LOG_INFO, "AdByBy-Open started on port %d", config.listen_port);
    
    // 主循环 - 改进的单线程处理（增强稳定性）
    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (!running) {
                // 如果收到退出信号，直接退出
                break;
            }
            if (errno == EINTR) {
                continue; // 被信号中断，继续循环
            }
            if (errno == EMFILE || errno == ENFILE) {
                // 文件描述符耗尽，短暂休息
                log_message(LOG_WARN, "File descriptor limit reached, waiting...");
                usleep(100000); // 等待100ms
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 非阻塞模式下没有连接，短暂休息
                usleep(10000); // 等待10ms
                continue;
            }
            if (errno == EBADF) {
                // socket已关闭（信号处理导致），正常退出
                break;
            }
            log_message(LOG_ERROR, "Accept failed: %s", strerror(errno));
            break;
        }
        
        if (config.debug_mode) {
            log_message(LOG_DEBUG, "Connection from %s:%d", 
                   inet_ntoa(client_addr.sin_addr), 
                   ntohs(client_addr.sin_port));
        }
        
        // 增强的客户端处理（带错误恢复）
        handle_client_request(client_fd);
        
        // 每处理10个连接检查一次运行状态
        static int connection_count = 0;
        if (++connection_count >= 10) {
            connection_count = 0;
            // 短暂休眠，避免CPU占用过高
            usleep(1000); // 1ms
        }
    }
    
    // 清理
    if (server_fd >= 0) {
        close(server_fd);
        server_fd = -1;
    }
    
    // 再次确保PID文件被清理
    cleanup_pid_files();
    
    // 显示最终统计
    int final_total_rules, final_enabled_rules, final_total_hits;
    rule_manager_get_stats(rule_manager, &final_total_rules, &final_enabled_rules, &final_total_hits);
    log_message(LOG_INFO, "Final stats: %d total blocks", final_total_hits);
    
    // 清理规则管理器
    if (rule_manager) {
        rule_manager_destroy(rule_manager);
    }
    
    log_message(LOG_INFO, "AdByBy-Open stopped cleanly");
    
    return 0;
}