////NOTE: MEMORY FUNCTIONS
#ifndef EASY_PLATFORM_H
/*
Code for platform depedent functions 
*/

typedef enum {
    EASY_PLATFORM_MEMORY_NONE,
    EASY_PLATFORM_MEMORY_ZERO,
} EasyPlatform_MemoryFlag;

static void *easyPlatform_allocateMemory(u32 sizeInBytes, EasyPlatform_MemoryFlag flags) {
    
    void *result = 0;
    
    result = HeapAlloc(GetProcessHeap(), 0, sizeInBytes);
    
    if(!result) {
        // easyLogger_addLog("Platform out of memory on heap allocate!");
    }
    
    if(flags & EASY_PLATFORM_MEMORY_ZERO) {
        // easyMemory_zeroSize(result, sizeInBytes);
    }
    
    return result;
}

static void easyPlatform_freeMemory(void *memory) {
    HeapFree(GetProcessHeap(), 0, memory);
}


static inline void easyPlatform_copyMemory(void *to, void *from, u32 sizeInBytes) {
    memcpy(to, from, sizeInBytes);
}

static inline u8 * easyPlatform_reallocMemory(void *from, u32 oldSize, u32 newSize) {
    u8 *result = (u8 *)easyPlatform_allocateMemory(newSize, EASY_PLATFORM_MEMORY_ZERO);

    easyPlatform_copyMemory(result, from, oldSize);

    easyPlatform_freeMemory(from);

    return result;
}

#define EASY_PLATFORM_H 1
#endif


/////////////////////////

