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
#include <linux/ratelimit.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Padavan Firmware");
MODULE_ALIAS("ip6t_sni");
MODULE_ALIAS("ipt_sni");
MODULE_DESCRIPTION("Xtables: match SNI from TLS ClientHello packets");

#define SNI_MAX_LEN 256
// 增加一个更大的缓冲区定义用于处理TLS数据
#define TLS_MAX_HANDSHAKE_LEN 1024
#define TLS_HANDSHAKE 22
#define TLS_CLIENT_HELLO 1
#define TLS_EXTENSION_SNI 0x0000

// 对于小数据包使用栈分配，大数据包才使用堆分配
#define STACK_BUFFER_SIZE 512

/* 模块参数配置 */
static int enable_debug = 0;
module_param(enable_debug, int, 0644);
MODULE_PARM_DESC(enable_debug, "Enable debugging messages (0=off, 1=on)");

// 用来控制是否保存异常数据的全局变量
static bool save_failed_packets = false;
module_param(save_failed_packets, bool, 0644);
MODULE_PARM_DESC(save_failed_packets, "Save failed packets to file for debugging");

// 速率限制定义
#define SNI_DEBUG_RATELIMIT_INTERVAL (60 * HZ)  // 60秒间隔
#define SNI_DEBUG_RATELIMIT_BURST 10            // 每次最多输出10条

/* 调试宏定义 */
#ifdef DEBUG
#define DEBUGP(fmt, args...) do { \
    if (net_ratelimit()) { \
        printk(KERN_DEBUG "[SNI-FILTER] %s:%d: " fmt, __func__, __LINE__, ##args); \
    } \
} while (0)
#else
#define DEBUGP(fmt, args...) do { \
    if (enable_debug && net_ratelimit()) { \
        printk(KERN_DEBUG "[SNI-FILTER] %s:%d: " fmt, __func__, __LINE__, ##args); \
    } \
} while (0)
#endif

// 增强调试宏
#define ENHANCED_DEBUG(fmt, args...) do { \
    if (enable_debug && net_ratelimit()) { \
        printk(KERN_DEBUG "[SNI-FILTER-ENHANCED] %s:%d: " fmt, __func__, __LINE__, ##args); \
    } \
} while (0)

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

