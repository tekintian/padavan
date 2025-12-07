/* Shared library add-on to iptables to add SNI matching support. 
 * 
 * Copyright (C) 2000 Emmanuel Roger  <winfield@freegates.be>
 *
 * 2005-08-05 Pablo Neira Ayuso <pablo@eurodev.net>
 * 	- reimplemented to use new string matching iptables match
 * 	- add functionality to match packets by using window offsets
 * 	- add functionality to select the string matching algorithm
 *
 * ChangeLog
 *     29.12.2003: Michael Rash <mbr@cipherdyne.org>
 *             Fixed iptables save/restore for ascii strings
 *             that contain space chars, and hex strings that
 *             contain embedded NULL chars.  Updated to print
 *             strings in hex mode if any non-printable char
 *             is contained within the string.
 *
 *     27.01.2001: Gianni Tedesco <gianni@ecsc.co.uk>
 *             Changed --tos to --sni in save(). Also
 *             updated to work with slightly modified
 *             ipt_sni_info.
 */
#define _GNU_SOURCE 1 /* strnlen for older glibcs */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <xtables.h>
#include <linux/netfilter/xt_sni.h>

enum {
	O_FROM = 0,
	O_TO,
	O_ALGO,
	O_ICASE,
	O_STRING,
	O_HEX_STRING,
	F_STRING     = 1 << O_STRING,
	F_HEX_STRING = 1 << O_HEX_STRING,
	F_OP_ANY     = F_STRING | F_HEX_STRING,
};

static void sni_help(void)
{
	printf(
"sni match options:\n"
"--from                       Offset to start searching from\n"
"--to                         Offset to stop searching\n"
"--algo                       Algorithm (bm, kmp, router)\n"
"--icase                      Ignore case (default: 0)\n"
"[!] --sni string             Match a SNI string in a packet\n"
"[!] --hex-sni string         Match a hex SNI string in a packet\n"
"\n"
"Router algorithm supports:\n"
"  exact domain:     google.com\n"
"  subdomain:        *.google.com\n"
"  wildcard:         *.gov.cn\n"
"  contains:         video\n"
"  case-insensitive: use --icase flag\n");
}

#define s struct xt_sni_info
static const struct xt_option_entry sni_opts[] = {
	{.name = "from", .id = O_FROM, .type = XTTYPE_UINT16,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, from_offset)},
	{.name = "to", .id = O_TO, .type = XTTYPE_UINT16,
	 .flags = XTOPT_PUT, XTOPT_POINTER(s, to_offset)},
	{.name = "algo", .id = O_ALGO, .type = XTTYPE_STRING,
	 .flags = XTOPT_MAND | XTOPT_PUT, XTOPT_POINTER(s, algo)},
	{.name = "sni", .id = O_STRING, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT, .excl = F_HEX_STRING},
	{.name = "hex-sni", .id = O_HEX_STRING, .type = XTTYPE_STRING,
	 .flags = XTOPT_INVERT, .excl = F_STRING},
	{.name = "icase", .id = O_ICASE, .type = XTTYPE_NONE},
	XTOPT_TABLEEND,
};
#undef s

static void sni_init(struct xt_entry_match *m)
{
	struct xt_sni_info *i = (struct xt_sni_info *) m->data;

	i->to_offset = UINT16_MAX;
}

static void
parse_string(const char *s, struct xt_sni_info *info)
{	
	/* xt_sni does not need \0 at the end of the pattern */
	if (strlen(s) <= XT_SNI_MAX_PATTERN_SIZE) {
		strncpy(info->pattern, s, XT_SNI_MAX_PATTERN_SIZE);
		info->patlen = strnlen(s, XT_SNI_MAX_PATTERN_SIZE);
		return;
	}
	xtables_error(PARAMETER_PROBLEM, "SNI too long \"%s\"", s);
}

