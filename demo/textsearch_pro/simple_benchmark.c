/*
 * Simple performance comparison between algorithms
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

static long long get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

struct result {
    const char *algorithm;
    const char *pattern;
    long long time_us;
    double avg_us;
    int success;
};

static struct result test_algorithm(const char *algo_name, const char *registered_name,
                                   const char *pattern, int flags,
                                   const char *text, int iterations,
                                   void (*init)(void), void (*exit)(void))
{
    struct result r = {0};
    struct ts_config *conf;
    struct ts_state state;
    unsigned int pos;
    long long start, end;
    
    r.algorithm = algo_name;
    r.pattern = pattern;
    
    init();
    conf = textsearch_prepare(registered_name, pattern, strlen(pattern), 0, flags);
    if (!conf) {
        printf("ERROR preparing %s\n", algo_name);
        exit();
        return r;
    }
    
    start = get_time_us();
    for (int i = 0; i < iterations; i++) {
        pos = textsearch_find_continuous(conf, &state, text, strlen(text));
        if (pos != UINT_MAX) r.success = 1;
    }
    end = get_time_us();
    
    r.time_us = end - start;
    r.avg_us = (double)r.time_us / iterations;
    
    textsearch_destroy(conf);
    exit();
    return r;
}

int main(void)
{
    printf("Algorithm Performance Comparison\n");
    printf("=================================\n\n");
    
    // Test data
    const char *text = 
        "This is a comprehensive test string designed to evaluate the performance "
        "differences between various string searching algorithms including KMP, "
        "Boyer-Moore, and our wildcard implementation. The performance characteristics "
        "can vary significantly based on pattern complexity, text size, and algorithm "
        "optimization strategies. Pattern matching is a fundamental operation in "
        "computer science with applications ranging from text editors to network "
        "intrusion detection systems.";
    
    const char *patterns[] = {
        "performance",    // Simple word
        "Pattern",        // Case sensitive
        "a*gorithm",      // Wildcard
        "*matching*",      // Wildcard contains
        "????????",       // Exact length wildcard
        "xyz",            // Non-existent
        NULL
    };
    
    int iterations = 100000;
    
    for (int i = 0; patterns[i]; i++) {
        printf("Pattern: \"%s\"\n", patterns[i]);
        printf("%-12s %-12s %-15s %-10s %s\n", "Algorithm", "Time(μs)", "Avg(μs/search)", "Success", "Notes");
        printf("%-12s %-12s %-15s %-10s %s\n", "--------", "-------", "-------------", "------", "-----");
        
        // Test KMP (non-wildcard only)
        if (!strchr(patterns[i], '*') && !strchr(patterns[i], '?')) {
            struct result r = test_algorithm("KMP", "kmp", patterns[i], 0, text, iterations,
                                             init_kmp, exit_kmp);
            printf("%-12s %-12lld %-15.3f %-10d %s\n", 
                   r.algorithm, r.time_us, r.avg_us, r.success, 
                   r.success ? "Found" : "Not found");
        }
        
        // Test BM (non-wildcard only)  
        if (!strchr(patterns[i], '*') && !strchr(patterns[i], '?')) {
            struct result r = test_algorithm("BM", "bm", patterns[i], 0, text, iterations,
                                             init_bm, exit_bm);
            printf("%-12s %-12lld %-15.3f %-10d %s\n", 
                   r.algorithm, r.time_us, r.avg_us, r.success,
                   r.success ? "Found" : "Not found");
        }
        
        // Test Wildcard (all patterns)
        int flags = (strchr(patterns[i], '*') || strchr(patterns[i], '?')) ? TS_WILDCARD : 0;
        struct result r = test_algorithm("WILDCARD", "wildcard", patterns[i], flags, text, iterations,
                                         init_wildcard, exit_wildcard);
        printf("%-12s %-12lld %-15.3f %-10d %s\n", 
               r.algorithm, r.time_us, r.avg_us, r.success,
               flags ? "Wildcard" : (r.success ? "Found" : "Not found"));
        
        printf("\n");
    }
    
    return 0;
}