static void save_packet_data(const u_int8_t *data, size_t len, const char *reason)
{
    struct file *filp;
    loff_t pos = 0;
    char filename[64];
    char header[128];
    int header_len;
    int ret;

    if (!save_failed_packets)
        return;

    // 使用ktime_get_seconds代替get_seconds
    snprintf(filename, sizeof(filename), "/tmp/sni_failed_packet_%llu.dat", 
             (unsigned long long)ktime_get_seconds());
    
    filp = filp_open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (IS_ERR(filp)) {
        DEBUGP("Failed to open file %s for writing failed packet\n", filename);
        return;
    }

    // 写入头部信息时添加更多上下文
    header_len = snprintf(header, sizeof(header), 
                         "Packet saved at: %llu\n"
                         "Reason: %s\n"
                         "Length: %zu\n"
                         "Process: %s (PID: %d)\n"  // 添加进程信息
                         "First 64 bytes (hex): ",
                         (unsigned long long)ktime_get_seconds(), 
                         reason, len,
                         current->comm, current->pid);  // 添加进程信息
    
    ret = kernel_write(filp, header, header_len, &pos);
    if (ret < 0) {
        DEBUGP("Failed to write header to %s\n", filename);
        filp_close(filp, NULL);
        return;
    }
    pos += ret;

    // 写入数据的十六进制表示
    if (len > 0 && data) {
        char hex_buf[128];
        size_t i;
        size_t bytes_to_write = min(len, (size_t)64); // 只写前64字节
        
        for (i = 0; i < bytes_to_write; i++) {
            if (i % 16 == 0 && i > 0) {
                snprintf(hex_buf, sizeof(hex_buf), "\n");
                kernel_write(filp, hex_buf, strlen(hex_buf), &pos);
            }
            snprintf(hex_buf, sizeof(hex_buf), "%02x ", data[i]);
            kernel_write(filp, hex_buf, strlen(hex_buf), &pos);
        }
        snprintf(hex_buf, sizeof(hex_buf), "\n");
        kernel_write(filp, hex_buf, strlen(hex_buf), &pos);
    }

    filp_close(filp, NULL);
    DEBUGP("Saved failed packet to %s\n", filename);
}
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
    
    ENHANCED_DEBUG("TLS Handshake verification passed, type=%u, length=%u\n", handshake->type, handshake_len);
    
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
    
    ENHANCED_DEBUG("Extensions parsing started, extensions_len=%u\n", extensions_len);
    
    while (ptr < ext_end && remaining >= 4) {
        u_int16_t ext_type = ntohs(*(u_int16_t *)ptr);
        u_int16_t ext_len = ntohs(*(u_int16_t *)(ptr + 2));
        
        ENHANCED_DEBUG("Processing extension: type=%u, len=%u\n", ext_type, ext_len);
        
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
                        ENHANCED_DEBUG("Extracted SNI: %s\n", sni_out);
                        
                        // 特殊处理 news.qq.com 的情况
                        if (strcmp(sni_out, "news.qq.com") == 0) {
                            ENHANCED_DEBUG("Special case detected: news.qq.com\n");
                        }
                        
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

/* 检查是否为TLS ClientHello包 - 改进版本 */
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
    
    ENHANCED_DEBUG("Checking TLS ClientHello for protocol %u\n", protocol);
    
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
            
            ENHANCED_DEBUG("IPv4 packet analysis: tot_len=%u, ihl=%u, doff=%u, payload_offset=%d, data_len=%u\n",
                ntohs(iph->tot_len), iph->ihl, tcph->doff, payload_offset, data_len);
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
            DEBUGP("Data too short for TLS record header: %u\n", data_len);
            return false;
        }
        
        /* 检查TLS记录头 */
        if (skb_copy_bits(skb, payload_offset, &record_type, 1) != 0) {
            DEBUGP("Failed to copy record type\n");
            // 保存失败的数据包
            u_int8_t temp_byte;
            if (skb_copy_bits(skb, payload_offset, &temp_byte, 1) != 0) {
                save_packet_data(NULL, 0, "Failed to copy record type");
            }
            return false;
        }
        
        // 允许一些变种的记录类型
        if (record_type != TLS_HANDSHAKE) {
            DEBUGP("Record type mismatch: expected %u, got %u\n", TLS_HANDSHAKE, record_type);
            // 统一保存失败的数据包
            u_int8_t first_bytes[32];
            if (skb_copy_bits(skb, payload_offset, first_bytes, min(32U, data_len)) == 0) {
                save_packet_data(first_bytes, min(32U, data_len), "Record type mismatch");
            }
            return false;
        }
        
        ENHANCED_DEBUG("TLS record: type=%u, data_len=%u, payload_offset=%d\n", 
               record_type, data_len, payload_offset);
        
        /* 检查TLS ClientHello消息类型 */
        if (data_len < 5 + 1) { /* 记录头 + 消息类型 */
            DEBUGP("Data too short for message type: %u\n", data_len);
            return false;
        }
        
        if (skb_copy_bits(skb, payload_offset + 5, &message_type, 1) != 0) {
            DEBUGP("Failed to copy message type\n");
            return false;
        }
        
        if (message_type != TLS_CLIENT_HELLO) {
            DEBUGP("Message type mismatch: expected %u, got %u\n", TLS_CLIENT_HELLO, message_type);
            return false;
        }
        
        ENHANCED_DEBUG("TLS ClientHello detected\n");
        
    } else if (protocol == IPPROTO_UDP) {
        /* 处理DTLS情况 */
        // 保持原有实现
        // ...
    } else {
        DEBUGP("Unsupported protocol: %u\n", protocol);
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
    
    ENHANCED_DEBUG("Matching haystack='%.*s' (%zu) with needle='%.*s' (%zu)\n", 
           (int)haystack_len, haystack, haystack_len, 
           (int)needle_len, needle, needle_len);
    
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
            
            ENHANCED_DEBUG("Wildcard format *.domain.com detected, domain_part='%s', domain_len=%zu\n", domain_part, domain_len);
            
            // 匹配主域名（domain.com）
            if (haystack_len == domain_len - 1 && 
                strncmp(haystack, domain_part + 1, domain_len - 1) == 0) {
                ENHANCED_DEBUG("Matched main domain: %.*s\n", (int)(domain_len - 1), domain_part + 1);
                return true;
            }
            
            // 匹配子域名（sub.domain.com）
            if (haystack_len > domain_len && 
                haystack[haystack_len - domain_len] == '.' && 
                strncmp(haystack + haystack_len - domain_len, domain_part, domain_len) == 0) {
                ENHANCED_DEBUG("Matched subdomain: %.*s\n", (int)haystack_len, haystack);
                return true;
            }
            
            return false;
        }
        
        // 情况2：*domain.com 格式 - 匹配以domain.com结尾的任何字符串
        if (needle_len >= 2) {
            const char *suffix_part = needle + 1; // 跳过*，取剩余部分
            size_t suffix_len = needle_len - 1;
            
            ENHANCED_DEBUG("Wildcard format *domain.com detected, suffix_part='%s', suffix_len=%zu\n", suffix_part, suffix_len);
            
            // 检查长度是否足够
            if (haystack_len < suffix_len) {
                return false;
            }
            
            // 检查末尾是否匹配
            if (strncmp(haystack + haystack_len - suffix_len, suffix_part, suffix_len) == 0) {
                ENHANCED_DEBUG("Matched suffix: %.*s\n", (int)suffix_len, suffix_part);
                return true;
            }
            
            return false;
        }
    }
    
    // 精确域名匹配（性能最优）
    if (haystack_len == needle_len) {
        bool result = memcmp(haystack, needle, needle_len) == 0;
        if (result) {
            ENHANCED_DEBUG("Exact match found: %.*s\n", (int)needle_len, needle);
        }
        return result;
    }
    
    ENHANCED_DEBUG("No match found\n");
    return false;
}

