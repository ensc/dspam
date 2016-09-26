#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include "hash_drv.h"

#include <assert.h>

#include "tools.hash_drv/utils.h"

#include "../hash_drv.c"

int DO_DEBUG = 0;

static void add_rec(struct _hash_drv_map *map,
		    unsigned long long hashcode,
		    unsigned long code)
{
	struct _hash_drv_spam_record	rec = {
		.hashcode = hashcode,
		.spam = code,
	};
	unsigned long			offs;

	assert(_hash_drv_set_spamrecord(map, &rec, 0) == 0);

	offs = _hash_drv_get_spamrecord(map, &rec);
	assert(offs > 0);
	assert(offs < map->file_len);
}

static void verify_rec(struct _hash_drv_map *map,
		       unsigned long long hashcode,
		       unsigned long code)
{
	struct _hash_drv_spam_record	rec = {
		.hashcode = hashcode,
	};
	unsigned long			offs;

	offs = _hash_drv_get_spamrecord(map, &rec);
	assert(offs > 0);
	assert(offs < map->file_len);
	assert(rec.spam == code);
}

int main(int argc, char *argv[])
{
	unsigned int const	NUM_APPEND   = 10;
	unsigned long		max_seek     = 1;
	unsigned long		max_extents  = 0;
	unsigned long		extent_size  = 4096*10;
	int			pctincrease  = 0;
	int			flags	     = (HMAP_FALLOCATE |
						HMAP_AUTOEXTEND);
	struct _hash_drv_map	map;
	char const		*fname = argv[1];
	ssize_t			l;

	unlink(fname);

	if (_hash_drv_open(fname, &map, extent_size, max_seek,
			   max_extents, extent_size, pctincrease, flags))
		return EXIT_FAILURE;

	extent_size = roundup_hash_op(&map, extent_size);

	/* add distinct elements which have all a modulos of '1'; this should
	 * add one extent per element */
	for (unsigned int i = 0; i < NUM_APPEND; ++i)
		add_rec(&map, extent_size * i + 1, i);

	assert(map.num_extents == NUM_APPEND);

	l = css_optimize(&map, HMAP_HOLES);
	assert(l > 0);

	_hash_drv_close(&map);

	if (_hash_drv_open(fname, &map, extent_size, max_seek,
			   max_extents, extent_size, pctincrease, flags))
		return EXIT_FAILURE;

	for (unsigned int i = 0; i < NUM_APPEND; ++i)
		verify_rec(&map, extent_size * i + 1, i);

	_hash_drv_close(&map);
}
