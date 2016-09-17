#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include "hash_drv.h"

#undef NDEBUG

#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>

#include "../hash_drv.c"

static void test_prime(void)
{
	assert(is_prime(2));
	assert(is_prime(3));
	assert(is_prime(5));
	assert(is_prime(1021));
	assert(!is_prime(4));
	assert(!is_prime(1024));
}

static void test_roundup_pow2(void)
{
	struct _hash_drv_map	map = {
		.flags		= HMAP_POW2
	};

	assert(roundup_hash_op(&map, 0) == 1);
	assert(roundup_hash_op(&map, 1) == 1);
	assert(roundup_hash_op(&map, 2) == 2);
	assert(roundup_hash_op(&map, 3) == 4);
	assert(roundup_hash_op(&map, 4) == 4);
	assert(roundup_hash_op(&map, 1023) == 1024);
	assert(roundup_hash_op(&map, 1024) == 1024);
}

static void test_roundup_prime(void)
{
	struct _hash_drv_map	map = {
		.flags		= 0,
	};

	assert(roundup_hash_op(&map, 0) == 2);
	assert(roundup_hash_op(&map, 1) == 2);
	assert(roundup_hash_op(&map, 2) == 2);
	assert(roundup_hash_op(&map, 3) == 3);
	assert(roundup_hash_op(&map, 4) == 5);
	assert(roundup_hash_op(&map, 1020) == 1021);
	assert(roundup_hash_op(&map, 1021) == 1021);
}

static void add_rec(struct _hash_drv_map *map,
		    unsigned long long hashcode)
{
	struct _hash_drv_spam_record	rec = {
		.hashcode = hashcode
	};
	unsigned long			offs;

	assert(_hash_drv_set_spamrecord(map, &rec, 0) == 0);

	offs = _hash_drv_get_spamrecord(map, &rec);
	assert(offs > 0);
	assert(offs < map->file_len);
}

int DO_DEBUG = 0;

int main(int argc, char *argv[])
{
	unsigned long		max_seek     = HASH_SEEK_MAX;
	unsigned long		max_extents  = 0;
	unsigned long		extent_size  = 13;
	int			pctincrease  = 0;
	int			flags = HMAP_HOLES | HMAP_AUTOEXTEND | HMAP_POW2;
	struct _hash_drv_map	map;
	unsigned int		cnt;
	char const		*fname = argv[1];

	_Static_assert(sizeof(struct _hash_drv_header) % 8 == 0,
		       "badly aligned _hash_drv_header");

	test_prime();
	test_roundup_pow2();
	test_roundup_prime();

	unlink(fname);

	if (_hash_drv_open(fname, &map, 4, max_seek,
			   max_extents, extent_size, pctincrease, flags))
		return EXIT_FAILURE;

	for (cnt = atoi(argv[2]); cnt > 0; --cnt)
		add_rec(&map, cnt);
}
