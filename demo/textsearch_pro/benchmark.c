/*
 * Performance benchmark for textsearch algorithms
 * Compares KMP, Boyer-Moore, and Wildcard algorithms under various conditions
 */

#include "textsearch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>

/* External algorithm initialization functions */
extern void init_kmp(void);
extern void exit_kmp(void);
extern void init_bm(void);
extern void exit_bm(void);
extern void init_wildcard(void);
extern void exit_wildcard(void);

/* Get current time in microseconds */
static long long get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
}

/* Generate random text data */
static char *generate_text(size_t length, int use_letters_only)
{
    char *text = malloc(length + 1);
    if (!text) return NULL;
    
    const char *charset = use_letters_only ? 
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ " :
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 !@#$%^&*()_+-=[]{}|;':\",./<>?";
    
    size_t charset_len = strlen(charset);
    
    for (size_t i = 0; i < length; i++) {
        text[i] = charset[rand() % charset_len];
    }
    text[length] = '\0';
    return text;
}

/* Test structure */
struct test_case {
    const char *pattern;
    const char *description;
    int flags;
};

/* Benchmark result structure */
struct benchmark_result {
    const char *algorithm;
    const char *pattern;
    long long total_time_us;
    double avg_time_us;
    unsigned int iterations;
    int success_count;
};

/* Run benchmark for a specific algorithm and pattern */
static struct benchmark_result benchmark_algorithm(
    const char *algo_name, 
    const char *registered_name,
    const char *pattern,
    int flags,
    const char *text,
    unsigned int text_len,
    unsigned int iterations,
    void (*init_func)(void),
    void (*exit_func)(void))
{
    struct benchmark_result result = {0};
    struct ts_config *conf;
    struct ts_state state;
    unsigned int pos;
    long long start_time, end_time;
    
    result.algorithm = algo_name;
    result.pattern = pattern;
    result.iterations = iterations;
    
    /* Initialize algorithm */
    init_func();
    
    /* Prepare search configuration */
    conf = textsearch_prepare(registered_name, pattern, strlen(pattern), 0, flags);
    if (!conf) {
        printf("ERROR: Failed to prepare %s for pattern \"%s\"\n", algo_name, pattern);
        exit_func();
        return result;
    }
    
    /* Warm up run */
    textsearch_find_continuous(conf, &state, text, text_len);
    
    /* Benchmark runs */
    start_time = get_time_us();
    for (unsigned int i = 0; i < iterations; i++) {
        pos = textsearch_find_continuous(conf, &state, text, text_len);
        if (pos != UINT_MAX) {
            result.success_count++;
        }
    }
    end_time = get_time_us();
    
    result.total_time_us = end_time - start_time;
    result.avg_time_us = (double)result.total_time_us / iterations;
    
    /* Cleanup */
    textsearch_destroy(conf);
    exit_func();
    
    return result;
}

/* Print benchmark results in table format */
static void print_results_table(struct benchmark_result results[], int count)
{
    printf("\n%-12s %-15s %-10s %-15s %-10s %s\n", 
           "Algorithm", "Pattern", "Iter", "Total (μs)", "Avg (μs)", "Success");
    printf("%-12s %-15s %-10s %-15s %-10s %s\n", 
           "----------", "-------", "----", "-----------", "--------", "------");
    
    for (int i = 0; i < count; i++) {
        printf("%-12s %-15s %-10u %-15lld %-10.3f %d\n",
               results[i].algorithm,
               results[i].pattern,
               results[i].iterations,
               results[i].total_time_us,
               results[i].avg_time_us,
               results[i].success_count);
    }
}