static void
parse_hex_string(const char *s, struct xt_sni_info *info)
{
	int i=0, slen, sindex=0, schar;
	short hex_f = 0, literal_f = 0;
	char hextmp[3];

	slen = strlen(s);

	if (slen == 0) {
		xtables_error(PARAMETER_PROBLEM,
			"SNI must contain at least one char");
	}

	while (i < slen) {
		if (sindex >= XT_SNI_MAX_PATTERN_SIZE)
			xtables_error(PARAMETER_PROBLEM,
				      "SNI too long \"%s\"", s);
		if (s[i] == '\\' && !hex_f) {
			literal_f = 1;
		} else if (s[i] == '\\') {
			xtables_error(PARAMETER_PROBLEM,
				"Cannot include literals in hex data");
		} else if (s[i] == '|') {
			if (hex_f)
				hex_f = 0;
			else {
				hex_f = 1;
				/* get past any initial whitespace just after the '|' */
				while (s[i+1] == ' ')
					i++;
			}
			if (i+1 >= slen)
				break;
			else
				i++;  /* advance to the next character */
		}

		if (literal_f) {
			if (i+1 >= slen) {
				xtables_error(PARAMETER_PROBLEM,
					"Bad literal placement at end of string");
			}
			info->pattern[sindex] = s[i+1];
			i += 2;  /* skip over literal char */
			literal_f = 0;
		} else if (hex_f) {
			if (i+1 >= slen) {
				xtables_error(PARAMETER_PROBLEM,
					"Odd number of hex digits");
			}
			if (i+2 >= slen) {
				/* must end with a "|" */
				xtables_error(PARAMETER_PROBLEM, "Invalid hex block");
			}
			if (! isxdigit(s[i])) /* check for valid hex char */
				xtables_error(PARAMETER_PROBLEM, "Invalid hex char '%c'", s[i]);
			if (! isxdigit(s[i+1])) /* check for valid hex char */
				xtables_error(PARAMETER_PROBLEM, "Invalid hex char '%c'", s[i+1]);
			hextmp[0] = s[i];
			hextmp[1] = s[i+1];
			hextmp[2] = '\0';
			if (! sscanf(hextmp, "%x", &schar))
				xtables_error(PARAMETER_PROBLEM,
					"Invalid hex char `%c'", s[i]);
			info->pattern[sindex] = (char) schar;
			if (s[i+2] == ' ')
				i += 3;  /* spaces included in the hex block */
			else
				i += 2;
		} else {  /* char is not part of hex data, so just copy */
			info->pattern[sindex] = s[i];
			i++;
		}
		sindex++;
	}
	info->patlen = sindex;
}

static void sni_parse(struct xt_option_call *cb)
{
	struct xt_sni_info *sniinfo = cb->data;
	const unsigned int revision = (*cb->match)->u.user.revision;

	xtables_option_parse(cb);
	switch (cb->entry->id) {
	case O_STRING:
		parse_string(cb->arg, sniinfo);
		if (cb->invert) {
			if (revision == 0)
				sniinfo->u.v0.invert = 1;
			else
				sniinfo->u.v1.flags |= XT_SNI_FLAG_INVERT;
		}
		break;
	case O_HEX_STRING:
		parse_hex_string(cb->arg, sniinfo);  /* sets length */
		if (cb->invert) {
			if (revision == 0)
				sniinfo->u.v0.invert = 1;
			else
				sniinfo->u.v1.flags |= XT_SNI_FLAG_INVERT;
		}
		break;
	case O_ICASE:
		if (revision == 0)
			xtables_error(VERSION_PROBLEM,
				   "Kernel doesn't support --icase");

		sniinfo->u.v1.flags |= XT_SNI_FLAG_IGNORECASE;
		break;
	}
}

static void sni_check(struct xt_fcheck_call *cb)
{
	if (!(cb->xflags & F_OP_ANY))
		xtables_error(PARAMETER_PROBLEM,
			   "SNI match: You must specify `--sni' or "
			   "`--hex-sni'");
}

