#ifndef __AFS_UTIL_H__
#define __AFS_UTIL_H__

#include <sys/time.h>
#include <sys/types.h>
#include <time.h>


#define mv_likely(x) __builtin_expect ((x), 1)
#define mv_unlikely(x) __builtin_expect ((x), 0)

static inline double now_us ()
{
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, &tv);
    return (tv.tv_sec * (uint64_t) 1000000 + (double)tv.tv_nsec/1000);
}

static inline void *memcpy_8(void *dst, const void *src) {
	unsigned char *dd = ((unsigned char*)dst) + 8;
	const unsigned char *ss = ((const unsigned char*)src) + 8;
    *((uint64_t*)(dd - 8)) = *((uint64_t*)(ss - 8));
    return dst;
}

#endif
