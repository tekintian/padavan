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
#include <linux/ipv6.h>

#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/ratelimit.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/time.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Padavan Firmware");
MODULE_ALIAS("ip6t_sni");
MODULE_ALIAS("ipt_sni");
MODULE_DESCRIPTION("Xtables: match SNI from TLS ClientHello packets");

#define SNI_MAX_LEN 256
/* 增加一个更大的缓冲区定义用于处理TLS数据 */
#define TLS_MAX_HANDSHAKE_LEN 1024
#define TLS_HANDSHAKE 22
#define TLS_CLIENT_HELLO 1
#define TLS_EXTENSION_SNI 0x0000

/* 对于小数据包使用栈分配，大数据包才使用堆分配 */
#define STACK_BUFFER_SIZE 512

/* 模块参数配置 */
static int enable_debug = 0;
module_param(enable_debug, int, 0644);
MODULE_PARM_DESC(enable_debug, "Enable debugging messages (0=off, 1=on)");

/* 用来控制是否保存异常数据的全局变量 */
static bool save_failed_packets = false;
module_param(save_failed_packets, bool, 0644);
MODULE_PARM_DESC(save_failed_packets, "Save failed packets to file for debugging");

/* 速率限制定义 */
#define SNI_DEBUG_RATELIMIT_INTERVAL (60 * HZ)  /* 60秒间隔 */
#define SNI_DEBUG_RATELIMIT_BURST 10            /* 每次最多输出10条 */

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

