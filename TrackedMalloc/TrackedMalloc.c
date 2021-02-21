#include "TrackedMalloc.h"

int init_finished = 0;
mcLinkedList mcMallocList = {0, 0, sizeof(mcMallocRecord)};

int mcMallocCount = 0;
int mcFreeCount = 0;
int mcMallocBytes = 0;

void *(*oriMalloc)(size_t);

void (*oriFree)(void *);

struct mcLinkedNode *mcAllocLinkedNode(void *data, int dataSize) {
    struct mcLinkedNode *node = (struct mcLinkedNode *) oriMalloc(sizeof(struct mcLinkedNode));
    if (node == NULL)
        return NULL;
    node->data = oriMalloc(dataSize);
    if (node->data == NULL) {
        oriFree(node);
        return NULL;
    }
    for (int i = 0; i < dataSize; ++i)
        *((char *) (node->data) + i) = *((char *) (data) + i);
    node->next = NULL;
    return node;
}

void mcFreeLinkedNode(struct mcLinkedNode *node) {
    oriFree(node->data);
    oriFree(node);
}

int mcLinkedListPush(mcLinkedList *list, void *data) {
    int dataSize = list->dataSize;
    struct mcLinkedNode *node = mcAllocLinkedNode(data, dataSize);
    if (node == NULL)
        return 0;
    if (list->head == NULL) {
        list->head = node;
    } else {
        struct mcLinkedNode *current = list->head;
        while (current->next != NULL)
            current = current->next;
        current->next = node;
    }
    list->length++;
    return 1;
}

int mcLinkedListRemove(mcLinkedList *list, int index) {
    if (index < 0 || index >= list->length)
        return 0;
    struct mcLinkedNode *removed;
    if (index == 0) {
        removed = list->head;
        list->head = removed->next;
    } else {
        struct mcLinkedNode *current = list->head;
        for (int i = 0; i < index - 1; ++i)
            current = current->next;
        removed = current->next;
        current->next = removed->next;
    }
    mcFreeLinkedNode(removed);
    list->length--;
    return 1;
}

void mcLinkedListIterate(mcLinkedList *list, int (*iterFunc)(int, void *)) {
    struct mcLinkedNode *current = list->head;
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

void mcFreeLinkedList(mcLinkedList *list) {
    struct mcLinkedNode *current = list->head;
    while (current != NULL) {
        struct mcLinkedNode *next = current->next;
        mcFreeLinkedNode(current);
        current = next;
    }
}


int searchRecord(void *mallocAddr) {
    struct mcLinkedNode *current = mcMallocList.head;
    int index = 0;
    while (current != NULL) {
        mcMallocRecord *record = (mcMallocRecord *) current->data;
        if (record->mallocAddr == mallocAddr)
            return index;
        current = current->next;
        index++;
    }
    return -1;
}

int outputTraverse(int index, void *record) {
    mcMallocRecord *mRecord = (mcMallocRecord *) record;
    printf("\tFile:\"%s\" Func:\"%s\" Line:%d Address:%p Size:%zu\n", mRecord->srcFile, mRecord->srcFunc,
           mRecord->srcLine, mRecord->mallocAddr, mRecord->mallocSize);
    return 1;
}

void mcOnExitMemoryCheck(void) {
    printf("\nSummary:\n");
    printf("\t%d valid malloc calls, %d valid free calls, total %d bytes allocated.\n", mcMallocCount, mcFreeCount,
           mcMallocBytes);
    if (mcMallocList.length != 0) {
        printf("Possible memory leak detected:\n");
        mcLinkedListIterate(&mcMallocList, outputTraverse);
    } else {
        printf("No possible memory leak detected.\n");
    }
    mcFreeLinkedList(&mcMallocList);
}

void *mcMalloc(size_t size, const char *file, int line, const char *func) {
    if (!init_finished) {
        oriMalloc = malloc;
        oriFree = free;
        atexit(mcOnExitMemoryCheck);
        init_finished = 1;
    }
    void *p = oriMalloc(size);
    //printf("Allocated = %s, %i, %s, %p[%zu]\n", file, line, func, p, size);

    if (p != NULL) {
        mcMallocCount++;
        mcMallocBytes += size;
        mcMallocRecord record = {p, size, file, line, func};
        mcLinkedListPush(&mcMallocList, &record);
    }

    return p;
}

void mcFree(void *ptr) {
    int freedIndex = searchRecord(ptr);

    if (ptr != NULL) {
        oriFree(ptr);
        mcFreeCount++;
    }

    mcLinkedListRemove(&mcMallocList, freedIndex);
}