/* Stress test with different text sizes */
static void stress_test_text_sizes(void)
{
    printf("\n============================================================\n");
    printf("STRESS TEST: Different Text Sizes\n");
    printf("============================================================\n");
    
    size_t text_sizes[] = {1000, 10000, 100000, 1000000};
    const char *pattern = "target";
    int num_sizes = sizeof(text_sizes) / sizeof(text_sizes[0]);
    
    for (int i = 0; i < num_sizes; i++) {
        printf("\nText size: %zu bytes\n", text_sizes[i]);
        
        /* Generate test text */
        char *text = generate_text(text_sizes[i], 1);
        if (!text) {
            printf("ERROR: Failed to generate text\n");
            continue;
        }
        
        /* Insert some targets */
        for (size_t j = 0; j < text_sizes[i]; j += text_sizes[i] / 10) {
            if (j + 6 < text_sizes[i]) {
                strncpy(text + j, "target", 6);
            }
        }
        
        /* Benchmark algorithms */
        struct benchmark_result results[3];
        unsigned int iterations = text_sizes[i] > 100000 ? 1000 : 10000;
        
        results[0] = benchmark_algorithm("KMP", "kmp", pattern, 0, 
                                        text, text_sizes[i], iterations, 
                                        init_kmp, exit_kmp);
        results[1] = benchmark_algorithm("BM", "bm", pattern, 0, 
                                        text, text_sizes[i], iterations, 
                                        init_bm, exit_bm);
        results[2] = benchmark_algorithm("WILDCARD", "wildcard", pattern, 0, 
                                        text, text_sizes[i], iterations, 
                                        init_wildcard, exit_wildcard);
        
        print_results_table(results, 3);
        free(text);
    }
}

/* Stress test with different pattern types */
static void stress_test_patterns(void)
{
    printf("\n============================================================\n");
    printf("STRESS TEST: Different Pattern Types\n");
    printf("============================================================\n");
    
    /* Generate large text */
    size_t text_len = 100000;
    char *text = generate_text(text_len, 1);
    if (!text) {
        printf("ERROR: Failed to generate text\n");
        return;
    }
    
    /* Insert some specific patterns */
    strncpy(text + 1000, "testing", 7);
    strncpy(text + 5000, "algorithm", 9);
    strncpy(text + 10000, "performance", 11);
    
    struct test_case test_patterns[] = {
        {"testing", "Simple word", 0},
        {"algorithm", "Simple word 2", 0},
        {"performance", "Simple word 3", 0},
        {"*", "Wildcard - match all", TS_WILDCARD},
        {"t*", "Wildcard - starts with t", TS_WILDCARD},
        {"*ing", "Wildcard - ends with ing", TS_WILDCARD},
        {"*ing", "Wildcard - contains ing", TS_WILDCARD},
        {"t*ing", "Wildcard - starts with t ends with ing", TS_WILDCARD},
        {"??????", "Wildcard - exactly 6 chars", TS_WILDCARD},
        {"noexistpattern", "Non-existent pattern", 0},
        {"*noexist*", "Wildcard non-existent", TS_WILDCARD},
        {NULL, NULL, 0}
    };
    
    int num_patterns = sizeof(test_patterns) / sizeof(test_patterns[0]) - 1;
    struct benchmark_result results[3 * num_patterns];
    int result_count = 0;
    
    for (int i = 0; i < num_patterns; i++) {
        printf("\nTesting pattern: \"%s\" (%s)\n", 
               test_patterns[i].pattern, test_patterns[i].description);
        
        /* Test KMP (only for non-wildcard patterns) */
        if (!(test_patterns[i].flags & TS_WILDCARD)) {
            results[result_count++] = benchmark_algorithm(
                "KMP", "kmp", test_patterns[i].pattern, test_patterns[i].flags,
                text, text_len, 10000, init_kmp, exit_kmp);
        }
        
        /* Test BM (only for non-wildcard patterns) */
        if (!(test_patterns[i].flags & TS_WILDCARD)) {
            results[result_count++] = benchmark_algorithm(
                "BM", "bm", test_patterns[i].pattern, test_patterns[i].flags,
                text, text_len, 10000, init_bm, exit_bm);
        }
        
        /* Test Wildcard (for all patterns) */
        results[result_count++] = benchmark_algorithm(
            "WILDCARD", "wildcard", test_patterns[i].pattern, test_patterns[i].flags,
            text, text_len, 10000, init_wildcard, exit_wildcard);
        
        /* Print current results */
        if (result_count > 0) {
            print_results_table(results + result_count - (test_patterns[i].flags & TS_WILDCARD ? 1 : 3), 
                               test_patterns[i].flags & TS_WILDCARD ? 1 : 3);
        }
    }
    
    free(text);
}

