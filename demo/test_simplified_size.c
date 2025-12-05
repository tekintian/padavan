#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define XT_SNI_MAX_PATTERN_SIZE 128

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

/* 🔥 URL过滤专用SNI结构体 - 精简优化版 */
struct xt_sni_info {
	char pattern[XT_SNI_MAX_PATTERN_SIZE];      /* 原始模式 */
	uint8_t  wildcard_type;                         /* 通配符类型 */
	uint8_t  invert;                                /* 反转标志 */
	uint8_t  reserved1;                             /* 保留对齐 */
	uint8_t  reserved2;                             /* 保留对齐 */
	
	/* 内部使用字段 - textsearch优化 */
	char search_pattern[XT_SNI_MAX_PATTERN_SIZE]; /* 转换后的搜索模式 */
	uint32_t pattern_len;                            /* 模式长度 */
	
	/* Used internally by the kernel */
	struct ts_config __attribute__((aligned(8))) *ts_config;
};

int main() {
	printf("Size of struct xt_sni_info: %zu bytes\n", sizeof(struct xt_sni_info));
	printf("XT_ALIGN size: %zu bytes\n", XT_ALIGN(sizeof(struct xt_sni_info)));
	printf("Expected size: Should be consistent between kernel and user space\n");
	
	// 测试字段偏移
	struct xt_sni_info info;
	memset(&info, 0, sizeof(info));
	strcpy(info.pattern, "test.com");
	info.wildcard_type = XT_SNI_MATCH_EXACT;
	info.invert = 0;
	
	printf("Pattern: %s\n", info.pattern);
	printf("Wildcard type: %d\n", info.wildcard_type);
	printf("Invert: %d\n", info.invert);
	
	return 0;
}