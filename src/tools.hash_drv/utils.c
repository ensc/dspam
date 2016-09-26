/*	--*- c -*--
 * Copyright (C) 2016 Enrico Scholz <enrico.scholz@sigma-chemnitz.de>
 */

#ifdef HAVE_CONFIG_H
#include <auto-config.h>
#endif

#include "utils.h"

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <linux/falloc.h>

#include "hash_drv.h"


static ssize_t create_hole(struct _hash_drv_map const *map,
			   size_t start, size_t cur, size_t blksize)
{
	int	rc;

	if (cur - start < blksize)
		return 0;

	if (0)
		LOG(LOG_DEBUG, "%s: punching hole %zu->%zu", map->filename,
		    start, cur);

	rc = fallocate(map->fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
		       start, cur - start);
	if (rc < 0) {
		LOG(LOG_ERR, "%s: fallocate(): %s", map->filename,
		    strerror(errno));
		return -1;
	}

	return cur - start;
}

ssize_t css_optimize(struct _hash_drv_map *map, unsigned long flags)
{
	struct hash_drv_extent const *ext;
	struct stat		st;
	size_t			i;
	size_t			start = 0;
	void const		*pos;
	size_t			saved = 0;

	if (!(flags & HMAP_HOLES))
		return 0;

	if (fstat(map->fd, &st) < 0) {
		LOG(LOG_ERR, "%s: fstat() failed: %s",
		    map->filename, strerror(errno));
		return -1;
	}

	if (st.st_blksize == 0 ||
	    st.st_blksize % sizeof(unsigned long long) != 0) {
		LOG(LOG_ERR, "%s: unsupported blocksize %u", map->filename,
		    (unsigned int)st.st_blksize);
		return -1;
	}

	/* ensure that file has been mapped completely */
	ext = NULL;
	do {
		ext = _hash_drv_next_extent(map, ext);
		if (!ext)
			break;

		hash_drv_ext_prefetch(ext);
	} while (!hash_drv_ext_is_eof(map, ext));

	pos = map->addr;
	i = 0;

	{
		unsigned long long const	*data = pos;

		while (i + sizeof *data <= map->file_len) {
			if (data[i / sizeof *data] == 0) {
				i += sizeof *data;
			} else {
				size_t		blk = (i / st.st_blksize
						       * st.st_blksize);
				ssize_t		cnt;

				cnt = create_hole(map, start, blk, st.st_blksize);
				if (cnt < 0)
					goto out;

				saved += cnt;
				i      = blk + st.st_blksize;
				start  = i;
			}
		}

		pos = &data[i / sizeof *data];
	}

	{
		uint8_t const		*data = pos;

		while (i + sizeof *data <= map->file_len) {
			if (data[i / sizeof *data] == 0) {
				i += sizeof *data;
			} else {
				size_t		blk = (i / st.st_blksize
						       * st.st_blksize);
				ssize_t		cnt;

				cnt = create_hole(map, start, blk, st.st_blksize);
				if (cnt < 0)
					goto out;

				saved += cnt;
				i      = blk + st.st_blksize;
				start  = i;
			}
		}

		pos = &data[i / sizeof *data];
	}

	if (start < map->file_len) {
		unsigned int	blk = (i / st.st_blksize * st.st_blksize);
		ssize_t		cnt;

		/* round up the last block */
		cnt = create_hole(map, start, blk + st.st_blksize, st.st_blksize);
		if (cnt < 0)
			goto out;

		saved += cnt;
	}

out:
	return saved;
}
