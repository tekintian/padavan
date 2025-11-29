/* SNI (Server Name Indication) filter kernel module for TLS ClientHello inspection
 * Copyright (C) 2024 Padavan Firmware
 *
 * This module extracts and filters SNI fields from TLS ClientHello packets
 * during the TLS handshake phase when SNI is transmitted in plaintext
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>

#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Padavan Firmware");
MODULE_ALIAS("ip6t_sni");
MODULE_ALIAS("ipt_sni");
MODULE_DESCRIPTION("Xtables: match SNI from TLS ClientHello packets");

#define SNI_MAX_LEN 256
#define TLS_HANDSHAKE 22
#define TLS_CLIENT_HELLO 1
#define TLS_EXTENSION_SNI 0x0000

struct xt_sni_info {
    char sni[SNI_MAX_LEN];
    u_int16_t invert;
    u_int16_t len;
};

/* TLS解析相关结构 */
struct tls_handshake {
    u_int8_t type;
    u_int8_t version_major;
    u_int8_t version_minor;
    u_int16_t length;
} __attribute__((packed));

struct tls_client_hello {
    u_int8_t version_major;
    u_int8_t version_minor;
    u_int8_t random[32];
    u_int8_t session_id_len;
    /* session_id follows */
} __attribute__((packed));

/* 从TLS ClientHello中提取SNI */
static int extract_sni_from_tls(const u_int8_t *data, u_int32_t data_len, char *sni_out, u_int32_t sni_out_len)
{
    const u_int8_t *ptr = data;
    u_int32_t remaining = data_len;
    u_int16_t cipher_suites_len;
    u_int8_t compression_methods_len;
    u_int16_t extensions_len;
    
    if (remaining < sizeof(struct tls_client_hello) + 1)
        return -1;
    
    ptr += sizeof(struct tls_client_hello); /* 跳过版本、随机数 */
    remaining -= sizeof(struct tls_client_hello);
    
    /* 跳过session_id */
    if (remaining < 1)
        return -1;
    ptr += 1 + ptr[0]; /* session_id长度 + session_id */
    remaining -= 1 + ptr[-1];
    
    if (remaining < 2)
        return -1;
    
    /* 跳过cipher_suites */
    cipher_suites_len = ntohs(*(u_int16_t *)ptr);
    ptr += 2 + cipher_suites_len;
    remaining -= 2 + cipher_suites_len;
    
    if (remaining < 1)
        return -1;
    
    /* 跳过compression_methods */
    compression_methods_len = ptr[0];
    ptr += 1 + compression_methods_len;
    remaining -= 1 + compression_methods_len;
    
    if (remaining < 2)
        return -1;
    
    /* 解析extensions */
    extensions_len = ntohs(*(u_int16_t *)ptr);
    ptr += 2;
    remaining -= 2;
    
    if (remaining < extensions_len)
        return -1;
    
    const u_int8_t *ext_end = ptr + extensions_len;
    
    while (ptr < ext_end && ptr + 4 <= ext_end) {
        u_int16_t ext_type = ntohs(*(u_int16_t *)ptr);
        u_int16_t ext_len = ntohs(*(u_int16_t *)(ptr + 2));
        
        ptr += 4;
        
        if (ptr + ext_len > ext_end)
            break;
            
        if (ext_type == TLS_EXTENSION_SNI) {
            /* SNI扩展 */
            const u_int8_t *sni_ptr = ptr;
            u_int16_t server_name_list_len = ntohs(*(u_int16_t *)sni_ptr);
            sni_ptr += 2;
            
            if (sni_ptr + server_name_list_len > ptr + ext_len)
                break;
                
            while (sni_ptr < ptr + ext_len && sni_ptr + 3 <= ptr + ext_len) {
                u_int8_t name_type = sni_ptr[0];
                u_int16_t name_len = ntohs(*(u_int16_t *)(sni_ptr + 1));
                
                sni_ptr += 3;
                
                if (name_type == 0 && name_len > 0) { /* host_name */
                    if (sni_ptr + name_len <= ptr + ext_len && name_len < sni_out_len) {
                        memcpy(sni_out, sni_ptr, name_len);
                        sni_out[name_len] = '\0';
                        return name_len;
                    }
                }
                
                sni_ptr += name_len;
            }
        }
        
        ptr += ext_len;
    }
    
    return -1;
}

