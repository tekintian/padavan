/* SNI matching match for iptables
 *
 * Based on xt_string.c by Pablo Neira Ayuso <pablo@eurodev.net>
 * Modified for SNI matching functionality
 * Conservative version with minimal changes for stability
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

MODULE_AUTHOR("SNI Module");
MODULE_DESCRIPTION("Xtables: SNI-based matching");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ipt_sni");
MODULE_ALIAS("ip6t_sni");

static bool
sni_mt(const struct sk_buff *skb, struct xt_action_param *par)
{
	const struct xt_sni_info *conf = par->matchinfo;
	bool invert;

	invert = conf->u.v1.flags & XT_SNI_FLAG_INVERT;

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

	/* Damn, can't handle this case properly with iptables... */
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
	
	/* Handle router algorithm as special case of kmp */
	if (strcmp(conf->algo, "router") == 0) {
		/* Use kmp as fallback for now - safer than custom implementation */
		ts_conf = textsearch_prepare("kmp", conf->pattern, conf->patlen,
					     GFP_KERNEL, flags);
	} else {
		/* Use standard textsearch algorithm */
		ts_conf = textsearch_prepare(conf->algo, conf->pattern, conf->patlen,
					     GFP_KERNEL, flags);
	}
	
	if (IS_ERR(ts_conf))
		return PTR_ERR(ts_conf);

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
	return xt_register_match(&xt_sni_mt_reg);
}

static void __exit sni_mt_exit(void)
{
	xt_unregister_match(&xt_sni_mt_reg);
}

module_init(sni_mt_init);
module_exit(sni_mt_exit);