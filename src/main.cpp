#include "memory_arena.cpp"
#include "wl_memory.h"
#include "file_helper.cpp"
#include "lex_utf8.h"
#include "color.cpp"
#include "wl_buffer.cpp"
#include "font.cpp"
#include "ui.cpp"


inline char *easy_createString_printf(Memory_Arena *arena, char *formatString, ...) {

    va_list args;
    va_start(args, formatString);

    char bogus[1];
    int stringLengthToAlloc = vsnprintf(bogus, 1, formatString, args) + 1; //for null terminator, just to be sure
    
    char *strArray = pushArray(arena, stringLengthToAlloc, char);

    vsnprintf(strArray, stringLengthToAlloc, formatString, args); 

    va_end(args);

    return strArray;
}



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

	//NOTE: This should be stored in something else along with the buffer 
	char *name;
	char *file_name_utf8;
	bool is_up_to_date;
	float2 scroll_pos;
	/////////////////////////


	Rect2f bounds_; //This is a percentage of the window

	int buffer_index; //NOTE: index into all the active buffers 
} WL_Window;

typedef struct {
	bool initialized;

	Renderer renderer;

	u32 active_window_index;

	u32 window_count_used;
	WL_Window windows[MAX_WINDOW_COUNT];

	u32 buffer_count_used;
	WL_Buffer buffers_loaded[MAX_BUFFER_COUNT]; 

	EditorMode mode;

	Font font;

	Editor_Color_Palette color_palette;

	float fontScale;

	bool draw_debug_memory_stats;

} EditorState;


#include "wl_window.cpp"

static WL_Window *open_new_window(EditorState *editorState) {
	//NOTE: Set up the first buffer
	WL_Window *w = &editorState->windows[editorState->window_count_used++];
	w->buffer_index = editorState->buffer_count_used++;

	assert(editorState->buffer_count_used < MAX_BUFFER_COUNT);
	initBuffer(&editorState->buffers_loaded[w->buffer_index]);
	w->name = "untitled";
	w->file_name_utf8 = "null";
	w->is_up_to_date = true;


	if(editorState->window_count_used > 1) {
		WL_Window *last_w = &editorState->windows[editorState->window_count_used - 2];

		//NOTE: Bounds now uses percentage

		float2 last_scale = get_scale_rect2f(last_w->bounds_);

		
		last_w->bounds_.maxX = last_w->bounds_.minX + 0.5f*last_scale.x;
		w->bounds_ = make_rect2f(last_w->bounds_.maxX, last_w->bounds_.minY, last_w->bounds_.maxX + 0.5f*last_scale.x, last_w->bounds_.maxY);

	} else {
		w->bounds_ = make_rect2f(0, 0, 1, 1);	
	}

	editorState->active_window_index = editorState->window_count_used - 1;

	return w;
	
}


static void DEBUG_draw_stats(Renderer *renderer, Font *font, float windowWidth, float windowHeight) {

	float16 orthoMatrix = make_ortho_matrix_top_left_corner(windowWidth, windowHeight, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE);
	pushMatrix(renderer, orthoMatrix);

	//NOTE: Draw the backing
	pushShader(renderer, &textureShader);
	float2 scale = make_float2(200, 400);
	// pushTexture(renderer, global_white_texture, make_float3(100, -200, 1.0f), scale, make_float4(0.3f, 0.3f, 0.3f, 1), make_float4(0, 0, 1, 1));
	///////////////////////////


	//NOTE: Draw the name of the file
	pushShader(renderer, &sdfFontShader);
		
	float fontScale = 0.6f;
	float4 color = make_float4(1, 1, 1, 1);

	float xAt = 0;
	float yAt = -1.5f*font->fontHeight*fontScale;

	float spacing = font->fontHeight*fontScale;

#define DEBUG_draw_stats_MACRO(title, size, draw_kilobytes) { char *name_str = 0; if(draw_kilobytes) { name_str = easy_createString_printf(&globalPerFrameArena, "%s  %d %dkilobytes", title, size, size/1000); } else { name_str = easy_createString_printf(&globalPerFrameArena, "%s  %d", title, size); } draw_text(renderer, font, name_str, xAt, yAt, fontScale, color); yAt -= spacing; }
	
	DEBUG_draw_stats_MACRO("Total Heap Allocated", global_debug_stats.total_heap_allocated, true);
	DEBUG_draw_stats_MACRO("Total Virtual Allocated", global_debug_stats.total_virtual_alloc, true);
	DEBUG_draw_stats_MACRO("Render Command Count", global_debug_stats.render_command_count, false);
	DEBUG_draw_stats_MACRO("Draw Count", global_debug_stats.draw_call_count, false);
	DEBUG_draw_stats_MACRO("Heap Block Count ", global_debug_stats.memory_block_count, false);

	DEBUG_draw_stats_MACRO("Per Frame Arena Total Size", DEBUG_get_total_arena_size(&globalPerFrameArena), true);

}


