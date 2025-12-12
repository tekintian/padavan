/*
 * Router-optimized text search implementation
 * Optimized for URL matching in resource-constrained environments
 */

#include "textsearch.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Router-optimized configuration types */
enum router_pattern_type {
    ROUTER_EXACT_DOMAIN,    // exact domain match: "google.com"
    ROUTER_SUBDOMAIN,       // subdomain match: "*.google.com"
    ROUTER_PREFIX,          // prefix match: "/api/*"
    ROUTER_SIMPLE_CONTAINS, // simple contains: "*video*"
    ROUTER_EXACT_URL        // exact URL match
};

struct ts_router
{
    enum router_pattern_type type;
    uint8_t *pattern;
    unsigned int pattern_len;
    uint8_t *domain_part;   // extracted domain for domain-based matching
    unsigned int domain_len;
    uint8_t *path_part;     // extracted path for path-based matching
    unsigned int path_len;
    int case_insensitive;
};

/* Extract domain from URL */
static char* extract_domain(const char *url, unsigned int *domain_len)
{
    if (!url) return NULL;
    
    const char *start = url;
    const char *end = NULL;
    
    // Skip protocol
    if (strncmp(url, "http://", 7) == 0) {
        start = url + 7;
    } else if (strncmp(url, "https://", 8) == 0) {
        start = url + 8;
    }
    
    // Find end of domain (first '/' or end of string)
    end = strchr(start, '/');
    if (!end) {
        end = start + strlen(start);
    }
    
    *domain_len = end - start;
    char *domain = malloc(*domain_len + 1);
    if (domain) {
        memcpy(domain, start, *domain_len);
        domain[*domain_len] = '\0';
    }
    
    return domain;
}

/* Extract path from URL */
static char* extract_path(const char *url, unsigned int *path_len)
{
    if (!url) return NULL;
    
    const char *path_start = strchr(url, '/');
    if (!path_start) {
        *path_len = 0;
        return strdup("");
    }
    
    // Skip first '/' to get actual path
    path_start++;
    *path_len = strlen(path_start);
    
    return strdup(path_start);
}

/* Parse pattern and determine type */
static enum router_pattern_type parse_pattern_type(const char *pattern)
{
    if (!pattern) return ROUTER_EXACT_URL;
    
    // Exact domain: "google.com" or "www.google.com"
    if (strchr(pattern, '*') == NULL && strchr(pattern, '?') == NULL) {
        // Check if it looks like a domain (contains dots but no slashes)
        if (strchr(pattern, '.') && !strchr(pattern, '/')) {
            return ROUTER_EXACT_DOMAIN;
        }
        return ROUTER_EXACT_URL;
    }
    
    // Subdomain pattern: "*.domain.com"
    if (strncmp(pattern, "*.", 2) == 0) {
        return ROUTER_SUBDOMAIN;
    }
    
    // Prefix pattern: "/path/*"
    if (pattern[strlen(pattern) - 1] == '*') {
        return ROUTER_PREFIX;
    }
    
    // Default to simple contains
    return ROUTER_SIMPLE_CONTAINS;
}

/* Optimized exact domain match */
static int match_exact_domain(struct ts_router *router, const char *domain)
{
    if (router->case_insensitive) {
        return strcasecmp(domain, (char*)router->domain_part) == 0;
    } else {
        return strcmp(domain, (char*)router->domain_part) == 0;
    }
}

/* Optimized subdomain match */
static int match_subdomain(struct ts_router *router, const char *domain)
{
    const char *needle = (char*)router->domain_part; // domain_part without "*."
    const char *found = strstr(domain, needle);
    
    if (!found) return 0;
    
    // Check that needle is at the end or followed by a dot
    size_t needle_len = router->domain_len;
    if (found[needle_len] == '\0' || found[needle_len] == '.') {
        return 1;
    }
    
    return 0;
}

/* Optimized prefix match */
static int match_prefix(struct ts_router *router, const char *path)
{
    if (router->case_insensitive) {
        return strncasecmp(path, (char*)router->path_part, 
                          router->path_len - 1) == 0; // -1 to exclude '*'
    } else {
        return strncmp(path, (char*)router->path_part, 
                      router->path_len - 1) == 0; // -1 to exclude '*'
    }
}

/* Simple contains match */
static int match_simple_contains(struct ts_router *router, const char *url)
{
    if (router->case_insensitive) {
        // Simple case-insensitive contains (slower)
        char *url_lower = strdup(url);
        char *pattern_lower = strdup((char*)router->pattern);
        
        if (url_lower && pattern_lower) {
            for (char *p = url_lower; *p; p++) *p = tolower(*p);
            for (char *p = pattern_lower; *p; p++) *p = tolower(*p);
            
            int result = strstr(url_lower, pattern_lower) != NULL;
            free(url_lower);
            free(pattern_lower);
            return result;
        }
        
        free(url_lower);
        free(pattern_lower);
        return 0;
    } else {
        return strstr(url, (char*)router->pattern) != NULL;
    }
}

