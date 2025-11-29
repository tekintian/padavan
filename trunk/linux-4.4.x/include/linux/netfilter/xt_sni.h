#ifndef _XT_SNI_H
#define _XT_SNI_H

#include <linux/types.h>

#define SNI_MAX_LEN 256

struct xt_sni_info {
    char sni[SNI_MAX_LEN];
    __u16 invert;
    __u16 len;
};

#endif /* _XT_SNI_H */