/* Case sensitivity stress test */
static void stress_test_case_sensitivity(void)
{
    printf("\n============================================================\n");
    printf("STRESS TEST: Case Sensitivity\n");
    printf("============================================================\n");
    
    char *text = strdup("This is a Test text with MIXED case and Patterns. TESTING case sensitivity.");
    size_t text_len = strlen(text);
    const char *pattern = "test";
    
    struct benchmark_result results[6];
    
    /* KMP case sensitive */
    results[0] = benchmark_algorithm("KMP-CS", "kmp", pattern, 0, 
                                    text, text_len, 50000, 
                                    init_kmp, exit_kmp);
    
    /* KMP case insensitive */
    results[1] = benchmark_algorithm("KMP-CI", "kmp", pattern, TS_IGNORECASE, 
                                    text, text_len, 50000, 
                                    init_kmp, exit_kmp);
    
    /* BM case sensitive */
    results[2] = benchmark_algorithm("BM-CS", "bm", pattern, 0, 
                                    text, text_len, 50000, 
                                    init_bm, exit_bm);
    
    /* BM case insensitive */
    results[3] = benchmark_algorithm("BM-CI", "bm", pattern, TS_IGNORECASE, 
                                    text, text_len, 50000, 
                                    init_bm, exit_bm);
    
    /* Wildcard case sensitive */
    results[4] = benchmark_algorithm("WC-CS", "wildcard", pattern, 0, 
                                    text, text_len, 50000, 
                                    init_wildcard, exit_wildcard);
    
    /* Wildcard case insensitive */
    results[5] = benchmark_algorithm("WC-CI", "wildcard", pattern, TS_IGNORECASE, 
                                    text, text_len, 50000, 
                                    init_wildcard, exit_wildcard);
    
    print_results_table(results, 6);
    free(text);
}

/* Performance scaling test */
static void stress_test_scaling(void)
{
    printf("\n============================================================\n");
    printf("STRESS TEST: Performance Scaling\n");
    printf("============================================================\n");
    
    unsigned int iterations[] = {1000, 10000, 100000, 1000000};
    int num_scales = sizeof(iterations) / sizeof(iterations[0]);
    
    char *text = generate_text(10000, 1);
    strncpy(text + 1000, "testpattern", 11);
    
    struct benchmark_result results[4 * num_scales];
    int result_count = 0;
    
    for (int i = 0; i < num_scales; i++) {
        printf("\nIterations: %u\n", iterations[i]);
        
        results[result_count++] = benchmark_algorithm(
            "KMP", "kmp", "testpattern", 0,
            text, 10000, iterations[i], init_kmp, exit_kmp);
        
        results[result_count++] = benchmark_algorithm(
            "BM", "bm", "testpattern", 0,
            text, 10000, iterations[i], init_bm, exit_bm);
        
        results[result_count++] = benchmark_algorithm(
            "WILDCARD", "wildcard", "testpattern", 0,
            text, 10000, iterations[i], init_wildcard, exit_wildcard);
        
        results[result_count++] = benchmark_algorithm(
            "WILDCARD-*", "wildcard", "test*", TS_WILDCARD,
            text, 10000, iterations[i], init_wildcard, exit_wildcard);
    }
    
    print_results_table(results, result_count);
    free(text);
}

int main(void)
{
    printf("Textsearch Algorithm Stress Test & Benchmark\n");
    printf("===========================================\n");
    
    /* Seed random number generator */
    srand((unsigned int)time(NULL));
    
    /* Run all stress tests */
    stress_test_text_sizes();
    stress_test_patterns();
    stress_test_case_sensitivity();
    stress_test_scaling();
    
    printf("\n============================================================\n");
    printf("STRESS TEST COMPLETE\n");
    printf("============================================================\n");
    
    return 0;
}