#include "ccCommon.h"
#include "sys/time.h"

void MemoryClear(void *dst, int size) {
    int currentOffset = 0;
    int mulCount = size / (int) sizeof(long long);
    for (int i = 0; i < mulCount; ++i) {
        *((long long *) dst + i) = 0;
        currentOffset += sizeof(long long);
    }
    while (currentOffset < size) {
        *((char *) dst + currentOffset) = 0;
        currentOffset += 1;
    }
}

void MemoryCopy(void *src, void *dst, int size) {
    int currentOffset = 0;
    int mulCount = size / (int) sizeof(long long);
    for (int i = 0; i < mulCount; ++i) {
        *((long long *) dst + i) = *((long long *) src + i);
        currentOffset += sizeof(long long);
    }
    while (currentOffset < size) {
        *((char *) dst + currentOffset) = *((char *) src + currentOffset);
        currentOffset += 1;
    }
}

void MemoryCopyReversed(void *src, void *dst, int size) {
    int currentOffset = size - 1;
    int mulCount = size / (int) sizeof(long long);
    int sigCount = size % (int) sizeof(long long);
    for (int i = 0; i < sigCount; ++i) {
        *((char *) dst + currentOffset) = *((char *) src + currentOffset);
        currentOffset -= 1;
    }
    for (int i = mulCount - 1; i >= 0; --i)
        *((long long *) dst + i) = *((long long *) src + i);
}

bool MemoryEqual(void *src, void *dst, int size) {
    int currentOffset = 0;
    int mulCount = size / (int) sizeof(long long);
    for (int i = 0; i < mulCount; ++i) {
        if (*((long long *) dst + i) != *((long long *) src + i))
            return false;
        currentOffset += sizeof(long long);
    }
    while (currentOffset < size) {
        if (*((char *) dst + currentOffset) != *((char *) src + currentOffset))
            return false;
        currentOffset += 1;
    }
    return true;
}

uint64 GetTimeMicroSeconds() {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec * 1000000 + time.tv_nsec / 1000;
}