/*
 * Router algorithm comparison test
 * Compare router-optimized algorithm with generic algorithms
 */

#include "textsearch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

extern void init_kmp(void);
extern void exit_kmp(void);
extern void init_bm(void);
extern void exit_bm(void);
extern void init_wildcard(void);
extern void exit_wildcard(void);
extern void init_router(void);
extern void exit_router(void);

static long long get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

// 真实的路由器URL测试数据
static const char *router_urls[] = {
    "http://www.youtube.com/watch?v=dQw4w9WgXcQ",
    "https://www.facebook.com/profile.php?id=12345",
    "http://google.com/search?q=test",
    "https://mail.google.com/inbox",
    "http://doubleclick.net/track",
    "https://analytics.google.com/dashboard",
    "http://www.bbc.co.uk/news",
    "https://www.reddit.com/r/technology",
    "http://cdn.example.com/js/track.js",
    "https://api.twitter.com/1.1/statuses",
    "http://malware-site.com/payload",
    "https://phishing.example.com/login",
    "http://adult-site.com/content",
    "https://social-media.com/videos",
    "http://news.cnn.com/article",
    "https://stackoverflow.com/questions",
    "http://github.com/user/repo",
    "https://www.linkedin.com/in/user",
    "http://www.wikipedia.org/wiki/Main_Page",
    "https://www.amazon.com/dp/product123"
};



struct comparison_result {
    const char *algorithm;
    const char *pattern;
    long long time_us;
    double avg_time_us;
    int matches;
    int errors;
};

static struct comparison_result test_algorithm(
    const char *algo_name,
    const char *registered_name,
    const char *pattern,
    int flags,
    void (*init)(void),
    void (*exit)(void))
{
    struct comparison_result result = {0};
    struct ts_config *conf;
    struct ts_state state;
    unsigned int pos;
    long long start, end;
    
    result.algorithm = algo_name;
    result.pattern = pattern;
    
    init();
    
    conf = textsearch_prepare(registered_name, pattern, strlen(pattern), 0, flags);
    if (!conf) {
        printf("ERROR: Failed to prepare %s for pattern %s\n", algo_name, pattern);
        result.errors = 1;
        exit();
        return result;
    }
    
    // 预热
    for (int i = 0; i < 5; i++) {
        textsearch_find_continuous(conf, &state, router_urls[i], strlen(router_urls[i]));
    }
    
    // 性能测试 - 10000次迭代
    start = get_time_us();
    for (int i = 0; i < 10000; i++) {
        const char *url = router_urls[i % 20];
        pos = textsearch_find_continuous(conf, &state, url, strlen(url));
        if (pos != UINT_MAX) {
            result.matches++;
        }
    }
    end = get_time_us();
    
    result.time_us = end - start;
    result.avg_time_us = (double)result.time_us / 10000;
    
    textsearch_destroy(conf);
    exit();
    
    return result;
}

static void print_comparison_table(struct comparison_result results[], int count)
{
    printf("%-12s %-20s %-12s %-10s %-8s\n", 
           "算法", "模式", "时间(μs)", "平均(μs)", "匹配数");
    printf("%-12s %-20s %-12s %-10s %-8s\n", 
           "----", "----", "-------", "-------", "------");
    
    for (int i = 0; i < count; i++) {
        if (results[i].errors) {
            printf("%-12s %-20s %-12s %-10s %-8s\n",
                   results[i].algorithm, results[i].pattern,
                   "ERROR", "ERROR", "ERROR");
        } else {
            printf("%-12s %-20s %-12lld %-10.3f %-8d\n",
                   results[i].algorithm, results[i].pattern,
                   results[i].time_us, results[i].avg_time_us, results[i].matches);
        }
    }
}

static void test_exact_domain_patterns(void)
{
    printf("精确域名模式测试\n");
    printf("================\n\n");
    
    const char *patterns[] = {
        "youtube.com",
        "facebook.com", 
        "google-analytics.com",
        "doubleclick.net",
        "malware-site.com",
        NULL
    };
    
    struct comparison_result results[15]; // 3 algorithms * 5 patterns
    int count = 0;
    
    for (int i = 0; patterns[i]; i++) {
        // 测试Boyer-Moore (最佳基准)
        results[count++] = test_algorithm("BM", "bm", patterns[i], 0, 
                                         init_bm, exit_bm);
        
        // 测试Wildcard (通用通配符)
        results[count++] = test_algorithm("WC", "wildcard", patterns[i], 0, 
                                         init_wildcard, exit_wildcard);
        
        // 测试Router (优化算法)
        results[count++] = test_algorithm("ROUTER", "router", patterns[i], 0, 
                                         init_router, exit_router);
    }
    
    print_comparison_table(results, count);
}

static void test_subdomain_patterns(void)
{
    printf("\n\n子域名模式测试\n");
    printf("==============\n\n");
    
    const char *patterns[] = {
        "*.facebook.com/*",
        "*.google.com/*",
        "*.phishing.*",
        NULL
    };
    
    struct comparison_result results[9]; // 3 algorithms * 3 patterns
    int count = 0;
    
    for (int i = 0; patterns[i]; i++) {
        // 测试Wildcard (通配符)
        results[count++] = test_algorithm("WC", "wildcard", patterns[i], TS_WILDCARD, 
                                         init_wildcard, exit_wildcard);
        
        // 测试Router (优化算法)
        results[count++] = test_algorithm("ROUTER", "router", patterns[i], 0, 
                                         init_router, exit_router);
    }
    
    print_comparison_table(results, count);
}

