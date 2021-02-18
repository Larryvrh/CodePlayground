/*H**********************************************************************
* FILENAME : malloc_ex.c
*
* DESCRIPTION :
*       Simple memory leak detection.
*
* NOTES :
*       Inspired by Alok Save, https://stackoverflow.com/questions/9074229/detecting-memory-leaks-in-c-programs.
*
* AUTHOR :    T                   START DATE :    2/17/2021
*
* CHANGES :
*
* REF NO  VERSION DATE    WHO     DETAIL
* 0       0.1     2/17    T       Skeleton of "replacing" malloc/free.
* 1       0.2     2/17    T       Finish necessary linkedlist implementation.
* 3       0.3     2/18    T       Integrate together.
*
*H***********************************************************************/

#ifndef _INC_STDLIB
#include <stdlib.h>
#endif

#ifndef _INC_STDIO
#include <stdio.h>
#endif

int init_finished = 0;

struct mcLinkedNode {
    void* data;
    struct mcLinkedNode* next;
};

typedef struct {
    struct mcLinkedNode* head;
    int length;
    int dataSize;
} mcLinkedList;

typedef struct {
    void* mallocAddr;
    size_t mallocSize;
    const char* srcFile;
    int srcLine;
    const char* srcFunc;
} mcMallocRecord;

struct mcLinkedNode* mcAllocLinkedNode(void* data, int dataSize) {
    struct mcLinkedNode* node = malloc(sizeof(struct mcLinkedNode));
    if (node == NULL)
        return NULL;
    node->data = malloc(dataSize);
    if (node->data == NULL) {
        free(node);
        return NULL;
    }
    for (int i = 0; i < dataSize; ++i)
        *((char*)(node->data) + i) = *((char*)(data)+i);
    node->next = NULL;
    return node;
}

void mcFreeLinkedNode(struct mcLinkedNode* node) {
    free(node->data);
    free(node);
}

int mcLinkedListPush(mcLinkedList* list, void* data) {
    int dataSize = list->dataSize;
    struct mcLinkedNode* node = mcAllocLinkedNode(data, dataSize);
    if (node == NULL)
        return 0;
    if (list->head == NULL) {
        list->head = node;
    }
    else {
        struct mcLinkedNode* current = list->head;
        while (current->next != NULL)
            current = current->next;
        current->next = node;
    }
    list->length++;
    return 1;
}

int mcLinkedListRemove(mcLinkedList* list, int index) {
    if (index < 0 || index >= list->length)
        return 0;
    struct mcLinkedNode* removed;
    if (index == 0) {
        removed = list->head;
        list->head = removed->next;
    }
    else {
        struct mcLinkedNode* current = list->head;
        for (int i = 0; i < index - 1; ++i)
            current = current->next;
        removed = current->next;
        current->next = removed->next;
    }
    mcFreeLinkedNode(removed);
    list->length--;
    return 1;
}

void mcLinkedListIterate(mcLinkedList* list, int (*iterFunc)(int, void*)) {
    struct mcLinkedNode* current = list->head;
    int index = 0;
    while (current != NULL) {
        int cot = iterFunc(index, current->data);
        if (cot)
            current = current->next;
        else
            current = NULL;
        index++;
    }
}

void mcFreeLinkedList(mcLinkedList* list) {
    struct mcLinkedNode* current = list->head;
    while (current != NULL) {
        struct mcLinkedNode* next = current->next;
        mcFreeLinkedNode(current);
        current = next;
    }
}

mcLinkedList mcMallocList = { 0, 0, sizeof(mcMallocRecord) };

int mcMallocCount = 0;
int mcFreeCount = 0;
int mcMallocBytes = 0;

void* mcSearchTargetAddr = NULL;
int mcSearchResultIndex = -1;

int searchTraverse(int index, void* record) {
    mcMallocRecord* mallocRecord = (mcMallocRecord*)record;
    if (mallocRecord->mallocAddr == mcSearchTargetAddr)
        mcSearchResultIndex = index;
    return 1;
}

int searchRecord(void* mallocAddr) {
    mcSearchTargetAddr = mallocAddr;
    mcSearchResultIndex = -1;
    mcLinkedListIterate(&mcMallocList, searchTraverse);
    return mcSearchResultIndex;
}

int outputTraverse(int index, void* record) {
    mcMallocRecord* mRecord = (mcMallocRecord*)record;
    printf("\tFile:\"%s\" Func:\"%s\" Line:%d Address:%p Size:%zu\n", mRecord->srcFile, mRecord->srcFunc,
        mRecord->srcLine, mRecord->mallocAddr, mRecord->mallocSize);
    return 1;
}

void mcOnExitMemoryCheck(void) {
    printf("Summary:\n");
    printf("\t%d valid malloc calls, %d valid free calls, total %d bytes allocated.\n", mcMallocCount, mcFreeCount,
        mcMallocBytes);
    if (mcMallocList.length != 0) {
        printf("Possible memory leak detected:\n");
        mcLinkedListIterate(&mcMallocList, outputTraverse);
    }
    else {
        printf("No possible memory leak detected.\n");
    }
    mcFreeLinkedList(&mcMallocList);
}

void* mcMalloc(size_t size, const char* file, int line, const char* func) {
    if (!init_finished) {
        atexit(mcOnExitMemoryCheck);
        init_finished = 1;
    }
    void* p = malloc(size);
    //printf("Allocated = %s, %i, %s, %p[%zu]\n", file, line, func, p, size);

    if (p != NULL) {
        mcMallocCount++;
        mcMallocBytes += size;
        mcMallocRecord record = { p, size, file, line, func };
        mcLinkedListPush(&mcMallocList, &record);
    }

    return p;
}

void mcFree(void* ptr) {
    int freedIndex = searchRecord(ptr);

    if (ptr != NULL) {
        free(ptr);
        mcFreeCount++;
    }

    mcLinkedListRemove(&mcMallocList, freedIndex);
}

#define malloc(ARG) mcMalloc( ARG, __FILE__, __LINE__, __FUNCTION__)
#define free(ARG) mcFree( ARG )