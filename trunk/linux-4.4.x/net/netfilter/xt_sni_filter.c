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

/* 模块参数配置 */
static int enable_debug = 0;
module_param(enable_debug, int, 0644);
MODULE_PARM_DESC(enable_debug, "Enable debugging messages (0=off, 1=on)");

/* 调试宏定义 */
#ifdef DEBUG
#define DEBUGP(fmt, args...) printk(KERN_DEBUG "[SNI-FILTER] %s:%d: " fmt, __func__, __LINE__, ##args)
#else
#define DEBUGP(fmt, args...) do { if (enable_debug) printk(KERN_DEBUG "[SNI-FILTER] %s:%d: " fmt, __func__, __LINE__, ##args); } while (0)
#endif

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
        DEBUGP("Invalid output buffer\n");
        return -1;
    }
    
    /* 验证TLS握手记录长度 */
    if (remaining < sizeof(struct tls_handshake)) {
        DEBUGP("Data too short for TLS handshake header\n");
        return -1;
    }
    
    const struct tls_handshake *handshake = (const struct tls_handshake *)data;
    u_int32_t handshake_len = ntohs(handshake->length);
    if (handshake_len > data_len - sizeof(struct tls_handshake)) {
        DEBUGP("Invalid handshake length: %u > %u\n", handshake_len, data_len - sizeof(struct tls_handshake));
        return -1;
    }
    
    /* 确保是ClientHello消息 */
    if (handshake->type != TLS_CLIENT_HELLO) {
        DEBUGP("Not a ClientHello message\n");
        return -1;
    }
    
    /* 跳过TLS握手头部，直接处理ClientHello内容 */
    ptr += sizeof(struct tls_handshake);
    remaining -= sizeof(struct tls_handshake);
    
    if (remaining < sizeof(struct tls_client_hello)) {
        DEBUGP("Data too short for ClientHello\n");
        return -1;
    }
    
    ptr += sizeof(struct tls_client_hello); /* 跳过版本、随机数 */
    remaining -= sizeof(struct tls_client_hello);
    
    /* 跳过session_id */
    if (remaining < 1) {
        DEBUGP("No session_id length\n");
        return -1;
    }
    u_int8_t session_id_len = ptr[0];
    if (remaining < 1 + session_id_len) {
        DEBUGP("Invalid session_id length\n");
        return -1;
    }
    ptr += 1 + session_id_len;
    remaining -= 1 + session_id_len;
    
    if (remaining < 2) {
        DEBUGP("No cipher_suites length\n");
        return -1;
    }
    
    /* 跳过cipher_suites */
    cipher_suites_len = ntohs(*(u_int16_t *)ptr);
    if (remaining < 2 + cipher_suites_len) {
        DEBUGP("Invalid cipher_suites length\n");
        return -1;
    }
    ptr += 2 + cipher_suites_len;
    remaining -= 2 + cipher_suites_len;
    
    if (remaining < 1) {
        DEBUGP("No compression_methods length\n");
        return -1;
    }
    
    /* 跳过compression_methods */
    compression_methods_len = ptr[0];
    if (remaining < 1 + compression_methods_len) {
        DEBUGP("Invalid compression_methods length\n");
        return -1;
    }
    ptr += 1 + compression_methods_len;
    remaining -= 1 + compression_methods_len;
    
    if (remaining < 2) {
        DEBUGP("No extensions length\n");
        return -1;
    }
    
    /* 解析extensions */
    extensions_len = ntohs(*(u_int16_t *)ptr);
    if (extensions_len > remaining - 2) {
        DEBUGP("Invalid extensions length\n");
        return -1;
    }
    ptr += 2;
    remaining -= 2;
    
    const u_int8_t *ext_end = ptr + extensions_len;
    if (ext_end > data + data_len) {
        DEBUGP("Extensions exceed data bounds\n");
        return -1;
    }
    
    while (ptr < ext_end && remaining >= 4) {
        u_int16_t ext_type = ntohs(*(u_int16_t *)ptr);
        u_int16_t ext_len = ntohs(*(u_int16_t *)(ptr + 2));
        
        if (ext_len > remaining - 4 || ext_type == TLS_EXTENSION_SNI) {
            DEBUGP("Processing extension: type=%u, len=%u\n", ext_type, ext_len);
        }
        
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
                DEBUGP("SNI extension too short\n");
                ptr += ext_len;
                remaining -= ext_len;
                continue;
            }
            
            server_name_list_len = ntohs(*(u_int16_t *)sni_ptr);
            if (server_name_list_len > ext_len - 2 || server_name_list_len > remaining - 2) {
                DEBUGP("Invalid server_name_list length\n");
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
                    DEBUGP("Invalid name length: %u\n", name_len);
                    break;
                }
                
                sni_ptr += 3;
                remaining -= 3;
                
                if (name_type == 0 && name_len > 0) { /* host_name */
                    if (sni_ptr + name_len <= ptr + ext_len && remaining >= name_len) {
                        memset(sni_out, 0, sni_out_len); /* 确保缓冲区初始化 */
                        memcpy(sni_out, sni_ptr, name_len);
                        sni_out[name_len] = '\0'; /* 确保字符串终止 */
                        DEBUGP("Extracted SNI: %s\n", sni_out);
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
    
    DEBUGP("No SNI found\n");
    return -1;
}

/* 检查是否为TLS ClientHello包 */
static bool is_tls_client_hello(const struct sk_buff *skb, u_int8_t protocol)
{
    const u_int8_t *data = NULL;
    u_int32_t data_len = 0;
    int offset = 0;
    int payload_offset = 0;
    u_int8_t record_type = 0;
    u_int8_t message_type = 0;
    
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
        
        /* 获取TCP有效载荷长度 */
        if (skb->protocol == htons(ETH_P_IP)) {
            struct iphdr *iph = ip_hdr(skb);
            struct tcphdr _tcph, *tcph;
            
            tcph = skb_header_pointer(skb, iph->ihl * 4, sizeof(_tcph), &_tcph);
            if (!tcph) {
                DEBUGP("Failed to get TCP header\n");
                return false;
            }
            payload_offset = iph->ihl * 4 + tcph->doff * 4;
            data_len = ntohs(iph->tot_len) - payload_offset;
        } else {
            struct ipv6hdr *ipv6h = ipv6_hdr(skb);
            struct tcphdr _tcph, *tcph;
            
            tcph = skb_header_pointer(skb, sizeof(struct ipv6hdr), sizeof(_tcph), &_tcph);
            if (!tcph) {
                DEBUGP("Failed to get TCP header\n");
                return false;
            }
            payload_offset = sizeof(struct ipv6hdr) + tcph->doff * 4;
            data_len = ntohs(ipv6h->payload_len) - (tcph->doff * 4);
        }
        
        if (data_len < 5) { /* TLS记录头最小长度 */
            return false;
        }
        
        /* 检查TLS记录头 */
        if (skb_copy_bits(skb, payload_offset, &record_type, 1) != 0) {
            return false;
        }
        if (record_type != TLS_HANDSHAKE) {
            return false;
        }
        
        DEBUGP("TLS record: type=%u, data_len=%u, payload_offset=%d\n", 
               record_type, data_len, payload_offset);
        
        /* 检查TLS ClientHello消息类型 */
        if (data_len < 5 + 1) { /* 记录头 + 消息类型 */
            return false;
        }
        
        if (skb_copy_bits(skb, payload_offset + 5, &message_type, 1) != 0) {
            return false;
        }
        if (message_type != TLS_CLIENT_HELLO) {
            return false;
        }
        DEBUGP("TLS ClientHello detected\n");
        
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
        if (skb_copy_bits(skb, payload_offset, &record_type, 1) != 0) {
            return false;
        }
        if (record_type != TLS_HANDSHAKE) {
            return false;
        }
        
        DEBUGP("TLS record: type=%u, data_len=%u, payload_offset=%d\n", 
               record_type, data_len, payload_offset);
        
        /* 检查DTLS ClientHello消息类型 */
        if (data_len < 13 + 1) {
            return false;
        }
        
        if (skb_copy_bits(skb, payload_offset + 13, &message_type, 1) != 0) {
            return false;
        }
        if (message_type != TLS_CLIENT_HELLO) {
            return false;
        }
        DEBUGP("TLS ClientHello detected\n");
    } else {
        return false;
    }
    
    return true;
}

