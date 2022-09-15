enum BufferControllerOption {
    BUFFER_ALL,
    BUFFER_SIMPLE //NOTE: Single line text box so you can't move up and down or add newline to buffer
};

typedef struct {
	s64 cursorAt_inBytes;
	s64 markAt_inBytes;
	bool markActive;

	u32 bufferSize_inUse_inBytes;
	u32 bufferSize_inBytes;
	//NOTE: This DOESN'T include a null terminator 
	u8 *bufferMemory;


	//NOTE: Use a gap buffer for temporarily 
	u32 gapBuffer_endAt;
	u32 gapBuffer_startAt;

	UndoRedoState undo_redo_state;

} WL_Buffer;

#define GAP_BUFFER_SIZE_IN_BYTES 2

/*
Functions to use: 

//NOTE: Whenever you want to add text to it
addTextToBuffer(buffer, stringToAdd, start);

//NOTE: Whenever you move the cursor, you'll want to end the gap buffer
endGapBuffer(buffer); 

//NOTE: Whenever you want to remove text from the buffer
removeTextFromBuffer(buffer, bytesStart, toRemoveCount_inBytes)

*/

static void initBuffer(WL_Buffer *b) {
	memset(b, 0, sizeof(WL_Buffer));

	init_undo_redo_state(&b->undo_redo_state);
}

static void wl_emptyBuffer(WL_Buffer *b) {

	platform_free_memory(b->bufferMemory);

	memset(b, 0, sizeof(WL_Buffer));

}

static void makeGapBuffer_(WL_Buffer *b, int byteStart, int gapSize) {

	/*Gap Buffer always assumes the start of the gap buffer is at the cursor position

	*/

	if(!b->bufferMemory) {
		b->bufferMemory = (u8 *)platform_alloc_memory(gapSize, true);
		b->bufferSize_inBytes = gapSize;
	}

	//NOTE: Make total buffer bigger if full
	if((b->bufferSize_inUse_inBytes + gapSize) > b->bufferSize_inBytes) {

		b->bufferMemory = (u8 *)platform_realloc_memory(b->bufferMemory, b->bufferSize_inUse_inBytes, b->bufferSize_inBytes + gapSize);
		b->bufferSize_inBytes += gapSize;
		
	} 

	//NOTE: Move everything down to make room in the buffer for the gap
	for(int i = (b->bufferSize_inUse_inBytes - 1); i >= byteStart; i--) {
		b->bufferMemory[i + gapSize] = b->bufferMemory[i];
	}
	b->gapBuffer_startAt = byteStart;
	b->gapBuffer_endAt = b->gapBuffer_startAt + gapSize; 

	b->bufferSize_inUse_inBytes += gapSize;
}


//NOTE: We end the gap buffer whenever we move the cursor
static void endGapBuffer(WL_Buffer *b) {
	//NOTE: Remove gap that was open
	if(b->gapBuffer_startAt < b->gapBuffer_endAt) {
		int gapDiff = b->gapBuffer_endAt - b->gapBuffer_startAt;

		//NOTE: Move all memory down to fill the gap
		for(int i = 0; i < (b->bufferSize_inUse_inBytes - b->gapBuffer_endAt); i++) {
			b->bufferMemory[b->gapBuffer_startAt + i] = b->bufferMemory[b->gapBuffer_startAt + gapDiff + i];
		}

		//NOTE: remove empty room on buffer
		b->bufferSize_inUse_inBytes -= gapDiff;

		b->gapBuffer_endAt = 0;
		b->gapBuffer_startAt = 0;

	} else {
		assert(b->gapBuffer_startAt == b->gapBuffer_endAt);
	}

}


