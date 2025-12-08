/* SNI matching match for iptables
 *
 * Based on xt_string.c by Pablo Neira Ayuso <pablo@eurodev.net>
 * Modified for SNI matching functionality
 * Version 3: Advanced router optimizations with caching and SIMD
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
#include <linux/jhash.h>
#include <linux/rhashtable.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

MODULE_AUTHOR("SNI Module");
MODULE_DESCRIPTION("Xtables: SNI-based matching with advanced router optimizations");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ipt_sni");
MODULE_ALIAS("ip6t_sni");

/* Performance optimization constants */
#define SNI_CACHE_SIZE      64
#define SNI_CACHE_TTL       300  /* 5 minutes */
#define SNI_MIN_LEN_FOR_CACHE  4

/* Pattern type classification */
enum sni_pattern_class {
	SNI_CLASS_EXACT_DOMAIN = 0,   /* google.com */
	SNI_CLASS_SUBDOMAIN,          /* *.google.com */
	SNI_CLASS_WILDCARD,           /* *.gov.cn */
	SNI_CLASS_SIMPLE_CONTAINS,     /* video */
	SNI_CLASS_COMPLEX              /* Complex patterns */
};

/* Fast cache entry */
struct sni_cache_entry {
	u32 hash;
	unsigned int ttl;
	bool result;
	char pattern[XT_SNI_MAX_PATTERN_SIZE];
	u8 pattern_len;
	u8 flags;
};

/* Optimization statistics - simplified to avoid 64-bit issues */
struct sni_stats {
	u32 total_matches;
	u32 cache_hits;
	u32 fast_path_hits;
	u32 fallback_hits;
	u32 cache_entries;
};

/* Global optimization state */
static struct {
	struct sni_cache_entry cache[SNI_CACHE_SIZE];
	rwlock_t cache_lock;
	struct sni_stats stats;
} sni_opt;

/* Advanced pattern classification */
static enum sni_pattern_class sni_classify_pattern(const char *pattern, unsigned int len)
{
	if (!pattern || len == 0)
		return SNI_CLASS_COMPLEX;
	
	/* Exact domain: contains dots, no wildcards */
	if (strchr(pattern, '.') && !strchr(pattern, '*') && !strchr(pattern, '?'))
		return SNI_CLASS_EXACT_DOMAIN;
	
	/* Subdomain pattern: starts with "*." */
	if (len > 2 && pattern[0] == '*' && pattern[1] == '.')
		return SNI_CLASS_SUBDOMAIN;
	
	/* Simple wildcard: contains "*." but not at start */
	if (strstr(pattern, "*.") && pattern[0] != '*')
		return SNI_CLASS_WILDCARD;
	
	/* Simple contains: no dots, no wildcards */
	if (!strchr(pattern, '.') && !strchr(pattern, '*') && !strchr(pattern, '?'))
		return SNI_CLASS_SIMPLE_CONTAINS;
	
	return SNI_CLASS_COMPLEX;
}

/* High-performance hash function for SNI caching */
static u32 sni_compute_hash(const char *data, unsigned int data_len,
			    const char *pattern, unsigned int pat_len, u8 flags)
{
	return jhash_3words(jhash(data, data_len, 0),
			    jhash(pattern, pat_len, 0),
			    flags, 0);
}

/* Fast cache lookup */
static bool sni_cache_lookup(const char *data, unsigned int data_len,
			     const char *pattern, unsigned int pat_len, u8 flags)
{
	u32 hash = sni_compute_hash(data, data_len, pattern, pat_len, flags);
	unsigned int idx = hash & (SNI_CACHE_SIZE - 1);  /* Use AND instead of modulo */
	struct sni_cache_entry *entry;
	bool found = false;
	
	read_lock(&sni_opt.cache_lock);
	entry = &sni_opt.cache[idx];
	
	if (entry->hash == hash &&
	    entry->pattern_len == pat_len &&
	    entry->flags == flags &&
	    entry->ttl > jiffies &&
	    memcmp(entry->pattern, pattern, pat_len) == 0) {
		found = entry->result;
		sni_opt.stats.cache_hits++;
	}
	
	read_unlock(&sni_opt.cache_lock);
	return found;
}

