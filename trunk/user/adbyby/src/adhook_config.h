#ifndef ADHOOK_CONFIG_H
#define ADHOOK_CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int listen_port;
    char listen_address[64];
    int buffer_limit;
    int keep_alive_timeout;
    int socket_timeout;
    int max_client_connections;
    int stack_size;
    int auto_restart;
    int debug_mode;
    int use_ipset;
} adhook_config_t;

// 初始化配置为默认值
void adhook_config_init(adhook_config_t* config);

// 从配置文件加载设置
int adhook_config_load(adhook_config_t* config, const char* config_file);

// 获取监听端口
int adhook_config_get_port(adhook_config_t* config);

// 获取调试模式
int adhook_config_get_debug(adhook_config_t* config);

#endif