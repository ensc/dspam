#define FALLOC_FL_PUNCH_HOLE 0
#define FALLOC_FL_KEEP_SIZE 0
#define posix_fallocate(fd, offset, len) fallocate((fd), 0, (offset), (len))

int fallocate(int fd, int mode, off_t offset, off_t len);