// 自定义memmem实现，因为内核可能不提供此函数
static void *custom_memmem(const void *haystack, size_t haystack_len,
                          const void *needle, size_t needle_len)
{
    const char *h = haystack;
    const char *n = needle;
    size_t i, j;

    if (needle_len == 0)
        return (void *)haystack;

    if (haystack_len < needle_len)
        return NULL;

    for (i = 0; i <= haystack_len - needle_len; i++) {
        for (j = 0; j < needle_len; j++) {
            if (h[i+j] != n[j])
                break;
        }
        if (j == needle_len)
            return (void *)(h + i);
    }

    return NULL;
}

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
    
    // 修复kernel_write参数问题：传递pos的值而不是地址，并手动更新pos
    ret = kernel_write(filp, header, header_len, pos);
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
                // 修复kernel_write参数问题：传递pos的值而不是地址，并手动更新pos
                ret = kernel_write(filp, hex_buf, strlen(hex_buf), pos);
                if (ret > 0)
                    pos += ret;
            }
            snprintf(hex_buf, sizeof(hex_buf), "%02x ", data[i]);
            // 修复kernel_write参数问题：传递pos的值而不是地址，并手动更新pos
            ret = kernel_write(filp, hex_buf, strlen(hex_buf), pos);
            if (ret > 0)
                pos += ret;
        }
        snprintf(hex_buf, sizeof(hex_buf), "\n");
        // 修复kernel_write参数问题：传递pos的值而不是地址，并手动更新pos
        ret = kernel_write(filp, hex_buf, strlen(hex_buf), pos);
        if (ret > 0)
            pos += ret;
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
    const struct tls_handshake *handshake;
    u_int32_t handshake_len;
    const struct tls_client_hello *client_hello;
    
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
    
    handshake = (const struct tls_handshake *)data;
    handshake_len = ntohs(handshake->length);
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
        DEBUGP("Data too短 for ClientHello\n");
        return -1;
    }
    
    client_hello = (const struct tls_client_hello *)ptr;
    
    /* 跳过ClientHello固定部分 */
    ptr += sizeof(struct tls_client_hello);
    remaining -= sizeof(struct tls_client_hello);
    
    /* 跳过session_id */
    if (remaining < client_hello->session_id_len) {
        DEBUGP("Data too短 for session ID\n");
        return -1;
    }
    ptr += client_hello->session_id_len;
    remaining -= client_hello->session_id_len;
    
    /* 获取密码套件长度 */
    if (remaining < sizeof(cipher_suites_len)) {
        DEBUGP("Data too短 for cipher suites length\n");
        return -1;
    }
    cipher_suites_len = ntohs(*(u_int16_t *)ptr);
    ptr += sizeof(cipher_suites_len);
    remaining -= sizeof(cipher_suites_len);
    
    /* 跳过密码套件 */
    if (remaining < cipher_suites_len) {
        DEBUGP("Data too短 for cipher suites\n");
        return -1;
    }
    ptr += cipher_suites_len;
    remaining -= cipher_suites_len;
    
    /* 获取压缩方法长度 */
    if (remaining < sizeof(compression_methods_len)) {
        DEBUGP("Data too短 for compression methods length\n");
        return -1;
    }
    compression_methods_len = *ptr;
    ptr += sizeof(compression_methods_len);
    remaining -= sizeof(compression_methods_len);
    
    /* 跳过压缩方法 */
    if (remaining < compression_methods_len) {
        DEBUGP("Data too短 for compression methods\n");
        return -1;
    }
    ptr += compression_methods_len;
    remaining -= compression_methods_len;
    
    /* 获取扩展长度 */
    if (remaining < sizeof(extensions_len)) {
        DEBUGP("Data too短 for extensions length\n");
        return -1;
    }
    extensions_len = ntohs(*(u_int16_t *)ptr);
    ptr += sizeof(extensions_len);
    remaining -= sizeof(extensions_len);
    
    /* 解析扩展 */
    while (remaining > 0 && extensions_len > 0) {
        u_int16_t ext_type, ext_len;
        
        if (remaining < 2 * sizeof(u_int16_t)) {
            DEBUGP("Data too短 for extension header\n");
            return -1;
        }
        
        ext_type = ntohs(*(u_int16_t *)ptr);
        ptr += sizeof(ext_type);
        remaining -= sizeof(ext_type);
        extensions_len -= sizeof(ext_type);
        
        ext_len = ntohs(*(u_int16_t *)ptr);
        ptr += sizeof(ext_len);
        remaining -= sizeof(ext_len);
        extensions_len -= sizeof(ext_len);
        
        if (remaining < ext_len) {
            DEBUGP("Data too短 for extension data\n");
            return -1;
        }
        
        if (ext_type == TLS_EXTENSION_SNI) {
            u_int16_t sni_list_len;
            u_int8_t sni_type;
            u_int16_t sni_len;
            
            if (ext_len < 2 * sizeof(u_int16_t) + sizeof(u_int8_t)) {
                DEBUGP("Extension data too短 for SNI header\n");
                return -1;
            }
            
            sni_list_len = ntohs(*(u_int16_t *)ptr);
            ptr += sizeof(sni_list_len);
            remaining -= sizeof(sni_list_len);
            ext_len -= sizeof(sni_list_len);
            
            sni_type = *ptr;
            ptr += sizeof(sni_type);
            remaining -= sizeof(sni_type);
            ext_len -= sizeof(sni_type);
            
            if (ext_len < sizeof(sni_len)) {
                DEBUGP("Extension data too短 for SNI length\n");
                return -1;
            }
            
            sni_len = ntohs(*(u_int16_t *)ptr);
            ptr += sizeof(sni_len);
            remaining -= sizeof(sni_len);
            ext_len -= sizeof(sni_len);
            
            if (ext_len < sni_len) {
                DEBUGP("Extension data too短 for SNI\n");
                return -1;
            }
            
            if (sni_len >= sni_out_len) {
                DEBUGP("SNI too长 for output buffer\n");
                return -1;
            }
            
            memcpy(sni_out, ptr, sni_len);
            sni_out[sni_len] = '\0';
            
            ENHANCED_DEBUG("Extracted SNI: %s\n", sni_out);
            return sni_len;
        }
        
        ptr += ext_len;
        remaining -= ext_len;
        extensions_len -= ext_len;
    }
    
    DEBUGP("No SNI extension found\n");
    return -1;
}

/* 安全地进行字符串匹配 */
static bool match_string_safe(const char *haystack, size_t haystack_len, const char *needle, size_t needle_len)
{
    if (!haystack || !needle || needle_len == 0 || haystack_len < needle_len) {
        return false;
    }
    
    // 使用自定义memmem实现替换原来的memmem函数
    return custom_memmem(haystack, haystack_len, needle, needle_len) != NULL;
}

