
typedef struct {
	int cursorAt_inBytes;
	int markAt_inBytes;
	bool markActive;

	u32 bufferSize_inUse_inBytes;
	u32 bufferSize_inBytes;
	u8 *bufferMemory;


	//NOTE: Use a gap buffer for temporarily 
	u32 gapBuffer_endAt;
	u32 gapBuffer_startAt;

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
}

static void wl_emptyBuffer(WL_Buffer *b) {

	platform_free_memory(b->bufferMemory);

	memset(b, 0, sizeof(WL_Buffer));

}

static void makeGapBuffer_(WL_Buffer *b, int byteStart, int gapSize) {

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

static void addTextToBuffer(WL_Buffer *b, char *str, int indexStart) {

	u32 strSize_inBytes = easyString_getSizeInBytes_utf8(str); 

	if(strSize_inBytes > 0) {

		int gapBufferSize = b->gapBuffer_endAt - b->gapBuffer_startAt;

		if(gapBufferSize < strSize_inBytes) {
			//NOTE: Make gap buffer bigger
			makeGapBuffer_(b, indexStart, max(strSize_inBytes, GAP_BUFFER_SIZE_IN_BYTES));
			assert((b->gapBuffer_endAt - b->gapBuffer_startAt) >= strSize_inBytes);
		}

		//NOTE: Now add the string to the buffer
		for(int i = 0; i < strSize_inBytes; ++i) {
			b->bufferMemory[b->gapBuffer_startAt + i] = str[i];
			assert(b->gapBuffer_startAt + i < b->gapBuffer_endAt);
		}

		b->gapBuffer_startAt += strSize_inBytes;
		b->cursorAt_inBytes += strSize_inBytes;
	}
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


static void removeTextFromBuffer(WL_Buffer *b, int bytesStart, int toRemoveCount_inBytes) {
	{	
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
	}
}

#define WRITE_GAP_BUFFER_AS_HASH 0

struct Compiled_Buffer_For_Drawing {
	size_t size_in_bytes;
	u8 *memory;

	size_t cursor_at;
	size_t shift_begin;
	size_t shift_end;
};


static Compiled_Buffer_For_Drawing compileBuffer_toDraw(WL_Buffer *b, Memory_Arena *tempArena) {
	Compiled_Buffer_For_Drawing result = {};

	bool wroteCursor = false;
	result.memory = (u8 *)pushSize(tempArena, b->bufferSize_inBytes + 1); //NOTE: For null terminator and cursor spot
	int at = 0;

	for(int i = 0; i < b->bufferSize_inUse_inBytes; i++) {
		
		if(i == b->cursorAt_inBytes) { result.cursor_at = result.size_in_bytes; wroteCursor = true; }

		if(i >= b->gapBuffer_startAt && i < b->gapBuffer_endAt) {
			//NOTE: In Gap Buffer
		} else {
			result.memory[result.size_in_bytes++] = b->bufferMemory[i];
		}
	}


	if(!wroteCursor) {
		result.cursor_at = result.size_in_bytes;
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
	for(int i = 0; i < b->bufferSize_inUse_inBytes && !found; i++) {

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