// 优化的域名匹配函数，支持多种通配符格式和精确匹配
// 支持严格匹配特定域名及其子域名：使用*.domain.com格式
// 匹配所有以特定后缀结尾的域名时：使用*domain.com格式（更灵活）
// 自动检测并在SNI规则里面跳过包含路径模式(/xxx)的规则
static bool match_string_safe(const char *haystack, size_t haystack_len, const char *needle, size_t needle_len)
{
    size_t i, j;
    
    if (!haystack || !needle || needle_len == 0 || haystack_len < needle_len) {
        return false;
    }
    
    // 检测规则中是否包含路径模式(/xxx)，如果包含则SNI模块跳过处理
    // 因为路径级匹配应该由HTTP过滤模块负责处理
    for (i = 0; i < needle_len; i++) {
        if (needle[i] == '/') {
            DEBUGP("Rule contains path pattern, SNI module skipped: %s\n", needle);
            return false;  // 包含路径模式，SNI模块不处理
        }
    }
    
    // 检查是否为通配符格式
    if (needle[0] == '*') {
        // 情况1：*.domain.com 格式 - 匹配主域名和所有子域名
        if (needle_len >= 3 && needle[1] == '.') {
            const char *domain_part = needle + 1; // 跳过*，从.开始
            size_t domain_len = needle_len - 1;
            
            // 匹配主域名（domain.com）
            if (haystack_len == domain_len - 1 && 
                strncmp(haystack, domain_part + 1, domain_len - 1) == 0) {
                return true;
            }
            
            // 匹配子域名（sub.domain.com）
            if (haystack_len > domain_len && 
                haystack[haystack_len - domain_len] == '.' && 
                strncmp(haystack + haystack_len - domain_len, domain_part, domain_len) == 0) {
                return true;
            }
            
            return false;
        }
        
        // 情况2：*domain.com 格式 - 匹配以domain.com结尾的任何字符串
        if (needle_len >= 2) {
            const char *suffix_part = needle + 1; // 跳过*，取剩余部分
            size_t suffix_len = needle_len - 1;
            
            // 检查长度是否足够
            if (haystack_len < suffix_len) {
                return false;
            }
            
            // 检查末尾是否匹配
            if (strncmp(haystack + haystack_len - suffix_len, suffix_part, suffix_len) == 0) {
                return true;
            }
            
            return false;
        }
    }
    
    // 精确域名匹配（性能最优）
    if (haystack_len == needle_len) {
        return memcmp(haystack, needle, needle_len) == 0;
    }
    
    return false;
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
        DEBUGP("Invalid parameters\n");
        return false;
    }
    
    if (info->len < 1 || info->len >= SNI_MAX_LEN) {
        DEBUGP("Invalid SNI length: %u\n", info->len);
        return false;
    }
    
    /* 确保info->sni以null结尾 */
    const char *sni_pattern = info->sni;
    DEBUGP("Matching pattern: %s\n", sni_pattern);
    
    /* 确定协议类型 */
    if (par->match->family == NFPROTO_IPV4) {
        struct iphdr *iph = ip_hdr(skb);
        if (!iph) {
            DEBUGP("Invalid IPv4 header\n");
            return false;
        }
        protocol = iph->protocol;
    } else if (par->match->family == NFPROTO_IPV6) {
        struct ipv6hdr *ipv6h = ipv6_hdr(skb);
        if (!ipv6h) {
            DEBUGP("Invalid IPv6 header\n");
            return false;
        }
        protocol = ipv6h->nexthdr;
    } else {
        DEBUGP("Unsupported protocol family\n");
        return false;
    }
    
    /* 检查是否为TLS/DTLS ClientHello */
    if (!is_tls_client_hello(skb, protocol)) {
        return false;
    }
    
    /* 计算有效载荷偏移量 */
    if (protocol == IPPROTO_TCP) {
        if (par->match->family == NFPROTO_IPV4) {
            struct iphdr *iph = ip_hdr(skb);
            struct tcphdr _tcph, *tcph;
            
            tcph = skb_header_pointer(skb, iph->ihl * 4, sizeof(_tcph), &_tcph);
            if (!tcph) {
                DEBUGP("Failed to get TCP header\n");
                return false;
            }
            
            payload_offset = iph->ihl * 4 + tcph->doff * 4;
            data_len = ntohs(iph->tot_len) - payload_offset;
        } else {
            struct ipv6hdr *ipv6h = ipv6_hdr(skb);
            struct tcphdr _tcph, *tcph;
            
            tcph = skb_header_pointer(skb, sizeof(struct ipv6hdr), sizeof(_tcph), &_tcph);
            if (!tcph) {
                DEBUGP("Failed to get TCP header\n");
                return false;
            }
            
            payload_offset = sizeof(struct ipv6hdr) + tcph->doff * 4;
            data_len = ntohs(ipv6h->payload_len) - (tcph->doff * 4);
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
        DEBUGP("Unsupported protocol: %u\n", protocol);
        return false;
    }
    
    /* 限制最大数据长度以防止过大的分配 */
    if (data_len < 5 || data_len > SNI_MAX_LEN * 2) {
        DEBUGP("Invalid data length: %u\n", data_len);
        return false;
    }
    
    /* 优化内存分配策略 - 根据实际需要的大小分配 */
    size_t buffer_size = min_t(size_t, data_len, SNI_MAX_LEN * 2);
    u_int8_t *tmp_buffer = kmalloc(buffer_size, GFP_ATOMIC);
    if (!tmp_buffer) {
        DEBUGP("Memory allocation failed\n");
        return false;
    }
    
    /* 复制数据到临时缓冲区 */
    if (skb_copy_bits(skb, payload_offset, tmp_buffer, buffer_size) != 0) {
        DEBUGP("Failed to copy packet data\n");
        kfree(tmp_buffer);
        return false;
    }
    
    /* 提取SNI */
    sni_len = extract_sni_from_tls(tmp_buffer, buffer_size, extracted_sni, sizeof(extracted_sni));
    
    if (sni_len < 0) {
        DEBUGP("Failed to extract SNI\n");
        /* 添加更多调试信息 */
        DEBUGP("Data length: %u, buffer size: %zu\n", data_len, buffer_size);
        if (buffer_size >= 6) {
            DEBUGP("First 6 bytes: %02x %02x %02x %02x %02x %02x\n", 
                   tmp_buffer[0], tmp_buffer[1], tmp_buffer[2], tmp_buffer[3], tmp_buffer[4], tmp_buffer[5]);
        }
        kfree(tmp_buffer);
        return false;
    }
    
    kfree(tmp_buffer);
    
    /* 安全地进行字符串匹配 */
    bool matched = false;
    if (sni_len > 0 && sni_len < SNI_MAX_LEN) {
        /* 确保提取的SNI以null结尾 */
        extracted_sni[SNI_MAX_LEN - 1] = '\0';
        matched = match_string_safe(extracted_sni, sni_len, info->sni, info->len);
        DEBUGP("SNI match result: %s\n", matched ? "true" : "false");
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
    DEBUGP("Initializing SNI filter module\n");
    int ret = xt_register_matches(sni_match, ARRAY_SIZE(sni_match));
    if (ret != 0) {
        printk(KERN_ERR "[SNI-FILTER] Failed to register matches: %d\n", ret);
    }
    return ret;
}

static void __exit xt_sni_fini(void)
{
    DEBUGP("Unloading SNI filter module\n");
    xt_unregister_matches(sni_match, ARRAY_SIZE(sni_match));
}

module_init(xt_sni_init);
module_exit(xt_sni_fini);