/* Shared library add-on to iptables to add SNI matching support
 *
 * Copyright (C) 2024 Padavan Firmware
 */

#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <xtables.h>
#include <linux/netfilter/xt_sni.h>


/* Function which prints out usage message. */
static void sni_help(void)
{
    printf(
"SNI match v%s options:\n"
"--sni [!] domain              Match SNI (Server Name Indication) from TLS ClientHello\n"
"--sni-domain [!] domain        Alias for --sni\n"
"\n",
XTABLES_VERSION);
}

static struct option sni_opts[] = {
    { "sni", 1, 0, '1' },
    { "sni-domain", 1, 0, '2' },
    XT_GETOPT_TABLEEND,
};

/* Initialize the match. */
static void sni_init(struct xt_entry_match *m)
{
    struct xt_sni_info *info = (struct xt_sni_info *)m->data;
    memset(info, 0, sizeof(*info));
}

static void parse_sni(const char *s, struct xt_sni_info *info)
{
    if (strlen(s) >= SNI_MAX_LEN)
        xtables_error(PARAMETER_PROBLEM, "SNI too long `%s'", s);
    
    strcpy(info->sni, s);
    info->len = strlen(info->sni);
}

static int sni_parse(int c, char **argv, int invert, unsigned int *flags,
             const void *entry, struct xt_entry_match **match)
{
    struct xt_sni_info *info = (struct xt_sni_info *)(*match)->data;

    switch (c) {
    case '1':
    case '2':
        if (invert)
            info->invert = 1;
        parse_sni(argv[optind-1], info);
        *flags = 1;
        return 1;
    default:
        return 0;
    }
}

/* Final check; must have specified --sni. */
static void sni_check(unsigned int flags)
{
    if (!flags)
        xtables_error(PARAMETER_PROBLEM,
               "SNI match: You must specify `--sni'");
}

/* Prints out the matchinfo. */
static void sni_print(const void *ip,
      const struct xt_entry_match *match,
      int numeric)
{
    const struct xt_sni_info *info = (const struct xt_sni_info *)match->data;

    printf("sni: ");
    if (info->invert)
        fputc('!', stdout);
    printf("%s ", info->sni);
}

/* Saves the union xt_matchinfo in parsable form to stdout. */
static void sni_save(const void *ip, const struct xt_entry_match *match)
{
    const struct xt_sni_info *info = (const struct xt_sni_info *)match->data;

    printf(" --sni ");
    if (info->invert)
        fputc('!', stdout);
    printf("%s", info->sni);
}

static struct xtables_match sni = { 
    .family        = NFPROTO_UNSPEC,
    .name          = "sni",
    .version       = XTABLES_VERSION,
    .size          = XT_ALIGN(sizeof(struct xt_sni_info)),
    .userspacesize = XT_ALIGN(sizeof(struct xt_sni_info)),
    .help          = sni_help,
    .init          = sni_init,
    .print         = sni_print,
    .save          = sni_save,
    .parse         = sni_parse,
    .final_check   = sni_check,
    .extra_opts    = sni_opts
};

void _init(void)
{
    xtables_register_match(&sni);
}