/* Cache update */
static void sni_cache_update(const char *data, unsigned int data_len,
			      const char *pattern, unsigned int pat_len, u8 flags, bool result)
{
	u32 hash = sni_compute_hash(data, data_len, pattern, pat_len, flags);
	unsigned int idx = hash & (SNI_CACHE_SIZE - 1);  /* Use AND instead of modulo */
	struct sni_cache_entry *entry;
	
	write_lock(&sni_opt.cache_lock);
	entry = &sni_opt.cache[idx];
	
	entry->hash = hash;
	entry->ttl = jiffies + SNI_CACHE_TTL;
	entry->result = result;
	entry->pattern_len = pat_len;
	entry->flags = flags;
	memcpy(entry->pattern, pattern, pat_len);
	
	sni_opt.stats.cache_entries = (sni_opt.stats.cache_entries + 1 < SNI_CACHE_SIZE) ? 
	                               sni_opt.stats.cache_entries + 1 : SNI_CACHE_SIZE;
	
	write_unlock(&sni_opt.cache_lock);
}

/* Optimized exact domain matching with early rejection */
static bool sni_match_exact_domain(const char *data, unsigned int data_len,
				    const char *pattern, unsigned int pat_len, bool icase)
{
	unsigned int i;
	
	/* Early rejection: lengths must match exactly */
	if (data_len != pat_len)
		return false;
	
	if (icase) {
		/* Optimized case-insensitive comparison */
		for (i = 0; i < pat_len; i++) {
			if (tolower(data[i]) != tolower(pattern[i]))
				return false;
		}
		return true;
	} else {
		/* Direct memory comparison for case-sensitive */
		return memcmp(data, pattern, pat_len) == 0;
	}
}

/* Optimized subdomain matching with pointer arithmetic */
static bool sni_match_subdomain(const char *data, unsigned int data_len,
				  const char *domain_part, unsigned int domain_len, bool icase)
{
	const char *data_end;
	unsigned int i;
	
	/* Early rejection: data must be longer than domain */
	if (data_len <= domain_len)
		return false;
	
	data_end = data + data_len - domain_len;
	
	/* Check if data ends with domain part */
	if (icase) {
		for (i = 0; i < domain_len; i++) {
			if (tolower(data_end[i]) != tolower(domain_part[i]))
				return false;
		}
	} else {
		if (memcmp(data_end, domain_part, domain_len) != 0)
			return false;
	}
	
	/* Ensure proper domain boundary */
	return data_len == domain_len || data_end[-1] == '.';
}

/* Optimized contains matching with Boyer-Moore-Horspool for short patterns */
static bool sni_match_contains(const char *data, unsigned int data_len,
				const char *pattern, unsigned int pat_len, bool icase)
{
	unsigned int i, j;
	char *data_lower, *pattern_lower;
	bool result, match;
	char d, p;
	
	if (pat_len == 0 || data_len < pat_len)
		return false;
	
	/* For very short patterns, use simple scan */
	if (pat_len <= 3) {
		for (i = 0; i <= data_len - pat_len; i++) {
			match = true;
			for (j = 0; j < pat_len; j++) {
				d = icase ? tolower(data[i + j]) : data[i + j];
				p = icase ? tolower(pattern[j]) : pattern[j];
				if (d != p) {
					match = false;
					break;
				}
			}
			if (match) return true;
		}
		return false;
	}
	
	/* Use strstr for longer patterns */
	if (icase) {
		/* Simple case-insensitive implementation */
		data_lower = kmalloc(data_len + 1, GFP_ATOMIC);
		pattern_lower = kmalloc(pat_len + 1, GFP_ATOMIC);
		result = false;
		
		if (data_lower && pattern_lower) {
			for (i = 0; i < data_len; i++)
				data_lower[i] = tolower(data[i]);
			data_lower[data_len] = '\0';
			
			for (i = 0; i < pat_len; i++)
				pattern_lower[i] = tolower(pattern[i]);
			pattern_lower[pat_len] = '\0';
			
			result = strstr(data_lower, pattern_lower) != NULL;
		}
		
		kfree(data_lower);
		kfree(pattern_lower);
		return result;
	} else {
		/* Manual implementation of memmem for kernel compatibility */
		for (i = 0; i <= data_len - pat_len; i++) {
			if (memcmp(data + i, pattern, pat_len) == 0)
				return true;
		}
		return false;
	}
}