static void addTextToBuffer(WL_Buffer *b, char *str, int indexStart, bool should_add_to_history = true, s32 groupId = -1) {

	//NOTE: Always make sure we end any gap buffers
	if(indexStart != b->cursorAt_inBytes) {
		endGapBuffer(b);
	}

	u32 strSize_inBytes = easyString_getSizeInBytes_utf8(str); 

	if(strSize_inBytes > 0) {

		if(should_add_to_history) {
			push_block(&b->undo_redo_state, UNDO_REDO_INSERT, b->cursorAt_inBytes, nullTerminate(str, strSize_inBytes), strSize_inBytes, b->cursorAt_inBytes, groupId);
		}
		
		int gapBufferSize = (int)b->gapBuffer_endAt - (int)b->gapBuffer_startAt;

		if(gapBufferSize < strSize_inBytes) {
			//NOTE: Make gap buffer bigger
			makeGapBuffer_(b, indexStart, max(strSize_inBytes, 0)); //GAP_BUFFER_SIZE_IN_BYTES
			assert((b->gapBuffer_endAt - b->gapBuffer_startAt) >= strSize_inBytes);
		}

		//NOTE: Now add the string to the buffer
		for(int i = 0; i < strSize_inBytes; ++i) {
			b->bufferMemory[b->gapBuffer_startAt + i] = str[i];
			assert(b->gapBuffer_startAt + i < b->gapBuffer_endAt);
		}

		b->gapBuffer_startAt += strSize_inBytes;
		b->cursorAt_inBytes = indexStart + strSize_inBytes;
		
	}
}


static void removeTextFromBuffer(WL_Buffer *b, int bytesStart, int toRemoveCount_inBytes, bool should_add_to_history = true, s32 groupId = -1) {
	
	if(bytesStart != b->cursorAt_inBytes) {
		endGapBuffer(b);
	}

	//NOTE: See if has an active gap
	int gapDiff = b->gapBuffer_endAt - b->gapBuffer_startAt;
	
	//NOTE: No active gap
	if(gapDiff == 0) {
		b->gapBuffer_startAt = bytesStart; 
		b->gapBuffer_endAt = bytesStart + toRemoveCount_inBytes;
	} else {
		assert((b->gapBuffer_startAt - toRemoveCount_inBytes) >= 0);
		b->gapBuffer_startAt -= toRemoveCount_inBytes;
	}

	if(should_add_to_history) {
		//NOTE: only add if this is a new command, not a repeat of the text 
		push_block(&b->undo_redo_state, UNDO_REDO_DELETE, b->gapBuffer_startAt, nullTerminate((char *)(b->bufferMemory + b->gapBuffer_startAt), toRemoveCount_inBytes), toRemoveCount_inBytes, b->cursorAt_inBytes, groupId);
	} 

	 b->cursorAt_inBytes = bytesStart;
	
}

#define WRITE_GAP_BUFFER_AS_HASH 0

struct Compiled_Buffer_For_Drawing {
	size_t size_in_bytes;
	u8 *memory;

	size_t cursor_at;
	size_t shift_begin;
	size_t shift_end;
};


static Compiled_Buffer_For_Drawing compileBuffer_toDraw(WL_Buffer *b, Memory_Arena *tempArena, Selectable_State *selectState) {
	Compiled_Buffer_For_Drawing result = {};

	bool wroteEndSelect = false;
	bool wroteStartSelect = false;

	bool wroteCursor = false;
	result.memory = (u8 *)pushSize(tempArena, b->bufferSize_inBytes + 1); //NOTE: For null terminator and cursor spot
	int at = 0;

	for(int i = 0; i < b->bufferSize_inUse_inBytes; i++) {
		
		if(i == b->cursorAt_inBytes) { result.cursor_at = result.size_in_bytes; wroteCursor = true; }

		if(i == selectState->start_offset_in_bytes) { result.shift_begin = result.size_in_bytes; wroteStartSelect = true; }

		if(i == selectState->end_offset_in_bytes) { result.shift_end = result.size_in_bytes; wroteEndSelect = true; }

		if(i >= b->gapBuffer_startAt && i < b->gapBuffer_endAt) {
			//NOTE: In Gap Buffer
		} else {
			result.memory[result.size_in_bytes++] = b->bufferMemory[i];
		}
	}

	if(!wroteStartSelect) {
		result.shift_begin = result.size_in_bytes;
	}

	if(!wroteEndSelect) {
		result.shift_end = result.size_in_bytes;
	}

	if(!wroteCursor) {
		result.cursor_at = result.size_in_bytes;
	}

	//NOTE: make begin always first
	if(result.shift_end < result.shift_begin) {
		size_t temp = result.shift_begin;
		result.shift_begin = result.shift_end;
		result.shift_end = temp;
	}

	result.memory[result.size_in_bytes] = '\0'; //null terminate the buffer


	return result;
}