/* Test to see if the string contains non-printable chars or quotes */
static unsigned short int
is_hex_string(const char *str, const unsigned short int len)
{
	unsigned int i;
	for (i=0; i < len; i++)
		if (! isprint(str[i]))
			return 1;  /* string contains at least one non-printable char */
	/* use hex output if last char is a "\" */
	if (str[len-1] == '\\')
		return 1;
	return 0;
}

/* Print string with "|" chars included as one would pass to --hex-sni */
static void
print_hex_string(const char *str, const unsigned short int len)
{
	unsigned int i;
	/* start hex block */
	printf(" \"|");
	for (i=0; i < len; i++)
		printf("%02x", (unsigned char)str[i]);
	/* close hex block */
	printf("|\"");
}

static void
print_string(const char *str, const unsigned short int len)
{
	unsigned int i;
	printf(" \"");
	for (i=0; i < len; i++) {
		if (str[i] == '\"' || str[i] == '\\')
			putchar('\\');
		printf("%c", (unsigned char) str[i]);
	}
	printf("\"");  /* closing quote */
}

static void
sni_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_sni_info *info =
	    (const struct xt_sni_info*) match->data;
	const int revision = match->u.user.revision;
	int invert = (revision == 0 ? info->u.v0.invert :
			    info->u.v1.flags & XT_SNI_FLAG_INVERT);

	if (is_hex_string(info->pattern, info->patlen)) {
		printf(" SNI match %s", invert ? "!" : "");
		print_hex_string(info->pattern, info->patlen);
	} else {
		printf(" SNI match %s", invert ? "!" : "");
		print_string(info->pattern, info->patlen);
	}
	printf(" ALGO name %s", info->algo);
	if (info->from_offset != 0)
		printf(" FROM %u", info->from_offset);
	if (info->to_offset != 0)
		printf(" TO %u", info->to_offset);
	if (revision > 0 && info->u.v1.flags & XT_SNI_FLAG_IGNORECASE)
		printf(" ICASE");
}

static void sni_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_sni_info *info =
	    (const struct xt_sni_info*) match->data;
	const int revision = match->u.user.revision;
	int invert = (revision == 0 ? info->u.v0.invert :
			    info->u.v1.flags & XT_SNI_FLAG_INVERT);

	if (is_hex_string(info->pattern, info->patlen)) {
		printf("%s --hex-sni", (invert) ? " !" : "");
		print_hex_string(info->pattern, info->patlen);
	} else {
		printf("%s --sni", (invert) ? " !": "");
		print_string(info->pattern, info->patlen);
	}
	printf(" --algo %s", info->algo);
	if (info->from_offset != 0)
		printf(" --from %u", info->from_offset);
	if (info->to_offset != 0)
		printf(" --to %u", info->to_offset);
	if (revision > 0 && info->u.v1.flags & XT_SNI_FLAG_IGNORECASE)
		printf(" --icase");
}


static struct xtables_match sni_mt_reg[] = {
	{
		.name          = "sni",
		.revision      = 0,
		.family        = NFPROTO_UNSPEC,
		.version       = XTABLES_VERSION,
		.size          = XT_ALIGN(sizeof(struct xt_sni_info)),
		.userspacesize = offsetof(struct xt_sni_info, config),
		.help          = sni_help,
		.init          = sni_init,
		.print         = sni_print,
		.save          = sni_save,
		.x6_parse      = sni_parse,
		.x6_fcheck     = sni_check,
		.x6_options    = sni_opts,
	},
	{
		.name          = "sni",
		.revision      = 1,
		.family        = NFPROTO_UNSPEC,
		.version       = XTABLES_VERSION,
		.size          = XT_ALIGN(sizeof(struct xt_sni_info)),
		.userspacesize = offsetof(struct xt_sni_info, config),
		.help          = sni_help,
		.init          = sni_init,
		.print         = sni_print,
		.save          = sni_save,
		.x6_parse      = sni_parse,
		.x6_fcheck     = sni_check,
		.x6_options    = sni_opts,
	},
};

void _init(void)
{
	xtables_register_matches(sni_mt_reg, sizeof(sni_mt_reg)/sizeof(struct xtables_match));
}