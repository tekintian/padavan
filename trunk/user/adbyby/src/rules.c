#include "rules.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>

#define INITIAL_RULE_CAPACITY 500  // 优化：减少初始内存占用
#define MAX_RULE_LENGTH 512

// 内置广告域名列表（与utils.c保持一致）
static const char* builtin_ad_domains[] = {
    // 国际主要广告平台
    "doubleclick.net",
    "googleadservices.com", 
    "googlesyndication.com",
    "google-analytics.com",
    "adnxs.com",
    "advertising.com",
    // 国内主要广告平台
    "allyes.com",
    "mediav.com",
    "iads.cn",
    
    NULL
};

// 内置URL模式列表
static const char* builtin_url_patterns[] = {
    // 通用广告路径
    "/ad.",
    "/ads.",
    "/advertisement",
    "/adserver",
    "/advertising",
    "/banner",
    "/popup",
    "/popunder",
    "/tracking",
    "/analytics",
    "/beacon",
    "/pixel",
    
    NULL
};

rule_manager_t* rule_manager_create(const char* rules_file) {
    rule_manager_t* rm = malloc(sizeof(rule_manager_t));
    if (!rm) {
        log_message(LOG_ERROR, "Failed to allocate memory for rule manager");
        return NULL;
    }
    
    memset(rm, 0, sizeof(rule_manager_t));
    
    if (rules_file) {
        strncpy(rm->rules_file, rules_file, sizeof(rm->rules_file) - 1);
        rm->rules_file[sizeof(rm->rules_file) - 1] = '\0';
    } else {
        strcpy(rm->rules_file, "/tmp/adbyby/data/rules.txt");
    }
    
    rm->capacity = INITIAL_RULE_CAPACITY;
    rm->rules = malloc(sizeof(ad_rule_t) * rm->capacity);
    if (!rm->rules) {
        log_message(LOG_ERROR, "Failed to allocate memory for rules array");
        free(rm);
        return NULL;
    }
    
    // 初始化规则数组
    memset(rm->rules, 0, sizeof(ad_rule_t) * rm->capacity);
    
    // 加载内置规则
    rule_manager_add_builtin_rules(rm);
    
    // 尝试从文件加载规则
    rule_manager_load_rules(rm);
    
    log_message(LOG_INFO, "Rule manager created with %d initial rules", rm->count);
    return rm;
}

void rule_manager_destroy(rule_manager_t* rm) {
    if (!rm) return;
    
    log_message(LOG_INFO, "Destroying rule manager with %d rules", rm->count);
    
    if (rm->rules) {
        // 清理每条规则（如果有动态分配的资源）
        for (int i = 0; i < rm->count; i++) {
            // 当前实现中没有动态分配的字符串，但保留扩展性
            rm->rules[i].pattern[0] = '\0';
            rm->rules[i].hit_count = 0;
        }
        free(rm->rules);
        rm->rules = NULL;
    }
    
    rm->count = 0;
    rm->capacity = 0;
    free(rm);
}

int rule_manager_load_rules(rule_manager_t* rm) {
    if (!rm) return 0;
    
    FILE* file = fopen(rm->rules_file, "r");
    if (!file) {
        log_message(LOG_WARN, "Cannot open rules file: %s", rm->rules_file);
        return 0;
    }
    
    char line[512];   // 规则文件行通常不会太长
    int loaded_count = 0;
    
    while (fgets(line, sizeof(line), file)) {
        // 跳过注释和空行
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        
        // 移除换行符
        line[strcspn(line, "\r\n")] = 0;
        
        if (strlen(line) == 0) continue;
        
        // 解析规则格式: pattern|type|description
        char pattern[MAX_RULE_LENGTH];
        char type_str[32];
        char description[256] = "";
        
        char* pipe1 = strchr(line, '|');
        if (pipe1) {
            *pipe1 = '\0';
            strncpy(pattern, line, sizeof(pattern) - 1);
            pattern[sizeof(pattern) - 1] = '\0';
            
            char* pipe2 = strchr(pipe1 + 1, '|');
            if (pipe2) {
                *pipe2 = '\0';
                strncpy(type_str, pipe1 + 1, sizeof(type_str) - 1);
                strncpy(description, pipe2 + 1, sizeof(description) - 1);
            } else {
                strncpy(type_str, pipe1 + 1, sizeof(type_str) - 1);
            }
        } else {
            strncpy(pattern, line, sizeof(pattern) - 1);
            strcpy(type_str, "0");
        }
        
        rule_type_t type = atoi(type_str);
        
        // 检查是否需要扩容
        if (rm->count >= rm->capacity) {
            rm->capacity *= 2;
            ad_rule_t* new_rules = realloc(rm->rules, sizeof(ad_rule_t) * rm->capacity);
            if (!new_rules) {
                log_message(LOG_ERROR, "Failed to expand rules array");
                break;
            }
            rm->rules = new_rules;
        }
        
        // 添加规则
        ad_rule_t* rule = &rm->rules[rm->count];
        strncpy(rule->pattern, pattern, sizeof(rule->pattern) - 1);
        rule->pattern[sizeof(rule->pattern) - 1] = '\0';
        rule->type = type;
        rule->enabled = 1;
        rule->last_updated = time(NULL);
        strncpy(rule->description, description, sizeof(rule->description) - 1);
        rule->description[sizeof(rule->description) - 1] = '\0';
        rule->hit_count = 0;
        
        rm->count++;
        loaded_count++;
    }
    
    fclose(file);
    rm->last_load_time = time(NULL);
    
    log_message(LOG_INFO, "Loaded %d rules from file: %s", loaded_count, rm->rules_file);
    return loaded_count;
}

