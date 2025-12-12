#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 日志级别
typedef enum {
    LOG_ERROR = 0,
    LOG_WARN  = 1,
    LOG_INFO  = 2,
    LOG_DEBUG = 3
} log_level_t;

// 日志函数
void log_message(log_level_t level, const char* format, ...);

// 字符串工具函数
int starts_with(const char* str, const char* prefix);
int ends_with(const char* str, const char* suffix);
char* trim_whitespace(char* str);
int is_empty_string(const char* str);

// URL解析函数
typedef struct {
    char scheme[16];
    char host[256];
    int port;
    char path[1024];
    char query[1024];
} url_info_t;

int parse_url(const char* url, url_info_t* info);
int is_ad_domain(const char* domain);

#endif /* UTILS_H */