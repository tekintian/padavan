#include "utils.h"
#include <stdarg.h>
#include <ctype.h>

static const char* log_level_strings[] = {
    "ERROR", "WARN", "INFO", "DEBUG"
};

void log_message(log_level_t level, const char* format, ...) {
    if (level > LOG_DEBUG) return;
    
    time_t now;
    struct tm* timeinfo;
    char timestamp[64];
    
    time(&now);
    timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    printf("[%s] %s: ", timestamp, log_level_strings[level]);
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    printf("\n");
    fflush(stdout);
}

int starts_with(const char* str, const char* prefix) {
    if (!str || !prefix) return 0;
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

int ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return 0;
    size_t len_str = strlen(str);
    size_t len_suffix = strlen(suffix);
    
    if (len_suffix > len_str) return 0;
    
    return strcmp(str + len_str - len_suffix, suffix) == 0;
}

char* trim_whitespace(char* str) {
    if (!str) return NULL;
    
    // 去除前导空白
    while (isspace((unsigned char)*str)) {
        str++;
    }
    
    if (*str == '\0') {
        return str;
    }
    
    // 去除尾随空白
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    
    *(end + 1) = '\0';
    return str;
}

int is_empty_string(const char* str) {
    return str == NULL || *str == '\0';
}

int parse_url(const char* url, url_info_t* info) {
    if (!url || !info) return 0;
    
    memset(info, 0, sizeof(url_info_t));
    
    // 简单的URL解析
    char url_copy[2048];
    strncpy(url_copy, url, sizeof(url_copy) - 1);
    url_copy[sizeof(url_copy) - 1] = '\0';
    
    char* current_pos = url_copy;
    
    char* scheme_end = strstr(current_pos, "://");
    if (scheme_end) {
        *scheme_end = '\0';
        strncpy(info->scheme, current_pos, sizeof(info->scheme) - 1);
        current_pos = scheme_end + 3;
    } else {
        strcpy(info->scheme, "http");
    }
    
    char* path_start = strchr(current_pos, '/');
    if (path_start) {
        *path_start = '\0';
        strcpy(info->path, path_start + 1);
    } else {
        strcpy(info->path, "");
    }
    
    char* port_start = strchr(current_pos, ':');
    if (port_start) {
        *port_start = '\0';
        info->port = atoi(port_start + 1);
        strncpy(info->host, current_pos, sizeof(info->host) - 1);
    } else {
        strncpy(info->host, current_pos, sizeof(info->host) - 1);
        info->port = (strcmp(info->scheme, "https") == 0) ? 443 : 80;
    }
    
    // 解析查询字符串
    char* query_start = strchr(info->path, '?');
    if (query_start) {
        *query_start = '\0';
        strcpy(info->query, query_start + 1);
    }
    
    return 1;
}

// 域名预处理：转换为小写，移除www.前缀
static void preprocess_domain(char* domain) {
    if (!domain) return;
    
    // 转换为小写
    for (int i = 0; domain[i]; i++) {
        domain[i] = tolower(domain[i]);
    }
    
    // 移除www.前缀
    if (strncmp(domain, "www.", 4) == 0) {
        memmove(domain, domain + 4, strlen(domain + 4) + 1);
    }
}

// 常见广告域名列表（国内环境优化版）
static const char* ad_domains[] = {
    // 国际主要广告平台
    "doubleclick.net",
    "googleadservices.com", 
    "googlesyndication.com",
    "google-analytics.com",
    "googletagmanager.com",
    "facebook.com/tr",
    "amazon-adsystem.com",
    "adsco.re",
    
    // 国内主要广告平台
    "tanx.com",           // 阿里妈妈
    "allyes.com",         // 好耶广告
    "guohead.com",        // 果壳
    "mediav.com",         // 亿玛
    "iads.cn",            // 爱广告
    "admaster.com.cn",    // 传漾
    "miaozhen.com",       // 秒针
    "pangolin-sdk.com",   // 穿山甲广告
    "gdt.qq.com",         // 腾讯广告
    "e.qq.com",           // 腾讯效果广告
    "ad.qq.com",          // 腾讯广告
    
    // 程序化广告平台
    "adnxs.com",          // AppNexus
    "taboola.com",        // Taboola
    "outbrain.com",       // Outbrain
    "adsafeprotected.com", // 广告验证
    "moatads.com",        // 广告监测
    "scorecardresearch.com", // 量化分析
    
    // 数据统计和分析
    "hm.baidu.com",       // 百度统计
    "cnzz.com",           // CNZZ统计
    "51.la",              // 51LA统计
    "log.mi.com",         // 小米统计
    "analysis.qq.com",    // 腾讯分析
    
    NULL
};

int is_ad_domain(const char* domain) {
    if (!domain) return 0;
    
    // 域名预处理
    char processed_domain[512];
    strncpy(processed_domain, domain, sizeof(processed_domain) - 1);
    processed_domain[sizeof(processed_domain) - 1] = '\0';
    preprocess_domain(processed_domain);
    
    // 先检查已知广告域名（精确匹配优先）
    for (int i = 0; ad_domains[i]; i++) {
        if (strstr(processed_domain, ad_domains[i])) {
            return 1;
        }
    }
    
    // 检查高优先级广告关键词（明确广告标识）
    const char* high_priority_keywords[] = {
        "doubleclick", "googlesyndication", "googleads", "tanx", "pangolin",
        "admaster", "adsystem", "adserver", "miaozhen", "dianjoy", "iads",
        "guanggao", "advertisement", NULL
    };
    
    for (int i = 0; high_priority_keywords[i]; i++) {
        if (strstr(domain, high_priority_keywords[i])) {
            return 1;
        }
    }
    
    // 检查中优先级关键词（可能误报的统计类）
    const char* medium_priority_keywords[] = {
        "analytics", "tracking", "stat", "tongji", "tracker",
        "ad-", "ad_", "-ad", "_ad", NULL
    };
    
    for (int i = 0; medium_priority_keywords[i]; i++) {
        if (strstr(domain, medium_priority_keywords[i])) {
            return 1;
        }
    }
    
    // 低优先级关键词（容易误报的）
    const char* low_priority_keywords[] = {
        "ad", "ads", "gg", NULL
    };
    
    for (int i = 0; low_priority_keywords[i]; i++) {
        // 对于低优先级关键词，进行更严格的检查
        const char* keyword = low_priority_keywords[i];
        char* found = strstr(domain, keyword);
        if (found) {
            // 检查关键词前后的字符，避免误报
            size_t pos = found - domain;
            size_t keyword_len = strlen(keyword);
            
            // 检查前面字符
            if (pos > 0) {
                char prev_char = domain[pos - 1];
                if (isalpha(prev_char) || isdigit(prev_char)) {
                    continue; // 前面有字母数字，跳过
                }
            }
            
            // 检查后面字符
            if (pos + keyword_len < strlen(domain)) {
                char next_char = domain[pos + keyword_len];
                if (isalpha(next_char) || isdigit(next_char)) {
                    continue; // 后面有字母数字，跳过
                }
            }
            
            return 1;
        }
    }
    
    return 0;
}