struct Compiled_Buffer_For_Save {
	size_t size_in_bytes;
	u8 *memory;
};


static Compiled_Buffer_For_Save compile_buffer_to_save_format(WL_Buffer *b, Memory_Arena *temp_arena) {
	Compiled_Buffer_For_Save result = {};

	result.memory = (u8 *)pushSize(temp_arena, b->bufferSize_inBytes); //NOTE: Have to think about this more if using a chunked linked list format for the buffer 



	for(int i = 0; i < b->bufferSize_inUse_inBytes; i++) {
		
		if(i >= b->gapBuffer_startAt && i < b->gapBuffer_endAt) {
			//NOTE: In Gap Buffer, don't write 
		} else {
			result.memory[result.size_in_bytes++] = b->bufferMemory[i];
		}
	}



	return result;
}

static size_t convert_compiled_byte_point_to_buffer_byte_point(WL_Buffer *b, size_t cursor_byte_in_compiled) {

	size_t result = 0;

	bool found = false;

	size_t byte_at = 0;
	for(int i = 0; i <= b->bufferSize_inUse_inBytes && !found; i++) {

		if(byte_at == cursor_byte_in_compiled) {
			result = i;
			found = true;
			break;
		}
		
		if(i >= b->gapBuffer_startAt && i < b->gapBuffer_endAt) {
			//NOTE: In Gap Buffer, don't write 
		} else {
			byte_at++;
		}
	}

	assert(result >= 0);

	return result;
}

//NOTE: To protect the integrity of the undo-redo buffer, we put this pretiffy into the undero-redo state aswell. 
//		Except we group them as one contigous undo-redo 
static void prettify_buffer(WL_Buffer *b) {
	endGapBuffer(b);

	//NOTE: Start the file with a new line
	bool hitNewLine = true;

	int byteAt = 0;

	s32 currentGroupId = b->undo_redo_state.groupIdAt++;

	int depthAt = 0;

	char *str = (char *)(b->bufferMemory);

	while (*str) {
		u8 byte = str[0];

		int incrementCount = 1;

		if(hitNewLine && (byte == ' ' || byte == '\t')) {
			//NOTE: We eat tabs and spaces if we are on a new line 
			//		This removes them then we add tabs in when we hit a glyph 
			char *temp = str;

			while(*str && str[0] == ' ' || str[0] == '\t') { //NOTE: Find how much white space we had
				str++;
			}
			
			char *start = (char *)(b->bufferMemory);
			removeTextFromBuffer(b, temp - start, (str - temp));

			endGapBuffer(b);
			
			//NOTE: because we modify the buffer we set rebase the pointer we're looping through
			str = (char *)(b->bufferMemory + b->cursorAt_inBytes);

			incrementCount = 0;
		} else {
			if(byte == '{') {
				depthAt++;
			} else if(byte == '}') {
				depthAt--;
				if(depthAt < 0) { depthAt = 0; }
			}
			
			if(hitNewLine) {
				char *temp = 0;
				for(int i = 0; i < depthAt; i++) {

					if(!temp) {
						temp = "\t";
					} else {
						//NOTE: Add in the tabs
						temp = easy_createString_printf(&globalPerFrameArena, "%s%s", temp, "\t");
					}
				}
				if(temp) {
					char *start = (char *)(b->bufferMemory);
					addTextToBuffer(b, temp, (str - start), true, currentGroupId);
					endGapBuffer(b);
			
					//NOTE: because we modify the buffer we set rebase the pointer we're looping through
					str = (char *)(b->bufferMemory + b->cursorAt_inBytes);

					incrementCount = 0;
				}
			}

			hitNewLine = false;

			if(byte == '\n' || byte == '\r') {
				//NOTE: We're at a new line so we have to indent, but only if we hit some new text
				hitNewLine = true;
			} 
		}

		str += incrementCount;
	}

}