int rule_manager_save_rules(rule_manager_t* rm) {
    if (!rm) return 0;
    
    FILE* file = fopen(rm->rules_file, "w");
    if (!file) {
        log_message(LOG_ERROR, "Cannot write to rules file: %s", rm->rules_file);
        return 0;
    }
    
    fprintf(file, "# AdByBy-Open Rules File\n");
    fprintf(file, "# Format: pattern|type|description\n");
    fprintf(file, "# Types: 0=simple, 1=regex, 2=domain, 3=url, 4=wildcard\n\n");
    
    int saved_count = 0;
    for (int i = 0; i < rm->count; i++) {
        ad_rule_t* rule = &rm->rules[i];
        fprintf(file, "%s|%d|%s\n", rule->pattern, rule->type, rule->description);
        saved_count++;
    }
    
    fclose(file);
    log_message(LOG_INFO, "Saved %d rules to file: %s", saved_count, rm->rules_file);
    return saved_count;
}

int rule_manager_add_rule(rule_manager_t* rm, const char* pattern, rule_type_t type, const char* description) {
    if (!rm || !pattern || strlen(pattern) == 0) {
        log_message(LOG_ERROR, "Invalid parameters for add_rule");
        return 0;
    }
    
    // 检查是否已存在相同规则（避免重复）
    for (int i = 0; i < rm->count; i++) {
        if (rm->rules[i].type == type && strcmp(rm->rules[i].pattern, pattern) == 0) {
            log_message(LOG_DEBUG, "Rule already exists: %s", pattern);
            rm->rules[i].enabled = 1; // 重新启用
            return 1;
        }
    }
    
    // 检查是否需要扩容
    if (rm->count >= rm->capacity) {
        int new_capacity = rm->capacity * 2;
        ad_rule_t* new_rules = realloc(rm->rules, sizeof(ad_rule_t) * new_capacity);
        if (!new_rules) {
            log_message(LOG_ERROR, "Failed to expand rules array to %d rules", new_capacity);
            return 0;
        }
        // 初始化新分配的内存
        memset(&new_rules[rm->capacity], 0, sizeof(ad_rule_t) * (new_capacity - rm->capacity));
        
        rm->rules = new_rules;
        rm->capacity = new_capacity;
        log_message(LOG_INFO, "Expanded rules array to %d rules", new_capacity);
    }
    
    ad_rule_t* rule = &rm->rules[rm->count];
    
    // 安全地初始化规则
    memset(rule, 0, sizeof(ad_rule_t));
    strncpy(rule->pattern, pattern, sizeof(rule->pattern) - 1);
    rule->pattern[sizeof(rule->pattern) - 1] = '\0';
    rule->type = type;
    rule->enabled = 1;
    rule->last_updated = time(NULL);
    
    if (description && strlen(description) > 0) {
        strncpy(rule->description, description, sizeof(rule->description) - 1);
        rule->description[sizeof(rule->description) - 1] = '\0';
    } else {
        strcpy(rule->description, "");
    }
    
    rule->hit_count = 0;
    
    rm->count++;
    log_message(LOG_DEBUG, "Added rule #%d: %s (type: %d)", rm->count, pattern, type);
    
    return 1;
}

