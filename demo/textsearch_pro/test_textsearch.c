/*
 * Textsearch Demo Test Program
 * Tests the KMP and Boyer-Moore string search algorithms
 */

#include "textsearch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/* External algorithm initialization functions */
extern void init_kmp(void);
extern void exit_kmp(void);
extern void init_bm(void);
extern void exit_bm(void);
extern void init_wildcard(void);
extern void exit_wildcard(void);

/* Test data */
static const char *test_text = 
    "This is a sample text for testing string search algorithms. "
    "The quick brown fox jumps over the lazy dog. "
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
    "Pattern matching is an important computer science concept.";

static const char *test_patterns[] = {
    "text",
    "Pattern", 
    "quick",
    "fox",
    "algorithms",
    "notfound",
    "*",
    "???",
    "t*xt",
    "qu?ck",
    "P*",
    "*ing",
    "a*n",
    NULL
};

/* Get current time in microseconds */
static long long get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

/* Test a single algorithm */
static void test_algorithm(const char *algo_name, const char *registered_name, void (*init_func)(void), void (*exit_func)(void), int test_wildcards)
{
    struct ts_config *conf;
    struct ts_state state;
    unsigned int pos;
    int i;
    long long start_time, end_time;
    
    printf("\n=== Testing %s Algorithm ===\n", algo_name);
    
    /* Initialize algorithm */
    init_func();
    
    /* Test each pattern */
    for (i = 0; test_patterns[i]; i++) {
        const char *pattern = test_patterns[i];
        int flags = 0;
        int is_wildcard = (strchr(pattern, '*') || strchr(pattern, '?'));
        
        /* Skip wildcards for non-wildcard algorithms */
        if (!test_wildcards && is_wildcard)
            continue;
        
        /* Add wildcard flag for wildcard patterns */
        if (test_wildcards && is_wildcard)
            flags |= TS_WILDCARD;
        
        printf("\nPattern: \"%s\"%s\n", pattern, is_wildcard ? " (wildcard)" : "");
        
        /* Prepare search configuration */
        conf = textsearch_prepare(registered_name, pattern, strlen(pattern), 0, flags);
        if (!conf) {
            printf("  ERROR: Failed to prepare search config\n");
            continue;
        }
        
        printf("  Config prepared successfully, pattern length: %zu\n", strlen(pattern));
        
        /* Perform search */
        start_time = get_time_us();
        pos = textsearch_find_continuous(conf, &state, test_text, strlen(test_text));
        end_time = get_time_us();
        
        if (pos != UINT_MAX) {
            printf("  MATCH: Found at position %u\n", pos);
            printf("  Context: \"%.20s\"\n", test_text + pos);
        } else {
            printf("  NO MATCH: Pattern not found\n");
        }
        
        printf("  Time: %lld microseconds\n", end_time - start_time);
        
        /* Cleanup */
        textsearch_destroy(conf);
    }
    
    /* Test case-insensitive search */
    printf("\nCase-insensitive test with pattern: \"PATTERN\"\n");
    conf = textsearch_prepare(registered_name, "PATTERN", strlen("PATTERN"), 0, TS_IGNORECASE);
    if (conf) {
        printf("  Config prepared successfully\n");
        start_time = get_time_us();
        pos = textsearch_find_continuous(conf, &state, test_text, strlen(test_text));
        end_time = get_time_us();
        
        if (pos != UINT_MAX) {
            printf("  MATCH: Found at position %u\n", pos);
            printf("  Context: \"%.20s\"\n", test_text + pos);
        } else {
            printf("  NO MATCH: Pattern not found\n");
        }
        
        printf("  Time: %lld microseconds\n", end_time - start_time);
        textsearch_destroy(conf);
    } else {
        printf("  ERROR: Failed to prepare case-insensitive search config\n");
    }
    
    /* Cleanup algorithm */
    exit_func();
}

