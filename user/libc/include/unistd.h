#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>
#include <stdint.h>

typedef long ssize_t;
typedef long off_t;

ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t count);
int open(const char* pathname, int flags);
int close(int fd);
int mkdir(const char* pathname, uint32_t mode);
int unlink(const char* pathname);

#endif
