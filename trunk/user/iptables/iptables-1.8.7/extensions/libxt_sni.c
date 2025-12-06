/*
 * URL Filter Optimized SNI userspace extension
 * 专注于URL过滤场景的简化版本
 */

#include <stdio.h>
#include <stdlib.h>
#include <xtables.h>
#include <linux/netfilter/xt_sni.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>

/* 帮助信息 */
static void sni_help(void)
{
    printf(
"URL Filter SNI match options:\n"
"  --str pattern             Match URL pattern (case-insensitive)\n"
"                            Pattern formats:\n"
"                              qq.com         -> exact match\n"
"                              *.qq.com       -> subdomain match\n"
"                              *qq.com        -> contains match\n"
"  --invert                  Invert the match\n"
"\n"
"Features:\n"
"  * Automatic protocol detection (HTTP/HTTPS/HTTP/2)\n"
"  * Supports non-standard ports (8080, 8443, etc.)\n"
"  * Zero configuration - no protocol selection needed\n"
"\n"
"Examples:\n"
"  iptables -A OUTPUT -m sni --str qq.com -j DROP\n"
"  iptables -A FORWARD -m sni --str *.youtube.com -j REJECT\n"
"  iptables -A INPUT -m sni --str *facebook -j LOG\n"
"\n"
"Protocol Detection:\n"
"  * HTTP  : GET, POST, HEAD, PUT, DELETE, OPTIONS methods\n"
"  * HTTPS : TLS/SSL ClientHello packets (port 443, 8443, etc.)\n"
"  * HTTP/2: PRI * HTTP/2.0 connection preface\n"
"  * Fallback: Boyer-Moore text search for unknown protocols\n");
}

/* 命令行选项定义 */
static const struct option sni_opts[] = {
    { .name = "str",      .has_arg = true,  .val = '1' },
    { .name = "invert",   .has_arg = false, .val = '2' },
    XT_GETOPT_TABLEEND,
};

/* 检查URL模式格式 */
static bool validate_url_pattern(const char *pattern) {
    unsigned int len = strlen(pattern);
    
    if (len == 0 || len >= XT_SNI_MAX_PATTERN_SIZE)
        return false;
    
    /* 检查模式格式 */
    if (len >= 2 && pattern[0] == '*') {
        if (len == 1)  /* 只有*，无效 */
            return false;
        
        if (pattern[1] == '.') {
            /* *.domain.com 格式 */
            if (len < 4)  /* 至少 *.a.b */
                return false;
        } else {
            /* *domain 格式 */
            /* 不允许连续通配符 */
            if (len >= 2 && pattern[1] == '*')
                return false;
        }
    }
    
    /* 检查字符有效性 */
    for (unsigned int i = 0; i < len; i++) {
        char c = pattern[i];
        
        if (c == '*')
            continue;
        
        /* 域名字符检查 */
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9') || c == '.' || c == '-')) {
            return false;
        }
    }
    
    return true;
}
static void sni_init(struct xt_entry_match *m)
{
	struct xt_sni_info *i = (struct xt_sni_info *) m->data;

   /* 初始化所有字段为默认值 */
	memset(i, 0, sizeof(struct xt_sni_info));
	
	/* 设置默认值 */
	i->invert = 0;
	i->wildcard_type = XT_SNI_MATCH_EXACT;
	i->pattern_len = 0;
	
	/* 确保字符串字段以NULL结尾 */
	i->pattern[0] = '\0';
	i->search_pattern[0] = '\0';
	i->ts_config = NULL;
}

/* 解析命令行参数 */
static int sni_parse(int c, char **argv, int invert, unsigned int *flags,
                    const void *entry, struct xt_entry_match **match)
{
    struct xt_sni_info *info = (struct xt_sni_info *)(*match)->data;
    
    switch (c) {
    case '1':  /* --str */
        if (*flags & 0x01)
            xtables_error(PARAMETER_PROBLEM, "Cannot specify --str twice");
        
        if (!argv[optind])
            xtables_error(PARAMETER_PROBLEM, "--str requires an argument");
        
        if (!validate_url_pattern(argv[optind]))
            xtables_error(PARAMETER_PROBLEM, "Invalid URL pattern format");
        
        /* 复制模式串 */
        strncpy(info->pattern, argv[optind], XT_SNI_MAX_PATTERN_SIZE - 1);
        info->pattern[XT_SNI_MAX_PATTERN_SIZE - 1] = '\0';
        info->pattern_len = strlen(info->pattern);
        
        /* 设置反转标志 - 优先级：--invert选项 > xtables的!符号 */
        if (*flags & 0x02) {
            /* 如果已经设置了--invert选项，则忽略!符号 */
            info->invert = 1;
        } else {
            /* 否则使用!符号的值 */
            info->invert = invert ? 1 : 0;
        }
        
        *flags |= 0x01;
        break;
    case '2':  /* --invert */
        /* 显式设置反转标志并记录标志位 */
        info->invert = 1;
        *flags |= 0x02;
        break;
        
    default:
        return 0;
    }
    
    return 1;
}

/* 参数检查 */
static void sni_check(unsigned int flags)
{
    if (!(flags & 0x01))
        xtables_error(PARAMETER_PROBLEM, "URL filter SNI match requires --str");
    
    /* URL过滤模式不需要额外的参数检查 */
}

/* 打印匹配规则 */
static void sni_print(const void *entry, const struct xt_entry_match *match,
                     int numeric)
{
    const struct xt_sni_info *info = (const struct xt_sni_info *)match->data;
    
    printf(" URL-SNI ");
    
    if (info->invert)
        printf("!");
    
    /* 使用已设置的wildcard_type字段显示匹配模式类型 */
    switch (info->wildcard_type) {
    case XT_SNI_MATCH_SUFFIX:
        printf("subdomain:%s", info->pattern + 2);
        break;
    case XT_SNI_MATCH_CONTAINS:
        printf("contains:%s", info->pattern + 1);
        break;
    case XT_SNI_MATCH_EXACT:
    default:
        printf("exact:%s", info->pattern);
        break;
    }
}

/* 保存规则 */
static void sni_save(const void *entry, const struct xt_entry_match *match)
{
    const struct xt_sni_info *info = (const struct xt_sni_info *)match->data;
    
    printf(" --str \"%s\"", info->pattern);
    
    if (info->invert) {
        printf(" --invert");
    }
}



static struct xtables_match sni_mt_reg[] = {
	{
		.name          = "sni",
		.revision      = 1,
		.family        = NFPROTO_UNSPEC,
		.version       = XTABLES_VERSION,
		.size          = XT_ALIGN(sizeof(struct xt_sni_info)),
		.userspacesize = offsetof(struct xt_sni_info, ts_config),
		.help          = sni_help,
		.init          = sni_init,
		.print         = sni_print,
		.save          = sni_save,
		.x6_parse      = sni_parse,
		.x6_fcheck     = sni_check,
		.x6_options    = sni_opts
	}
};

void _init(void)
{
	xtables_register_matches(sni_mt_reg, sizeof(sni_mt_reg)/sizeof(struct xtables_match));
}