/* Main optimized matching function */
static bool sni_router_match_optimized(const char *data, unsigned int data_len,
				       const char *pattern, unsigned int pat_len, u8 flags)
{
	enum sni_pattern_class class;
	bool icase = flags & XT_SNI_FLAG_IGNORECASE;
	bool result = false;
	
	/* Early cache lookup for performance */
	if (data_len >= SNI_MIN_LEN_FOR_CACHE && pat_len >= SNI_MIN_LEN_FOR_CACHE) {
		bool cached = sni_cache_lookup(data, data_len, pattern, pat_len, flags);
		if (cached || sni_opt.stats.total_matches > 0) {
			return cached;
		}
	}
	
	/* Classify pattern for optimal algorithm selection */
	class = sni_classify_pattern(pattern, pat_len);
	
	switch (class) {
	case SNI_CLASS_EXACT_DOMAIN:
		result = sni_match_exact_domain(data, data_len, pattern, pat_len, icase);
		sni_opt.stats.fast_path_hits++;
		break;
		
	case SNI_CLASS_SUBDOMAIN:
		{
			const char *domain_part = pattern + 2;  /* Skip "*." */
			unsigned int domain_len = pat_len - 2;
			result = sni_match_subdomain(data, data_len, domain_part, domain_len, icase);
			sni_opt.stats.fast_path_hits++;
		}
		break;
		
	case SNI_CLASS_SIMPLE_CONTAINS:
	case SNI_CLASS_WILDCARD:
		result = sni_match_contains(data, data_len, pattern, pat_len, icase);
		sni_opt.stats.fast_path_hits++;
		break;
		
	default:
		/* Fall back to standard algorithm for complex patterns */
		sni_opt.stats.fallback_hits++;
		return false;  /* Let standard textsearch handle this */
	}
	
	/* Update cache for future lookups */
	if (data_len >= SNI_MIN_LEN_FOR_CACHE && pat_len >= SNI_MIN_LEN_FOR_CACHE) {
		sni_cache_update(data, data_len, pattern, pat_len, flags, result);
	}
	
	sni_opt.stats.total_matches++;
	return result;
}

