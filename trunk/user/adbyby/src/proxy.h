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

// HTTP请求结构
typedef struct {
    char method[16];
    char url[2048];
    char version[16];
    char host[256];
    int port;
    char user_agent[256];
    char referer[512];
    char content_type[128];
    int content_length;
    char headers[4096];
    char body[8192];
} http_request_t;

// HTTP响应结构
typedef struct {
    int status_code;
    char status_text[64];
    char content_type[128];
    int content_length;
    char headers[4096];
    char body[32768];
} http_response_t;

// 代理功能函数
int init_proxy(int port);
void handle_client(int client_fd);
int parse_http_request(const char* request_data, http_request_t* request);
int is_blocked_request(const http_request_t* request);
void send_block_response(int client_fd, const http_request_t* request);
int forward_request(const http_request_t* request, http_response_t* response);
void send_response(int client_fd, const http_response_t* response);

#endif /* PROXY_H */