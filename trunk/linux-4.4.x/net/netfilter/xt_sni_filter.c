/* SNI (Server Name Indication) matching match for iptables
 *
 * Based on xt_string.c by Pablo Neira Ayuso <pablo@eurodev.net>
 * Refactored for SNI-specific optimization and stability
 *
 * Copyright (C) 2025 dev.tekin.cn
 * @author tekintian <tekintian@gmail.com>
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
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/ipv6.h>
#include <net/tcp.h>
#include <linux/etherdevice.h>

MODULE_AUTHOR("tekintian <tekintian@gmail.com>");
MODULE_DESCRIPTION("Xtables: SNI-based matching (refactored for stability)");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ipt_sni");
MODULE_ALIAS("ip6t_sni");

/* SNI相关常量 */
#define SNI_MAX_LEN 256
#define TLS_HANDSHAKE 22
#define TLS_CLIENT_HELLO 1
#define TLS_APPLICATION_DATA 23

/* 调试开关 */
static bool enable_debug = false;
module_param(enable_debug, bool, 0644);
MODULE_PARM_DESC(enable_debug, "Enable debug output");

#define DEBUGP(fmt, args...) \
    do { \
        if (enable_debug) \
            printk(KERN_INFO "[SNI] " fmt, ##args); \
    } while (0)

/* 
 * 简化的SNI提取函数
 * 只在TLS ClientHello中查找SNI，使用内核textsearch框架
 */
static bool extract_sni_from_packet(const struct sk_buff *skb, 
                                    struct ts_config *ts_config)
{
    const struct iphdr *iph;
    const struct ipv6hdr *ipv6h;
    const struct tcphdr *tcph;
    unsigned int payload_offset;
    unsigned char record_type, handshake_type;
    const unsigned char *data_ptr;
    unsigned int family = skb->protocol == htons(ETH_P_IP) ? NFPROTO_IPV4 : NFPROTO_IPV6;
    
    /* 获取网络层头部 */
    if (family == NFPROTO_IPV4) {
        iph = ip_hdr(skb);
        if (!iph || iph->protocol != IPPROTO_TCP) {
            return false;
        }
        tcph = skb_header_pointer(skb, iph->ihl * 4, sizeof(*tcph), &tcph);
        if (!tcph) {
            return false;
        }
        payload_offset = iph->ihl * 4 + tcph->doff * 4;
    } else {
        ipv6h = ipv6_hdr(skb);
        if (!ipv6h || ipv6h->nexthdr != IPPROTO_TCP) {
            return false;
        }
        tcph = skb_header_pointer(skb, sizeof(*ipv6h), sizeof(*tcph), &tcph);
        if (!tcph) {
            return false;
        }
        payload_offset = sizeof(*ipv6h) + tcph->doff * 4;
    }
    
    /* 边界检查 */
    if (payload_offset + 6 > skb->len) {
        return false;
    }
    
    /* 检查TLS记录类型 */
    data_ptr = skb_header_pointer(skb, payload_offset, 1, &record_type);
    if (!data_ptr || *data_ptr != TLS_HANDSHAKE) {
        DEBUGP("Not TLS handshake, type: %u\n", data_ptr ? *data_ptr : 0);
        return false;
    }
    
    /* 检查握手类型 */
    data_ptr = skb_header_pointer(skb, payload_offset + 5, 1, &handshake_type);
    if (!data_ptr || *data_ptr != TLS_CLIENT_HELLO) {
        DEBUGP("Not TLS ClientHello, type: %u\n", data_ptr ? *data_ptr : 0);
        return false;
    }
    
    DEBUGP("TLS ClientHello detected, searching for SNI pattern\n");
    
    /* 使用内核textsearch框架搜索SNI模式 */
    return skb_find_text((struct sk_buff *)skb, payload_offset, skb->len, ts_config) != UINT_MAX;
}

/* 主匹配函数 - 参照string_mt的简洁实现 */
static bool sni_mt(const struct sk_buff *skb, struct xt_action_param *par)
{
    const struct xt_sni_info *conf = par->matchinfo;
    bool invert;
    bool found;

    invert = conf->u.v1.flags & XT_SNI_FLAG_INVERT;

    found = extract_sni_from_packet(skb, conf->config);
    
    return found ^ invert;
}

/* 检查函数 - 参照string_mt_check */
static int sni_mt_check(const struct xt_mtchk_param *par)
{
    struct xt_sni_info *conf = par->matchinfo;
    struct ts_config *ts_conf;
    int flags = TS_AUTOLOAD;

    /* Damn, can't handle this case properly with iptables... */
    if (conf->from_offset > conf->to_offset)
        return -EINVAL;
    if (conf->algo[16 - 1] != '\0')
        return -EINVAL;
    if (conf->patlen > XT_SNI_MAX_PATTERN_SIZE)
        return -EINVAL;

    /* 使用内核textsearch框架准备搜索配置 */
    ts_conf = textsearch_prepare(conf->algo, conf->pattern, conf->patlen,
                                GFP_KERNEL, flags);
    if (IS_ERR(ts_conf))
        return PTR_ERR(ts_conf);

    conf->config = ts_conf;
    DEBUGP("SNI pattern registered: %.*s\n", conf->patlen, conf->pattern);
    return 0;
}

/* 销毁函数 - 参照string_mt_destroy */
static void sni_mt_destroy(const struct xt_mtdtor_param *par)
{
    struct xt_sni_info *conf = par->matchinfo;
    
    textsearch_destroy(conf->config);
    DEBUGP("SNI pattern destroyed\n");
}

/* 匹配注册结构 - 参照xt_string_mt_reg */
static struct xt_match xt_sni_mt_reg __read_mostly = {
    .name       = "sni",
    .revision   = 1,
    .family     = NFPROTO_UNSPEC,  /* 支持IPv4和IPv6 */
    .checkentry = sni_mt_check,
    .match      = sni_mt,
    .destroy    = sni_mt_destroy,
    .matchsize  = sizeof(struct xt_sni_info),
    .me         = THIS_MODULE,
};

/* 模块初始化 */
static int __init sni_mt_init(void)
{
    DEBUGP("Loading SNI match module\n");
    return xt_register_match(&xt_sni_mt_reg);
}

/* 模块退出 */
static void __exit sni_mt_exit(void)
{
    DEBUGP("Unloading SNI match module\n");
    xt_unregister_match(&xt_sni_mt_reg);
}

module_init(sni_mt_init);
module_exit(sni_mt_exit);