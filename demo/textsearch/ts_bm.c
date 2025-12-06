/*
 * Boyer-Moore text search implementation - userspace version
 * Based on Linux kernel implementation
 */

#include "textsearch.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* Alphabet size, use ASCII */
#define ASIZE 256

#if 0
#define DEBUGP printf
#else
#define DEBUGP(args...)
#endif

struct ts_bm
{
	uint8_t		*pattern;
	unsigned int	patlen;
	unsigned int 	bad_shift[ASIZE];
	unsigned int	good_shift[0];
};

static unsigned int bm_find(struct ts_config *conf, struct ts_state *state)
{
	struct ts_bm *bm = ts_config_priv(conf);
	unsigned int i, text_len, consumed = state->offset;
	const uint8_t *text;
	int shift = bm->patlen - 1, bs;
	const int icase = conf->flags & TS_IGNORECASE;

	for (;;) {
		text_len = conf->get_next_block(consumed, &text, conf, state);

		if (text_len == 0)
			break;

		while (shift < text_len) {
			DEBUGP("Searching in position %d (%c)\n", 
				shift, text[shift]);
			for (i = 0; i < bm->patlen; i++) 
				if ((icase ? toupper(text[shift-i])
				    : text[shift-i])
					!= bm->pattern[bm->patlen-1-i])
				     goto next;

			/* London calling... */
			DEBUGP("found!\n");
			return consumed += (shift-(bm->patlen-1));

next:			bs = bm->bad_shift[text[shift-i]];

			/* Now jumping to... */
			shift = (shift-i+bs > shift+bm->good_shift[i]) ? 
				shift-i+bs : shift+bm->good_shift[i];
		}
		consumed += text_len;
	}

	return UINT_MAX;
}

static int subpattern(uint8_t *pattern, int i, int j, int g)
{
	int x = i+g-1, y = j+g-1, ret = 0;

	while(pattern[x--] == pattern[y--]) {
		if (y < 0) {
			ret = 1;
			break;
		}
		if (--g == 0) {
			ret = pattern[i-1] != pattern[j-1];
			break;
		}
	}

	return ret;
}

static void compute_prefix_tbl(struct ts_bm *bm, int flags)
{
	int i, j, g;

	for (i = 0; i < ASIZE; i++)
		bm->bad_shift[i] = bm->patlen;
	for (i = 0; i < bm->patlen - 1; i++) {
		bm->bad_shift[bm->pattern[i]] = bm->patlen - 1 - i;
		if (flags & TS_IGNORECASE)
			bm->bad_shift[tolower(bm->pattern[i])]
			    = bm->patlen - 1 - i;
	}

	/* Compute the good shift array, used to match reocurrences 
	 * of a subpattern */
	bm->good_shift[0] = 1;
	for (i = 1; i < bm->patlen; i++)
		bm->good_shift[i] = bm->patlen;
        for (i = bm->patlen-1, g = 1; i > 0; g++, i--) {
		for (j = i-1; j >= 1-g ; j--)
			if (subpattern(bm->pattern, i, j, g)) {
				bm->good_shift[g] = bm->patlen-j-g;
				break;
			}
	}
}

static struct ts_config *bm_init(const void *pattern, unsigned int len,
				 int gfp_mask, int flags)
{
	struct ts_config *conf;
	struct ts_bm *bm;
	int i;
	unsigned int prefix_tbl_len = len * sizeof(unsigned int);
	size_t priv_size = sizeof(*bm) + len + prefix_tbl_len;

	conf = alloc_ts_config(priv_size, gfp_mask);
	if (!conf)
		return NULL;

	conf->flags = flags;
	bm = ts_config_priv(conf);
	bm->patlen = len;
	bm->pattern = (uint8_t *) bm->good_shift + prefix_tbl_len;
	if (flags & TS_IGNORECASE)
		for (i = 0; i < len; i++)
			bm->pattern[i] = toupper(((uint8_t *)pattern)[i]);
	else
		memcpy(bm->pattern, pattern, len);
	compute_prefix_tbl(bm, flags);

	return conf;
}

static void *bm_get_pattern(struct ts_config *conf)
{
	struct ts_bm *bm = ts_config_priv(conf);
	return bm->pattern;
}

static unsigned int bm_get_pattern_len(struct ts_config *conf)
{
	struct ts_bm *bm = ts_config_priv(conf);
	return bm->patlen;
}

static void bm_destroy(struct ts_config *conf)
{
	/* Nothing to do - memory is freed by textsearch_destroy */
}

static struct ts_ops bm_ops = {
	.name		  = "bm",
	.find		  = bm_find,
	.init		  = bm_init,
	.destroy		  = bm_destroy,
	.get_pattern	  = bm_get_pattern,
	.get_pattern_len  = bm_get_pattern_len,
	.owner		  = NULL,
	.next		  = NULL
};

/* Initialize Boyer-Moore algorithm */
void init_bm(void)
{
	textsearch_register(&bm_ops);
}

/* Cleanup Boyer-Moore algorithm */
void exit_bm(void)
{
	textsearch_unregister(&bm_ops);
}