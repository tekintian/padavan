/* SNI matching match for iptables
 *
 * Based on xt_string.c by Pablo Neira Ayuso <pablo@eurodev.net>
 * Modified for SNI matching functionality
 * Version 2: Conservative router optimizations with fast-path matching
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

/* Fast-path pattern matching for router-optimized scenarios */
static bool router_fast_match(const char *data, unsigned int data_len,
			      const char *pattern, unsigned int pat_len, int flags)
{
	char data_buf[XT_SNI_MAX_PATTERN_SIZE + 1];
	char pattern_buf[XT_SNI_MAX_PATTERN_SIZE + 1];
	unsigned int i;
	
	/* Safety checks */
	if (!data || !pattern || pat_len == 0 || data_len == 0 ||
	    pat_len > XT_SNI_MAX_PATTERN_SIZE || data_len > XT_SNI_MAX_PATTERN_SIZE)
		return false;
	
	/* Copy and null-terminate for string operations */
	memcpy(data_buf, data, data_len);
	data_buf[data_len] = '\0';
	memcpy(pattern_buf, pattern, pat_len);
	pattern_buf[pat_len] = '\0';
	
	/* Case-insensitive preprocessing */
	if (flags & XT_SNI_FLAG_IGNORECASE) {
		for (i = 0; i < data_len; i++)
			data_buf[i] = tolower(data_buf[i]);
		for (i = 0; i < pat_len; i++)
			pattern_buf[i] = tolower(pattern_buf[i]);
	}
	
	/* Fast exact domain matching */
	if (strchr(pattern_buf, '.') && !strchr(pattern_buf, '*') && !strchr(pattern_buf, '?')) {
		return strcmp(data_buf, pattern_buf) == 0;
	}
	
	/* Fast subdomain matching for *.domain.com patterns */
	if (pat_len > 2 && pattern_buf[0] == '*' && pattern_buf[1] == '.') {
		const char *domain_part = pattern_buf + 2;
		unsigned int domain_len = pat_len - 2;
		
		if (data_len >= domain_len) {
			const char *data_end = data_buf + data_len - domain_len;
			/* Check if ends with domain and is either exact match or has dot before */
			if (strcmp(data_end, domain_part) == 0) {
				if (data_len == domain_len || data_end[-1] == '.')
					return true;
			}
		}
		return false;
	}
	
	/* Default to contains match */
	return strstr(data_buf, pattern_buf) != NULL;
}

static bool
sni_mt(const struct sk_buff *skb, struct xt_action_param *par)
{
	const struct xt_sni_info *conf = par->matchinfo;
	bool invert;
	bool fast_match_result = false;

	invert = conf->u.v1.flags & XT_SNI_FLAG_INVERT;

	/* Try router fast path when using router algorithm */
	if (strcmp(conf->algo, "router") == 0) {
		unsigned int text_len;
		const uint8_t *text;
		struct ts_state state;
		
		/* Get the SNI data length first */
		state.offset = 0;
		text_len = skb_find_text((struct sk_buff *)skb, conf->from_offset,
					 conf->to_offset, NULL);
		
		if (text_len != UINT_MAX && text_len <= XT_SNI_MAX_PATTERN_SIZE) {
			/* Extract the actual SNI data */
			text_len = skb_copy_bits((struct sk_buff *)skb, conf->from_offset,
						 (void*)&text, text_len);
			if (text_len > 0) {
				fast_match_result = router_fast_match((const char*)text, text_len,
								       conf->pattern, conf->patlen,
								       conf->u.v1.flags);
				/* If fast path succeeded, return result immediately */
				return fast_match_result ^ invert;
			}
		}
		
		/* Fall back to standard textsearch if fast path failed */
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
	
	/* Handle router algorithm - use bm for reliability */
	if (strcmp(conf->algo, "router") == 0) {
		/* Use bm as fallback algorithm */
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

static int __init sni_mt_init(void)
{
	int ret;
	
	ret = xt_register_match(&xt_sni_mt_reg);
	if (ret) {
		pr_err("Failed to register SNI match: %d\n", ret);
		return ret;
	}
	
	pr_info("SNI match v2 registered with router fast-path optimizations\n");
	return 0;
}

static void __exit sni_mt_exit(void)
{
	xt_unregister_match(&xt_sni_mt_reg);
}

module_init(sni_mt_init);
module_exit(sni_mt_exit);