int rule_manager_match_pattern(const char* text, const char* pattern, rule_type_t type) {
    if (!text || !pattern) return 0;
    
    switch (type) {
        case RULE_TYPE_SIMPLE:
            return strstr(text, pattern) != NULL;
            
        case RULE_TYPE_DOMAIN:
            // 检查域名匹配
            if (strcmp(text, pattern) == 0) return 1;
            if (ends_with(text, pattern)) {
                int text_len = strlen(text);
                int pattern_len = strlen(pattern);
                if (text_len > pattern_len && text[text_len - pattern_len - 1] == '.') {
                    return 1;
                }
            }
            return 0;
            
        case RULE_TYPE_WILDCARD: {
            // 简单的通配符匹配
            char* regex_pattern = malloc(strlen(pattern) * 2 + 3);
            if (!regex_pattern) return 0;
            
            char* dst = regex_pattern;
            *dst++ = '^';
            
            for (const char* src = pattern; *src; src++) {
                if (*src == '*') {
                    *dst++ = '.';
                    *dst++ = '*';
                } else if (*src == '?') {
                    *dst++ = '.';
                } else if (*src == '.') {
                    *dst++ = '\\';
                    *dst++ = '.';
                } else {
                    *dst++ = *src;
                }
            }
            *dst++ = '$';
            *dst = '\0';
            
            regex_t regex;
            int result = regcomp(&regex, regex_pattern, REG_EXTENDED | REG_NOSUB);
            if (result == 0) {
                result = regexec(&regex, text, 0, NULL, 0);
                regfree(&regex);
            }
            
            free(regex_pattern);
            return result == 0;
        }
        
        default:
            return strstr(text, pattern) != NULL;
    }
}

int rule_manager_is_blocked(rule_manager_t* rm, const char* url, const char* host) {
    if (!rm) return 0;
    
    for (int i = 0; i < rm->count; i++) {
        ad_rule_t* rule = &rm->rules[i];
        if (!rule->enabled) continue;
        
        int matched = 0;
        
        if (rule->type == RULE_TYPE_DOMAIN) {
            if (host && rule_manager_match_pattern(host, rule->pattern, rule->type)) {
                matched = 1;
            }
        } else if (rule->type == RULE_TYPE_URL) {
            if (url && rule_manager_match_pattern(url, rule->pattern, rule->type)) {
                matched = 1;
            }
        } else {
            // 通用文本匹配
            if (url && rule_manager_match_pattern(url, rule->pattern, rule->type)) {
                matched = 1;
            } else if (host && rule_manager_match_pattern(host, rule->pattern, rule->type)) {
                matched = 1;
            }
        }
        
        if (matched) {
            rule->hit_count++;
            log_message(LOG_DEBUG, "Blocked by rule: %s (hits: %d)", rule->pattern, rule->hit_count);
            return 1;
        }
    }
    
    return 0;
}

void rule_manager_add_builtin_rules(rule_manager_t* rm) {
    if (!rm) return;
    
    // 添加内置域名规则
    for (int i = 0; builtin_ad_domains[i]; i++) {
        rule_manager_add_rule(rm, builtin_ad_domains[i], RULE_TYPE_DOMAIN, "Built-in ad domain");
    }
    
    // 添加内置URL模式规则
    for (int i = 0; builtin_url_patterns[i]; i++) {
        rule_manager_add_rule(rm, builtin_url_patterns[i], RULE_TYPE_SIMPLE, "Built-in URL pattern");
    }
    
    log_message(LOG_INFO, "Added %d built-in rules", rm->count);
}

void rule_manager_get_stats(rule_manager_t* rm, int* total_rules, int* enabled_rules, int* total_hits) {
    if (!rm) return;
    
    int enabled = 0;
    int hits = 0;
    
    for (int i = 0; i < rm->count; i++) {
        if (rm->rules[i].enabled) enabled++;
        hits += rm->rules[i].hit_count;
    }
    
    if (total_rules) *total_rules = rm->count;
    if (enabled_rules) *enabled_rules = enabled;
    if (total_hits) *total_hits = hits;
}

void rule_manager_reset_stats(rule_manager_t* rm) {
    if (!rm) return;
    
    for (int i = 0; i < rm->count; i++) {
        rm->rules[i].hit_count = 0;
    }
    
    log_message(LOG_INFO, "Rule statistics reset");
}

void rule_manager_print_stats(rule_manager_t* rm) {
    if (!rm) return;
    
    int total, enabled, hits;
    rule_manager_get_stats(rm, &total, &enabled, &hits);
    
    printf("=== Rule Statistics ===\n");
    printf("Total rules: %d\n", total);
    printf("Enabled rules: %d\n", enabled);
    printf("Total hits: %d\n", hits);
    printf("========================\n");
}