/* 主要的匹配函数  添加重试机制 */
static bool xt_sni_match(const struct sk_buff *skb, struct xt_action_param *par)
{
    const struct xt_sni_info *info = par->matchinfo;
    u_int32_t data_len = 0;
    char extracted_sni[SNI_MAX_LEN];
    int sni_len = -1;
    u_int8_t protocol;
    int payload_offset = 0;
    
    /* 参数验证 */
    if (!skb || !info || !par) {
        DEBUGP("Invalid parameters\n");
        return false;
    }
    
    if (info->len < 1 || info->len >= SNI_MAX_LEN) {
        DEBUGP("Invalid SNI length: %u\n", info->len);
        return false;
    }
    
    ENHANCED_DEBUG("Starting SNI matching process\n");
    
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
        ENHANCED_DEBUG("IPv4 protocol: %u\n", protocol);
    } else if (par->match->family == NFPROTO_IPV6) {
        struct ipv6hdr *ipv6h = ipv6_hdr(skb);
        if (!ipv6h) {
            DEBUGP("Invalid IPv6 header\n");
            return false;
        }
        protocol = ipv6h->nexthdr;
        ENHANCED_DEBUG("IPv6 protocol: %u\n", protocol);
    } else {
        DEBUGP("Unsupported protocol family\n");
        return false;
    }
    
    /* 检查是否为TLS/DTLS ClientHello */
    int retry_count = 0;
    const int max_retries = min(max_retries_param, 10); // 使用模块参数并限制最大值
    
    while (retry_count < max_retries) {
        if (is_tls_client_hello(skb, protocol)) {
            ENHANCED_DEBUG("Valid TLS ClientHello packet detected after %d retries\n", retry_count);
            break;
        }
        
        if (retry_count < max_retries - 1) {
            // 更轻量级的等待
            cpu_relax();
            retry_count++;
        } else {
            ENHANCED_DEBUG("Not a TLS ClientHello packet after %d retries\n", max_retries);
            
            // 保存无法识别的数据包用于分析
            if (save_failed_packets) {
                // 尝试获取数据包的一些基本信息用于保存
                int offset = 0;
                size_t save_size = min(packet_save_size_param, 256U); // 使用模块参数并限制最大值
                u_int8_t *first_bytes = kmalloc(save_size, GFP_ATOMIC);
                
                if (first_bytes) {
                    // 根据协议类型计算偏移量
                    if (protocol == IPPROTO_TCP) {
                        if (par->match->family == NFPROTO_IPV4) {
                            struct iphdr *iph = ip_hdr(skb);
                            struct tcphdr _tcph, *tcph;
                            
                            tcph = skb_header_pointer(skb, iph->ihl * 4, sizeof(_tcph), &_tcph);
                            if (tcph) {
                                offset = iph->ihl * 4 + tcph->doff * 4;
                            }
                        } else {
                            struct ipv6hdr *ipv6h = ipv6_hdr(skb);
                            struct tcphdr _tcph, *tcph;
                            
                            tcph = skb_header_pointer(skb, sizeof(struct ipv6hdr), sizeof(_tcph), &_tcph);
                            if (tcph) {
                                offset = sizeof(struct ipv6hdr) + tcph->doff * 4;
                            }
                        }
                    }
                    
                    // 尝试复制数据
                    if (skb_copy_bits(skb, offset, first_bytes, save_size) == 0) {
                        char reason[128];
                        snprintf(reason, sizeof(reason), "Not a TLS ClientHello packet (proto=%u, family=%u)", 
                                 protocol, par->match->family);
                        save_packet_data(first_bytes, save_size, reason);
                    } else {
                        save_packet_data(NULL, 0, "Not a TLS ClientHello packet - failed to copy data");
                    }
                    
                    kfree(first_bytes);
                }
            }
            
            return false;
        }
    }
    
    ENHANCED_DEBUG("Valid TLS ClientHello packet detected\n");
    
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
    
    ENHANCED_DEBUG("Payload offset calculated: %d, data_len: %u\n", payload_offset, data_len);
    
    /* 限制最大数据长度以防止过大的分配 */
    if (data_len < 5) {
        DEBUGP("Invalid data length: %u\n", data_len);
        return false;
    }
    
    /* 优化内存分配策略 - 根据实际需要的大小分配 */
    size_t buffer_size = min_t(size_t, data_len, TLS_MAX_HANDSHAKE_LEN);
    
    /* 对于小数据包使用栈分配，大数据包才使用堆分配 */
    #define STACK_BUFFER_SIZE 512
    u_int8_t *tmp_buffer = NULL;
    u_int8_t stack_buffer[STACK_BUFFER_SIZE];
    
    if (buffer_size <= STACK_BUFFER_SIZE) {
        tmp_buffer = stack_buffer;
        ENHANCED_DEBUG("Using stack buffer of size: %zu\n", buffer_size);
    } else {
        tmp_buffer = kmalloc(buffer_size, GFP_ATOMIC);
        if (!tmp_buffer) {
            DEBUGP("Memory allocation failed\n");
            return false;
        }
        ENHANCED_DEBUG("Allocated heap buffer of size: %zu\n", buffer_size);
    }
    
    /* 复制数据到临时缓冲区 */
    if (skb_copy_bits(skb, payload_offset, tmp_buffer, buffer_size) != 0) {
        DEBUGP("Failed to copy packet data\n");
        if (buffer_size > STACK_BUFFER_SIZE) {
            kfree(tmp_buffer);
        }
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
        if (buffer_size > STACK_BUFFER_SIZE) {
            kfree(tmp_buffer);
        }
        return false;
    }
    
    if (buffer_size > STACK_BUFFER_SIZE) {
        kfree(tmp_buffer);
    }
    
    ENHANCED_DEBUG("Successfully extracted SNI: %s (length: %d)\n", extracted_sni, sni_len);
    
    /* 特殊处理 news.qq.com */
    if (strcmp(extracted_sni, "news.qq.com") == 0) {
        ENHANCED_DEBUG("Special handling for news.qq.com\n");
    }
    
    /* 安全地进行字符串匹配 */
    bool matched = false;
    if (sni_len > 0 && sni_len < SNI_MAX_LEN) {
        /* 确保提取的SNI以null结尾 */
        extracted_sni[SNI_MAX_LEN - 1] = '\0';
        matched = match_string_safe(extracted_sni, sni_len, info->sni, info->len);
        ENHANCED_DEBUG("SNI match result: %s\n", matched ? "true" : "false");
    }
    
    bool result = (matched ^ info->invert);
    ENHANCED_DEBUG("Final match result (after invert): %s\n", result ? "true" : "false");
    
    return result;
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