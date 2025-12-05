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
 * 简化的URL过滤数据结构
 * ======================================== */

enum url_match_type {
    URL_MATCH_EXACT,      /* qq.com - 精确匹配 */
    URL_MATCH_SUBDOMAIN,  /* *.qq.com - 子域名匹配 */
    URL_MATCH_CONTAINS    /* *qq.com - 包含匹配 */
};

enum protocol_type {
    PROTOCOL_UNKNOWN = 0,
    PROTOCOL_HTTP = 1,
    PROTOCOL_HTTPS = 2,
    PROTOCOL_HTTP2 = 3
};

/* 扩展原有结构，保持兼容性 */
struct xt_sni_url_info {
    /* 基础配置（保持与原版兼容） */
    __u16 from_offset;
    __u16 to_offset;
    char algo[XT_SNI_MAX_ALGO_NAME_SIZE];
    char pattern[XT_SNI_MAX_PATTERN_SIZE];
    __u8 patlen;
    __u8 invert;
    
    /* URL过滤优化配置 */
    enum url_match_type match_type;  /* 匹配类型 */
    char search_key[64];             /* 搜索关键字 */
    unsigned int key_len;             /* 关键字长度 */
    
    /* Boyer-Moore配置缓存 */
    struct ts_config *bm_config;     /* BM算法配置 */
};

/* ========================================
 * URL匹配模式分析
 * ======================================== */

/**
 * analyze_url_pattern - 分析URL模式并提取搜索关键字
 * @info: SNI匹配信息
 */
static void analyze_url_pattern(struct xt_sni_url_info *info)
{
    const char *pattern = info->pattern;
    unsigned int patlen = info->patlen;
    const char *domain;
    unsigned int domain_len;
    const char *search;
    unsigned int search_len;
    unsigned int key_len;
    
    /* 检查匹配模式 */
    if (patlen >= 3 && pattern[0] == '*' && pattern[1] == '.') {
        /* *.domain.com -> 子域名匹配 */
        info->match_type = URL_MATCH_SUBDOMAIN;
        
        /* 提取域名作为搜索关键字 */
        domain = pattern + 2;
        domain_len = patlen - 2;
        
        if (domain_len < sizeof(info->search_key)) {
            memcpy(info->search_key, domain, domain_len);
            info->search_key[domain_len] = '\0';
            info->key_len = domain_len;
        }
    } else if (patlen >= 2 && pattern[0] == '*' && pattern[1] != '.') {
        /* *domain -> 包含匹配 */
        info->match_type = URL_MATCH_CONTAINS;
        
        /* 提取*后的字符串作为搜索关键字 */
        search = pattern + 1;
        search_len = patlen - 1;
        
        if (search_len < sizeof(info->search_key)) {
            memcpy(info->search_key, search, search_len);
            info->search_key[search_len] = '\0';
            info->key_len = search_len;
        }
    } else {
        /* 精确匹配 */
        info->match_type = URL_MATCH_EXACT;
        
        /* 使用整个模式作为搜索关键字 */
        key_len = patlen < sizeof(info->search_key) ? 
                 patlen : sizeof(info->search_key) - 1;
        memcpy(info->search_key, pattern, key_len);
        info->search_key[key_len] = '\0';
        info->key_len = key_len;
    }
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
        
        /* 转换为小写 */
        if (c >= 'A' && c <= 'Z')
            c += 32;
        
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
            unsigned int sni_ext_len = (data[i+2] << 8) | data[i+3];     /* 避免未使用警告 */
            unsigned int sni_list_len = (data[i+9] << 8) | data[i+10];  /* 避免未使用警告 */
            
            if (sni_name_len > 0 && sni_name_len < buffer_size - 1 && 
                i + 13 + sni_name_len < data_len) {
                
                /* 提取SNI域名 */
                for (j = 0; j < sni_name_len; j++) {
                    char c = data[i + 13 + j];
                    /* 转换为小写（不区分大小写） */
                    if (c >= 'A' && c <= 'Z')
                        c += 32;
                    sni_buffer[j] = c;
                }
                sni_buffer[j] = '\0';
                return sni_name_len;
            }
        }
    }
    
    return 0;
}

/**
 * match_url_pattern - 匹配URL模式
 * @sni_name: 提取的SNI域名
 * @info: 匹配信息
 * 
 * 返回: true表示匹配
 */
