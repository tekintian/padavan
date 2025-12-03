/* Shared library add-on to iptables to add SNI matching support. 
 * 
 * Based on libxt_string.c, adapted for SNI-specific matching
 *
 * Copyright (C) 2025 tekintian <tekintian@gmail.com>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <xtables.h>
#include <linux/netfilter/xt_sni.h>

enum {
    O_SNI = 0,
    O_ICASE,
    F_SNI = 1 << O_SNI,
};

static void sni_help(void)
{
    printf(
"sni match options:\n"
"[!] --sni string                Match SNI in TLS ClientHello packet\n"
"                                Only works with TLS handshake traffic\n"
"                                Example: -m sni --sni example.com\n");
}

#define s struct xt_sni_info
static const struct xt_option_entry sni_opts[] = {
    {.name = "sni", .id = O_SNI, .type = XTTYPE_STRING,
     .flags = XTOPT_MAND | XTOPT_INVERT},
    {.name = "icase", .id = O_ICASE, .type = XTTYPE_NONE},
    XTOPT_TABLEEND,
};
#undef s

static void sni_init(struct xt_entry_match *m)
{
    struct xt_sni_info *i = (struct xt_sni_info *) m->data;
    
    /* 设置默认值 */
    i->from_offset = 0;
    i->to_offset = 65535;
    strcpy(i->algo, "kmp");  /* 默认使用KMP算法 */
    i->u.v1.flags = 0;
}

static void parse_sni(const char *s, struct xt_sni_info *info)
{
    if (strlen(s) <= XT_SNI_MAX_PATTERN_SIZE) {
        strncpy(info->pattern, s, XT_SNI_MAX_PATTERN_SIZE);
        info->patlen = strlen(s);
        return;
    }
    xtables_error(PARAMETER_PROBLEM,
                  "SNI pattern too long. Max is %u characters",
                  XT_SNI_MAX_PATTERN_SIZE);
}

static void sni_parse(struct xt_option_call *cb)
{
    struct xt_sni_info *i = cb->data;

    xtables_option_parse(cb);
    switch (cb->entry->id) {
    case O_SNI:
        parse_sni(cb->arg, i);
        if (cb->invert)
            i->u.v1.flags |= XT_SNI_FLAG_INVERT;
        break;
    case O_ICASE:
        i->u.v1.flags |= XT_SNI_FLAG_IGNORECASE;
        break;
    }
}

static void sni_check(struct xt_fcheck_call *cb)
{
    struct xt_sni_info *i = cb->data;

    if (i->patlen == 0)
        xtables_error(PARAMETER_PROBLEM, "SNI pattern cannot be empty");
}

static void sni_print(const void *ip, const struct xt_entry_match *match,
                     int numeric)
{
    const struct xt_sni_info *info = (const struct xt_sni_info *) match->data;

    printf(" %s%s", info->u.v1.flags & XT_SNI_FLAG_INVERT ? "! " : "",
           "sni");
    if (info->u.v1.flags & XT_SNI_FLAG_IGNORECASE)
        printf(" -i");
    printf(" \"%s\"", info->pattern);
}

static void sni_save(const void *ip, const struct xt_entry_match *match)
{
    const struct xt_sni_info *info = (const struct xt_sni_info *) match->data;

    if (info->u.v1.flags & XT_SNI_FLAG_INVERT)
        printf(" !");
    printf(" --sni \"%s\"", info->pattern);
    if (info->u.v1.flags & XT_SNI_FLAG_IGNORECASE)
        printf(" --icase");
}

static struct xtables_match sni_match = {
    .family        = NFPROTO_UNSPEC,
    .name          = "sni",
    .version       = XTABLES_VERSION,
    .size          = XT_ALIGN(sizeof(struct xt_sni_info)),
    .userspacesize = XT_ALIGN(sizeof(struct xt_sni_info)) -
                     offsetof(struct xt_sni_info, config),
    .help          = sni_help,
    .init          = sni_init,
    .print         = sni_print,
    .save          = sni_save,
    .x6_parse      = sni_parse,
    .x6_fcheck     = sni_check,
    .x6_options    = sni_opts,
};

void _init(void)
{
    xtables_register_match(&sni_match);
}