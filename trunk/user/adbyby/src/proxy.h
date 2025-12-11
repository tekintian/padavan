#ifndef PROXY_H
#define PROXY_H

#include <sys/socket.h>
#include <netinet/in.h>

// 注意：HTTP状态码枚举已移除，因为当前极简版本不需要

// 代理功能函数
int init_proxy(int port);

#endif /* PROXY_H */