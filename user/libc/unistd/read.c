#include <unistd.h>
#include <sys/syscall.h>

extern long syscall3(long n, long a1, long a2, long a3);

ssize_t read(int fd, void* buf, size_t count) {
    return (ssize_t)syscall3(SYS_READ, fd, (long)buf, (long)count);
}
