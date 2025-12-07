/* SNI matching match for iptables
 *
 * Based on xt_string.c by Pablo Neira Ayuso <pablo@eurodev.net>
 * Modified for SNI matching functionality
 * Enhanced with router-optimized matching strategies
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_sni.h>
#include <linux/textsearch.h>
#include <linux/string.h>
#include <linux/ctype.h>

MODULE_AUTHOR("SNI Module");
MODULE_DESCRIPTION("Xtables: SNI-based matching with router optimizations");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ipt_sni");
MODULE_ALIAS("ip6t_sni");

/* Router-optimized SNI matching strategies */
enum sni_pattern_type {
	SNI_EXACT_DOMAIN,       // exact domain match: "google.com"
	SNI_SUBDOMAIN,          // subdomain match: "*.google.com"  
	SNI_CONTAINS,           // simple contains: "video"
	SNI_WILDCARD,           // wildcard pattern: "*.gov.cn"
	SNI_SIMPLE_CONTAINS     // case-insensitive contains
};

/* Enhanced SNI info structure */
struct sni_router_config {
	enum sni_pattern_type type;
	char *domain_part;          // extracted domain for exact matching
	unsigned int domain_len;
	char *pattern_clean;        // cleaned pattern without wildcards
	unsigned int pattern_len;
	int case_insensitive;
};

/* Helper function to determine pattern type */
static enum sni_pattern_type sni_parse_pattern_type(const char *pattern, unsigned int len)
{
	if (!pattern || len == 0)
		return SNI_CONTAINS;
	
	/* Exact domain: no wildcards, contains dots */
	if (!strchr(pattern, '*') && !strchr(pattern, '?') && strchr(pattern, '.'))
		return SNI_EXACT_DOMAIN;
	
	/* Subdomain pattern: starts with "*." */
	if (len > 2 && pattern[0] == '*' && pattern[1] == '.')
		return SNI_SUBDOMAIN;
	
	/* Wildcard pattern: contains "*." pattern */
	if (strstr(pattern, "*.") != NULL)
		return SNI_WILDCARD;
	
	/* Default to simple contains */
	return SNI_SIMPLE_CONTAINS;
}

/* Extract clean domain from pattern */
static char* sni_extract_domain_part(const char *pattern, unsigned int len, 
				     enum sni_pattern_type type, unsigned int *domain_len)
{
	char *domain;
	const char *start;
	size_t domain_length;
	
	switch (type) {
	case SNI_SUBDOMAIN:
		/* Skip "*." prefix */
		if (len > 2 && pattern[0] == '*' && pattern[1] == '.') {
			start = pattern + 2;
			domain_length = len - 2;
		} else {
			start = pattern;
			domain_length = len;
		}
		break;
		
	case SNI_EXACT_DOMAIN:
		start = pattern;
		domain_length = len;
		break;
		
	case SNI_WILDCARD:
		/* Extract the part after first "*." */
		{
			const char *dot_pos = strstr(pattern, "*.");
			if (dot_pos) {
				start = dot_pos + 2;
				domain_length = len - (start - pattern);
			} else {
				start = pattern;
				domain_length = len;
			}
		}
		break;
		
	default:
		*domain_len = 0;
		return NULL;
	}
	
	domain = kmalloc(domain_length + 1, GFP_KERNEL);
	if (domain) {
		memcpy(domain, start, domain_length);
		domain[domain_length] = '\0';
		*domain_len = domain_length;
	}
	
	return domain;
}

/* Optimized exact domain matching */
static bool sni_match_exact_domain(const char *sni_domain, const char *pattern_domain, 
				  unsigned int pattern_len, int case_insensitive)
{
	if (case_insensitive) {
		return strncasecmp(sni_domain, pattern_domain, pattern_len) == 0 &&
		       strlen(sni_domain) == pattern_len;
	} else {
		return strncmp(sni_domain, pattern_domain, pattern_len) == 0 &&
		       strlen(sni_domain) == pattern_len;
	}
}

/* Optimized subdomain matching */
static bool sni_match_subdomain(const char *sni_domain, const char *pattern_domain,
				unsigned int pattern_len, int case_insensitive)
{
	const char *found;
	unsigned int sni_len = strlen(sni_domain);
	
	if (case_insensitive) {
		/* Simple approach: convert to lowercase for comparison */
		char *sni_lower = kmalloc(sni_len + 1, GFP_KERNEL);
		char *pattern_lower = kmalloc(pattern_len + 1, GFP_KERNEL);
		bool result = false;
		
		if (sni_lower && pattern_lower) {
			int i;
			for (i = 0; i < sni_len; i++)
				sni_lower[i] = tolower(sni_domain[i]);
			sni_lower[i] = '\0';
			
			for (i = 0; i < pattern_len; i++)
				pattern_lower[i] = tolower(pattern_domain[i]);
			pattern_lower[i] = '\0';
			
			found = strstr(sni_lower, pattern_lower);
			if (found) {
				/* Check that pattern is at end or followed by dot */
				if (found[pattern_len] == '\0' || found[pattern_len] == '.') {
					result = true;
				}
			}
		}
		
		kfree(sni_lower);
		kfree(pattern_lower);
		return result;
	} else {
		found = strstr(sni_domain, pattern_domain);
		if (!found) return false;
		
		/* Check that pattern is at end or followed by dot */
		if (found[pattern_len] == '\0' || found[pattern_len] == '.') {
			return true;
		}
		
		return false;
	}
}

