#include <fcntl.h>
#include <unistd.h>
#include "fallocate_mac.h"

int fallocate(int fd, int mode, off_t offset, off_t len)
{
	off_t c_test;
	int ret = -1;

	if (!__builtin_saddll_overflow(offset, len, &c_test)) {
		fstore_t store = {F_ALLOCATECONTIG, F_PEOFPOSMODE, 0, offset + len};
		// Try to get a continuous chunk of disk space
		fcntl(fd, F_PREALLOCATE, &store);
		if (ret < 0) {
			// OK, perhaps we are too fragmented, allocate non-continuous
			store.fst_flags = F_ALLOCATEALL;
			ret = fcntl(fd, F_PREALLOCATE, &store);
			if (ret < 0) {
				return ret;
			}
		}
		ret = ftruncate(fd, offset + len);
	}

	return ret;
}