/* Router-optimized find implementation */
static unsigned int router_find(struct ts_config *conf, struct ts_state *state)
{
    struct ts_router *router = ts_config_priv(conf);
    unsigned int i, text_len, consumed = state->offset;
    const uint8_t *text;
    int match = 0;

    for (;;) {
        text_len = conf->get_next_block(consumed, &text, conf, state);

        if (text_len == 0)
            break;

        // Convert to null-terminated string for easier processing
        char *url_str = malloc(text_len + 1);
        if (!url_str) return UINT_MAX;
        
        memcpy(url_str, text, text_len);
        url_str[text_len] = '\0';

        switch (router->type) {
            case ROUTER_EXACT_DOMAIN: {
                char *domain = extract_domain(url_str, &(unsigned int){0});
                if (domain) {
                    match = match_exact_domain(router, domain);
                    free(domain);
                }
                break;
            }
            case ROUTER_SUBDOMAIN: {
                char *domain = extract_domain(url_str, &(unsigned int){0});
                if (domain) {
                    match = match_subdomain(router, domain);
                    free(domain);
                }
                break;
            }
            case ROUTER_PREFIX: {
                char *path = extract_path(url_str, &(unsigned int){0});
                if (path) {
                    match = match_prefix(router, path);
                    free(path);
                }
                break;
            }
            case ROUTER_SIMPLE_CONTAINS:
                match = match_simple_contains(router, url_str);
                break;
            case ROUTER_EXACT_URL:
                if (router->case_insensitive) {
                    match = strcasecmp(url_str, (char*)router->pattern) == 0;
                } else {
                    match = strcmp(url_str, (char*)router->pattern) == 0;
                }
                break;
        }

        free(url_str);

        if (match) {
            state->offset = consumed + text_len;
            return consumed;
        }

        consumed += text_len;
    }

    return UINT_MAX;
}

/* Initialize router-optimized configuration */
static struct ts_config *router_init(const void *pattern, unsigned int len,
                                     int gfp_mask, int flags)
{
    struct ts_config *conf;
    struct ts_router *router;
    char *pattern_str = malloc(len + 1);
    
    if (!pattern_str) return NULL;
    memcpy(pattern_str, pattern, len);
    pattern_str[len] = '\0';

    conf = alloc_ts_config(sizeof(*router), gfp_mask);
    if (!conf) {
        free(pattern_str);
        return NULL;
    }

    conf->flags = flags;
    router = ts_config_priv(conf);
    memset(router, 0, sizeof(*router));
    
    router->type = parse_pattern_type(pattern_str);
    router->pattern_len = len;
    router->case_insensitive = flags & TS_IGNORECASE;
    
    // Store pattern
    router->pattern = malloc(len + 1);
    if (router->pattern) {
        memcpy(router->pattern, pattern, len);
        router->pattern[len] = '\0';
    }

    // Extract domain and path parts based on pattern type
    switch (router->type) {
        case ROUTER_EXACT_DOMAIN:
        case ROUTER_SUBDOMAIN: {
            char *domain_part = strdup(pattern_str);
            if (router->type == ROUTER_SUBDOMAIN && domain_part) {
                // Remove "*." prefix
                if (strncmp(domain_part, "*.", 2) == 0) {
                    memmove(domain_part, domain_part + 2, strlen(domain_part + 2) + 1);
                }
            }
            router->domain_part = (uint8_t*)domain_part;
            router->domain_len = strlen(domain_part);
            break;
        }
        case ROUTER_PREFIX: {
            char *path_part = strdup(pattern_str);
            if (path_part) {
                // Remove trailing "*"
                char *star = strrchr(path_part, '*');
                if (star) *star = '\0';
            }
            router->path_part = (uint8_t*)path_part;
            router->path_len = strlen(path_part);
            break;
        }
        case ROUTER_SIMPLE_CONTAINS:
            // Remove wildcards from contains pattern
            {
                char *clean_pattern = strdup(pattern_str);
                if (clean_pattern) {
                    char *dst = clean_pattern;
                    for (const char *src = clean_pattern; *src; src++) {
                        if (*src != '*') {
                            *dst++ = *src;
                        }
                    }
                    *dst = '\0';
                    free(router->pattern);
                    router->pattern = (uint8_t*)strdup(clean_pattern);
                    router->pattern_len = strlen(clean_pattern);
                }
                free(clean_pattern);
            }
            break;
        case ROUTER_EXACT_URL:
            // Use pattern as-is
            break;
    }

    free(pattern_str);
    return conf;
}

static void *router_get_pattern(struct ts_config *conf)
{
    struct ts_router *router = ts_config_priv(conf);
    return router->pattern;
}

static unsigned int router_get_pattern_len(struct ts_config *conf)
{
    struct ts_router *router = ts_config_priv(conf);
    return router->pattern_len;
}

static void router_destroy(struct ts_config *conf)
{
    struct ts_router *router = ts_config_priv(conf);
    if (router) {
        if (router->pattern) free(router->pattern);
        if (router->domain_part) free(router->domain_part);
        if (router->path_part) free(router->path_part);
    }
}

static struct ts_ops router_ops = {
    .name              = "router",
    .find              = router_find,
    .init              = router_init,
    .destroy           = router_destroy,
    .get_pattern       = router_get_pattern,
    .get_pattern_len   = router_get_pattern_len,
    .owner             = NULL,
    .next              = NULL
};

/* Initialize router-optimized algorithm */
void init_router(void)
{
    textsearch_register(&router_ops);
}

/* Cleanup router-optimized algorithm */
void exit_router(void)
{
    textsearch_unregister(&router_ops);
}