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
} __attribute__((packed));

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
    
    /* 确保输出缓冲区有效 */
    if (!sni_out || sni_out_len < 1) {
        return -1;
    }
    
    if (remaining < sizeof(struct tls_client_hello) + 1) {
        return -1;
    }
    
    ptr += sizeof(struct tls_client_hello); /* 跳过版本、随机数 */
    remaining -= sizeof(struct tls_client_hello);
    
    /* 跳过session_id */
    if (remaining < 1) {
        return -1;
    }
    u_int8_t session_id_len = ptr[0];
    if (remaining < 1 + session_id_len) {
        return -1;
    }
    ptr += 1 + session_id_len;
    remaining -= 1 + session_id_len;
    
    if (remaining < 2) {
        return -1;
    }
    
    /* 跳过cipher_suites */
    cipher_suites_len = ntohs(*(u_int16_t *)ptr);
    if (remaining < 2 + cipher_suites_len) {
        return -1;
    }
    ptr += 2 + cipher_suites_len;
    remaining -= 2 + cipher_suites_len;
    
    if (remaining < 1) {
        return -1;
    }
    
    /* 跳过compression_methods */
    compression_methods_len = ptr[0];
    if (remaining < 1 + compression_methods_len) {
        return -1;
    }
    ptr += 1 + compression_methods_len;
    remaining -= 1 + compression_methods_len;
    
    if (remaining < 2) {
        return -1;
    }
    
    /* 解析extensions */
    extensions_len = ntohs(*(u_int16_t *)ptr);
    if (extensions_len > remaining - 2) {
        return -1;
    }
    ptr += 2;
    remaining -= 2;
    
    const u_int8_t *ext_end = ptr + extensions_len;
    if (ext_end > data + data_len) {
        return -1;
    }
    
    while (ptr < ext_end && remaining >= 4) {
        u_int16_t ext_type = ntohs(*(u_int16_t *)ptr);
        u_int16_t ext_len = ntohs(*(u_int16_t *)(ptr + 2));
        
        if (ext_len > remaining - 4) {
            break;
        }
        
        ptr += 4;
        remaining -= 4;
        
        if (ext_type == TLS_EXTENSION_SNI) {
            /* SNI扩展 */
            const u_int8_t *sni_ptr = ptr;
            u_int16_t server_name_list_len;
            
            if (remaining < 2 || ext_len < 2) {
                ptr += ext_len;
                remaining -= ext_len;
                continue;
            }
            
            server_name_list_len = ntohs(*(u_int16_t *)sni_ptr);
            if (server_name_list_len > ext_len - 2 || server_name_list_len > remaining - 2) {
                ptr += ext_len;
                remaining -= ext_len;
                continue;
            }
            
            sni_ptr += 2;
            remaining -= 2;
            
            while (sni_ptr < ptr + ext_len && remaining >= 3) {
                u_int8_t name_type = sni_ptr[0];
                u_int16_t name_len;
                
                if (ext_len - (sni_ptr - (ptr - 2)) < 3) {
                    break;
                }
                
                name_len = ntohs(*(u_int16_t *)(sni_ptr + 1));
                
                if (name_len > ext_len - (sni_ptr - (ptr - 2)) - 3 || 
                    name_len > remaining - 3 || 
                    name_len >= sni_out_len) {
                    break;
                }
                
                sni_ptr += 3;
                remaining -= 3;
                
                if (name_type == 0 && name_len > 0) { /* host_name */
                    if (sni_ptr + name_len <= ptr + ext_len && remaining >= name_len) {
                        memset(sni_out, 0, sni_out_len); /* 确保缓冲区初始化 */
                        memcpy(sni_out, sni_ptr, name_len);
                        sni_out[name_len] = '\0'; /* 确保字符串终止 */
                        return name_len;
                    }
                }
                
                sni_ptr += name_len;
                remaining -= name_len;
            }
        }
        
        ptr += ext_len;
        remaining -= ext_len;
    }
    
    return -1;
}

