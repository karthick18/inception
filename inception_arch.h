/*
 * Arch independent code, differentiating linux with other *Nixes
 */
#ifndef _INCEPTION_ARCH_H_
#define _INCEPTION_ARCH_H_

#include <sys/time.h>
#include <sys/mman.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MAP_ANONYMOUS
#ifndef MAP_ANON
#error "MAP_ANON or anonymous mmap not supported for this arch."
#endif
#define MAP_ANONYMOUS MAP_ANON
#endif

#ifdef __linux__
static __inline__ void arch_gettime(int delay, struct timespec *ts)
{
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += delay;
}
#else
static __inline__ void arch_gettime(int delay, struct timespec *ts)
{
    struct timeval t;
    gettimeofday(&t, NULL);
    t.tv_sec += delay;
    ts->tv_sec = t.tv_sec;
    ts->tv_nsec = t.tv_usec*1000L;
}
#endif

#ifdef __cplusplus
}
#endif

#endif
