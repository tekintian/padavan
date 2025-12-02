/*
 * xt_sni_filter.c
 * 
 * Copyright (C) 2025 dev.tekin.cn
 * @author tekintian <tekintian@gmail.com>
 * 
 * Xtables module for matching Server Name Indication (SNI) in TLS ClientHello packets
 *
 * Based on xt_string.c and various netfilter modules
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/ipv6.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv6/ip6_tables.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <net/tcp.h>
#include <net/udp.h>

#define SNI_MAX_LEN 256
#define TLS_MAX_HANDSHAKE_LEN 2048
#define TLS_HANDSHAKE 22
#define TLS_CLIENT_HELLO 1
#define TLS_EXTENSION_SNI 0
#define STACK_BUFFER_SIZE 512

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OpenWrt.org.cn, CSDN博客专家");
MODULE_DESCRIPTION("Xtables: SNI (Server Name Indication) matching");
MODULE_ALIAS("ipt_sni");
MODULE_ALIAS("ip6t_sni");

/* 模块参数 */
static bool enable_debug = false;
module_param(enable_debug, bool, 0644);
MODULE_PARM_DESC(enable_debug, "Enable debug output");

static bool save_failed_packets = false;
module_param(save_failed_packets, bool, 0644);
MODULE_PARM_DESC(save_failed_packets, "Save failed packets for analysis");

static char packet_save_path[256] = "/tmp/sni_failed_packets.log";
module_param_string(packet_save_path, packet_save_path, sizeof(packet_save_path), 0644);
MODULE_PARM_DESC(packet_save_path, "Path to save failed packets");

static int packet_save_size_param = 256;
module_param(packet_save_size_param, int, 0644);
MODULE_PARM_DESC(packet_save_size_param, "Size of packet data to save (default 256 bytes)");