/* 检查是否为TLS ClientHello包 */
static bool is_tls_client_hello(const struct sk_buff *skb, u_int8_t protocol)
{
    const u_int8_t *data = NULL;
    u_int32_t data_len = 0;
    int offset = 0;
    int payload_offset = 0;
    
    if (!skb) {
        return false;
    }
    
    /* 获取传输层头部偏移量 */
    if (protocol == IPPROTO_TCP) {
        if (skb->protocol == htons(ETH_P_IP)) {
            struct iphdr *iph = ip_hdr(skb);
            if (!iph || iph->protocol != IPPROTO_TCP) {
                return false;
            }
            payload_offset = iph->ihl * 4 + sizeof(struct tcphdr);
        } else if (skb->protocol == htons(ETH_P_IPV6)) {
            struct ipv6hdr *ipv6h = ipv6_hdr(skb);
            if (!ipv6h || ipv6h->nexthdr != IPPROTO_TCP) {
                return false;
            }
            payload_offset = sizeof(struct ipv6hdr) + sizeof(struct tcphdr);
        } else {
            return false;
        }
        
        /* 获取TCP有效载荷 */
        if (skb_copy_bits(skb, payload_offset, &data_len, sizeof(data_len)) != 0) {
            return false;
        }
        
        data_len = ntohs(data_len);
        if (data_len < 6) { /* TLS记录头最小长度 */
            return false;
        }
        
        /* 检查TLS记录头 */
        if (skb_copy_bits(skb, payload_offset, &offset, 1) != 0) {
            return false;
        }
        if (offset != TLS_HANDSHAKE) {
            return false;
        }
        
        if (skb_copy_bits(skb, payload_offset + 5, &offset, 1) != 0) {
            return false;
        }
        if (offset != TLS_CLIENT_HELLO) {
            return false;
        }
        
    } else if (protocol == IPPROTO_UDP) {
        /* 处理DTLS情况 */
        if (skb->protocol == htons(ETH_P_IP)) {
            struct iphdr *iph = ip_hdr(skb);
            if (!iph || iph->protocol != IPPROTO_UDP) {
                return false;
            }
            payload_offset = iph->ihl * 4 + sizeof(struct udphdr);
        } else if (skb->protocol == htons(ETH_P_IPV6)) {
            struct ipv6hdr *ipv6h = ipv6_hdr(skb);
            if (!ipv6h || ipv6h->nexthdr != IPPROTO_UDP) {
                return false;
            }
            payload_offset = sizeof(struct ipv6hdr) + sizeof(struct udphdr);
        } else {
            return false;
        }
        
        /* 获取UDP有效载荷长度 */
        if (skb->protocol == htons(ETH_P_IP)) {
            struct iphdr *iph = ip_hdr(skb);
            data_len = ntohs(iph->tot_len) - (iph->ihl * 4) - sizeof(struct udphdr);
        } else {
            struct ipv6hdr *ipv6h = ipv6_hdr(skb);
            data_len = ntohs(ipv6h->payload_len) - sizeof(struct udphdr);
        }
        
        if (data_len < 13) { /* DTLS记录头最小长度 */
            return false;
        }
        
        /* 检查DTLS记录头 */
        if (skb_copy_bits(skb, payload_offset, &offset, 1) != 0) {
            return false;
        }
        if (offset != TLS_HANDSHAKE) {
            return false;
        }
        
        if (skb_copy_bits(skb, payload_offset + 13, &offset, 1) != 0) {
            return false;
        }
        if (offset != TLS_CLIENT_HELLO) {
            return false;
        }
    }
    
    return true;
}

