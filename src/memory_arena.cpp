#define easyMemory_zeroStruct(memory, type) memset(memory, 0, sizeof(type))

typedef struct MemoryPiece MemoryPiece;
typedef struct MemoryPiece {
    void *memory;
    size_t totalSize; //size of just this memory block
    // size_t totalSizeOfArena; //size of total arena to roll back with
    size_t currentSize;

    MemoryPiece *next;

} MemoryPiece; //this is for the memory to remember 

typedef struct {
    //NOTE: everything is in pieces now
    // void *memory;
    // unsigned int totalSize; //include all memory blocks
    // unsigned int totalCurrentSize;//total current size of all memory blocks
    int markCount;

    MemoryPiece *pieces; //actual details in the memory block
    MemoryPiece *piecesFreeList;

} Memory_Arena;


static size_t DEBUG_get_total_arena_size(Memory_Arena *arena) {
    MemoryPiece *p = arena->pieces;
    
    size_t result = 0;
    while(p) {
        result += p->totalSize;

        p = p->next;
    }
    return result;
}

#define pushStruct(arena, type) (type *)pushSize(arena, sizeof(type))

#define pushArray(arena, size, type) (type *)pushSize(arena, sizeof(type)*size)

void *pushSize(Memory_Arena *arena, size_t size) {
    if(!arena->pieces || ((arena->pieces->currentSize + size) > arena->pieces->totalSize)){ //doesn't fit in arena
        MemoryPiece *piece = arena->piecesFreeList; //get one of the free list

        size_t extension = max(Kilobytes(1028), size);
        if(piece)  {
            MemoryPiece **piecePtr = &arena->piecesFreeList;
            assert(piece->totalSize > 0);
            while(piece && piece->totalSize < extension) {//find the right size piece. 
                piecePtr = &piece->next; 
                piece = piece->next;
            }
            if(piece) {
                //take off list
                *piecePtr = piece->next;             
                piece->currentSize = 0;
            }
            
        } 

        if(!piece) {//need to allocate a new piece

            //TODO: Change this to virtual alloc page sizes and put the 'MemoryPiece' as a header to the virtual alloc block
            u8 *memory_u8 = (u8 *)platform_alloc_memory_pages(extension + sizeof(MemoryPiece));
            piece = (MemoryPiece *)memory_u8;
            piece->memory = memory_u8 + sizeof(MemoryPiece);
            piece->totalSize = extension;
            piece->currentSize = 0;
        }
        assert(piece);
        assert(piece->memory);
        assert(piece->totalSize > 0);
        assert(piece->currentSize == 0);

        //stick on list
        piece->next = arena->pieces;
        arena->pieces = piece;

        // piece->totalSizeOfArena = arena->totalSize;
        // assert((arena->currentSize_ + size) <= arena->totalSize); 
    }

    MemoryPiece *piece = arena->pieces;

    assert(piece);
    assert((piece->currentSize + size) <= piece->totalSize); 
    
    void *result = ((u8 *)piece->memory) + piece->currentSize;
    piece->currentSize += size;
    
    memset(result, 0, size);
    return result;
}

Memory_Arena initMemoryArena(size_t size) {
    Memory_Arena result = {};
    pushSize(&result, size);
    assert(result.pieces);
    assert(result.pieces->memory);
    result.pieces->currentSize = 0;
    return result;
}

Memory_Arena initMemoryArena_withMemory(void *memory, size_t totalSize) {
    Memory_Arena arena = {};

    MemoryPiece *piece = (MemoryPiece *)memory;

    u8 *memory_u8 = (u8 *)memory;

    piece->memory = memory_u8 + sizeof(MemoryPiece);
    piece->totalSize = totalSize - sizeof(MemoryPiece);
    piece->currentSize = 0;

    assert(piece);
    assert(piece->memory);
    assert(piece->totalSize > 0);
    assert(piece->currentSize == 0);

    //stick on list
    piece->next = arena.pieces;
    arena.pieces = piece;

    return arena;
}

// Arena easyArena_subDivideArena(Arena *parentArena, size_t size) {
    
// }

typedef struct { 
    int id;
    Memory_Arena *arena;
    size_t memAt; //the actuall value we roll back, don't need to do anything else
    MemoryPiece *piece;
} MemoryArenaMark;

MemoryArenaMark takeMemoryMark(Memory_Arena *arena) {
    MemoryArenaMark result = {};
    result.arena = arena;
    result.memAt = arena->pieces->currentSize;
    result.id = arena->markCount++;
    result.piece = arena->pieces;
    return result;
}

void releaseMemoryMark(MemoryArenaMark *mark) {
    mark->arena->markCount--;
    Memory_Arena *arena = mark->arena;
    assert(mark->id == arena->markCount);
    assert(arena->markCount >= 0);
    assert(arena->pieces);
    //all ways the top piece is the current memory block for the arena. 
    MemoryPiece *piece = arena->pieces;
    if(mark->piece != piece) {
        //not on the same memory block
        bool found = false;
        while(!found) {
            piece = arena->pieces;
            if(piece == mark->piece) {
                //found the right one
                found = true;
                break;
            } else {
                arena->pieces = piece->next;
                assert(arena->pieces);
                //put on free list
                piece->next = arena->piecesFreeList;
                arena->piecesFreeList = piece;
            }
        }
        assert(found);
    } 
    assert(arena->pieces == mark->piece);
    //roll back size
    piece->currentSize = mark->memAt;
    assert(piece->currentSize <= piece->totalSize);
}

static Memory_Arena globalPerFrameArena;
static MemoryArenaMark global_perFrameArenaMark;


char *nullTerminateBuffer(char *result, char *string, int length) {
    for(int i = 0; i < length; ++i) {
        result[i]= string[i];
    }
    result[length] = '\0';
    return result;
}

#define nullTerminate(string, length) nullTerminateBuffer((char *)platform_alloc_memory(length + 1, false), string, length)
#define nullTerminateArena(string, length, arena) nullTerminateBuffer((char *)pushArray(arena, length + 1, char), string, length)
