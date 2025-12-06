/*
 * URL Filter Optimized SNI match for netfilter
 * 专注于URL过滤场景的简化版本
 * 
 * 特性：
 * 1. 默认不区分大小写（URL过滤标准）
 * 2. 固定使用Boyer-Moore算法（性能最佳）
 * 3. 支持三种匹配模式：
 *    - qq.com       -> 精确匹配
 *    - *.qq.com     -> 子域名匹配  
 *    - *qq.com      -> 包含匹配
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_sni.h>
#include <linux/textsearch.h>
#include <linux/string.h>

MODULE_AUTHOR("URL Filter SNI Framework");
MODULE_DESCRIPTION("Xtables: URL Filter Optimized HTTP/HTTPS matching");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ipt_sni");
MODULE_ALIAS("ip6t_sni");

/* ========================================
 * URL匹配模式分析
 * ======================================== */

enum protocol_type {
    PROTOCOL_UNKNOWN = 0,
    PROTOCOL_HTTP = 1,
    PROTOCOL_HTTPS = 2,
    PROTOCOL_HTTP2 = 3
};

/* 使用头文件中定义的结构体，避免重复定义 */

/* ========================================
 * URL匹配模式分析
 * ======================================== */

/**
 * convert_wildcard_to_pattern - 🔥 将通配符转换为textsearch模式
 * @wildcard: 通配符字符串，如 "*.qq.com"
 * @pattern: 输出的转换后模式
 * @pattern_size: 模式缓冲区大小
 * @wildcard_type: 输出通配符类型
 * 
 * 转换示例：
 * *.qq.com -> qq.com (后缀匹配)
 * *qq.com -> qq.com (包含匹配)
 * qq.com -> qq.com (精确匹配)
 * 
 * 返回: 成功返回0，失败返回负数
 */
static int convert_wildcard_to_pattern(const char *wildcard, 
                                       char *pattern, 
                                       size_t pattern_size,
                                       enum xt_sni_wildcard_type *wildcard_type)
{
    size_t wild_len = strlen(wildcard);
    
    if (wild_len == 0 || wild_len >= pattern_size)
        return -EINVAL;
    
    if (wild_len >= 3 && wildcard[0] == '*' && wildcard[1] == '.') {
        /* *.domain.com -> 后缀匹配 */
        strcpy(pattern, wildcard + 2);
        *wildcard_type = XT_SNI_MATCH_SUFFIX;
        return 0;
    } else if (wild_len >= 2 && wildcard[0] == '*' && 
               (wild_len == 1 || wildcard[1] != '.')) {
        /* *domain -> 包含匹配 */
        strcpy(pattern, wildcard + 1);
        *wildcard_type = XT_SNI_MATCH_CONTAINS;
        return 0;
    } else {
        /* 普通域名 -> 精确匹配 */
        strcpy(pattern, wildcard);
        *wildcard_type = XT_SNI_MATCH_EXACT;
        return 0;
    }
}

/**
 * analyze_wildcard_pattern - 🔥 使用textsearch分析通配符模式
 * @info: SNI匹配信息
 */
static int analyze_wildcard_pattern(struct xt_sni_info *info)
{
    int ret;
    char search_pattern[XT_SNI_MAX_PATTERN_SIZE];
    enum xt_sni_wildcard_type wildcard_type;
    
    /* 转换通配符为textsearch模式 */
    ret = convert_wildcard_to_pattern(info->pattern, 
                                     search_pattern, 
                                     sizeof(search_pattern),
                                     &wildcard_type);
    if (ret < 0)
        return ret;
    
    /* 保存转换后的模式 */
    strcpy(info->search_pattern, search_pattern);
    info->wildcard_type = wildcard_type;
    info->pattern_len = strlen(search_pattern);
    
    /* 固定使用Boyer-Moore算法（最高效） */

    
    printk(KERN_DEBUG "xt_sni: Analyzed pattern '%s' -> '%s' (type=%d, len=%u)\n",
           info->pattern, info->search_pattern, info->wildcard_type, info->pattern_len);
    
    return 0;
}

