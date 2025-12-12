/*
 * URL匹配场景性能测试 - 针对路由器资源受限环境
 */

#include "textsearch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>

extern void init_kmp(void);
extern void exit_kmp(void);
extern void init_bm(void);
extern void exit_bm(void);
extern void init_wildcard(void);
extern void exit_wildcard(void);

static long long get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

// 典型的路由器URL样本
static const char *url_samples[] = {
    "http://www.google.com/search?q=test",
    "https://www.youtube.com/watch?v=dQw4w9WgXcQ",
    "http://www.facebook.com/profile.php",
    "https://twitter.com/home",
    "http://www.baidu.com/s?wd=test",
    "https://www.amazon.com/dp/B08N5WRWNW",
    "http://www.reddit.com/r/technology",
    "https://www.instagram.com/p/CXW9z2YJt5F/",
    "http://www.wikipedia.org/wiki/Computer",
    "https://www.github.com/torvalds/linux",
    "http://www.stackoverflow.com/questions/123456",
    "https://www.netflix.com/watch/123456",
    "http://www.linkedin.com/in/profile",
    "https://www.tiktok.com/@user/video/123456",
    "http://www.pinterest.com/pin/123456"
};

static const char *url_patterns[] = {
    "*.youtube.com/*",          // YouTube域名
    "*.facebook.com/*",         // Facebook域名
    "http://*.baidu.com/*",     // 百度HTTP
    "https://*.amazon.com/*",   // Amazon HTTPS
    "*twitter.com/*",           // Twitter域名
    "*/watch/*",                // 视频页面
    "*/profile*",               // 用户资料页
    "*github.com*",            // GitHub相关
    "*/wiki/*",                 // Wiki页面
    "*.com/*",                  // 所有.com域名
    "https://*",                // 所有HTTPS
    "*video*",                  // 包含video的URL
    "?????????????.com/*",     // 特定长度域名
    "google*",                  // Google开头
    "*",
    NULL
};

// 内存使用估算
struct memory_usage {
    size_t pattern_size;
    size_t state_size;
    size_t total_size;
};

// 测量配置结构的内存使用
static struct memory_usage measure_memory_usage(struct ts_config *conf)
{
    struct memory_usage mem = {0};
    
    if (!conf) return mem;
    
    // 基础配置结构
    mem.total_size += sizeof(struct ts_config);
    
    // 模式长度
    mem.pattern_size = textsearch_get_pattern_len(conf);
    mem.total_size += mem.pattern_size;
    
    // 状态结构
    mem.state_size = sizeof(struct ts_state);
    
    // 估算算法特定内存（基于配置指针）
    void *priv = ts_config_priv(conf);
    if (priv) {
        // 这是一个粗略估算，实际取决于算法
        mem.total_size += 1024; // 假设每个算法额外使用1KB
    }
    
    return mem;
}

struct url_test_result {
    const char *algorithm;
    const char *pattern;
    int iterations;
    long long total_time_us;
    double avg_time_us;
    int success_count;
    struct memory_usage memory;
    double cpu_cycles;  // 估算CPU周期数
};

static struct url_test_result test_url_algorithm(
    const char *algo_name, 
    const char *registered_name,
    const char *pattern,
    int flags,
    int iterations,
    void (*init)(void), 
    void (*exit)(void))
{
    struct url_test_result result = {0};
    struct ts_config *conf;
    struct ts_state state;
    unsigned int pos;
    long long start, end;
    
    result.algorithm = algo_name;
    result.pattern = pattern;
    result.iterations = iterations;
    
    init();
    
    // 准备配置
    conf = textsearch_prepare(registered_name, pattern, strlen(pattern), 0, flags);
    if (!conf) {
        printf("ERROR preparing %s for pattern %s\n", algo_name, pattern);
        exit();
        return result;
    }
    
    // 测量内存使用
    result.memory = measure_memory_usage(conf);
    
    // 预热
    for (int i = 0; i < 100; i++) {
        textsearch_find_continuous(conf, &state, url_samples[i % 15], strlen(url_samples[i % 15]));
    }
    
    // 性能测试
    start = get_time_us();
    for (int i = 0; i < iterations; i++) {
        const char *url = url_samples[i % 15];
        pos = textsearch_find_continuous(conf, &state, url, strlen(url));
        if (pos != UINT_MAX) {
            result.success_count++;
        }
    }
    end = get_time_us();
    
    result.total_time_us = end - start;
    result.avg_time_us = (double)result.total_time_us / iterations;
    
    // 估算CPU周期 (假设1GHz处理器)
    result.cpu_cycles = result.avg_time_us * 1000.0;
    
    textsearch_destroy(conf);
    exit();
    
    return result;
}