static EditorState *updateEditor(float dt, float windowWidth, float windowHeight) {
	EditorState *editorState = (EditorState *)global_platform.permanent_storage;
	assert(sizeof(EditorState) < global_platform.permanent_storage_size);
	if(!editorState->initialized) {

		// longTermArena = initMemoryArena_withMemory(global_platform.permanent_storage, global_platform.permanent_storage_size - sizeof(EditorState));

		globalPerFrameArena = initMemoryArena(Kilobytes(100));
		global_perFrameArenaMark = takeMemoryMark(&globalPerFrameArena);

		
		editorState->initialized = true;

		initRenderer(&editorState->renderer);

		editorState->active_window_index = 0;
		editorState->mode = MODE_EDIT_BUFFER;

		editorState->font = initFont("..\\fonts\\liberation-mono.ttf");

		editorState->fontScale = 0.6f;
		editorState->buffer_count_used = 0;

		editorState->draw_debug_memory_stats = false;
		
		open_new_window(editorState);

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




	/////////KEYBOARD COMMANDS BELOW /////////////

	if(global_platformInput.keyStates[PLATFORM_KEY_F5].pressedCount > 0) {
		editorState->draw_debug_memory_stats = !editorState->draw_debug_memory_stats;
	}


	//NOTE: Ctrl Z -> toggle window
	if(global_platformInput.keyStates[PLATFORM_KEY_LEFT_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_Z].pressedCount > 0) 
	{
		editorState->active_window_index++;

		if(editorState->active_window_index >= editorState->window_count_used) {
			editorState->active_window_index = 0;
		}

	}



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

	//NOTE: Ctrl X -new window
	if(global_platformInput.keyStates[PLATFORM_KEY_LEFT_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_X].pressedCount > 0) 
	{
		open_new_window(editorState);
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

			WL_Window *w = open_new_window(editorState);
			WL_Buffer *b = &editorState->buffers_loaded[w->buffer_index];

			addTextToBuffer(b, (char *)data, b->cursorAt_inBytes);

			w->file_name_utf8 = (char *)platform_wide_char_to_utf8_allocates_on_heap(fileNameToOpen);
			w->name = getFileLastPortion(w->file_name_utf8);
			w->is_up_to_date = true;

			Win32FreeFileData(data);

		} else {
			// assert(!"Couldn't open file");
		}

		platform_free_memory(fileNameToOpen);
	}



	pushViewport(renderer, make_float4(0, 0, 0, 0));
	renderer_defaultScissors(renderer, windowWidth, windowHeight);
	pushClearColor(renderer, editorState->color_palette.background);

	switch(editorState->mode) {
		case MODE_EDIT_BUFFER: {	

			for(int i = 0; i < editorState->window_count_used; ++i) {
				WL_Window *w = &editorState->windows[i];

				WL_Buffer *b = &editorState->buffers_loaded[w->buffer_index];

				bool is_active = (i == editorState->active_window_index);

				if(is_active) 
				{

					w->scroll_pos.x += global_platformInput.mouseScrollX;
					w->scroll_pos.y += global_platformInput.mouseScrollY;

					if(w->scroll_pos.y < 0) {
						w->scroll_pos.y = 0;
					}

					if(w->scroll_pos.x < 0) {
						w->scroll_pos.x = 0;
					}

					if(easyString_getSizeInBytes_utf8((char *)global_platformInput.textInput_utf8) > 0) {
						w->is_up_to_date = false;
					}

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

					       w->is_up_to_date = false;
					        
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

				

				draw_wl_window(editorState, w, renderer, is_active, windowWidth, windowHeight, editorState->font, editorState->color_palette.standard, editorState->fontScale);
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


#if DEBUG_BUILD
	if(editorState->draw_debug_memory_stats) {
		renderer_defaultScissors(renderer, windowWidth, windowHeight);
		DEBUG_draw_stats(renderer, &editorState->font, windowWidth, windowHeight);
	}
#endif

	return editorState;

}