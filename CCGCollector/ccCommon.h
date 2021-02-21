#define true  1
#define false 0

typedef int bool;

typedef char int8;
typedef short int16;
typedef int int32;
typedef long long int64;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;


void MemoryClear(void *dst, int size);

void MemoryCopy(void *src, void *dst, int size);

void MemoryCopyReversed(void *src, void *dst, int size);

bool MemoryEqual(void *src, void *dst, int size);

uint64 GetTimeMicroSeconds();