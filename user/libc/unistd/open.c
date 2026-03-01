#include <unistd.h>
#include <sys/syscall.h>

extern long syscall2(long n, long a1, long a2);

int open(const char* pathname, int flags) {
    return (int)syscall2(SYS_OPEN, (long)pathname, flags);
}