// 测试不同的URL模式复杂度
static void test_url_pattern_complexity(void)
{
    printf("URL模式复杂度测试\n");
    printf("================\n\n");
    
    printf("%-12s %-20s %-12s %-12s %-15s %-10s %s\n", 
           "算法", "模式", "时间(μs)", "成功率", "内存(bytes)", "CPU周期", "适用性");
    printf("%-12s %-20s %-12s %-12s %-15s %-10s %s\n", 
           "----", "----", "-------", "------", "-----------", "-------", "------");
    
    // 测试不同复杂度的模式
    struct {
        const char *pattern;
        const char *complexity;
        const char *recommendation;
    } test_cases[] = {
        {"*.youtube.com/*", "简单域名", "推荐"},
        {"*/watch/*", "简单路径", "推荐"},
        {"*github.com*", "包含匹配", "适中"},
        {"*.com/*", "泛域名", "谨慎"},
        {"https://*", "协议匹配", "谨慎"},
        {"*/profile*", "路径包含", "适中"},
        {"?????????????.com/*", "固定长度", "特定"},
        {"*video*", "内容匹配", "适中"},
        {"google*", "前缀匹配", "推荐"},
        {"*", "全匹配", "不推荐"},
        {NULL, NULL, NULL}
    };
    
    int iterations = 50000; // 路由器典型处理量
    
    for (int i = 0; test_cases[i].pattern; i++) {
        printf("\n模式: \"%s\" (%s) - %s\n", 
               test_cases[i].pattern, test_cases[i].complexity, test_cases[i].recommendation);
        
        // 只测试支持通配符的算法
        struct url_test_result result = test_url_algorithm(
            "WILDCARD", "wildcard", test_cases[i].pattern, 
            TS_WILDCARD, iterations, init_wildcard, exit_wildcard);
        
        printf("%-12s %-20s %-12.3f %-12d %-15zu %-10.0f %s\n",
               result.algorithm, test_cases[i].complexity,
               result.avg_time_us, result.success_count,
               result.memory.total_size, result.cpu_cycles,
               test_cases[i].recommendation);
    }
}

// 资源受限场景对比
static void test_resource_constrained_scenario(void)
{
    printf("\n\n资源受限场景对比\n");
    printf("================\n\n");
    
    printf("模拟路由器环境：8MB内存，400MHz CPU\n");
    printf("假设每秒处理1000个URL请求\n\n");
    
    const char *simple_pattern = "*.youtube.com/*";
    const char *complex_pattern = "*video*";
    int iterations = 100000; // 100秒的处理量
    
    printf("%-12s %-20s %-12s %-12s %-15s %-15s %s\n", 
           "算法", "模式", "时间(μs)", "内存(KB)", "CPU负载(%%)", "吞吐量(req/s)", "推荐");
    printf("%-12s %-20s %-12s %-12s %-15s %-15s %s\n", 
           "----", "----", "-------", "--------", "---------", "-----------", "------");
    
    // 测试简单模式
    struct url_test_result wc_simple = test_url_algorithm(
        "WILDCARD", "wildcard", simple_pattern, 
        TS_WILDCARD, iterations, init_wildcard, exit_wildcard);
    
    // 测试复杂模式
    struct url_test_result wc_complex = test_url_algorithm(
        "WILDCARD", "wildcard", complex_pattern, 
        TS_WILDCARD, iterations, init_wildcard, exit_wildcard);
    
    // 计算性能指标
    double cpu_load_simple = (wc_simple.avg_time_us / 1000.0) * 100; // 假设1MHz = 100% CPU
    double cpu_load_complex = (wc_complex.avg_time_us / 1000.0) * 100;
    double throughput_simple = 1000000.0 / wc_simple.avg_time_us;
    double throughput_complex = 1000000.0 / wc_complex.avg_time_us;
    
    printf("%-12s %-20s %-12.3f %-12.2f %-15.2f %-15.0f %s\n",
           wc_simple.algorithm, "简单模式", wc_simple.avg_time_us,
           wc_simple.memory.total_size / 1024.0,
           cpu_load_simple, throughput_simple,
           cpu_load_simple < 50.0 ? "推荐" : "不推荐");
    
    printf("%-12s %-20s %-12.3f %-12.2f %-15.2f %-15.0f %s\n",
           wc_complex.algorithm, "复杂模式", wc_complex.avg_time_us,
           wc_complex.memory.total_size / 1024.0,
           cpu_load_complex, throughput_complex,
           cpu_load_complex < 50.0 ? "谨慎" : "不推荐");
}

// 内存使用分析
static void analyze_memory_usage(void)
{
    printf("\n\n内存使用分析\n");
    printf("============\n\n");
    
    printf("假设并发处理100个URL匹配请求\n\n");
    
    printf("%-12s %-15s %-15s %-15s %-15s\n", 
           "算法", "单实例(bytes)", "100并发(KB)", "1000并发(KB)", "推荐");
    printf("%-12s %-15s %-15s %-15s %-15s\n", 
           "----", "-------------", "------------", "-------------", "------");
    
    // 测试不同模式的内存使用
    const char *patterns[] = {
        "*.youtube.com/*",    // 简单
        "*video*",            // 中等
        "*/profile*",        // 中等
        "*.com/*",            // 复杂
        "*"                   // 最复杂
    };
    
    for (int i = 0; i < 5; i++) {
        struct url_test_result result = test_url_algorithm(
            "WILDCARD", "wildcard", patterns[i], 
            TS_WILDCARD, 1000, init_wildcard, exit_wildcard);
        
        size_t mem_100 = result.memory.total_size * 100;
        size_t mem_1000 = result.memory.total_size * 1000;
        
        const char *recommendation = (mem_100 < 102400) ? "推荐" : 
                                    (mem_1000 < 1024000) ? "谨慎" : "不推荐";
        
        printf("%-12s %-15zu %-15.2f %-15.2f %-15s\n",
               "WILDCARD", result.memory.total_size,
               mem_100 / 1024.0, mem_1000 / 1024.0, recommendation);
    }
}

int main(void)
{
    printf("路由器URL匹配场景性能分析\n");
    printf("==========================\n");
    printf("测试环境: 路由器典型资源限制\n\n");
    
    test_url_pattern_complexity();
    test_resource_constrained_scenario();
    analyze_memory_usage();
    
    printf("\n\n路由器场景建议\n");
    printf("============\n");
    printf("1. 优先使用简单的域名匹配 (*.domain.com/*)\n");
    printf("2. 避免过于复杂的通配符模式\n");
    printf("3. 控制并发匹配实例数量\n");
    printf("4. 考虑预处理和缓存机制\n");
    printf("5. 对于简单匹配，考虑使用固定字符串算法\n");
    
    return 0;
}