/* Test wildcard algorithm specifically */
static void test_wildcard_algorithm(void)
{
    struct ts_config *conf;
    struct ts_state state;
    unsigned int pos;
    int i;
    long long start_time, end_time;
    
    printf("\n=== Testing Wildcard Algorithm ===\n");
    
    /* Initialize algorithm */
    init_wildcard();
    
    /* Test patterns including wildcards */
    for (i = 0; test_patterns[i]; i++) {
        const char *pattern = test_patterns[i];
        int flags = 0;
        int is_wildcard = (strchr(pattern, '*') || strchr(pattern, '?'));
        
        /* Add wildcard flag for wildcard patterns */
        if (is_wildcard)
            flags |= TS_WILDCARD;
        
        printf("\nPattern: \"%s\"%s\n", pattern, is_wildcard ? " (wildcard)" : "");
        
        /* Test normal search */
        conf = textsearch_prepare("wildcard", pattern, strlen(pattern), 0, flags);
        if (!conf) {
            printf("  ERROR: Failed to prepare search config\n");
            continue;
        }
        
        start_time = get_time_us();
        pos = textsearch_find_continuous(conf, &state, test_text, strlen(test_text));
        end_time = get_time_us();
        
        if (pos != UINT_MAX) {
            printf("  MATCH: Found at position %u\n", pos);
            printf("  Context: \"%.20s\"\n", test_text + pos);
        } else {
            printf("  NO MATCH: Pattern not found\n");
        }
        
        printf("  Time: %lld microseconds\n", end_time - start_time);
        textsearch_destroy(conf);
        
        /* Test case-insensitive wildcard search */
        if (is_wildcard) {
            conf = textsearch_prepare("wildcard", pattern, strlen(pattern), 0, flags | TS_IGNORECASE);
            if (conf) {
                start_time = get_time_us();
                pos = textsearch_find_continuous(conf, &state, test_text, strlen(test_text));
                end_time = get_time_us();
                
                printf("  Case-insensitive: ");
                if (pos != UINT_MAX) {
                    printf("MATCH at position %u\n", pos);
                } else {
                    printf("NO MATCH\n");
                }
                printf("  Time: %lld microseconds\n", end_time - start_time);
                textsearch_destroy(conf);
            }
        }
    }
    
    /* Cleanup algorithm */
    exit_wildcard();
}

/* Performance comparison */
static void performance_comparison(void)
{
    struct ts_config *kmp_conf, *bm_conf, *wild_conf;
    struct ts_state state;
    const char *pattern = "algorithms";
    const char *wild_pattern = "a*";
    long long start_time, end_time;
    unsigned int pos;
    int iterations = 10000;
    
    printf("\n=== Performance Comparison (%d iterations) ===\n", iterations);
    printf("Pattern: \"%s\"\n", pattern);
    printf("Wildcard Pattern: \"%s\"\n", wild_pattern);
    printf("Text length: %zu bytes\n", strlen(test_text));
    
    /* Initialize algorithms */
    init_kmp();
    init_bm();
    init_wildcard();
    
    /* Prepare configurations */
    kmp_conf = textsearch_prepare("kmp", pattern, strlen(pattern), 0, 0);
    bm_conf = textsearch_prepare("bm", pattern, strlen(pattern), 0, 0);
    wild_conf = textsearch_prepare("wildcard", wild_pattern, strlen(wild_pattern), 0, TS_WILDCARD);
    
    if (!kmp_conf || !bm_conf || !wild_conf) {
        printf("ERROR: Failed to prepare search configurations\n");
        return;
    }
    
    /* Test KMP performance */
    start_time = get_time_us();
    for (int i = 0; i < iterations; i++) {
        pos = textsearch_find_continuous(kmp_conf, &state, test_text, strlen(test_text));
    }
    end_time = get_time_us();
    printf("KMP:      %lld microseconds total (%.2f us per search)\n", 
           end_time - start_time, (double)(end_time - start_time) / iterations);
    
    /* Test Boyer-Moore performance */
    start_time = get_time_us();
    for (int i = 0; i < iterations; i++) {
        pos = textsearch_find_continuous(bm_conf, &state, test_text, strlen(test_text));
    }
    end_time = get_time_us();
    printf("BM:       %lld microseconds total (%.2f us per search)\n", 
           end_time - start_time, (double)(end_time - start_time) / iterations);
    
    /* Test Wildcard performance */
    start_time = get_time_us();
    for (int i = 0; i < iterations; i++) {
        pos = textsearch_find_continuous(wild_conf, &state, test_text, strlen(test_text));
    }
    end_time = get_time_us();
    printf("WILDCARD: %lld microseconds total (%.2f us per search)\n", 
           end_time - start_time, (double)(end_time - start_time) / iterations);
    
    /* Cleanup */
    textsearch_destroy(kmp_conf);
    textsearch_destroy(bm_conf);
    textsearch_destroy(wild_conf);
    exit_kmp();
    exit_bm();
    exit_wildcard();
}

int main(void)
{
    printf("Textsearch Algorithm Demo\n");
    printf("========================\n");
    printf("Test text length: %zu bytes\n", strlen(test_text));
    printf("Test text: \"%s\"\n", test_text);
    
    /* Test KMP algorithm */
    test_algorithm("KMP", "kmp", init_kmp, exit_kmp, 0);
    
    /* Test Boyer-Moore algorithm */
    test_algorithm("Boyer-Moore", "bm", init_bm, exit_bm, 0);
    
    /* Test Wildcard algorithm */
    test_wildcard_algorithm();
    
    /* Performance comparison */
    performance_comparison();
    
    printf("\n=== Demo Complete ===\n");
    return 0;
}