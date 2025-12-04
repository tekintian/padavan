#ifndef _XT_SNI_H
#define _XT_SNI_H

#include <linux/types.h>

#define XT_SNI_MAX_PATTERN_SIZE 128
#define XT_SNI_MAX_ALGO_NAME_SIZE 16

enum {
	XT_SNI_FLAG_INVERT		= 0x01,
	XT_SNI_FLAG_IGNORECASE	= 0x02
};

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

#endif /*_XT_SNI_H*/