/* 调试宏 */
#define DEBUGP(format, args...) \
    do { \
        if (enable_debug) \
            printk(KERN_INFO "[SNI-FILTER] " format, ##args); \
    } while (0)

#define ENHANCED_DEBUG(format, args...) \
    do { \
        if (enable_debug) \
            printk(KERN_INFO "[SNI-FILTER-ENHANCED] " format, ##args); \
    } while (0)

struct xt_sni_info {
    char sni[SNI_MAX_LEN];
    uint16_t len;
    uint16_t invert;
};

/* TLS解析相关结构 */
struct tls_handshake {
    uint8_t type;
    uint8_t version_major;
    uint8_t version_minor;
    uint16_t length;
} __attribute__((packed));

struct tls_client_hello {
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t random[32];
    uint8_t session_id_len;
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

static void save_packet_data(const unsigned char *data, size_t len, const char *reason) {
    struct file *filp;
    loff_t pos = 0;
    char header[256];
    char hex_buf[6]; // 2 chars + space/null
    int header_len;
    int ret;
    int i;
    long sec;

    if (!packet_save_path[0]) {
        DEBUGP("Packet save path is empty\n");
        return;
    }

    filp = filp_open(packet_save_path, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (IS_ERR(filp)) {
        DEBUGP("Cannot open file %s for writing\n", packet_save_path);
        return;
    }

    // 获取当前时间戳
    sec = ktime_get_seconds();
    
    // 写入头部信息
    header_len = snprintf(header, sizeof(header), 
                         "\n--- Packet saved at %ld seconds ---\n"
                         "Reason: %s\n"
                         "Data length: %zu bytes\n"
                         "Process: %s (PID: %d)\n"
                         "----------------------------------------\n",
                         sec, reason ? reason : "Unknown", len,
                         current->comm, current->pid);
                         
    // 修复kernel_write参数问题：传递pos的值而不是地址，并手动更新pos
    ret = kernel_write(filp, header, header_len, pos);
    if (ret < 0) {
        DEBUGP("Failed to write header to file: %d\n", ret);
        filp_close(filp, NULL);
        return;
    }
    pos += ret;

    // 写入十六进制转储
    for (i = 0; i < len && i < packet_save_size_param; i++) {
        if (i % 16 == 0) {
            if (i > 0) {
                snprintf(hex_buf, sizeof(hex_buf), "\n");
                // 修复kernel_write参数问题：传递pos的值而不是地址，并手动更新pos
                ret = kernel_write(filp, hex_buf, strlen(hex_buf), pos);
                if (ret < 0) {
                    DEBUGP("Failed to write newline to file: %d\n", ret);
                    filp_close(filp, NULL);
                    return;
                }
                pos += ret;
            }
            snprintf(hex_buf, sizeof(hex_buf), "%04x: ", i);
            // 修复kernel_write参数问题：传递pos的值而不是地址，并手动更新pos
            ret = kernel_write(filp, hex_buf, strlen(hex_buf), pos);
            if (ret < 0) {
                DEBUGP("Failed to write address to file: %d\n", ret);
                filp_close(filp, NULL);
                return;
            }
            pos += ret;
        }
        
        snprintf(hex_buf, sizeof(hex_buf), "%02x ", data[i]);
        // 修复kernel_write参数问题：传递pos的值而不是地址，并手动更新pos
        ret = kernel_write(filp, hex_buf, strlen(hex_buf), pos);
        if (ret < 0) {
            DEBUGP("Failed to write hex byte to file: %d\n", ret);
            filp_close(filp, NULL);
            return;
        }
        pos += ret;
    }
    
    // 结束换行
    snprintf(hex_buf, sizeof(hex_buf), "\n\n");
    // 修复kernel_write参数问题：传递pos的值而不是地址，并手动更新pos
    ret = kernel_write(filp, hex_buf, strlen(hex_buf), pos);
    if (ret < 0) {
        DEBUGP("Failed to write final newline to file: %d\n", ret);
        filp_close(filp, NULL);
        return;
    }
    pos += ret;

    filp_close(filp, NULL);
    DEBUGP("Saved packet data to %s\n", packet_save_path);
}

/* 从TLS ClientHello消息中提取SNI */
static int extract_sni_from_tls(const unsigned char *data, size_t data_len, 
                               char *sni_output, size_t sni_output_size) {
    const unsigned char *p = data;
    size_t remaining = data_len;
    uint16_t session_id_len, cipher_suites_len, compression_methods_len;
    uint16_t extensions_len, extension_type, extension_length;
    uint16_t sni_list_length, sni_type, sni_length;
    size_t parsed = 0;
    
    /* 至少需要TLS头部(5字节)+Handshake头部(6字节)=11字节 */
    if (remaining < 11) {
        DEBUGP("Insufficient data for TLS header and Handshake header\n");
        return -1;
    }
    
    /* 跳过TLS记录层头部(5字节) */
    p += 5;
    remaining -= 5;
    parsed += 5;
    
    /* 验证Handshake类型和长度 */
    if (p[0] != TLS_CLIENT_HELLO) {
        DEBUGP("Not a ClientHello message, type: %u\n", p[0]);
        return -1;
    }
    
    /* 跳过Handshake头部(4字节长度字段) */
    p += 4;
    remaining -= 4;
    parsed += 4;
    
    /* 跳过版本号(2字节)和随机数(32字节) */
    if (remaining < 34) {
        DEBUGP("Insufficient data for version and random\n");
        return -1;
    }
    p += 34;
    remaining -= 34;
    parsed += 34;
    
    /* 读取Session ID长度 */
    if (remaining < 1) {
        DEBUGP("No data left for Session ID length\n");
        return -1;
    }
    session_id_len = p[0];
    p += 1;
    remaining -= 1;
    parsed += 1;
    
    /* 跳过Session ID */
    if (remaining < session_id_len) {
        DEBUGP("Insufficient data for Session ID\n");
        return -1;
    }
    p += session_id_len;
    remaining -= session_id_len;
    parsed += session_id_len;
    
    /* 读取密码套件长度 */
    if (remaining < 2) {
        DEBUGP("No data left for cipher suites length\n");
        return -1;
    }
    cipher_suites_len = (p[0] << 8) | p[1];
    p += 2;
    remaining -= 2;
    parsed += 2;
    
    /* 跳过密码套件 */
    if (remaining < cipher_suites_len) {
        DEBUGP("Insufficient data for cipher suites\n");
        return -1;
    }
    p += cipher_suites_len;
    remaining -= cipher_suites_len;
    parsed += cipher_suites_len;
    
    /* 读取压缩方法长度 */
    if (remaining < 1) {
        DEBUGP("No data left for compression methods length\n");
        return -1;
    }
    compression_methods_len = p[0];
    p += 1;
    remaining -= 1;
    parsed += 1;
    
    /* 跳过压缩方法 */
    if (remaining < compression_methods_len) {
        DEBUGP("Insufficient data for compression methods\n");
        return -1;
    }
    p += compression_methods_len;
    remaining -= compression_methods_len;
    parsed += compression_methods_len;
    
    /* 检查是否有扩展 */
    if (remaining < 2) {
        DEBUGP("No data left for extensions length\n");
        return -1;
    }
    
    extensions_len = (p[0] << 8) | p[1];
    p += 2;
    remaining -= 2;
    parsed += 2;
    
    /* 检查扩展数据是否足够 */
    if (remaining < extensions_len) {
        DEBUGP("Insufficient data for extensions, need %u, have %zu\n", extensions_len, remaining);
        return -1;
    }
    
    /* 遍历扩展 */
    while (extensions_len > 0 && remaining >= 4) {
        if (remaining < 4) {
            DEBUGP("Insufficient data for extension header\n");
            break;
        }
        
        extension_type = (p[0] << 8) | p[1];
        extension_length = (p[2] << 8) | p[3];
        p += 4;
        remaining -= 4;
        extensions_len -= 4;
        parsed += 4;
        
        if (remaining < extension_length || extensions_len < extension_length) {
            DEBUGP("Insufficient data for extension data\n");
            break;
        }
        
        /* 查找SNI扩展 */
        if (extension_type == TLS_EXTENSION_SNI) {
            if (extension_length < 5) {
                DEBUGP("SNI extension too short\n");
                return -1;
            }
            
            /* 读取SNI列表长度 */
            sni_list_length = (p[0] << 8) | p[1];
            p += 2;
            remaining -= 2;
            extensions_len -= 2;
            parsed += 2;
            
            if (sni_list_length < 3 || sni_list_length > extension_length - 2) {
                DEBUGP("Invalid SNI list length\n");
                return -1;
            }
            
            /* 读取第一个SNI条目 */
            if (remaining < 3) {
                DEBUGP("Insufficient data for SNI entry header\n");
                return -1;
            }
            
            sni_type = p[0];
            sni_length = (p[1] << 8) | p[2];
            p += 3;
            remaining -= 3;
            extensions_len -= 3;
            parsed += 3;
            
            if (sni_type != 0) {  // 0 = hostname
                DEBUGP("Unsupported SNI type: %u\n", sni_type);
                return -1;
            }
            
            if (sni_length == 0 || sni_length > remaining || sni_length > sni_output_size - 1) {
                DEBUGP("Invalid SNI length: %u\n", sni_length);
                return -1;
            }
            
            /* 复制SNI到输出缓冲区 */
            memcpy(sni_output, p, sni_length);
            sni_output[sni_length] = '\0';
            
            DEBUGP("Successfully extracted SNI: %s\n", sni_output);
            return sni_length;
        }
        
        /* 跳到下一个扩展 */
        p += extension_length;
        remaining -= extension_length;
        extensions_len -= extension_length;
        parsed += extension_length;
    }
    
    DEBUGP("SNI extension not found\n");
    return -1;
}

/* 安全的字符串匹配函数 */
static bool match_string_safe(const char *haystack, size_t haystack_len,
                             const char *needle, size_t needle_len) {
    const char *found;
    
    if (!haystack || !needle || needle_len == 0 || haystack_len < needle_len)
        return false;
        
    found = custom_memmem(haystack, haystack_len, needle, needle_len);
    return (found != NULL);
}

/* 主匹配函数 */
static bool xt_sni_match(const struct sk_buff *skb,
                        struct xt_action_param *par) {
    const struct xt_sni_info *info = par->matchinfo;
    const struct iphdr *iph_const = NULL;
    const struct ipv6hdr *ipv6h_const = NULL;
    const struct tcphdr *_tcph_const = NULL;
    const struct tcphdr *tcph_const = NULL;
    struct iphdr *iph = NULL;
    struct tcphdr *_tcph = NULL;
    struct tcphdr *tcph = NULL;
    unsigned int payload_offset = 0;
    unsigned int data_len = 0;
    unsigned char extracted_sni[SNI_MAX_LEN] = {0};
    int sni_len = 0;
    bool matched = false;
    bool result = false;
    unsigned char *tmp_buffer = NULL;
    unsigned char stack_buffer[STACK_BUFFER_SIZE];
    size_t buffer_size = 0;
    unsigned char record_type;
    unsigned char handshake_type;
    const unsigned char *data_ptr;
    unsigned int offset = 0;
    unsigned char *first_bytes = NULL;
    size_t save_size = 0;
    char reason[128];
    unsigned int protocol;
    int max_retries = 3;
    int retry_count = 0;
    
    ENHANCED_DEBUG("Starting SNI match\n");
    
    /* 获取协议类型 */
    if (par->match->family == NFPROTO_IPV4) {
        iph_const = ip_hdr(skb);
        protocol = iph_const->protocol;
    } else {
        ipv6h_const = ipv6_hdr(skb);
        protocol = ipv6h_const->nexthdr;
    }
    
    ENHANCED_DEBUG("Protocol: %u\n", protocol);
    
    /* 只处理TCP协议 */
    if (protocol != IPPROTO_TCP) {
        DEBUGP("Not a TCP packet, skipping SNI matching\n");
        return false;
    }
    
    /* 获取TCP头部 */
    if (par->match->family == NFPROTO_IPV4) {
        tcph_const = skb_header_pointer(skb, iph_const->ihl * 4, sizeof(_tcph_const), &_tcph_const);
    } else {
        tcph_const = skb_header_pointer(skb, sizeof(struct ipv6hdr), sizeof(_tcph_const), &_tcph_const);
    }
    
    if (!tcph_const) {
        DEBUGP("Failed to get TCP header\n");
        return false;
    }
    
    ENHANCED_DEBUG("TCP source port: %u, dest port: %u\n", ntohs(tcph_const->source), ntohs(tcph_const->dest));
    
    /* 重试机制：尝试多次解析TLS数据包 */
    for (retry_count = 0; retry_count < max_retries; retry_count++) {
        /* 重新获取TCP头部指针 */
        if (par->match->family == NFPROTO_IPV4) {
            tcph_const = skb_header_pointer(skb, iph_const->ihl * 4, sizeof(_tcph_const), &_tcph_const);
        } else {
            tcph_const = skb_header_pointer(skb, sizeof(struct ipv6hdr), sizeof(_tcph_const), &_tcph_const);
        }
        
        if (!tcph_const) {
            DEBUGP("Failed to get TCP header on retry %d\n", retry_count);
            return false;
        }
        
        /* 计算有效载荷偏移量 */
        if (par->match->family == NFPROTO_IPV4) {
            payload_offset = iph_const->ihl * 4 + tcph_const->doff * 4;
        } else {
            payload_offset = sizeof(struct ipv6hdr) + tcph_const->doff * 4;
        }
        
        ENHANCED_DEBUG("Payload offset: %u\n", payload_offset);
        
        /* 检查偏移量是否合理 */
        if (payload_offset >= skb->len) {
            DEBUGP("Invalid payload offset: %u, skb len: %u\n", payload_offset, skb->len);
            if (retry_count < max_retries - 1) {
                continue;
            }
            return false;
        }
        
        /* 获取TLS记录类型 */
        data_ptr = skb_header_pointer(skb, payload_offset, 1, &record_type);
        if (!data_ptr) {
            DEBUGP("Failed to get record type on retry %d\n", retry_count);
            if (retry_count < max_retries - 1) {
                continue;
            }
            goto failed_packet;
        }
        record_type = *data_ptr;
        
        ENHANCED_DEBUG("Record type: %u (expected TLS_HANDSHAKE: %u)\n", record_type, TLS_HANDSHAKE);
        
        /* 检查是否为TLS握手记录 */
        if (record_type != TLS_HANDSHAKE) {
            ENHANCED_DEBUG("Not a TLS Handshake record, type: %u\n", record_type);
            if (retry_count < max_retries - 1) {
                continue;
            }
            goto failed_packet;
        }
        
        /* 获取握手类型 */
        if (payload_offset + 5 >= skb->len) {
            DEBUGP("Insufficient data for handshake type\n");
            if (retry_count < max_retries - 1) {
                continue;
            }
            goto failed_packet;
        }
        
        data_ptr = skb_header_pointer(skb, payload_offset + 5, 1, &handshake_type);
        if (!data_ptr) {
            DEBUGP("Failed to get handshake type on retry %d\n", retry_count);
            if (retry_count < max_retries - 1) {
                continue;
            }
            goto failed_packet;
        }
        handshake_type = *data_ptr;
        
        ENHANCED_DEBUG("Handshake type: %u (expected TLS_CLIENT_HELLO: %u)\n", handshake_type, TLS_CLIENT_HELLO);
        
        /* 检查是否为ClientHello */
        if (handshake_type != TLS_CLIENT_HELLO) {
            ENHANCED_DEBUG("Not a TLS ClientHello, type: %u\n", handshake_type);
            if (retry_count < max_retries - 1) {
                continue;
            }
            goto failed_packet;
        }
        
        /* 如果到这里，说明成功识别了TLS ClientHello */
        break;
    }
    
    /* 如果重试次数用完仍未成功，则保存数据包并返回false */
    if (retry_count >= max_retries) {
        ENHANCED_DEBUG("Not a TLS ClientHello packet after %d retries\n", max_retries);
        goto failed_packet;
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
    
    /* 检查数据长度是否有效 */
    if (data_len == 0) {
        DEBUGP("Data length is zero, trying alternative calculation\n");
        // 尝试使用skb->len - payload_offset作为备选方案
        data_len = skb->len - payload_offset;
        ENHANCED_DEBUG("Alternative data_len: %u\n", data_len);
    }
    
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

failed_packet:
    // 保存无法识别的数据包用于分析
    if (save_failed_packets) {
        // 尝试获取数据包的一些基本信息用于保存
        // 修复min宏的类型问题
        save_size = min_t(size_t, (size_t)packet_save_size_param, (size_t)256U); // 使用模块参数并限制最大值
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