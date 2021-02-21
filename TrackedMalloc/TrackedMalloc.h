#ifndef INC_TRACKED_MALLOC
#define INC_TRACKED_MALLOC

#include <stdlib.h>
#include <stdio.h>

struct mcLinkedNode {
    void *data;
    struct mcLinkedNode *next;
};

typedef struct {
    struct mcLinkedNode *head;
    int length;
    int dataSize;
} mcLinkedList;

typedef struct {
    void *mallocAddr;
    size_t mallocSize;
    const char *srcFile;
    int srcLine;
    const char *srcFunc;
} mcMallocRecord;


void *mcMalloc(size_t size, const char *file, int line, const char *func);
void mcFree(void *ptr);

#define malloc(ARG) mcMalloc( ARG, __FILE__, __LINE__, __FUNCTION__)
#define free(ARG) mcFree( ARG )

#endif