static bool match_url_pattern(const char *sni_name, const struct xt_sni_url_info *info)
{
    const char *pattern = info->pattern;
    unsigned int patlen = info->patlen;
    unsigned int sni_len = strlen(sni_name);
    const char *sni_end;
    unsigned int i;
    
    switch (info->match_type) {
    case URL_MATCH_EXACT:
        /* 精确匹配 */
        if (sni_len != patlen)
            return false;
        return (strncmp(sni_name, pattern, patlen) == 0);
        
    case URL_MATCH_SUBDOMAIN:
        /* 子域名匹配 *.domain.com */
        if (sni_len < info->key_len)
            return false;
        
        /* 检查是否以domain.com结尾 */
        sni_end = sni_name + sni_len - info->key_len;
        if (strncmp(sni_end, info->search_key, info->key_len) != 0)
            return false;
        
        /* 确保是子域名（前面有.或者是完整域名） */
        if (sni_len == info->key_len)
            return true;  /* 完整匹配 */
        
        return (*(sni_end - 1) == '.');
        
    case URL_MATCH_CONTAINS:
        /* 包含匹配 *domain */
        if (sni_len < info->key_len)
            return false;
        
        /* 搜索包含关键字 */
        for (i = 0; i <= sni_len - info->key_len; i++) {
            if (strncmp(sni_name + i, info->search_key, info->key_len) == 0)
                return true;
        }
        return false;
    }
    
    return false;
}

/**
 * sni_mt - URL过滤SNI智能匹配主函数
 */
static bool sni_mt(const struct sk_buff *skb, struct xt_action_param *par)
{
    const struct xt_sni_url_info *info = (const struct xt_sni_url_info *)par->matchinfo;
    char url_buffer[256];
    unsigned int url_len;
    bool match_result;
    
    /* 智能协议检测 */
    enum protocol_type protocol = detect_protocol_type(skb, info->from_offset);
    
    switch (protocol) {
    case PROTOCOL_HTTPS:
        /* HTTPS/TLS协议：提取SNI */
        url_len = extract_sni_from_tls(skb, info->from_offset, info->to_offset,
                                       url_buffer, sizeof(url_buffer));
        break;
        
    case PROTOCOL_HTTP:
    case PROTOCOL_HTTP2:
        /* HTTP协议：提取URL路径 */
        url_len = extract_http_url(skb, info->from_offset, info->to_offset,
                                   url_buffer, sizeof(url_buffer));
        break;
        
    default:  // PROTOCOL_UNKNOWN
        /* 通用回退：Boyer-Moore全文搜索 */
        if (!info->bm_config)
            return false ^ info->invert;
        
        {
            unsigned int pos = skb_find_text((struct sk_buff *)skb, 
                                           info->from_offset, info->to_offset, 
                                           info->bm_config);
            match_result = (pos != UINT_MAX);
        }
        return match_result ^ info->invert;
    }
    
    /* 精确匹配提取的域名 */
    if (url_len > 0) {
        match_result = match_url_pattern(url_buffer, info);
    } else {
        /* 提取失败，回退到Boyer-Moore */
        {
            unsigned int pos = skb_find_text((struct sk_buff *)skb, 
                                           info->from_offset, info->to_offset, 
                                           info->bm_config);
            match_result = (pos != UINT_MAX);
        }
    }
    
    return match_result ^ info->invert;
}

/* ========================================
 * 模块初始化和清理
 * ======================================== */

/**
 * sni_mt_check - 参数检查和初始化
 */
static int sni_mt_check(const struct xt_mtchk_param *par)
{
    struct xt_sni_url_info *info = (struct xt_sni_url_info *)par->matchinfo;
    
    /* 基础参数检查 */
    if (info->from_offset > info->to_offset)
        return -EINVAL;
    if (info->patlen > XT_SNI_MAX_PATTERN_SIZE)
        return -EINVAL;
    if (info->patlen == 0)
        return -EINVAL;
    
    /* 分析URL模式 */
    analyze_url_pattern(info);
    
    /* 固定使用Boyer-Moore算法 */
    strcpy(info->algo, "bm");
    
    /* 预配置Boyer-Moore算法（回退用） */
    info->bm_config = textsearch_prepare("bm", info->search_key, 
                                        info->key_len, GFP_KERNEL, TS_IGNORECASE);
    if (IS_ERR(info->bm_config))
        info->bm_config = NULL;
    
    return 0;
}

/**
 * sni_mt_destroy - 资源清理
 */
static void sni_mt_destroy(const struct xt_mtdtor_param *par)
{
    struct xt_sni_url_info *info = (struct xt_sni_url_info *)par->matchinfo;
    
    if (info->bm_config) {
        textsearch_destroy(info->bm_config);
        info->bm_config = NULL;
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
    .matchsize  = sizeof(struct xt_sni_url_info),
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