/* ========================================
 * 智能协议检测
 * ======================================== */

/**
 * detect_protocol_type - 智能检测协议类型
 * @skb: 网络数据包
 * @offset: 检测起始位置
 * 
 * 返回: 协议类型
 */
static enum protocol_type detect_protocol_type(const struct sk_buff *skb, unsigned int offset)
{
    unsigned char *data;
    unsigned int data_len = min_t(unsigned int, 512, skb->len - offset);
    
    if (data_len < 16)
        return PROTOCOL_UNKNOWN;
    
    data = skb_header_pointer(skb, offset, data_len, NULL);
    if (!data)
        return PROTOCOL_UNKNOWN;
    
    /* 1. TLS/SSL 检测 - ClientHello */
    if (data_len >= 6 && data[0] == 0x16) {  // TLS Handshake
        if (data[1] == 0x03 && data[2] >= 0x01 && data[2] <= 0x04) {
            return PROTOCOL_HTTPS;  // TLS 1.0-1.3
        }
    }
    
    /* 2. HTTP 检测 - 请求方法 */
    if (data_len >= 4) {
        if (strncmp(data, "GET ", 4) == 0 ||
            strncmp(data, "POST ", 5) == 0 ||
            strncmp(data, "HEAD ", 5) == 0 ||
            strncmp(data, "PUT ", 4) == 0 ||
            strncmp(data, "DELETE ", 7) == 0 ||
            strncmp(data, "OPTIONS ", 8) == 0) {
            return PROTOCOL_HTTP;
        }
    }
    
    /* 3. HTTP/2 检测 - PRI * HTTP/2.0 */
    if (data_len >= 24 && strncmp(data, "PRI * HTTP/2.0\r\n\r\n", 24) == 0) {
        return PROTOCOL_HTTP2;
    }
    
    return PROTOCOL_UNKNOWN;
}

/* ========================================
 * URL匹配函数
 * ======================================== */

/**
 * extract_http_url - 从HTTP请求中提取URL路径
 * @skb: 网络数据包
 * @from: 搜索起始位置
 * @to: 搜索结束位置
 * @url_buffer: URL输出缓冲区
 * @buffer_size: 缓冲区大小
 * 
 * 返回: 提取的URL长度，0表示失败
 * 
 * 解析示例：
 * GET /rain/a/UTR2025112005617000 HTTP/1.1
 * Host: news.qq.com
 */
static unsigned int extract_http_url(const struct sk_buff *skb,
                                     unsigned int from, unsigned int to,
                                     char *url_buffer, unsigned int buffer_size)
{
    unsigned char *data;
    unsigned int data_len = to - from;
    unsigned int i, j;
    unsigned int method_len;
    
    if (data_len < 20 || buffer_size < 64)
        return 0;
    
    /* 获取数据包数据 */
    data = skb_header_pointer(skb, from, data_len, NULL);
    if (!data)
        return 0;
    
    /* 查找HTTP方法并提取URL路径 */
    if (data_len < 8)
        return 0;
    
    /* 检查HTTP方法 */
    method_len = 0;
    if (strncmp(data, "GET ", 4) == 0) {
        method_len = 4;
    } else if (strncmp(data, "POST ", 5) == 0) {
        method_len = 5;
    } else if (strncmp(data, "HEAD ", 5) == 0) {
        method_len = 5;
    } else if (strncmp(data, "PUT ", 4) == 0) {
        method_len = 4;
    } else if (strncmp(data, "DELETE ", 7) == 0) {
        method_len = 7;
    } else {
        return 0;  /* 不是HTTP请求 */
    }
    
    /* 跳过方法后的空格，开始提取URL路径 */
    i = method_len;
    
    /* 提取URL路径 (从 / 开始到 HTTP/1.x) */
    for (j = 0; i + j < data_len && j < buffer_size - 1; j++) {
        char c = data[i + j];
        
        /* 遇到空格表示URL结束 */
        if (c == ' ')
            break;
        
        url_buffer[j] = c;
    }
    url_buffer[j] = '\0';
    return j;
    
    
    return 0;  /* 未找到URL */
}

