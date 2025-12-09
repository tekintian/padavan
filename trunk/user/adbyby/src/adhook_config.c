#include "adhook_config.h"
#include <ctype.h>

void adhook_config_init(adhook_config_t* config) {
    if (!config) return;
    
    // 设置默认值（基于原版adhook.ini）
    config->listen_port = 8118;
    strcpy(config->listen_address, "0.0.0.0");
    config->buffer_limit = 1024;
    config->keep_alive_timeout = 30;
    config->socket_timeout = 60;
    config->max_client_connections = 0;  // 0表示无限制
    config->stack_size = 200;
    config->auto_restart = 0;
    config->debug_mode = 0;
    config->use_ipset = 0;
}

int adhook_config_load(adhook_config_t* config, const char* config_file) {
    if (!config || !config_file) return 0;
    
    // 先初始化默认值
    adhook_config_init(config);
    
    FILE* file = fopen(config_file, "r");
    if (!file) {
        return 0;  // 配置文件不存在是正常的，使用默认值
    }
    
    char line[512];
    char section[64] = "";
    
    while (fgets(line, sizeof(line), file)) {
        // 移除换行符和空白
        char* ptr = line;
        while (*ptr && isspace(*ptr)) ptr++;
        if (!*ptr || *ptr == '#' || *ptr == ';' || *ptr == '\n') continue;
        
        // 移除尾部空白
        int len = strlen(ptr);
        while (len > 0 && isspace(ptr[len-1])) {
            ptr[--len] = '\0';
        }
        
        // 解析section
        if (*ptr == '[' && ptr[strlen(ptr)-1] == ']') {
            strncpy(section, ptr + 1, strlen(ptr) - 2);
            section[strlen(ptr) - 2] = '\0';
            continue;
        }
        
        // 解析key=value
        char* eq = strchr(ptr, '=');
        if (!eq) continue;
        
        *eq = '\0';
        char* key = ptr;
        char* value = eq + 1;
        
        // 移除key和value的空白
        while (*key && isspace(*key)) key++;
        while (*value && isspace(*value)) value++;
        
        // 解析配置项
        if (strcmp(key, "listen-address") == 0) {
            char* colon = strrchr(value, ':');
            if (colon) {
                *colon = '\0';
                config->listen_port = atoi(colon + 1);
                strncpy(config->listen_address, value, sizeof(config->listen_address) - 1);
                config->listen_address[sizeof(config->listen_address) - 1] = '\0';
            }
        } else if (strcmp(key, "buffer-limit") == 0) {
            config->buffer_limit = atoi(value);
        } else if (strcmp(key, "keep-alive-timeout") == 0) {
            config->keep_alive_timeout = atoi(value);
        } else if (strcmp(key, "socket-timeout") == 0) {
            config->socket_timeout = atoi(value);
        } else if (strcmp(key, "max_client_connections") == 0) {
            config->max_client_connections = atoi(value);
        } else if (strcmp(key, "stack_size") == 0) {
            config->stack_size = atoi(value);
        } else if (strcmp(key, "auto_restart") == 0) {
            config->auto_restart = atoi(value);
        } else if (strcmp(key, "debug") == 0) {
            config->debug_mode = atoi(value);
        } else if (strcmp(key, "ipset") == 0) {
            config->use_ipset = atoi(value);
        }
    }
    
    fclose(file);
    return 1;
}

int adhook_config_get_port(adhook_config_t* config) {
    return config ? config->listen_port : 8118;
}

int adhook_config_get_debug(adhook_config_t* config) {
    return config ? config->debug_mode : 0;
}