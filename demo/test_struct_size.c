#include <stdio.h>
#include <stdint.h>

// 模拟xt_sni_info结构体定义
#define XT_SNI_MAX_PATTERN_SIZE 128
#define XT_SNI_MAX_ALGO_NAME_SIZE 16

struct {
	uint16_t from_offset;
	uint16_t to_offset;
	char	 algo[XT_SNI_MAX_ALGO_NAME_SIZE];
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

	/* Used internally by the kernel */
	struct ts_config {
		void *config;
	} __attribute__((aligned(8))) *config;
} __attribute__((aligned(8))) xt_sni_info;

int main() {
	printf("Size of struct xt_sni_info: %zu bytes\n", sizeof(xt_sni_info));
	printf("Expected size should be close to 160 bytes\n");
	
	// 计算各字段大小
	size_t total = sizeof(xt_sni_info.from_offset) + 
	               sizeof(xt_sni_info.to_offset) + 
	               sizeof(xt_sni_info.algo) + 
	               sizeof(xt_sni_info.pattern) + 
	               sizeof(xt_sni_info.patlen) + 
	               sizeof(xt_sni_info.u) + 
	               sizeof(xt_sni_info.config);
	
	printf("Manual field size calculation: %zu bytes\n", total);
	
	return 0;
}