/**
 * extract_sni_from_tls - 从TLS包中提取SNI字段
 * @skb: 网络数据包
 * @from: 搜索起始位置
 * @to: 搜索结束位置
 * @sni_buffer: SNI输出缓冲区
 * @buffer_size: 缓冲区大小
 * 
 * 返回: 提取的SNI长度，0表示失败
 */
static unsigned int extract_sni_from_tls(const struct sk_buff *skb,
                                         unsigned int from, unsigned int to,
                                         char *sni_buffer, unsigned int buffer_size)
{
    unsigned char *data;
    unsigned int data_len = to - from;
    unsigned int i, j;
    
    if (data_len < 100 || buffer_size < 64)
        return 0;
    
    /* 获取数据包数据 */
    data = skb_header_pointer(skb, from, data_len, NULL);
    if (!data)
        return 0;
    
    /* TLS握手包标识: 0x16 0x03 0x0x */
    if (data_len < 5 || data[0] != 0x16 || data[1] != 0x03)
        return 0;
    
    /* 查找SNI扩展 (0x00 0x00) */
    for (i = 43; i < data_len - 10; i++) {
        if (data[i] == 0x00 && data[i+1] == 0x00) {
            /* 找到SNI扩展，解析长度 */
            unsigned int sni_name_len = (data[i+11] << 8) | data[i+12];
            
            if (sni_name_len > 0 && sni_name_len < buffer_size - 1 && 
                i + 13 + sni_name_len < data_len) {
                
                /* 提取SNI域名 */
                for (j = 0; j < sni_name_len; j++) {
                    sni_buffer[j] = data[i + 13 + j];
                }
                sni_buffer[j] = '\0';
                return sni_name_len;
            }
        }
    }
    
    return 0;
}

/**
 * sni_match_with_textsearch - 🔥 使用textsearch进行高效通配符匹配
 * @sni_name: 提取的SNI域名
 * @info: 匹配信息
 * 
 * 返回: true表示匹配
 */
static bool sni_match_with_textsearch(const char *sni_name, 
                                     const struct xt_sni_info *info)
{
    struct ts_config *ts_conf = info->ts_config;
    struct ts_state ts_state;
    unsigned int pos;
    size_t sni_len = strlen(sni_name);
    
    if (!ts_conf || sni_len == 0)
        return false;
    
    switch (info->wildcard_type) {
    case XT_SNI_MATCH_EXACT:
        /* 精确匹配：长度和内容都必须完全匹配 */
        if (sni_len != info->pattern_len)
            return false;
        
        /* 使用textsearch进行精确匹配 */
        pos = textsearch_find_continuous(ts_conf, &ts_state, 
                                       sni_name, sni_len);
        return (pos == 0);
        
    case XT_SNI_MATCH_SUFFIX:
        /* 后缀匹配：*.domain.com，检查是否以模式结尾 */
        if (sni_len < info->pattern_len)
            return false;
        
        /* 在可能的起始位置搜索模式（使用Boyer-Moore的高效搜索） */
        ts_state.offset = sni_len - info->pattern_len;
        pos = textsearch_find_continuous(ts_conf, &ts_state, 
                                       sni_name, sni_len);
        return (pos != UINT_MAX);
        
    case XT_SNI_MATCH_CONTAINS:
        /* 包含匹配：*domain，全文搜索任意位置 */
        pos = textsearch_find_continuous(ts_conf, &ts_state, 
                                       sni_name, sni_len);
        return (pos != UINT_MAX);
    }
    
    return false;
}

/**
 * sni_mt - URL过滤SNI智能匹配主函数
 */