/* Optimized contains matching */
static bool sni_match_contains(const char *sni_domain, const char *pattern,
			       unsigned int pattern_len, int case_insensitive)
{
	if (case_insensitive) {
		/* Case-insensitive contains match */
		char *sni_lower = kmalloc(strlen(sni_domain) + 1, GFP_KERNEL);
		char *pattern_lower = kmalloc(pattern_len + 1, GFP_KERNEL);
		bool result = false;
		
		if (sni_lower && pattern_lower) {
			int i;
			for (i = 0; sni_domain[i]; i++)
				sni_lower[i] = tolower(sni_domain[i]);
			sni_lower[i] = '\0';
			
			for (i = 0; i < pattern_len; i++)
				pattern_lower[i] = tolower(pattern[i]);
			pattern_lower[i] = '\0';
			
			result = strstr(sni_lower, pattern_lower) != NULL;
		}
		
		kfree(sni_lower);
		kfree(pattern_lower);
		return result;
	} else {
		return strstr(sni_domain, pattern) != NULL;
	}
}

/* Router-optimized SNI matching function */
static bool sni_router_match(const char *sni_data, unsigned int sni_len,
			      struct sni_router_config *router_config)
{
	char *sni_domain;
	bool result = false;
	
	/* Extract domain from SNI data (SNI typically contains just the hostname) */
	sni_domain = kmalloc(sni_len + 1, GFP_KERNEL);
	if (!sni_domain)
		return false;
		
	memcpy(sni_domain, sni_data, sni_len);
	sni_domain[sni_len] = '\0';
	
	switch (router_config->type) {
	case SNI_EXACT_DOMAIN:
		result = sni_match_exact_domain(sni_domain, router_config->domain_part,
						router_config->domain_len, 
						router_config->case_insensitive);
		break;
		
	case SNI_SUBDOMAIN:
	case SNI_WILDCARD:
		result = sni_match_subdomain(sni_domain, router_config->domain_part,
					     router_config->domain_len,
					     router_config->case_insensitive);
		break;
		
	case SNI_CONTAINS:
	case SNI_SIMPLE_CONTAINS:
		result = sni_match_contains(sni_domain, router_config->pattern_clean,
					    router_config->pattern_len,
					    router_config->case_insensitive);
		break;
	}
	
	kfree(sni_domain);
	return result;
}

/* Router-optimized textsearch operations */
static struct ts_config *sni_router_init(const void *pattern, unsigned int len,
					  int gfp_mask, int flags)
{
	struct ts_config *conf;
	struct sni_router_config *router;
	char *pattern_str;
	
	conf = alloc_ts_config(sizeof(*router), gfp_mask);
	if (!conf)
		return NULL;
	
	conf->flags = flags;
	router = ts_config_priv(conf);
	memset(router, 0, sizeof(*router));
	
	/* Convert pattern to null-terminated string */
	pattern_str = kmalloc(len + 1, GFP_KERNEL);
	if (!pattern_str) {
		kfree(conf);
		return NULL;
	}
	memcpy(pattern_str, pattern, len);
	pattern_str[len] = '\0';
	
	/* Parse pattern type and extract components */
	router->type = sni_parse_pattern_type(pattern_str, len);
	router->case_insensitive = flags & TS_IGNORECASE;
	router->pattern_len = len;
	
	/* Store clean pattern */
	router->pattern_clean = kmalloc(len + 1, GFP_KERNEL);
	if (router->pattern_clean) {
		memcpy(router->pattern_clean, pattern, len);
		router->pattern_clean[len] = '\0';
	}
	
	/* Extract domain part for domain-based matching */
	router->domain_part = sni_extract_domain_part(pattern_str, len, 
						      router->type, 
						      &router->domain_len);
	
	kfree(pattern_str);
	return conf;
}

static void sni_router_destroy(struct ts_config *conf)
{
	struct sni_router_config *router = ts_config_priv(conf);
	if (router) {
		kfree(router->domain_part);
		kfree(router->pattern_clean);
	}
}

static void *sni_router_get_pattern(struct ts_config *conf)
{
	struct sni_router_config *router = ts_config_priv(conf);
	return router->pattern_clean;
}

static unsigned int sni_router_get_pattern_len(struct ts_config *conf)
{
	struct sni_router_config *router = ts_config_priv(conf);
	return router->pattern_len;
}

