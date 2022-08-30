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

};

struct UndoRedoState {
    int at_in_history; //NOTE where we are in the history array - index
    int block_count;
    int total_block_count;
    UndoRedoBlock *history;

    u32 idAt; //id to give to blocks, increments each block, must start at 1 not 0
};

static void init_undo_redo_state(UndoRedoState *state) {
    state->idAt = 0;
    state->at_in_history = 0;
    state->block_count = 0;
    state->total_block_count = 64;
    state->history = (UndoRedoBlock *)easyPlatform_allocateMemory(state->total_block_count*sizeof(UndoRedoBlock), EASY_PLATFORM_MEMORY_ZERO);
}

static void push_block(UndoRedoState *state, UndoRedo_BlockType type, size_t byteAt, char *string, int stringLength) {
    state->block_count = state->at_in_history;

    //TODO: Free strings if the blocks ahead are getting deleted 

    if(state->block_count >= state->total_block_count) {
        state->total_block_count += 32;
        state->history =(UndoRedoBlock *)easyPlatform_reallocMemory(state->history, state->block_count*sizeof(UndoRedoBlock), state->total_block_count*sizeof(UndoRedoBlock));
    }

    UndoRedoBlock block = {};
    block.type = type;
    block.byteAt = byteAt;
    block.string = string;
    block.stringLength = stringLength;
    block.id = ++state->idAt; //

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


static UndoRedoBlock *get_redo_block(UndoRedoState *state) {
    
    UndoRedoBlock *result = NULL;
    if(state->at_in_history < state->block_count) {
        result = &state->history[state->at_in_history++];
    }
    return result;
}