/*
 * Text search framework - simplified userspace version
 * Based on Linux kernel textsearch implementation
 */

#include "textsearch.h"

static struct ts_ops *textsearch_ops = NULL;

int textsearch_register(struct ts_ops *ops)
{
	struct ts_ops **pos = &textsearch_ops;
	
	/* Check if already registered */
	while (*pos) {
		if (strcmp((*pos)->name, ops->name) == 0)
			return -1; /* Already exists */
		pos = &(*pos)->next;
	}
	
	/* Add to list */
	ops->next = NULL;
	*pos = ops;
	
	return 0;
}

int textsearch_unregister(struct ts_ops *ops)
{
	struct ts_ops **pos = &textsearch_ops;
	
	while (*pos) {
		if (*pos == ops) {
			*pos = ops->next;
			return 0;
		}
		pos = &(*pos)->next;
	}
	
	return -1; /* Not found */
}

struct ts_config *textsearch_prepare(const char *algo, const void *pattern,
				    unsigned int len, int gfp_mask, int flags)
{
	struct ts_ops *ops;
	struct ts_config *conf;
	
	/* Find algorithm */
	ops = textsearch_ops;
	while (ops) {
		if (strcmp(ops->name, algo) == 0)
			break;
		ops = ops->next;
	}
	
	if (!ops)
		return NULL; /* Algorithm not found */
	
	/* Initialize config */
	conf = ops->init(pattern, len, gfp_mask, flags);
	if (!conf)
		return NULL;
	
	conf->ops = ops;
	conf->flags = flags;
	
	return conf;
}

void textsearch_destroy(struct ts_config *conf)
{
	if (conf) {
		if (conf->ops && conf->ops->destroy)
			conf->ops->destroy(conf);
		free_ts_config(conf);
	}
}

/* Forward declaration for block provider */
static unsigned int simple_block_provider(unsigned int consumed, const uint8_t **dst,
					  struct ts_config *conf, struct ts_state *state);

/* Simple block provider for continuous search */
static unsigned int simple_block_provider(unsigned int consumed, const uint8_t **dst,
					  struct ts_config *conf, struct ts_state *state)
{
	const void *data = *(const void **)state->cb;
	unsigned int len = *(unsigned int *)(state->cb + sizeof(void *));
	
	if (consumed >= len)
		return 0;
	
	*dst = (const uint8_t *)data + consumed;
	return len - consumed;
}

/* Simple continuous search implementation */
unsigned int textsearch_find_continuous(struct ts_config *conf,
				       struct ts_state *state,
				       const void *data, unsigned int len)
{
	if (!conf || !state || !conf->ops || !conf->ops->find)
		return UINT_MAX;
	
	/* Set up state for continuous search */
	state->offset = 0;
	
	/* Store data pointer in state->cb for the block provider */
	*(const void **)state->cb = data;
	*(unsigned int *)(state->cb + sizeof(void *)) = len;
	
	/* Set up block provider */
	conf->get_next_block = simple_block_provider;
	conf->finish = NULL;
	
	/* Call search algorithm */
	return conf->ops->find(conf, state);
}