/* Router-optimized find implementation */
static unsigned int sni_router_find(struct ts_config *conf, struct ts_state *state)
{
	struct sni_router_config *router = ts_config_priv(conf);
	unsigned int consumed = state->offset;
	unsigned int text_len;
	const uint8_t *text;
	bool match = false;
	
	for (;;) {
		text_len = conf->get_next_block(consumed, &text, conf, state);
		if (text_len == 0)
			break;
			
		/* Try router-optimized matching on text block */
		match = sni_router_match((const char*)text, text_len, router);
		
		if (match) {
			state->offset = consumed + text_len;
			return consumed;
		}
		
		consumed += text_len;
	}
	
	return UINT_MAX;
}

/* Router textsearch operations */
static struct ts_ops sni_router_ops = {
	.name              = "router",
	.find              = sni_router_find,
	.init              = sni_router_init,
	.destroy           = sni_router_destroy,
	.get_pattern       = sni_router_get_pattern,
	.get_pattern_len   = sni_router_get_pattern_len,
	.owner             = THIS_MODULE,
};

/* Original SNI matching function with router optimizations */
static bool sni_mt(const struct sk_buff *skb, struct xt_action_param *par)
{
	const struct xt_sni_info *conf = par->matchinfo;
	bool invert;

	invert = conf->u.v1.flags & XT_SNI_FLAG_INVERT;

	/* Try router-optimized matching first if available */
	if (conf->config && conf->config->ops && 
	    strcmp(conf->config->ops->name, "router") == 0) {
		/* Use router-optimized algorithm */
		struct ts_state state;
		unsigned int match_pos;
		
		state.offset = 0;
		match_pos = conf->config->ops->find(conf->config, &state);
		
		return (match_pos != UINT_MAX) ^ invert;
	}
	
	/* Standard textsearch matching as fallback */
	return (skb_find_text((struct sk_buff *)skb, conf->from_offset,
			      conf->to_offset, conf->config)
			      != UINT_MAX) ^ invert;
}

#define SNI_TEXT_PRIV(m) ((struct xt_sni_info *)(m))

static int sni_mt_check(const struct xt_mtchk_param *par)
{
	struct xt_sni_info *conf = par->matchinfo;
	struct ts_config *ts_conf;
	int flags = TS_AUTOLOAD;

	/* Parameter validation */
	if (conf->from_offset > conf->to_offset)
		return -EINVAL;
	if (conf->algo[XT_SNI_MAX_ALGO_NAME_SIZE - 1] != '\0')
		return -EINVAL;
	if (conf->patlen > XT_SNI_MAX_PATTERN_SIZE)
		return -EINVAL;
	if (conf->u.v1.flags &
	    ~(XT_SNI_FLAG_IGNORECASE | XT_SNI_FLAG_INVERT))
		return -EINVAL;
	if (conf->u.v1.flags & XT_SNI_FLAG_IGNORECASE)
		flags |= TS_IGNORECASE;
	
	/* Try router-optimized algorithm if "router" is specified */
	if (strcmp(conf->algo, "router") == 0) {
		/* Use router algorithm */
		ts_conf = sni_router_init(conf->pattern, conf->patlen, GFP_KERNEL, flags);
		if (IS_ERR(ts_conf))
			return PTR_ERR(ts_conf);
			
		/* Set router operations */
		ts_conf->ops = &sni_router_ops;
	} else {
		/* Use standard textsearch algorithm */
		ts_conf = textsearch_prepare(conf->algo, conf->pattern, conf->patlen,
					     GFP_KERNEL, flags);
		if (IS_ERR(ts_conf))
			return PTR_ERR(ts_conf);
	}

	conf->config = ts_conf;
	return 0;
}

static void sni_mt_destroy(const struct xt_mtdtor_param *par)
{
	struct xt_sni_info *conf = par->matchinfo;
	
	if (conf->config && conf->config->ops && 
	    strcmp(conf->config->ops->name, "router") == 0) {
		/* Use router-specific destroy */
		sni_router_destroy(conf->config);
	} else {
		/* Use standard textsearch destroy */
		textsearch_destroy(conf->config);
	}
}

static struct xt_match xt_sni_mt_reg __read_mostly = {
	.name       = "sni",
	.revision   = 1,
	.family     = NFPROTO_UNSPEC,
	.checkentry = sni_mt_check,
	.match      = sni_mt,
	.destroy    = sni_mt_destroy,
	.matchsize  = sizeof(struct xt_sni_info),
	.me         = THIS_MODULE,
};

static int __init sni_mt_init(void)
{
	int ret;
	
	/* Register router-optimized textsearch algorithm */
	ret = textsearch_register(&sni_router_ops);
	if (ret) {
		pr_err("Failed to register router SNI algorithm: %d\n", ret);
		return ret;
	}
	
	/* Register SNI match */
	ret = xt_register_match(&xt_sni_mt_reg);
	if (ret) {
		pr_err("Failed to register SNI match: %d\n", ret);
		textsearch_unregister(&sni_router_ops);
		return ret;
	}
	
	pr_info("SNI match with router optimizations registered\n");
	return 0;
}

static void __exit sni_mt_exit(void)
{
	xt_unregister_match(&xt_sni_mt_reg);
	textsearch_unregister(&sni_router_ops);
	pr_info("SNI match with router optimizations unregistered\n");
}

module_init(sni_mt_init);
module_exit(sni_mt_exit);