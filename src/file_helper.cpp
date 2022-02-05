char *getFileLastPortion_(char *buffer, int bufferLen, char *at, Memory_Arena *arena) {
    char *recent = at;
    while(*at) {
        if((*at == '/' || (*at == '\\'))&& at[1] != '\0') { 
            recent = (at + 1); //plus 1 to pass the slash
        }
        at++;
    }
    
    char *result = buffer;
    int length = (int)(at - recent) + 1; //for null termination
    if(!result) {
        if(arena) {
            result = (char *)pushArray(arena, length, char);
        } else {
            result = (char *)easyPlatform_allocateMemory(length, EASY_PLATFORM_MEMORY_ZERO);    
        }
        
    } else {
        assert(bufferLen >= length);
        buffer[length] = '\0'; //null terminate. 
    }
    
    easyPlatform_copyMemory(result, recent, length - 1);
    result[length - 1] = '\0';
    
    return result;
}
#define getFileLastPortion(at) getFileLastPortion_(0, 0, at, 0)
#define getFileLastPortionWithBuffer(buffer, bufferLen, at) getFileLastPortion_(buffer, bufferLen, at, 0)
#define getFileLastPortionWithArena(at, arena) getFileLastPortion_(0, 0, at, arena)