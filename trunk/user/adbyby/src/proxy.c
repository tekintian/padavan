#include "proxy.h"
#include "utils.h"
#include "rules.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

// 注意：parse_http_request, is_blocked_request, send_block_response函数已移除
// 因为在极简版本中不再使用复杂的HTTP解析逻辑

int init_proxy(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        log_message(LOG_ERROR, "Failed to create socket");
        return -1;
    }
    
    // 设置socket选项：允许地址重用但确保端口能快速释放
    int opt_val = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));
    
    // 在支持的平台设置更短的TIME_WAIT
#ifdef SO_REUSEPORT
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt_val, sizeof(opt_val));
#endif
    
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