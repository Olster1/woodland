#include "memory_arena.cpp"
#include "lex_utf8.h"
#include "color.cpp"
#include "wl_buffer.cpp"
#include "font.cpp"
#include "ui.cpp"

/*
Next:

1. Change to windows referencing buffer
2. Convert lex.cpp to use runes instead of chars
3. Create new windows and be able to resize them

*/

#define MAX_WINDOW_COUNT 8
#define MAX_BUFFER_COUNT 256 //TODO: Allow user to open unlimited buffers

typedef enum {
	MODE_EDIT_BUFFER,
} EditorMode;

typedef struct {
	char *name;
	bool is_up_to_date;

	Rect2f bounds;

	int buffer_index; //NOTE: index into all the active buffers 
} WL_Window;

#include "wl_window.cpp"

typedef struct {
	bool initialized;

	Renderer renderer;

	u32 active_buffer_index;
	WL_Window windows[MAX_WINDOW_COUNT];

	WL_Buffer buffers_loaded[MAX_BUFFER_COUNT]; 

	EditorMode mode;

	Font font;

	Editor_Color_Palette color_palette;

	float fontScale;

} EditorState;



static EditorState *updateEditor(float dt, float windowWidth, float windowHeight) {
	EditorState *editorState = (EditorState *)global_platform.permanent_storage;
	assert(sizeof(EditorState) < global_platform.permanent_storage_size);
	if(!editorState->initialized) {

		// longTermArena = initMemoryArena_withMemory(global_platform.permanent_storage, global_platform.permanent_storage_size - sizeof(EditorState));

		globalPerFrameArena = initMemoryArena(Kilobytes(100));
		global_perFrameArenaMark = takeMemoryMark(&globalPerFrameArena);

		
		editorState->initialized = true;

		initRenderer(&editorState->renderer);

		editorState->active_buffer_index = 0;
		editorState->mode = MODE_EDIT_BUFFER;

		editorState->font = initFont("..\\fonts\\SpaceMono.ttf");

		editorState->fontScale = 1.0f;

		//NOTE: Set up the first buffer
		WL_Window *w = &editorState->windows[0];
		initBuffer(&w->buffer);
		w->name = "untitled";
		w->is_up_to_date = true;
		w->bounds = make_rect2f(0, 0, windowWidth, windowHeight);


		{
			editorState->color_palette.background = color_hexARGBTo01(0xFF161616);
			editorState->color_palette.standard =  color_hexARGBTo01(0xFFA08563);
			// float4 variable;
			// float4 bracket;
			// float4 function;
			// float4 keyword;
			// float4 comment;
			// float4 preprocessor;
		}

	} else {
		releaseMemoryMark(&global_perFrameArenaMark);
		global_perFrameArenaMark = takeMemoryMark(&globalPerFrameArena);
	}

	Renderer *renderer = &editorState->renderer;

	//NOTE: Clear the renderer out so we can start again
	clearRenderer(renderer);




	//NOTE: Ctrl + -> zoom in -> change this to have the windows lag 
	if(global_platformInput.keyStates[PLATFORM_KEY_LEFT_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_PLUS].pressedCount > 0) 
	{
		editorState->fontScale += 0.25f;

		if(editorState->fontScale > 5.0f) {
			editorState->fontScale = 5.0f;
		}
	}


	//NOTE: Ctrl - -> zoom out -> change this to have the windows lag 
	if(global_platformInput.keyStates[PLATFORM_KEY_LEFT_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_MINUS].pressedCount > 0) 
	{
		editorState->fontScale -= 0.25f;

		if(editorState->fontScale < 0.5f) {
			editorState->fontScale = 0.5f;
		}
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







	pushViewport(renderer, make_float4(0, 0, 0, 0));
	renderer_defaultScissors(renderer, windowWidth, windowHeight);
	pushClearColor(renderer, editorState->color_palette.background);

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

				

				draw_wl_window(w, renderer, is_active, windowWidth, windowHeight, editorState->font, editorState->color_palette.standard, editorState->fontScale);
			}
		} break;
	}

	
	//////////////////////////////////////////////////////// 

	// if(global_platformInput.keyStates[PLATFORM_KEY_LEFT_CTRL].isDown) {
	// 	// OutputDebugStringA((char *)"Ctrl Down");
	// }

	// if(global_platformInput.keyStates[PLATFORM_KEY_O].pressedCount > 0) {
	// 	OutputDebugStringA((char *)"O Pressed\n");
	// }

	// if(global_platformInput.keyStates[PLATFORM_KEY_O].isDown) {
	// 	OutputDebugStringA((char *)"O down\n");
	// }

	// if(global_platformInput.keyStates[PLATFORM_KEY_O].releasedCount > 0) {
	// 	OutputDebugStringA((char *)"O released\n");
	// }



	

	return editorState;

}