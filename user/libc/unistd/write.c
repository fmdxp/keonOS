#include <unistd.h>
#include <sys/syscall.h>

extern long syscall3(long n, long a1, long a2, long a3);

ssize_t write(int fd, const void* buf, size_t count) {
    return (ssize_t)syscall3(SYS_WRITE, fd, (long)buf, (long)count);
}
