#ifndef _XT_SNI_H
#define _XT_SNI_H

#include <linux/types.h>

#define XT_SNI_MAX_PATTERN_SIZE 128

/* 🔥 通配符匹配类型 */
enum xt_sni_wildcard_type {
	XT_SNI_MATCH_EXACT = 0,      /* qq.com - 精确匹配 */
	XT_SNI_MATCH_SUFFIX = 1,     /* *.qq.com - 后缀匹配 */
	XT_SNI_MATCH_CONTAINS = 2    /* *qq.com - 包含匹配 */
};

/* 🔥 URL过滤专用SNI结构体 - 精简优化版 */
struct xt_sni_info {
	char pattern[XT_SNI_MAX_PATTERN_SIZE];      /* 原始模式 */
	__u8  wildcard_type;                         /* 通配符类型 */
	__u8  invert;                                /* 反转标志 */
	__u8  reserved[2];                           /* 保留对齐 */
	
	/* 内部使用字段 - textsearch优化 */
	char search_pattern[XT_SNI_MAX_PATTERN_SIZE]; /* 转换后的搜索模式 */
	__u32 pattern_len;                            /* 模式长度 */
	
	/* Used internally by the kernel */
	struct ts_config __attribute__((aligned(8))) *ts_config;
};

#endif /*_XT_SNI_H*/