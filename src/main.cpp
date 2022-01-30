#include "wl_buffer.cpp"
#include "font.cpp"

/*
Next:

1. Get glyphs to actually draw with the blend mode

*/

#define MAX_WINDOW_COUNT 16

typedef enum {
	MODE_EDIT_BUFFER,
} EditorMode;

typedef struct {
	char *name;
	bool is_up_to_date;

	Rect2f bounds;

	WL_Buffer buffer;
} WL_Window;

#include "wl_window.cpp"

typedef struct {
	bool initialized;

	Renderer renderer;

	u32 active_buffer_index;
	WL_Window windows[MAX_WINDOW_COUNT];

	EditorMode mode;

	Font font;

} EditorState;



static EditorState *updateEditor(float dt, float windowWidth, float windowHeight) {
	EditorState *editorState = (EditorState *)global_platform.permanent_storage;
	assert(sizeof(EditorState) < global_platform.permanent_storage_size);
	if(!editorState->initialized) {
		
		editorState->initialized = true;

		initRenderer(&editorState->renderer);

		editorState->active_buffer_index = 0;
		editorState->mode = MODE_EDIT_BUFFER;

		editorState->font = initFont("..\\fonts\\SpaceMono.ttf");

		//NOTE: Set up the first buffer
		WL_Window *w = &editorState->windows[0];
		initBuffer(&w->buffer);
		w->name = "untitled";
		w->is_up_to_date = true;
		w->bounds = make_rect2f(0, 0, windowWidth, windowHeight);

	}

	Renderer *renderer = &editorState->renderer;

	//NOTE: Clear the renderer out so we can start again
	clearRenderer(renderer);

	pushViewport(renderer, make_float4(0, 0, 0, 0));
	renderer_defaultScissors(renderer, windowWidth, windowHeight);
	pushClearColor(renderer, make_float4(1, 0.5f, 0, 1)); 

	switch(editorState->mode) {
		case MODE_EDIT_BUFFER: {	

			for(int i = 0; i < 1/*MAX_WINDOW_COUNT*/; ++i) {
				WL_Window *w = &editorState->windows[i];

				WL_Buffer *b = &w->buffer;

				bool is_active = (i == editorState->active_buffer_index);

				if(is_active) 
				{

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
				}

				

				draw_wl_window(w, renderer, is_active, windowWidth, windowHeight, editorState->font);
			}
		} break;
	}

	
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


	

	return editorState;

}