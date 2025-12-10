#ifndef PROXY_H
#define PROXY_H

#include <sys/socket.h>
#include <netinet/in.h>

// HTTP状态码
typedef enum {
    HTTP_OK = 200,
    HTTP_BAD_REQUEST = 400,
    HTTP_FORBIDDEN = 403,
    HTTP_NOT_FOUND = 404,
    HTTP_METHOD_NOT_ALLOWED = 405,
    HTTP_INTERNAL_ERROR = 500
} http_status_t;

// HTTP请求结构（优化版：适合路由器状态页面）
typedef struct {
    char method[8];     // GET, POST, etc
    char url[256];      // 状态页面URL都很短
    char version[16];
    char host[128];      // 内网IP地址很短
    int port;
    // 移除非必要字段以节省内存
    // char user_agent[256];
    // char referer[512];
    // char content_type[128];
    int content_length;
    // char headers[4096];
    // char body[8192];
    char headers[512];   // 减小到512，状态页面头部很简单
    // char body[1024];   // 状态页面没有body
} http_request_t;

// 代理功能函数
int init_proxy(int port);
int parse_http_request(const char* request_data, http_request_t* request);
int is_blocked_request(const http_request_t* request);
void send_block_response(int client_fd, const http_request_t* request);

#endif /* PROXY_H */