#include <stdio.h>
#include <stdint.h>

#define XT_SNI_MAX_PATTERN_SIZE 128
#define XT_SNI_MAX_ALGO_NAME_SIZE 16

// 原始的 xt_sni_info 结构体
struct xt_sni_info {
	uint16_t from_offset;
	uint16_t to_offset;
	char	  algo[XT_SNI_MAX_ALGO_NAME_SIZE];
	char 	 pattern[XT_SNI_MAX_PATTERN_SIZE];
	uint8_t  patlen;
	union {
		struct {
			uint8_t  invert;
		} v0;
		struct {
			uint8_t  flags;
		} v1;
	} u;
	struct ts_config {
		void *config;
	} __attribute__((aligned(8))) *config;
} __attribute__((aligned(8)));

// 之前定义的 xt_sni_url_info 结构体（从删除的内容重建）
struct xt_sni_url_info {
	uint16_t from_offset;
	uint16_t to_offset;
	char	  algo[XT_SNI_MAX_ALGO_NAME_SIZE];
	char 	 pattern[XT_SNI_MAX_PATTERN_SIZE];
	uint8_t  patlen;
	uint8_t  invert;
	
	/* 新增：textsearch高效通配符匹配配置 */
	uint8_t wildcard_type;                           /* 通配符类型 */
	uint8_t reserved;                                /* 对齐保留 */
	char search_pattern[XT_SNI_MAX_PATTERN_SIZE]; /* 转换后的搜索模式 */
	uint32_t pattern_len;                            /* 模式长度 */
	
	/* Used internally by the kernel */
	struct ts_config {
		void *config;
	} __attribute__((aligned(8))) *ts_config;
} __attribute__((aligned(8)));

int main() {
	printf("=== 结构体大小对比 ===\n");
	printf("xt_sni_info:    %zu bytes\n", sizeof(struct xt_sni_info));
	printf("xt_sni_url_info: %zu bytes\n", sizeof(struct xt_sni_url_info));
	printf("差异: %zu bytes\n", sizeof(struct xt_sni_url_info) - sizeof(struct xt_sni_info));
	
	printf("\n=== 详细字段对比 ===\n");
	printf("共同的字段:\n");
	printf("  from_offset: %zu bytes\n", sizeof(uint16_t));
	printf("  to_offset:   %zu bytes\n", sizeof(uint16_t));
	printf("  algo:        %zu bytes\n", XT_SNI_MAX_ALGO_NAME_SIZE);
	printf("  pattern:     %zu bytes\n", XT_SNI_MAX_PATTERN_SIZE);
	printf("  patlen:      %zu bytes\n", sizeof(uint8_t));
	
	printf("\nxt_sni_info 特有字段:\n");
	printf("  union {invert/flags}: %zu bytes\n", sizeof(((struct xt_sni_info*)0)->u));
	printf("  config pointer:       %zu bytes\n", sizeof(void*));
	
	printf("\nxt_sni_url_info 特有字段:\n");
	printf("  invert:               %zu bytes\n", sizeof(uint8_t));
	printf("  wildcard_type:        %zu bytes\n", sizeof(uint8_t));
	printf("  reserved:             %zu bytes\n", sizeof(uint8_t));
	printf("  search_pattern:       %zu bytes\n", XT_SNI_MAX_PATTERN_SIZE);
	printf("  pattern_len:          %zu bytes\n", sizeof(uint32_t));
	printf("  ts_config pointer:     %zu bytes\n", sizeof(void*));
	
	// 计算额外的字段大小
	size_t extra_fields = sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t) + 
	                      XT_SNI_MAX_PATTERN_SIZE + sizeof(uint32_t) + sizeof(void*);
	printf("\n额外字段总大小: %zu bytes\n", extra_fields);
	
	printf("\n=== 内存布局分析 ===\n");
	printf("xt_sni_info 内存布局:\n");
	printf("  0-1:   from_offset (2 bytes)\n");
	printf("  2-3:   to_offset (2 bytes)\n");
	printf("  4-19:  algo (16 bytes)\n");
	printf("  20-147: pattern (128 bytes)\n");
	printf("  148:    patlen (1 byte)\n");
	printf("  149:    union (1 byte)\n");
	printf("  150-151: padding (2 bytes for alignment)\n");
	printf("  152-159: config pointer (8 bytes)\n");
	printf("  总计: 160 bytes\n");
	
	printf("\nxt_sni_url_info 内存布局:\n");
	printf("  0-1:   from_offset (2 bytes)\n");
	printf("  2-3:   to_offset (2 bytes)\n");
	printf("  4-19:  algo (16 bytes)\n");
	printf("  20-147: pattern (128 bytes)\n");
	printf("  148:    patlen (1 byte)\n");
	printf("  149:    invert (1 byte)\n");
	printf("  150:    wildcard_type (1 byte)\n");
	printf("  151:    reserved (1 byte)\n");
	printf("  152-279: search_pattern (128 bytes)\n");
	printf("  280-283: pattern_len (4 bytes)\n");
	printf("  284-287: padding (4 bytes for alignment)\n");
	printf("  288-295: ts_config pointer (8 bytes)\n");
	printf("  总计: 296 bytes\n");
	
	return 0;
}