static bool xt_sni_match(const struct sk_buff *skb, struct xt_action_param *par)
{
    // C90兼容性：将所有变量声明移到函数开始处
    const struct xt_sni_info *info = par->matchinfo;
    char extracted_sni[SNI_MAX_LEN];
    int sni_len = 0;
    unsigned int payload_offset = 0;
    unsigned int data_len = 0;
    u_int8_t protocol = 0;
    int max_retries = 3;
    int retry_count = 0;
    unsigned int packet_save_size_param = 32; // 默认保存大小
    size_t save_size;
    u_int8_t *first_bytes;
    int offset = 0;
    struct iphdr *iph;
    struct tcphdr _tcph, *tcph;
    char reason[128];
    size_t buffer_size;
    u_int8_t *tmp_buffer = NULL;
    u_int8_t stack_buffer[STACK_BUFFER_SIZE];
    bool matched = false;
    bool result;
    const struct iphdr *iph_const;
    const struct tcphdr *tcph_const;
    const struct ipv6hdr *ipv6h_const; // 重命名变量以避免冲突
    u_int8_t record_type;
    u_int8_t handshake_type;
    
    ENHANCED_DEBUG("Matching SNI: %.*s (invert: %u)\n", info->len, info->sni, info->invert);
    
    /* 获取协议类型 */
    if (par->match->family == NFPROTO_IPV4) {
        iph_const = ip_hdr(skb);
        protocol = iph_const->protocol;
    } else {
        ipv6h_const = ipv6_hdr(skb);
        protocol = ipv6h_const->nexthdr;
    }
    
    /* 检查是否为TCP协议 */
    if (protocol != IPPROTO_TCP) {
        DEBUGP("Not a TCP packet, protocol: %u\n", protocol);
        return false;
    }
    
    /* 循环检查是否为TLS ClientHello */
    while (retry_count < max_retries) {
        if (protocol == IPPROTO_TCP) {
            if (par->match->family == NFPROTO_IPV4) {
                iph_const = ip_hdr(skb);
                
                tcph_const = skb_header_pointer(skb, iph_const->ihl * 4, sizeof(*tcph_const), &(struct tcphdr){0});
                if (!tcph_const) {
                    DEBUGP("Failed to get TCP header\n");
                    return false;
                }
                
                payload_offset = iph_const->ihl * 4 + tcph_const->doff * 4;
                data_len = ntohs(iph_const->tot_len) - payload_offset;
            } else {
                ipv6h_const = ipv6_hdr(skb);
                
                tcph_const = skb_header_pointer(skb, sizeof(struct ipv6hdr), sizeof(*tcph_const), &(struct tcphdr){0});
                if (!tcph_const) {
                    DEBUGP("Failed to get TCP header\n");
                    return false;
                }
                
                payload_offset = sizeof(struct ipv6hdr) + tcph_const->doff * 4;
                data_len = ntohs(ipv6h_const->payload_len) - (tcph_const->doff * 4);
            }
            
            /* 检查是否足够长 */
            if (data_len < 5) {
                DEBUGP("Data too短: %u\n", data_len);
                if (retry_count < max_retries - 1) {
                    cpu_relax();
                    retry_count++;
                    continue;
                } else {
                    break;
                }
            }
            
            /* 检查是否为TLS记录 */
            if (skb_copy_bits(skb, payload_offset, &record_type, 1) != 0) {
                DEBUGP("Failed to copy record type\n");
                if (retry_count < max_retries - 1) {
                    cpu_relax();
                    retry_count++;
                    continue;
                } else {
                    break;
                }
            }
            
            if (record_type != TLS_HANDSHAKE) {
                DEBUGP("Not a TLS handshake record: %u\n", record_type);
                if (retry_count < max_retries - 1) {
                    cpu_relax();
                    retry_count++;
                    continue;
                } else {
                    break;
                }
            }
            
            /* 检查握手类型 */
            if (skb_copy_bits(skb, payload_offset + 5, &handshake_type, 1) != 0) {
                DEBUGP("Failed to copy handshake type\n");
                if (retry_count < max_retries - 1) {
                    cpu_relax();
                    retry_count++;
                    continue;
                } else {
                    break;
                }
            }
            
            if (handshake_type != TLS_CLIENT_HELLO) {
                DEBUGP("Not a ClientHello handshake: %u\n", handshake_type);
                if (retry_count < max_retries - 1) {
                    cpu_relax();
                    retry_count++;
                    continue;
                } else {
                    break;
                }
            }
            
            /* 成功识别到TLS ClientHello */
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
                // 修复min宏的类型问题
                save_size = min((size_t)packet_save_size_param, (size_t)256U); // 使用模块参数并限制最大值
                first_bytes = kmalloc(save_size, GFP_ATOMIC);
                
                if (first_bytes) {
                    // 根据协议类型计算偏移量
                    if (protocol == IPPROTO_TCP) {
                        if (par->match->family == NFPROTO_IPV4) {
                            iph = ip_hdr(skb);
                            
                            tcph = skb_header_pointer(skb, iph->ihl * 4, sizeof(_tcph), &_tcph);
                            if (tcph) {
                                offset = iph->ihl * 4 + tcph->doff * 4;
                            }
                        } else {
                            tcph = skb_header_pointer(skb, sizeof(struct ipv6hdr), sizeof(_tcph), &_tcph);
                            if (tcph) {
                                offset = sizeof(struct ipv6hdr) + tcph->doff * 4;
                            }
                        }
                    }
                    
                    // 尝试复制数据
                    if (skb_copy_bits(skb, offset, first_bytes, save_size) == 0) {
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
            iph = ip_hdr(skb);
            
            tcph = skb_header_pointer(skb, iph->ihl * 4, sizeof(_tcph), &_tcph);
            if (!tcph) {
                DEBUGP("Failed to get TCP header\n");
                return false;
            }
            
            payload_offset = iph->ihl * 4 + tcph->doff * 4;
            data_len = ntohs(iph->tot_len) - payload_offset;
        } else {
            tcph = skb_header_pointer(skb, sizeof(struct ipv6hdr), sizeof(_tcph), &_tcph);
            if (!tcph) {
                DEBUGP("Failed to get TCP header\n");
                return false;
            }
            
            payload_offset = sizeof(struct ipv6hdr) + tcph->doff * 4;
            // 注意：对于IPv6，我们不能简单地从payload_len计算总长度
            // 我们需要从skb中获取实际数据长度
            data_len = skb->len - payload_offset;
        }
    } else if (protocol == IPPROTO_UDP) {
        if (par->match->family == NFPROTO_IPV4) {
            iph = ip_hdr(skb);
            payload_offset = iph->ihl * 4 + sizeof(struct udphdr);
            data_len = ntohs(iph->tot_len) - iph->ihl * 4 - sizeof(struct udphdr);
        } else {
            payload_offset = sizeof(struct ipv6hdr) + sizeof(struct udphdr);
            // 注意：对于IPv6 UDP，我们需要从skb中获取实际数据长度
            data_len = skb->len - payload_offset;
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
    buffer_size = min_t(size_t, (size_t)data_len, (size_t)TLS_MAX_HANDSHAKE_LEN);
    
    /* 对于小数据包使用栈分配，大数据包才使用堆分配 */
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
    if (sni_len > 0 && sni_len < SNI_MAX_LEN) {
        /* 确保提取的SNI以null结尾 */
        extracted_sni[SNI_MAX_LEN - 1] = '\0';
        matched = match_string_safe(extracted_sni, sni_len, info->sni, info->len);
        ENHANCED_DEBUG("SNI match result: %s\n", matched ? "true" : "false");
    }
    
    result = (matched ^ info->invert);
    ENHANCED_DEBUG("Final match result (after invert): %s\n", result ? "true" : "false");
    
    return result;
}

static struct xt_match sni_match[] = {
    {
        .name       = "sni",
        .family     = NFPROTO_IPV4,
        .match      = xt_sni_match,
        .matchsize  = sizeof(struct xt_sni_info),
        .me         = THIS_MODULE,
    },
#if defined(CONFIG_IPV6)
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
    int ret;
    DEBUGP("Initializing SNI filter module\n");
    ret = xt_register_matches(sni_match, ARRAY_SIZE(sni_match));
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