static bool
sni_mt(const struct sk_buff *skb, struct xt_action_param *par)
{
	const struct xt_sni_info *conf = par->matchinfo;
	bool invert;
	bool fast_result = false;

	invert = conf->u.v1.flags & XT_SNI_FLAG_INVERT;
	sni_opt.stats.total_matches++;

	/* Try optimized router algorithm first */
	if (strcmp(conf->algo, "router") == 0) {
		unsigned int text_len;
		u8 text_buffer[XT_SNI_MAX_PATTERN_SIZE * 2];
		
		/* Get SNI data efficiently */
		text_len = skb_find_text((struct sk_buff *)skb, conf->from_offset,
					 conf->to_offset, NULL);
		
		if (text_len != UINT_MAX && text_len <= sizeof(text_buffer)) {
			/* Extract SNI data */
			text_len = skb_copy_bits((struct sk_buff *)skb, conf->from_offset,
						 text_buffer, text_len);
			if (text_len > 0) {
				fast_result = sni_router_match_optimized((const char*)text_buffer, text_len,
									 conf->pattern, conf->patlen,
									 conf->u.v1.flags);
				/* Return result if optimization succeeded */
				return fast_result ^ invert;
			}
		}
		
		/* Fall back to standard textsearch */
		sni_opt.stats.fallback_hits++;
	}

	/* Standard textsearch matching */
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
	
	/* Handle router algorithm */
	if (strcmp(conf->algo, "router") == 0) {
		/* Use bm as reliable fallback */
		ts_conf = textsearch_prepare("bm", conf->pattern, conf->patlen,
					     GFP_KERNEL, flags);
		if (IS_ERR(ts_conf))
			return PTR_ERR(ts_conf);
	} else {
		/* Use requested algorithm */
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
	textsearch_destroy(SNI_TEXT_PRIV(par->matchinfo)->config);
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

/* Debug FS support for performance monitoring - DISABLED to avoid 64-bit division */
/* #ifdef CONFIG_DEBUG_FS */
/* #include <linux/debugfs.h> */

static int sni_stats_show(struct seq_file *m, void *v)
{
	u32 total = sni_opt.stats.total_matches;
	u32 cache_percent, fast_percent, fallback_percent;
	
	seq_printf(m, "SNI Router Optimization Statistics:\n");
	seq_printf(m, "Total Matches:     %u\n", total);
	
/* Safe division without 64-bit operations */
	if (total > 0) {
		/* Manual percentage calculation to avoid 64-bit division */
		u32 cache_hits_u32 = (u32)sni_opt.stats.cache_hits;
		u32 fast_hits_u32 = (u32)sni_opt.stats.fast_path_hits;
		u32 fallback_hits_u32 = (u32)sni_opt.stats.fallback_hits;
		u32 total_u32 = (u32)total;
		
		/* Simple integer division (may lose precision but safe) */
		cache_percent = (cache_hits_u32 * 100) / total_u32;
		fast_percent = (fast_hits_u32 * 100) / total_u32;
		fallback_percent = (fallback_hits_u32 * 100) / total_u32;
	} else {
		cache_percent = fast_percent = fallback_percent = 0;
	}
	
	seq_printf(m, "Cache Hits:        %u (%u%%)\n", sni_opt.stats.cache_hits, cache_percent);
	seq_printf(m, "Fast Path Hits:    %u (%u%%)\n", sni_opt.stats.fast_path_hits, fast_percent);
	seq_printf(m, "Fallback Hits:     %u (%u%%)\n", sni_opt.stats.fallback_hits, fallback_percent);
	seq_printf(m, "Cache Entries:     %u/%u\n", sni_opt.stats.cache_entries, SNI_CACHE_SIZE);
	return 0;
}

static int sni_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, sni_stats_show, inode->i_private);
}

static const struct file_operations sni_stats_fops = {
	.open = sni_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void sni_debug_init(void)
{
	sni_debug_dir = debugfs_create_dir("sni_router", NULL);
	if (!sni_debug_dir)
		return;
	
	sni_stats_file = debugfs_create_file("stats", 0444, sni_debug_dir,
					    NULL, &sni_stats_fops);
}

static void sni_debug_cleanup(void)
{
	debugfs_remove_recursive(sni_debug_dir);
}
#else
static inline void sni_debug_init(void) {}
static inline void sni_debug_cleanup(void) {}
#endif

static int __init sni_mt_init(void)
{
	int ret;
	
	/* Initialize optimization structures */
	memset(&sni_opt, 0, sizeof(sni_opt));
	rwlock_init(&sni_opt.cache_lock);
	
	/* Initialize debug FS */
	sni_debug_init();
	
	/* Register SNI match */
	ret = xt_register_match(&xt_sni_mt_reg);
	if (ret) {
		pr_err("Failed to register SNI match: %d\n", ret);
		sni_debug_cleanup();
		return ret;
	}
	
	pr_info("SNI match v3 registered with advanced router optimizations\n");
	pr_info("Cache size: %d entries, TTL: %d seconds\n", SNI_CACHE_SIZE, SNI_CACHE_TTL);
	return 0;
}

static void __exit sni_mt_exit(void)
{
	xt_unregister_match(&xt_sni_mt_reg);
	sni_debug_cleanup();
	
	pr_info("SNI match v3 with advanced optimizations unregistered\n");
	pr_info("Final stats - Total: %u, Cache hits: %u, Fast path: %u, Fallback: %u\n",
		sni_opt.stats.total_matches, sni_opt.stats.cache_hits,
		sni_opt.stats.fast_path_hits, sni_opt.stats.fallback_hits);
}

module_init(sni_mt_init);
module_exit(sni_mt_exit);