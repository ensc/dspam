/*	--*- c -*--
 * Copyright (C) 2016 Enrico Scholz <enrico.scholz@sigma-chemnitz.de>
 */

#ifndef H_DSPAM_SRC_TOOLS_HASHDRV_UTILS_H
#define H_DSPAM_SRC_TOOLS_HASHDRV_UTILS_H

#include <stdlib.h>
#include <unistd.h>

struct _hash_drv_map;
ssize_t css_optimize(struct _hash_drv_map *map, unsigned long flags);

#endif	/* H_DSPAM_SRC_TOOLS_HASHDRV_UTILS_H */
