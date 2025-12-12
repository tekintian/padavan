/*
 * Wildcard text search implementation - userspace version
 * Supports * (matches any sequence) and ? (matches any single character)
 */

#include "textsearch.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct ts_wildcard
{
	uint8_t		*pattern;
	unsigned int	pattern_len;
	unsigned int	has_wildcard;
	/* Pre-processed pattern segments */
	unsigned char	*processed;
	unsigned int	processed_len;
};

/* Check if a character is a wildcard */
static int is_wildcard(char c)
{
	return c == '*' || c == '?';
}

/* Convert wildcard pattern to regular expression-like pattern */
static int preprocess_pattern(const uint8_t *pattern, unsigned int len, 
			    unsigned char **processed, unsigned int *processed_len,
			    int flags)
{
	unsigned char *result;
	unsigned int i, j = 0;
	const int icase = flags & TS_IGNORECASE;
	int has_wildcard = 0;
	
	/* Allocate maximum possible size (worst case: no compression) */
	result = malloc(len * 2 + 1);
	if (!result)
		return -1;
	
	for (i = 0; i < len; i++) {
		char c = pattern[i];
		
		if (c == '*') {
			has_wildcard = 1;
			/* Handle multiple consecutive * as one */
			if (j == 0 || result[j-1] != '*') {
				result[j++] = '*';
			}
		} else if (c == '?') {
			has_wildcard = 1;
			result[j++] = '?';
		} else {
			/* Escape special characters if needed and handle case */
			if (icase)
				c = toupper(c);
			result[j++] = c;
		}
	}
	
	result[j] = '\0';
	*processed = result;
	*processed_len = j;
	return has_wildcard;
}

/* Match pattern against text starting at position pos */
static int wildcard_match_at(const unsigned char *pattern, unsigned int pat_len,
			    const unsigned char *text, unsigned int text_len,
			    unsigned int pos, int icase)
{
	unsigned int p = 0, t = pos;
	int star_pos = -1;
	int match_pos = pos;
	
	while (t < text_len && p < pat_len) {
		if (pattern[p] == '*') {
			/* Skip consecutive * */
			while (p + 1 < pat_len && pattern[p + 1] == '*')
				p++;
			
			star_pos = p;
			match_pos = t;
			p++;
		} else if (pattern[p] == '?') {
			p++;
			t++;
		} else {
			char pc = pattern[p];
			char tc = icase ? toupper(text[t]) : text[t];
			
			if (pc == tc) {
				p++;
				t++;
			} else if (star_pos >= 0) {
				/* Backtrack: try to match * with more characters */
				p = star_pos + 1;
				match_pos++;
				t = match_pos;
			} else {
				return 0; /* No match */
			}
		}
	}
	
	/* Skip trailing * in pattern */
	while (p < pat_len && pattern[p] == '*')
		p++;
	
	return p == pat_len; /* Match if we've consumed the entire pattern */
}

static unsigned int wildcard_find(struct ts_config *conf, struct ts_state *state)
{
	struct ts_wildcard *wild = ts_config_priv(conf);
	unsigned int i, text_len, consumed = state->offset;
	const uint8_t *text;
	const int icase = conf->flags & TS_IGNORECASE;

	for (;;) {
		text_len = conf->get_next_block(consumed, &text, conf, state);

		if (text_len == 0)
			break;

		/* Search for pattern in current block */
		if (wild->has_wildcard) {
			/* Wildcard pattern: try to match at each position */
			for (i = 0; i <= text_len - 1; i++) {
				if (wildcard_match_at(wild->processed, wild->processed_len,
						    text, text_len, i, icase)) {
					state->offset = consumed + i + 1;
					return consumed + i;
				}
			}
		} else {
			/* No wildcards: use simple string matching */
			if (wild->processed_len <= text_len) {
				for (i = 0; i <= text_len - wild->processed_len; i++) {
					int match = 1;
					for (unsigned int j = 0; j < wild->processed_len; j++) {
						char pc = wild->processed[j];
						char tc = icase ? toupper(text[i + j]) : text[i + j];
						if (pc != tc) {
							match = 0;
							break;
						}
					}
					if (match) {
						state->offset = consumed + i + wild->processed_len;
						return consumed + i;
					}
				}
			}
		}

		consumed += text_len;
	}

	return UINT_MAX;
}

static struct ts_config *wildcard_init(const void *pattern, unsigned int len,
				       int gfp_mask, int flags)
{
	struct ts_config *conf;
	struct ts_wildcard *wild;
	size_t priv_size = sizeof(*wild);

	conf = alloc_ts_config(priv_size, gfp_mask);
	if (!conf)
		return NULL;

	conf->flags = flags;
	wild = ts_config_priv(conf);
	wild->pattern_len = len;
	
	/* Process the pattern for wildcard matching */
	if (preprocess_pattern(pattern, len, &wild->processed, &wild->processed_len, flags) < 0) {
		free_ts_config(conf);
		return NULL;
	}
	
	wild->has_wildcard = (flags & TS_WILDCARD) && 
			    (strchr(pattern, '*') || strchr(pattern, '?'));
	
	/* Store original pattern for get_pattern */
	wild->pattern = malloc(len);
	if (wild->pattern) {
		memcpy(wild->pattern, pattern, len);
	}

	return conf;
}

static void *wildcard_get_pattern(struct ts_config *conf)
{
	struct ts_wildcard *wild = ts_config_priv(conf);
	return wild->pattern ? wild->pattern : wild->processed;
}

static unsigned int wildcard_get_pattern_len(struct ts_config *conf)
{
	struct ts_wildcard *wild = ts_config_priv(conf);
	return wild->pattern_len;
}

static void wildcard_destroy(struct ts_config *conf)
{
	struct ts_wildcard *wild = ts_config_priv(conf);
	if (wild->processed)
		free(wild->processed);
	if (wild->pattern)
		free(wild->pattern);
}

static struct ts_ops wildcard_ops = {
	.name		  = "wildcard",
	.find		  = wildcard_find,
	.init		  = wildcard_init,
	.destroy	  = wildcard_destroy,
	.get_pattern	  = wildcard_get_pattern,
	.get_pattern_len  = wildcard_get_pattern_len,
	.owner		  = NULL,
	.next		  = NULL
};

/* Initialize wildcard algorithm */
void init_wildcard(void)
{
	textsearch_register(&wildcard_ops);
}

/* Cleanup wildcard algorithm */
void exit_wildcard(void)
{
	textsearch_unregister(&wildcard_ops);
}