/* 主要的匹配函数 */
static bool xt_sni_match(const struct sk_buff *skb, struct xt_action_param *par)
{
    const struct xt_sni_info *info = par->matchinfo;
    u_int32_t data_len = 0;
    char extracted_sni[SNI_MAX_LEN];
    int sni_len = -1;
    u_int8_t protocol;
    int payload_offset = 0;
    int tcp_doff = 0;
    
    /* 参数验证 */
    if (!skb || !info || !par) {
        return false;
    }
    
    if (info->len < 1 || info->len >= SNI_MAX_LEN) {
        return false;
    }
    
    /* 确定协议类型 */
    if (par->match->family == NFPROTO_IPV4) {
        struct iphdr *iph = ip_hdr(skb);
        if (!iph) {
            return false;
        }
        protocol = iph->protocol;
    } else if (par->match->family == NFPROTO_IPV6) {
        struct ipv6hdr *ipv6h = ipv6_hdr(skb);
        if (!ipv6h) {
            return false;
        }
        protocol = ipv6h->nexthdr;
    } else {
        return false;
    }
    
    /* 检查是否为TLS/DTLS ClientHello */
    if (!is_tls_client_hello(skb, protocol)) {
        return false;
    }
    
    /* 提取传输层数据 - 使用临时缓冲区而不是直接访问 */
    u_int8_t *tmp_buffer = kmalloc(SNI_MAX_LEN * 2, GFP_ATOMIC);
    if (!tmp_buffer) {
        return false;
    }
    
    /* 计算有效载荷偏移量 */
    if (protocol == IPPROTO_TCP) {
        if (par->match->family == NFPROTO_IPV4) {
            struct iphdr *iph = ip_hdr(skb);
            struct tcphdr _tcph, *tcph;
            
            tcph = skb_header_pointer(skb, iph->ihl * 4, sizeof(_tcph), &_tcph);
            if (!tcph) {
                kfree(tmp_buffer);
                return false;
            }
            
            tcp_doff = tcph->doff * 4;
            payload_offset = iph->ihl * 4 + tcp_doff;
            data_len = ntohs(iph->tot_len) - iph->ihl * 4 - tcp_doff;
        } else {
            struct ipv6hdr *ipv6h = ipv6_hdr(skb);
            struct tcphdr _tcph, *tcph;
            
            tcph = skb_header_pointer(skb, sizeof(struct ipv6hdr), sizeof(_tcph), &_tcph);
            if (!tcph) {
                kfree(tmp_buffer);
                return false;
            }
            
            tcp_doff = tcph->doff * 4;
            payload_offset = sizeof(struct ipv6hdr) + tcp_doff;
            data_len = ntohs(ipv6h->payload_len) - tcp_doff;
        }
    } else if (protocol == IPPROTO_UDP) {
        if (par->match->family == NFPROTO_IPV4) {
            struct iphdr *iph = ip_hdr(skb);
            payload_offset = iph->ihl * 4 + sizeof(struct udphdr);
            data_len = ntohs(iph->tot_len) - iph->ihl * 4 - sizeof(struct udphdr);
        } else {
            struct ipv6hdr *ipv6h = ipv6_hdr(skb);
            payload_offset = sizeof(struct ipv6hdr) + sizeof(struct udphdr);
            data_len = ntohs(ipv6h->payload_len) - sizeof(struct udphdr);
        }
    } else {
        kfree(tmp_buffer);
        return false;
    }
    
    /* 限制最大数据长度以防止过大的分配 */
    if (data_len < 6 || data_len > SNI_MAX_LEN * 2) {
        kfree(tmp_buffer);
        return false;
    }
    
    /* 复制数据到临时缓冲区 */
    if (skb_copy_bits(skb, payload_offset, tmp_buffer, data_len) != 0) {
        kfree(tmp_buffer);
        return false;
    }
    
    /* 提取SNI */
    sni_len = extract_sni_from_tls(tmp_buffer, data_len, extracted_sni, sizeof(extracted_sni));
    kfree(tmp_buffer);
    
    if (sni_len < 0) {
        return false;
    }
    
    /* 安全地进行字符串匹配 */
    bool matched = false;
    if (sni_len > 0 && sni_len < SNI_MAX_LEN) {
        /* 确保提取的SNI以null结尾 */
        extracted_sni[SNI_MAX_LEN - 1] = '\0';
        matched = (strstr(extracted_sni, info->sni) != NULL);
    }
    
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