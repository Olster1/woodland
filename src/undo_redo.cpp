enum UndoRedo_BlockType {
    UNDO_REDO_INSERT,
    UNDO_REDO_DELETE
};

struct UndoRedoBlock {
    UndoRedo_BlockType type;

    size_t byteAt;
    char *string;
    int stringLength;

    u32 id; //id auto increment for every block
    s32 groupId; //NOTE: If undo redo operations should be batched together 

    size_t cursorAt; //NOTE: Save the cursor position

};

struct UndoRedoState {
    int at_in_history; //NOTE where we are in the history array - index
    int block_count;
    int total_block_count;
    UndoRedoBlock *history;

    u32 idAt; //id to give to blocks, increments each block, must start at 1 not 0

    s32 groupIdAt; //id to give to blocks if they are grouped, increments each block so can start at 0
    //NOTE: -1 for no group
};

static void init_undo_redo_state(UndoRedoState *state) {
    state->idAt = 0;
    state->groupIdAt = 0;
    state->at_in_history = 0;
    state->block_count = 0;
    state->total_block_count = 64;
    state->history = (UndoRedoBlock *)easyPlatform_allocateMemory(state->total_block_count*sizeof(UndoRedoBlock), EASY_PLATFORM_MEMORY_ZERO);
}

static void push_block(UndoRedoState *state, UndoRedo_BlockType type, size_t byteAt, char *string, int stringLength, size_t cursorAt, s32 groupId = -1) {
    
    //NOTE: Free strings if the blocks ahead are getting deleted 
    for(int i = state->at_in_history; i < state->block_count; ++i) {
        UndoRedoBlock *block = &state->history[i];
        easyPlatform_freeMemory(block->string);
    }

    state->block_count = state->at_in_history;

    //NOTE: If the block history is full, realloc memory
    if(state->block_count >= state->total_block_count) {
        state->total_block_count += 32;
        state->history =(UndoRedoBlock *)easyPlatform_reallocMemory(state->history, state->block_count*sizeof(UndoRedoBlock), state->total_block_count*sizeof(UndoRedoBlock));
    }

    UndoRedoBlock block = {};
    block.type = type;
    block.byteAt = byteAt;
    block.string = string;
    block.stringLength = stringLength;
    block.id = ++state->idAt; //NOTE: Increment before so it starts at 1
    block.groupId = groupId;
    block.cursorAt = cursorAt;

    state->history[state->block_count++] = block;
    state->at_in_history = state->block_count;
}

static UndoRedoBlock *get_undo_block(UndoRedoState *state) {
    UndoRedoBlock *result = NULL;
    if(state->at_in_history > 0) {
        result = &state->history[--state->at_in_history];
    }
    return result;
}

static UndoRedoBlock *see_undo_block(UndoRedoState *state) {
    UndoRedoBlock *result = NULL;
    if(state->at_in_history > 0) {
        result = &state->history[state->at_in_history];
    }
    return result;
}

static UndoRedoBlock *see_redo_block(UndoRedoState *state) {
    
    UndoRedoBlock *result = NULL;
    if(state->at_in_history < state->block_count) {
        result = &state->history[state->at_in_history];
    }
    return result;
}

static UndoRedoBlock *get_redo_block(UndoRedoState *state) {
    
    UndoRedoBlock *result = NULL;
    if(state->at_in_history < state->block_count) {
        result = &state->history[state->at_in_history++];
    }
    return result;
}