#ifndef _XT_SNI_H
#define _XT_SNI_H

#include <linux/types.h>

#define XT_SNI_MAX_PATTERN_SIZE 128
#define XT_SNI_MAX_ALGO_NAME_SIZE 16

enum {
	XT_SNI_FLAG_INVERT		= 0x01,
	XT_SNI_FLAG_IGNORECASE	= 0x02
};

/* 🔥 通配符匹配类型 */
enum xt_sni_wildcard_type {
	XT_SNI_MATCH_EXACT = 0,      /* qq.com - 精确匹配 */
	XT_SNI_MATCH_SUFFIX = 1,     /* *.qq.com - 后缀匹配 */
	XT_SNI_MATCH_CONTAINS = 2    /* *qq.com - 包含匹配 */
};

/* 原始结构 - 保持向后兼容 */
struct xt_sni_info {
	__u16 from_offset;
	__u16 to_offset;
	char	  algo[XT_SNI_MAX_ALGO_NAME_SIZE];
	char 	  pattern[XT_SNI_MAX_PATTERN_SIZE];
	__u8  patlen;
	union {
		struct {
			__u8  invert;
		} v0;

		struct {
			__u8  flags;
		} v1;
	} u;

	/* Used internally by the kernel */
	struct ts_config __attribute__((aligned(8))) *config;
};

/* 🔥 增强版URL过滤优化结构 */
struct xt_sni_url_info {
	__u16 from_offset;
	__u16 to_offset;
	char	  algo[XT_SNI_MAX_ALGO_NAME_SIZE];
	char 	  pattern[XT_SNI_MAX_PATTERN_SIZE];
	__u8  patlen;
	__u8  invert;
	
	/* 🔥 新增：textsearch高效通配符匹配配置 */
	__u8 wildcard_type;                           /* 通配符类型 */
	__u8 reserved;                                /* 对齐保留 */
	char search_pattern[XT_SNI_MAX_PATTERN_SIZE]; /* 转换后的搜索模式 */
	__u32 pattern_len;                            /* 模式长度 */
	
	/* Used internally by the kernel */
	struct ts_config __attribute__((aligned(8))) *ts_config;
};

#endif /*_XT_SNI_H*/
