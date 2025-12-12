#include <stdio.h>
#include <stdint.h>

#define XT_SNI_MAX_PATTERN_SIZE 128
#define XT_SNI_MAX_ALGO_NAME_SIZE 16

enum xt_sni_wildcard_type {
	XT_SNI_MATCH_EXACT = 0,
	XT_SNI_MATCH_SUFFIX = 1,
	XT_SNI_MATCH_CONTAINS = 2
};

struct _xt_align {
	uint8_t u8;
	uint16_t u16;
	uint32_t u32;
	uint64_t u64;
};

#define XT_ALIGN(s) (((s) + (__alignof__(struct _xt_align)-1)) & ~(__alignof__(struct _xt_align)-1))

struct ts_config {
	void *config;
};

/* 🔥 增强版URL过滤优化结构 */
struct xt_sni_url_info {
	uint16_t from_offset;
	uint16_t to_offset;
	char	  algo[XT_SNI_MAX_ALGO_NAME_SIZE];
	char 	 pattern[XT_SNI_MAX_PATTERN_SIZE];
	uint8_t  patlen;
	uint8_t  invert;
	
	/* 🔥 新增：textsearch高效通配符匹配配置 */
	uint8_t wildcard_type;                           /* 通配符类型 */
	uint8_t reserved;                                /* 对齐保留 */
	char search_pattern[XT_SNI_MAX_PATTERN_SIZE]; /* 转换后的搜索模式 */
	uint32_t pattern_len;                            /* 模式长度 */
	
	/* Used internally by the kernel */
	struct ts_config __attribute__((aligned(8))) *ts_config;
};

int main() {
	printf("Size of struct xt_sni_url_info: %zu bytes\n", sizeof(struct xt_sni_url_info));
	printf("XT_ALIGN size: %zu bytes\n", XT_ALIGN(sizeof(struct xt_sni_url_info)));
	printf("Expected kernel size: 296 bytes\n");
	printf("Expected user space size: 160 bytes (old) or 296 bytes (new)\n");
	
	return 0;
}