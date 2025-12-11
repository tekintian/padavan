#ifndef RULES_H
#define RULES_H

#include <stdio.h>
#include <time.h>

// 规则类型
typedef enum {
    RULE_TYPE_SIMPLE = 0,  // 简单字符串匹配
    RULE_TYPE_REGEX = 1,    // 正则表达式
    RULE_TYPE_DOMAIN = 2,   // 域名匹配
    RULE_TYPE_URL = 3,      // URL匹配
    RULE_TYPE_WILDCARD = 4  // 通配符匹配
} rule_type_t;

// 广告规则结构
typedef struct {
    char pattern[512];
    rule_type_t type;
    int enabled;
    time_t last_updated;
    char description[256];
    int hit_count;  // 匹配次数统计
} ad_rule_t;

// 规则管理器
typedef struct {
    ad_rule_t* rules;
    int capacity;
    int count;
    char rules_file[256];
    time_t last_load_time;
} rule_manager_t;

// 规则管理函数
rule_manager_t* rule_manager_create(const char* rules_file);
void rule_manager_destroy(rule_manager_t* rm);
int rule_manager_load_rules(rule_manager_t* rm);
int rule_manager_save_rules(rule_manager_t* rm);
int rule_manager_add_rule(rule_manager_t* rm, const char* pattern, rule_type_t type, const char* description);

// 规则匹配函数
int rule_manager_is_blocked(rule_manager_t* rm, const char* url, const char* host);
int rule_manager_match_pattern(const char* text, const char* pattern, rule_type_t type);

// 规则统计函数
void rule_manager_get_stats(rule_manager_t* rm, int* total_rules, int* enabled_rules, int* total_hits);
void rule_manager_reset_stats(rule_manager_t* rm);
void rule_manager_print_stats(rule_manager_t* rm);

// 内置规则生成
void rule_manager_add_builtin_rules(rule_manager_t* rm);

#endif /* RULES_H */