static bool sni_mt(const struct sk_buff *skb, struct xt_action_param *par)
{
    const struct xt_sni_info *info = (const struct xt_sni_info *)par->matchinfo;
    char url_buffer[256];
    unsigned int url_len;
    bool match_result;
    
    /* 智能协议检测 - 从包开始检测 */
    enum protocol_type protocol = detect_protocol_type(skb, 0);
    
    switch (protocol) {
    case PROTOCOL_HTTPS:
        /* HTTPS/TLS协议：提取SNI */
        url_len = extract_sni_from_tls(skb, 0, 65535,
                                       url_buffer, sizeof(url_buffer));
        break;
        
    case PROTOCOL_HTTP:
    case PROTOCOL_HTTP2:
        /* HTTP协议：提取URL路径 */
        url_len = extract_http_url(skb, 0, 65535,
                                   url_buffer, sizeof(url_buffer));
        break;
        
    default:  // PROTOCOL_UNKNOWN
        /* 通用回退：textsearch全文搜索 */
        if (!info->ts_config)
            return false ^ info->invert;
        
        {
            unsigned int pos = skb_find_text((struct sk_buff *)skb, 
                                           0, 65535, 
                                           info->ts_config);
            match_result = (pos != UINT_MAX);
        }
        return match_result ^ info->invert;
    }
    
    /* 🔥 使用textsearch进行高效匹配 */
    if (url_len > 0) {
        match_result = sni_match_with_textsearch(url_buffer, info);
    } else {
        /* 提取失败，回退到全文搜索 */
        if (info->ts_config) {
            unsigned int pos = skb_find_text((struct sk_buff *)skb, 
                                           0, 65535, 
                                           info->ts_config);
            match_result = (pos != UINT_MAX);
        } else {
            match_result = false;
        }
    }
    
    return match_result ^ info->invert;
}

/* ========================================
 * 模块初始化和清理
 * ======================================== */

/**
 * sni_mt_check - 🔥 增强版参数检查和textsearch初始化
 */
static int sni_mt_check(const struct xt_mtchk_param *par)
{
    struct xt_sni_info *info = (struct xt_sni_info *)par->matchinfo;
    int ret;
    int flags = 0;
    
    /* 基础参数检查 */
    if (strlen(info->pattern) == 0 || strlen(info->pattern) >= XT_SNI_MAX_PATTERN_SIZE)
        return -EINVAL;
    
    /* 🔥 使用textsearch分析通配符模式 */
    ret = analyze_wildcard_pattern(info);
    if (ret < 0)
        return ret;
    
    /* 🔥 配置textsearch标志 */
    /* URL过滤默认不区分大小写 */
    flags = TS_IGNORECASE;
    
    /* 🔥 预配置textsearch算法（使用Boyer-Moore） */
    info->ts_config = textsearch_prepare("bm", info->search_pattern, 
                                        info->pattern_len, GFP_KERNEL, flags);
    if (IS_ERR(info->ts_config)) {
        printk(KERN_ERR "xt_sni: textsearch_prepare failed: %ld\n", 
               PTR_ERR(info->ts_config));
        return PTR_ERR(info->ts_config);
    }
    
    printk(KERN_INFO "xt_sni: Loaded pattern '%s' -> '%s' (type=%d, len=%u)\n",
           info->pattern, info->search_pattern, info->wildcard_type, info->pattern_len);
    
    return 0;
}

/**
 * sni_mt_destroy - 🔥 textsearch资源清理
 */
static void sni_mt_destroy(const struct xt_mtdtor_param *par)
{
    struct xt_sni_info *info = (struct xt_sni_info *)par->matchinfo;
    
    if (info->ts_config) {
        textsearch_destroy(info->ts_config);
        info->ts_config = NULL;
        printk(KERN_DEBUG "xt_sni: Destroyed textsearch config for pattern '%s'\n", 
               info->pattern);
    }
}

/* ========================================
 * 模块注册
 * ======================================== */

static struct xt_match sni_mt_reg __read_mostly = {
    .name       = "sni",
    .revision   = 1,  /* 保持原版本号 */
    .family     = NFPROTO_UNSPEC,
    .checkentry = sni_mt_check,
    .match      = sni_mt,
    .destroy    = sni_mt_destroy,
    .matchsize  = sizeof(struct xt_sni_info),
    .me         = THIS_MODULE,
};

static int __init sni_mt_init(void)
{
    return xt_register_match(&sni_mt_reg);
}

static void __exit sni_mt_exit(void)
{
    xt_unregister_match(&sni_mt_reg);
}

module_init(sni_mt_init);
module_exit(sni_mt_exit);