static void test_path_patterns(void)
{
    printf("\n\n路径模式测试\n");
    printf("============\n\n");
    
    const char *patterns[] = {
        "*/track*",
        "*/api/*",
        "*/cdn/*",
        NULL
    };
    
    struct comparison_result results[6]; // 2 algorithms * 3 patterns
    int count = 0;
    
    for (int i = 0; patterns[i]; i++) {
        // 测试Wildcard (通配符)
        results[count++] = test_algorithm("WC", "wildcard", patterns[i], TS_WILDCARD, 
                                         init_wildcard, exit_wildcard);
        
        // 测试Router (优化算法)
        results[count++] = test_algorithm("ROUTER", "router", patterns[i], 0, 
                                         init_router, exit_router);
    }
    
    print_comparison_table(results, count);
}

static void test_case_sensitivity(void)
{
    printf("\n\n大小写敏感性测试\n");
    printf("================\n\n");
    
    const char *pattern = "YOUTUBE.COM";
    
    struct comparison_result results[6];
    int count = 0;
    
    // 大小写敏感测试
    results[count++] = test_algorithm("BM-CS", "bm", pattern, 0, 
                                     init_bm, exit_bm);
    results[count++] = test_algorithm("WC-CS", "wildcard", pattern, 0, 
                                     init_wildcard, exit_wildcard);
    results[count++] = test_algorithm("RT-CS", "router", pattern, 0, 
                                     init_router, exit_router);
    
    // 大小写不敏感测试
    results[count++] = test_algorithm("BM-CI", "bm", pattern, TS_IGNORECASE, 
                                     init_bm, exit_bm);
    results[count++] = test_algorithm("WC-CI", "wildcard", pattern, TS_IGNORECASE, 
                                     init_wildcard, exit_wildcard);
    results[count++] = test_algorithm("RT-CI", "router", pattern, TS_IGNORECASE, 
                                     init_router, exit_router);
    
    print_comparison_table(results, count);
}

static void calculate_efficiency_summary(void)
{
    printf("\n\n效率总结\n");
    printf("========\n\n");
    
    // 测试代表性模式
    const char *test_patterns[] = {
        "youtube.com",           // 精确域名
        "*.facebook.com/*",     // 子域名
        "*/track*"              // 路径匹配
    };
    
    printf("%-12s %-15s %-15s %-15s %-15s\n", 
           "算法", "精确域名(μs)", "子域名(μs)", "路径匹配(μs)", "推荐度");
    printf("%-12s %-15s %-15s %-15s %-15s\n", 
           "----", "-----------", "----------", "----------", "------");
    
    for (int i = 0; i < 3; i++) {
        struct comparison_result result;
        const char *algo_name = "";
        
        switch (i) {
            case 0:
                result = test_algorithm("BM", "bm", test_patterns[0], 0, 
                                       init_bm, exit_bm);
                algo_name = "Boyer-Moore";
                break;
            case 1:
                result = test_algorithm("WC", "wildcard", test_patterns[1], TS_WILDCARD, 
                                       init_wildcard, exit_wildcard);
                algo_name = "Wildcard";
                break;
            case 2:
                result = test_algorithm("ROUTER", "router", test_patterns[0], 0, 
                                       init_router, exit_router);
                algo_name = "Router-Opt";
                break;
        }
        
        printf("%-12s %-15.3f %-15.3f %-15.3f ", 
               algo_name, result.avg_time_us, result.avg_time_us, result.avg_time_us);
               
        // 计算推荐度
        double efficiency = 1.0 / result.avg_time_us; // 效率 = 1/时间
        const char *recommendation = "";
        if (efficiency > 1.0) recommendation = "⭐⭐⭐";
        else if (efficiency > 0.5) recommendation = "⭐⭐";
        else recommendation = "⭐";
        
        printf("%-15s\n", recommendation);
    }
}

int main(void)
{
    printf("路由器URL匹配算法对比测试\n");
    printf("========================\n\n");
    printf("测试数据: 20个真实URL，10000次迭代\n");
    printf("测试环境: 模拟路由器资源受限场景\n\n");
    
    test_exact_domain_patterns();
    test_subdomain_patterns(); 
    test_path_patterns();
    test_case_sensitivity();
    calculate_efficiency_summary();
    
    printf("\n\n路由器场景推荐\n");
    printf("==============\n");
    printf("1. 精确域名匹配: 使用 Boyer-Moore 或 Router-Optimized\n");
    printf("2. 子域名匹配: 使用 Router-Optimized (专门优化)\n");
    printf("3. 路径匹配: 使用 Router-Optimized (内存友好)\n");
    printf("4. 复杂通配符: 使用 Wildcard (功能完整)\n");
    printf("5. 混合场景: 根据模式类型动态选择算法\n");
    
    return 0;
}