#include <stdlib.h>
#include <sys/syscall.h>

extern long syscall1(long n, long a1);

void exit(int status) {
    syscall1(SYS_EXIT, status);
    while(1) { }
}
