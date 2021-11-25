#include "wl_buffer.cpp"

#define MAX_WINDOW_COUNT 16

typedef enum {
	MODE_EDIT_BUFFER,
} EditorMode;

typedef struct {
	char *name;
	bool hasSaved;

	WL_Buffer buffer;
} WL_Window;

typedef struct {
	bool initialized;

	Renderer renderer;

	u32 bufferAt;
	WL_Window windows[MAX_WINDOW_COUNT];

	EditorMode mode;

} EditorState;



static void updateEditor() {
	EditorState *editorState = (EditorState *)global_platform.permanent_storage;
	assert(sizeof(EditorState) < global_platform.permanent_storage_size);
	if(!editorState->initialized) {


		
		editorState->initialized = true;

		initRenderer(&editorState->renderer);

		editorState->bufferAt = 0;
		editorState->mode = MODE_EDIT_BUFFER;
	}

	WL_Window *w = &editorState->windows[editorState->bufferAt];

	WL_Buffer *b = &w->buffer;

	switch(editorState->mode) {
		case MODE_EDIT_BUFFER: {
			//NOTE: Any text added
			addTextToBuffer(b, (char *)global_platformInput.textInput_utf8, b->cursorAt_inBytes);

			// OutputDebugStringA((char *)global_platformInput.textInput_utf8);

			//NOTE: Process our command buffer
			for(int i = 0; i < global_platformInput.keyInputCommand_count; ++i) {
			    PlatformKeyType command = global_platformInput.keyInputCommandBuffer[i];
			    if(command == PLATFORM_KEY_BACKSPACE && b->cursorAt_inBytes > 0) {
			       u32 bytesOfPrevRune = 1;
			       removeTextFromBuffer(b, b->cursorAt_inBytes - 1, bytesOfPrevRune);
			       b->cursorAt_inBytes -= bytesOfPrevRune;
			        
			    }

			    if(command == PLATFORM_KEY_LEFT) {
			    	endGapBuffer(b);

			        //NOTE: Move cursor left 
			        if(b->cursorAt_inBytes > 0) {
			        	
			        	

			        	u32 bytesOfPrevRune = 1;
			            b->cursorAt_inBytes -= bytesOfPrevRune;

			            
			        }
			    }

			    if(command == PLATFORM_KEY_RIGHT) {
			    	u32 bytesOfNextRune = 1;
			    	endGapBuffer(b);
			        //NOTE: Move cursor right 
			        if(b->cursorAt_inBytes < b->bufferSize_inUse_inBytes) {
			            b->cursorAt_inBytes += bytesOfNextRune;
			        }
			    }       
			}

			//NOTE: Render character buffer now
			

		} break;
	}

	
	u8 *str = compileBuffer_toNullTerminateString(b);
	OutputDebugStringA((char *)str);
	OutputDebugStringA((char *)"\n");
	Win32HeapFree(str);

	//////////////////////////////////////////////////////// 

	if(global_platformInput.keyStates[PLATFORM_KEY_LEFT_CTRL].isDown) {
		// OutputDebugStringA((char *)"Ctrl Down");
	}

	if(global_platformInput.keyStates[PLATFORM_KEY_O].pressedCount > 0) {
		OutputDebugStringA((char *)"O Pressed\n");
	}

	if(global_platformInput.keyStates[PLATFORM_KEY_O].isDown) {
		OutputDebugStringA((char *)"O down\n");
	}

	if(global_platformInput.keyStates[PLATFORM_KEY_O].releasedCount > 0) {
		OutputDebugStringA((char *)"O released\n");
	}

	//NOTE: Ctrl + O -> open file 
	if(global_platformInput.keyStates[PLATFORM_KEY_LEFT_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_O].pressedCount > 0) 
	{	
		//NOTE: Make sure free the string
		char *fileNameToOpen = (char *)Platform_OpenFile_withDialog_wideChar_haveToFree();

		size_t data_size = 0;
		void *data = 0;
		if(Platform_LoadEntireFile_wideChar(fileNameToOpen, &data, &data_size)) {
			int i = 0;

			// addTextToBuffer(b, (char *)data, b->cursorAt_inBytes);

			Win32FreeFileData(data);

		} else {
			assert(!"Couldn't open file");
		}

		Win32HeapFree(fileNameToOpen);
	}

}