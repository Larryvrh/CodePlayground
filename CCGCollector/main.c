#include <stdio.h>
#include <stdlib.h>
#include "ccCommon.h"
#include "setjmp.h"

//#include "TrackedMalloc.h"

int HashAddress(void *address) {
    uint64 key = (uint64) address;
    key = (~key) + (key << 18);
    key = key ^ (key >> 31);
    key = key * 21;
    key = key ^ (key >> 11);
    key = key + (key << 6);
    key = key ^ (key >> 22);
    int result = (int) key;
    return result > 0 ? result : -result;
}

typedef struct MRE_ {
    void *mallocAddr;
    int mallocSize;
    bool isInUse;
    struct MRE_ *next;
} RecordEntry;

typedef struct {
    RecordEntry *entries;
    int size, capacity;
    double loadFactor;
} RecordMap;

RecordMap records;

void InitRecordMap(RecordMap *map) {
    map->size = 0;
    map->capacity = 7;
    map->loadFactor = 0.86;
    map->entries = malloc(map->capacity * sizeof(RecordEntry));
    MemoryClear(map->entries, (int) (map->capacity * sizeof(RecordEntry)));
}

void FreeRecordMap(RecordMap *map) {
    for (int i = 0; i < map->capacity; ++i) {
        RecordEntry *current = (map->entries + i)->next;
        while (current != NULL) {
            RecordEntry *next = current->next;
            free(current);
            current = next;
        }
    }
    free(map->entries);
}

RecordEntry *MallocRecordEntry(RecordEntry *src) {
    RecordEntry *record = (RecordEntry *) malloc(sizeof(RecordEntry));
    MemoryCopy(src, record, sizeof(RecordEntry));
    return record;
}

void Expand(RecordMap *map);

bool AddRecord(RecordMap *map, RecordEntry *entry, bool entryFromHeap) {
    if ((double) map->size / map->capacity > map->loadFactor)
        Expand(map);
    int index = HashAddress(entry->mallocAddr) % map->capacity;
    if (map->entries[index].mallocAddr == NULL) {
        map->entries[index] = *entry;
        if (entryFromHeap)
            free(entry);
        map->size += 1;
        return true;
    } else {
        RecordEntry *current = map->entries + index;
        while (true) {
            if (current->mallocAddr == entry->mallocAddr)
                return false;
            if (current->next != NULL)
                current = current->next;
            else
                break;
        }
        current->next = entryFromHeap ? entry : MallocRecordEntry(entry);
        map->size += 1;
        return true;
    }
}

void Expand(RecordMap *map) {
    int oldCapacity = map->capacity;
    RecordEntry *oldEntries = map->entries;

    map->capacity *= 2;
    map->size = 0;
    map->entries = malloc(map->capacity * sizeof(RecordEntry));
    MemoryClear(map->entries, (int) (map->capacity * sizeof(RecordEntry)));

    for (int i = 0; i < oldCapacity; ++i) {
        RecordEntry *current = (oldEntries + i);
        if (current->mallocAddr == 0)
            continue;
        RecordEntry *next = current->next;
        current->next = NULL;
        AddRecord(map, current, false);
        current = next;
        while (current != NULL) {
            next = current->next;
            current->next = NULL;
            AddRecord(map, current, true);
            current = next;
        }
    }

    free(oldEntries);
}

RecordEntry *GetRecord(RecordMap *map, void *mallocAddr) {
    int index = HashAddress(mallocAddr) % map->capacity;
    if (map->entries[index].mallocAddr == NULL) {
        return NULL;
    } else {
        RecordEntry *current = map->entries + index;
        while (current != NULL) {
            if (current->mallocAddr == mallocAddr)
                return current;
            current = current->next;
        }
    }
    return NULL;
}

bool RemoveRecord(RecordMap *map, void *mallocAddr) {
    int index = HashAddress(mallocAddr) % map->capacity;
    if (map->entries[index].mallocAddr == NULL) {
        return false;
    } else {
        RecordEntry *current = map->entries + index;
        if (current->mallocAddr == mallocAddr) {
            if (map->entries[index].next != NULL) {
                RecordEntry *next = map->entries[index].next;
                map->entries[index] = *(map->entries[index].next);
                free(next);
            } else {
                map->entries[index].mallocAddr = NULL;
            }
            map->size -= 1;
            return true;
        }
        while (current != NULL) {
            if (current->next && current->next->mallocAddr == mallocAddr) {
                RecordEntry *newNext = current->next->next;
                free(current->next);
                current->next = newNext;
                map->size -= 1;
                return true;
            }
            current = current->next;
        }
    }
    return false;
}

void TraverseRecordMap(RecordMap *map, void (*traverseFunc)(RecordEntry *, void *), void *closure) {
    for (int i = 0; i < map->capacity; ++i) {
        RecordEntry *current = (map->entries + i);
        if (current->mallocAddr == 0)
            continue;
        while (current != NULL) {
            traverseFunc(current, closure);
            current = current->next;
        }
    }
}

typedef struct {
    RecordMap records;
    void *FrameTop;
    void *minAddr, *maxAddr;
    int sectionCount, byteCount;
    int collectThreshold;
} GCollector;

GCollector gc;