/* 检查是否为TLS ClientHello包 */
static bool is_tls_client_hello(const struct sk_buff *skb, u_int8_t protocol)
{
    const u_int8_t *data;
    u_int32_t data_len;
    const struct iphdr *iph;
    const struct ipv6hdr *ipv6h;
    const struct tcphdr *tcph;
    const struct udphdr *udph;
    
    if (protocol == IPPROTO_TCP) {
        if (skb->protocol == htons(ETH_P_IP)) {
            iph = ip_hdr(skb);
            if (!iph || iph->protocol != IPPROTO_TCP)
                return false;
            tcph = (void *)iph + iph->ihl * 4;
            data = (void *)tcph + tcph->doff * 4;
            data_len = ntohs(iph->tot_len) - iph->ihl * 4 - tcph->doff * 4;
        } else if (skb->protocol == htons(ETH_P_IPV6)) {
            ipv6h = ipv6_hdr(skb);
            if (!ipv6h)
                return false;
            tcph = (void *)ipv6h + sizeof(struct ipv6hdr);
            data = (void *)tcph + tcph->doff * 4;
            data_len = ntohs(ipv6h->payload_len) - sizeof(struct tcphdr);
        } else {
            return false;
        }
        
        if (data_len < 6) /* TLS记录头最小长度 */
            return false;
            
        /* 检查TLS记录头 */
        if (data[0] == TLS_HANDSHAKE && data[5] == TLS_CLIENT_HELLO)
            return true;
            
    } else if (protocol == IPPROTO_UDP) {
        /* 处理DTLS情况 */
        if (skb->protocol == htons(ETH_P_IP)) {
            iph = ip_hdr(skb);
            if (!iph || iph->protocol != IPPROTO_UDP)
                return false;
            udph = (void *)iph + iph->ihl * 4;
            data = (void *)udph + sizeof(struct udphdr);
            data_len = ntohs(udph->len) - sizeof(struct udphdr);
        } else if (skb->protocol == htons(ETH_P_IPV6)) {
            ipv6h = ipv6_hdr(skb);
            if (!ipv6h)
                return false;
            udph = (void *)ipv6h + sizeof(struct ipv6hdr);
            data = (void *)udph + sizeof(struct udphdr);
            data_len = ntohs(ipv6h->payload_len) - sizeof(struct udphdr);
        } else {
            return false;
        }
        
        if (data_len < 13) /* DTLS记录头最小长度 */
            return false;
            
        /* 检查DTLS记录头 */
        if (data[0] == TLS_HANDSHAKE && data[13] == TLS_CLIENT_HELLO)
            return true;
    }
    
    return false;
}

/* 主要的匹配函数 */
static bool xt_sni_match(const struct sk_buff *skb, struct xt_action_param *par)
{
    const struct xt_sni_info *info = par->matchinfo;
    const u_int8_t *data;
    u_int32_t data_len;
    char extracted_sni[SNI_MAX_LEN];
    int sni_len;
    const struct iphdr *iph;
    const struct ipv6hdr *ipv6h;
    const struct tcphdr *tcph;
    const struct udphdr *udph;
    u_int8_t protocol;
    
    if (info->len < 1)
        return false;
        
    /* 确定协议类型 */
    if (par->match->family == NFPROTO_IPV4) {
        iph = ip_hdr(skb);
        if (!iph)
            return false;
        protocol = iph->protocol;
    } else if (par->match->family == NFPROTO_IPV6) {
        ipv6h = ipv6_hdr(skb);
        if (!ipv6h)
            return false;
        protocol = ipv6h->nexthdr;
    } else {
        return false;
    }
    
    /* 检查是否为TLS/DTLS ClientHello */
    if (!is_tls_client_hello(skb, protocol))
        return false;
    
    /* 提取传输层数据 */
    if (protocol == IPPROTO_TCP) {
        if (par->match->family == NFPROTO_IPV4) {
            iph = ip_hdr(skb);
            tcph = (void *)iph + iph->ihl * 4;
            data = (void *)tcph + tcph->doff * 4;
            data_len = ntohs(iph->tot_len) - iph->ihl * 4 - tcph->doff * 4;
        } else {
            ipv6h = ipv6_hdr(skb);
            tcph = (void *)ipv6h + sizeof(struct ipv6hdr);
            data = (void *)tcph + tcph->doff * 4;
            data_len = ntohs(ipv6h->payload_len) - sizeof(struct tcphdr);
        }
    } else if (protocol == IPPROTO_UDP) {
        if (par->match->family == NFPROTO_IPV4) {
            iph = ip_hdr(skb);
            udph = (void *)iph + iph->ihl * 4;
            data = (void *)udph + sizeof(struct udphdr);
            data_len = ntohs(udph->len) - sizeof(struct udphdr);
        } else {
            ipv6h = ipv6_hdr(skb);
            udph = (void *)ipv6h + sizeof(struct ipv6hdr);
            data = (void *)udph + sizeof(struct udphdr);
            data_len = ntohs(ipv6h->payload_len) - sizeof(struct udphdr);
        }
    } else {
        return false;
    }
    
    if (data_len < 6)
        return false;
        
    /* 提取SNI */
    sni_len = extract_sni_from_tls(data, data_len, extracted_sni, sizeof(extracted_sni));
    if (sni_len < 0)
        return false;
        
    /* 匹配SNI */
    bool matched = (strstr(extracted_sni, info->sni) != NULL);
    
    return (matched ^ info->invert);
}

static struct xt_match sni_match[] __read_mostly = {
    {
        .name       = "sni",
        .family     = NFPROTO_IPV4,
        .match      = xt_sni_match,
        .matchsize  = sizeof(struct xt_sni_info),
        .me         = THIS_MODULE,
    },
#if IS_ENABLED(CONFIG_IPV6)
    {
        .name       = "sni",
        .family     = NFPROTO_IPV6,
        .match      = xt_sni_match,
        .matchsize  = sizeof(struct xt_sni_info),
        .me         = THIS_MODULE,
    }
#endif
};

static int __init xt_sni_init(void)
{
    return xt_register_matches(sni_match, ARRAY_SIZE(sni_match));
}

static void __exit xt_sni_fini(void)
{
    xt_unregister_matches(sni_match, ARRAY_SIZE(sni_match));
}

module_init(xt_sni_init);
module_exit(xt_sni_fini);