void GCInit(GCollector *gCollector, void *pArgc) {
    InitRecordMap(&gCollector->records);
    gCollector->FrameTop = pArgc;
    gCollector->minAddr = 0;
    gCollector->maxAddr = 0;
    gCollector->sectionCount = 0;
    gCollector->byteCount = 0;
    gCollector->collectThreshold = 0;
}

void GCEnd(GCollector *gCollector) {
    FreeRecordMap(&gCollector->records);
}

void OutputGCInfo(GCollector *gCollector) {
    printf("GC Summary:\n");
    printf("\t Minimal Address: [%p] Maximal Address: [%p]\n", gCollector->minAddr, gCollector->maxAddr);
    printf("\t Memory sections count: %d \t Total memory allocated: %d bytes\n", gCollector->sectionCount,
           gCollector->byteCount);
}

void *StackBottom() {
    int x = 1;
    int *ptr = &x + 0;
    return ptr;
}

void *GetStackBottom(void) {
    jmp_buf env;
    setjmp(env);
    void *(*volatile f)(void) = StackBottom;
    return f();
}

struct ScanHeapClosure {
    void *minAddr, *maxAddr;
    RecordMap *possibleRefs;
};


void ScanHeapTraverser(RecordEntry *record, void *closure) {
    struct ScanHeapClosure *sClosure = (struct ScanHeapClosure *) closure;
    if (record->mallocSize < 8)
        return;
    char *endAddr = (char *) record->mallocAddr + record->mallocSize;
    for (void **current = record->mallocAddr; (char *) current < endAddr; current++) {
        void *ref = *current;
        if (ref < sClosure->minAddr || ref > sClosure->maxAddr)
            continue;
        RecordEntry entry = {ref, 0, 0, 0};
        AddRecord(sClosure->possibleRefs, &entry, false);
    }
}

void ReferencedMarkTraverser(RecordEntry *record, void *closure) {
    //printf("Check mark for: %p\n", record->mallocAddr);
    RecordMap *possibleRefs = (RecordMap *) closure;
    if (GetRecord(possibleRefs, record->mallocAddr) != NULL) {
        record->isInUse = true;
        //printf("%p is in use\n", record->mallocAddr);
    } else {
        record->isInUse = false;
    }
}

void GCMark(GCollector *gCollector) {
    void **stackTop = gCollector->FrameTop;
    void **stackBot = GetStackBottom();
    RecordMap possibleRefs;
    InitRecordMap(&possibleRefs);
    for (void **current = stackTop; current > stackBot; current--) {
        void *ref = *current;
        if (ref < gCollector->minAddr || ref > gCollector->maxAddr)
            continue;
        RecordEntry entry = {ref, 0, 0, 0};
        AddRecord(&possibleRefs, &entry, false);
        //printf("Found [%p] @ [%p]\n", ref, current);
    }
    struct ScanHeapClosure closure = {gCollector->minAddr, gCollector->maxAddr, &possibleRefs};
    TraverseRecordMap(&gCollector->records, ScanHeapTraverser, &closure);
    TraverseRecordMap(&gCollector->records, ReferencedMarkTraverser, &possibleRefs);
    FreeRecordMap(&possibleRefs);
}

void SweepTraverser(RecordEntry *record, void *closure);

void GCSweep(GCollector *gCollector) {
    TraverseRecordMap(&gCollector->records, SweepTraverser, gCollector);
}

void GCRun(GCollector *gCollector) {
    GCMark(gCollector);
    GCSweep(gCollector);
}

void *GCMalloc(GCollector *gCollector, size_t size) {
    if (gCollector->byteCount > gCollector->collectThreshold)
        GCRun(gCollector);
    void *ptr = malloc(size);
    if (ptr == NULL)
        return NULL;
    RecordEntry record = {ptr, size, false, 0};
    AddRecord(&gCollector->records, &record, false);
    if (ptr < gCollector->minAddr || gCollector->minAddr == 0)
        gCollector->minAddr = ptr;
    if (ptr > gCollector->maxAddr || gCollector->maxAddr == 0)
        gCollector->maxAddr = ptr;
    gCollector->sectionCount += 1;
    gCollector->byteCount += size;
    return ptr;
}

void GCFree(GCollector *gCollector, void *ptr) {
    if (ptr == NULL)
        return;
    RecordEntry *entry = GetRecord(&gCollector->records, ptr);
    if (entry == NULL)
        return;
    if (ptr == gCollector->minAddr)
        gCollector->minAddr += entry->mallocSize;
    if (ptr == gCollector->maxAddr)
        gCollector->maxAddr -= 1;
    gCollector->sectionCount -= 1;
    gCollector->byteCount -= entry->mallocSize;
    printf("Free %d bytes @ [%p]\n", entry->mallocSize, entry->mallocAddr);
    RemoveRecord(&gCollector->records, ptr);
    free(ptr);
}

void SweepTraverser(RecordEntry *record, void *closure) {
    GCollector *gCollector = (GCollector *) closure;
    if (!record->isInUse)
        GCFree(gCollector, record->mallocAddr);
    else
        record->isInUse = false;
}

static void testFunction() {
    char *string = GCMalloc(&gc, 50);
    for (int i = 0; i < 50; ++i) {
        string[i] = 'H';
    }
    printf("Trash address: %p\n", string);
}

int main(int argc, char *argv[]) {
    GCInit(&gc, &argc);

    testFunction();
    testFunction();
    GCRun(&gc);

    GCEnd(&gc);
}
