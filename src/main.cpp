#include "wl_memory.h"
#include "file_helper.cpp"
#include "lex_utf8.h"
#include "color.cpp"
#include "wl_buffer.cpp"
#include "font.cpp"
#include "ui.cpp"
#include "selectable.cpp"
#include "save_settings.cpp"


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
	char *file_name_utf8;
	bool is_up_to_date;
	float2 scroll_pos;
	float2 scroll_target_pos; //NOTE: Velocity of our scrolling
	float2 scroll_dp;
	bool should_scroll_to;

	float2 max_scroll_bounds;

	WL_Buffer buffer;
} WL_Open_Buffer;

typedef struct {
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
	WL_Open_Buffer buffers_loaded[MAX_BUFFER_COUNT]; 

	EditorMode mode;

	Font font;

	Editor_Color_Palette color_palette;

	float fontScale;

	bool draw_debug_memory_stats;

	Selectable_State selectable_state;

	Ui_State ui_state;

	Settings_To_Save settings_to_save;

	char *save_file_location_utf8;

} EditorState;


#include "wl_window.cpp"

static int open_new_backing_buffer(EditorState *editorState) {
	assert(editorState->buffer_count_used < MAX_BUFFER_COUNT);
		
	int buffer_index_to_use = editorState->buffer_count_used++;

	WL_Open_Buffer *open_buffer = &editorState->buffers_loaded[buffer_index_to_use];


	initBuffer(&open_buffer->buffer);

	open_buffer->should_scroll_to = false;

	open_buffer->name = "untitled";
	open_buffer->file_name_utf8 = "null";
	open_buffer->is_up_to_date = true;

	open_buffer->max_scroll_bounds = make_float2(0, 0);

	return buffer_index_to_use;

}

static WL_Window *open_new_window(EditorState *editorState) {
	//NOTE: Set up the first buffer
	WL_Window *w = &editorState->windows[editorState->window_count_used++];
	w->buffer_index = open_new_backing_buffer(editorState);
	

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

static void close_current_window(EditorState *editorState) {
	if(editorState->window_count_used > 1) {
			
		if(editorState->active_window_index < (editorState->window_count_used - 1)) {
			//NOTE: Not the last buffer
			editorState->windows[editorState->active_window_index + 1].bounds_.minX = editorState->windows[editorState->active_window_index].bounds_.minX; 
		} else {
			editorState->windows[editorState->active_window_index - 1].bounds_.maxX = editorState->windows[editorState->active_window_index].bounds_.maxX; 
		}

		//Move all the windows down - we need to do this since the index value needs to be preseverd becuase it's what the window knows what is in order
		for(int i = editorState->active_window_index; i < (editorState->window_count_used - 1); ++i) {
			editorState->windows[i] = editorState->windows[i + 1];
		}

		editorState->window_count_used--;
		editorState->active_window_index--;

		if(editorState->active_window_index < 0) {
			editorState->active_window_index = 0;
		}
	}

}

enum Open_File_Into_Buffer_Type {
	OPEN_FILE_INTO_NEW_WINDOW,
	OPEN_FILE_INTO_CURRENT_WINDOW,
};

static void open_file_and_add_to_window(EditorState *editorState, char *file_name_wide_char, Open_File_Into_Buffer_Type open_type) {
	size_t data_size = 0;
	void *data = 0;
	if(Platform_LoadEntireFile_wideChar(file_name_wide_char, &data, &data_size)) {

		WL_Window *w = 0;
		if(open_type == OPEN_FILE_INTO_NEW_WINDOW) {
			w = open_new_window(editorState);	
		} else if(open_type == OPEN_FILE_INTO_CURRENT_WINDOW) {
			w = &editorState->windows[editorState->active_window_index];
			w->buffer_index = open_new_backing_buffer(editorState);
		} else {
			assert(!"invalid code path"); 
		}
		

		WL_Open_Buffer *open_buffer = &editorState->buffers_loaded[w->buffer_index];

		WL_Buffer *b = &open_buffer->buffer;

		addTextToBuffer(b, (char *)data, b->cursorAt_inBytes);

		open_buffer->file_name_utf8 = (char *)platform_wide_char_to_utf8_allocates_on_heap(file_name_wide_char);
		open_buffer->name = getFileLastPortion(open_buffer->file_name_utf8);
		open_buffer->is_up_to_date = true;

		platform_free_memory(data);

	} else {
		// assert(!"Couldn't open file");
	}
}


static void DEBUG_draw_stats(EditorState *editorState, Renderer *renderer, Font *font, float windowWidth, float windowHeight) {

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
#define DEBUG_draw_stats_FLOAT_MACRO(title, f0, f1) { char *name_str = 0; name_str = easy_createString_printf(&globalPerFrameArena, "%s  %f  %f", title, f0, f1); draw_text(renderer, font, name_str, xAt, yAt, fontScale, color); yAt -= spacing; }
	
	DEBUG_draw_stats_MACRO("Total Heap Allocated", global_debug_stats.total_heap_allocated, true);
	DEBUG_draw_stats_MACRO("Total Virtual Allocated", global_debug_stats.total_virtual_alloc, true);
	DEBUG_draw_stats_MACRO("Render Command Count", global_debug_stats.render_command_count, false);
	DEBUG_draw_stats_MACRO("Draw Count", global_debug_stats.draw_call_count, false);
	DEBUG_draw_stats_MACRO("Heap Block Count ", global_debug_stats.memory_block_count, false);
	DEBUG_draw_stats_MACRO("Per Frame Arena Total Size", DEBUG_get_total_arena_size(&globalPerFrameArena), true);

	// WL_Window *w = &editorState->windows[editorState->active_window_index];
	DEBUG_draw_stats_FLOAT_MACRO("Start at: ", editorState->selectable_state.start_pos.x, editorState->selectable_state.start_pos.y);
	// DEBUG_draw_stats_FLOAT_MACRO("Target Scroll: ", w->scroll_target_pos.x, w->scroll_target_pos.y);

	DEBUG_draw_stats_FLOAT_MACRO("mouse scroll x ", global_platformInput.mouseX / windowWidth, global_platformInput.mouseY / windowHeight);

}


static EditorState *updateEditor(float dt, float windowWidth, float windowHeight, bool resized_window) {
	EditorState *editorState = (EditorState *)global_platform.permanent_storage;
	assert(sizeof(EditorState) < global_platform.permanent_storage_size);
	if(!editorState->initialized) {

		global_long_term_arena = initMemoryArena_withMemory(((u8 *)global_platform.permanent_storage) + sizeof(EditorState), global_platform.permanent_storage_size - sizeof(EditorState));

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

		editorState->save_file_location_utf8 = platform_get_save_file_location_utf8(&global_long_term_arena);

		editorState->ui_state.id.id = -1;

		{
			editorState->color_palette.background = color_hexARGBTo01(0xFF161616);
			editorState->color_palette.standard =  color_hexARGBTo01(0xFFA08563);
			editorState->color_palette.variable = color_hexARGBTo01(0xFF6B8E23);
			editorState->color_palette.bracket = color_hexARGBTo01(0xFFA08563);
			editorState->color_palette.function = color_hexARGBTo01(0xFF008563);
			editorState->color_palette.keyword = color_hexARGBTo01(0xFFCD950C);
			editorState->color_palette.comment = color_hexARGBTo01(0xFF7D7D7D);
			editorState->color_palette.preprocessor = color_hexARGBTo01(0xFFDAB98F);
		}

		

	} else {
		releaseMemoryMark(&global_perFrameArenaMark);
		global_perFrameArenaMark = takeMemoryMark(&globalPerFrameArena);
	}

	if(resized_window) {
		char *file_path = concatInArena(editorState->save_file_location_utf8, "/user.settings", &globalPerFrameArena);
		save_settings(&editorState->settings_to_save, file_path);
	}

	Renderer *renderer = &editorState->renderer;

	//NOTE: Clear the renderer out so we can start again
	clearRenderer(renderer);

	/////////KEYBOARD COMMANDS BELOW /////////////

	if(global_platformInput.drop_file_name_wide_char_need_to_free != 0) {
		open_file_and_add_to_window(editorState, global_platformInput.drop_file_name_wide_char_need_to_free, OPEN_FILE_INTO_NEW_WINDOW);

		platform_free_memory(global_platformInput.drop_file_name_wide_char_need_to_free);
		global_platformInput.drop_file_name_wide_char_need_to_free = 0;

		end_select(&editorState->selectable_state);
	}

	if(global_platformInput.keyStates[PLATFORM_KEY_F5].pressedCount > 0) {
		editorState->draw_debug_memory_stats = !editorState->draw_debug_memory_stats;
	}


	//NOTE: Ctrl Z -> toggle active window
	if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_COMMA].pressedCount > 0) 
	{
		editorState->active_window_index++;

		if(editorState->active_window_index >= editorState->window_count_used) {
			editorState->active_window_index = 0;
		}

	}



	//NOTE: Ctrl + -> zoom in -> change this to have the windows lag 
	if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_PLUS].pressedCount > 0) 
	{
		editorState->fontScale += 0.25f;

		if(editorState->fontScale > 5.0f) {
			editorState->fontScale = 5.0f;
		}
	}


	//NOTE: For removing a window
	if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_FULL_FORWARD_SLASH].pressedCount > 0 && !is_interaction_active(&editorState->ui_state, WL_INTERACTION_RESIZE_WINDOW)) 
	{
		close_current_window(editorState);
	}

	//NOTE: Ctrl - -> zoom out -> change this to have the windows lag 
	if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_MINUS].pressedCount > 0) 
	{
		editorState->fontScale -= 0.25f;

		if(editorState->fontScale < 0.5f) {
			editorState->fontScale = 0.5f;
		}
	}

	//NOTE: Ctrl X -new window
	if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_FULL_STOP].pressedCount > 0 && !is_interaction_active(&editorState->ui_state, WL_INTERACTION_RESIZE_WINDOW)) 
	{
		open_new_window(editorState);
	}


	//NOTE: Ctrl + O -> open file 
	if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_O].pressedCount > 0) 
	{	
		//NOTE: Make sure free the string
		char *fileNameToOpen = (char *)Platform_OpenFile_withDialog_wideChar_haveToFree();

		open_file_and_add_to_window(editorState, fileNameToOpen, OPEN_FILE_INTO_CURRENT_WINDOW);

		platform_free_memory(fileNameToOpen);

		end_select(&editorState->selectable_state);
	}



	pushViewport(renderer, make_float4(0, 0, 0, 0));
	renderer_defaultScissors(renderer, windowWidth, windowHeight);
	pushClearColor(renderer, editorState->color_palette.background);

	float2 mouse_point_top_left_origin = make_float2(global_platformInput.mouseX, global_platformInput.mouseY);	
	float2 mouse_point_top_left_origin_01 = make_float2(global_platformInput.mouseX / windowWidth, global_platformInput.mouseY / windowHeight);

	switch(editorState->mode) {
		case MODE_EDIT_BUFFER: {	

			if(is_interaction_active(&editorState->ui_state, WL_INTERACTION_RESIZE_WINDOW)) {
				if(global_platformInput.keyStates[PLATFORM_MOUSE_LEFT_BUTTON].isDown) {
					WL_Window *w = &editorState->windows[editorState->ui_state.id.id];

					float max_border = mouse_point_top_left_origin_01.x;

					if(max_border <= w->bounds_.minX) {
						max_border = w->bounds_.minX + 0.001;

						if(max_border > 1.0f) { max_border = 1.0f; }
					}

					w->bounds_.maxX = max_border;

					assert(editorState->ui_state.id.id < (editorState->window_count_used - 1));

					WL_Window *w1 = &editorState->windows[editorState->ui_state.id.id + 1];

					w1->bounds_.minX = max_border;

				} else {
					end_interaction(&editorState->ui_state);
				}
			}

			for(int i = 0; i < editorState->window_count_used; ++i) {
				WL_Window *w = &editorState->windows[i];

				WL_Open_Buffer *open_buffer = &editorState->buffers_loaded[w->buffer_index];

				WL_Buffer *b = &open_buffer->buffer;
				

				bool is_active = (i == editorState->active_window_index);

				if(is_active) 
				{	

					bool user_scrolled = false;
					if(get_abs_value(global_platformInput.mouseScrollX) > 0 || get_abs_value(global_platformInput.mouseScrollY) > 0) {
						open_buffer->should_scroll_to = false;
						user_scrolled = true;
					}

					open_buffer->scroll_pos = lerp_float2(open_buffer->scroll_pos, open_buffer->scroll_target_pos, 0.3f);

					if(!open_buffer->should_scroll_to) {
						float speed_factor = 10.0f;

						if(user_scrolled) {
							open_buffer->scroll_dp = make_float2(speed_factor*global_platformInput.mouseScrollX, -speed_factor*global_platformInput.mouseScrollY);
						}

						open_buffer->scroll_pos.x += dt*open_buffer->scroll_dp.x;
						open_buffer->scroll_pos.y += dt*open_buffer->scroll_dp.y; 						

						float drag = 0.9f;

						//TODO: This should be in a fixed update loop

						open_buffer->scroll_dp.x *= drag;
						open_buffer->scroll_dp.y *= drag;
					}


					Rect2f window_bounds = make_rect2f(w->bounds_.minX*windowWidth, w->bounds_.minY*windowWidth, w->bounds_.maxX*windowWidth, w->bounds_.maxY*windowHeight);


					if(open_buffer->scroll_pos.x < 0) {
						open_buffer->scroll_pos.x = 0;
						open_buffer->scroll_dp.x = 0;
					}	

					if(open_buffer->scroll_pos.y < 0) {
						open_buffer->scroll_pos.y = 0;
						open_buffer->scroll_dp.y = 0;
					}

					float2 window_scale = get_scale_rect2f(window_bounds);

					float2 padding = make_float2(50, 0.5f*window_scale.y);

					float max_w = (open_buffer->max_scroll_bounds.x - window_scale.x);

					if(max_w < 0) { max_w = 0; padding.x = 0; }

					max_w += padding.x;

					if(open_buffer->scroll_pos.x > max_w) {
						open_buffer->scroll_pos.x = max_w;
						open_buffer->scroll_dp.x = 0;
					}

					float max_h = (open_buffer->max_scroll_bounds.y - window_scale.y);

					if(max_h < 0) { max_h = 0; padding.y = 0; }

					max_h += padding.y;

					if(open_buffer->scroll_pos.y > max_h) {
						open_buffer->scroll_pos.y = max_h;
						open_buffer->scroll_dp.y = 0;
					}

					if(easyString_getSizeInBytes_utf8((char *)global_platformInput.textInput_utf8) > 0) {
						open_buffer->is_up_to_date = false;
					}

					//NOTE: Ctrl P -> paste text from clipboard
					if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_V].pressedCount > 0) 
					{
						char *text_from_clipboard = platform_get_text_utf8_from_clipboard(&globalPerFrameArena);
						open_buffer->is_up_to_date = false;

						//NOTE: Any text added
						addTextToBuffer(b, (char *)text_from_clipboard, b->cursorAt_inBytes);

						open_buffer->should_scroll_to = true;
						end_select(&editorState->selectable_state);

					}

					//NOTE: Ctrl P -> paste text from clipboard
					if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_C].pressedCount > 0 && editorState->selectable_state.is_active) 
					{
						Selectable_Diff diff = selectable_get_bytes_diff(&editorState->selectable_state);
						platform_copy_text_utf8_to_clipboard((char *)(((u8 *)b->bufferMemory) + diff.start), diff.size);

					}


					

					//NOTE: Any text added if not pressing ctrl
					if(global_platformInput.textInput_utf8[0] != '\0' && !global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown) {
						addTextToBuffer(b, (char *)global_platformInput.textInput_utf8, b->cursorAt_inBytes);
						open_buffer->should_scroll_to = true;
					}

					// OutputDebugStringA((char *)global_platformInput.textInput_utf8);

					//NOTE: Process our command buffer
					for(int i = 0; i < global_platformInput.keyInputCommand_count; ++i) {
					    PlatformKeyType command = global_platformInput.keyInputCommandBuffer[i];

					    open_buffer->should_scroll_to = true;


					    if(command == PLATFORM_KEY_BACKSPACE && b->cursorAt_inBytes > 0) {
					       u32 bytesOfPrevRune = 1;
					       removeTextFromBuffer(b, b->cursorAt_inBytes - 1, bytesOfPrevRune);
					       b->cursorAt_inBytes -= bytesOfPrevRune;

					       open_buffer->is_up_to_date = false;

					       end_select(&editorState->selectable_state);
					        
					    }

					    if(command == PLATFORM_KEY_LEFT) {
					    	u32 bytesOfPrevRune = 1;
					    	endGapBuffer(b);
					        //NOTE: Move cursor left 
					        if(b->cursorAt_inBytes > 0) {

					        	if(global_platformInput.keyStates[PLATFORM_KEY_SHIFT].isDown) 
					        	{
					        		update_select(&editorState->selectable_state, b->cursorAt_inBytes);
					        	} else {
					        		end_select(&editorState->selectable_state);
					        	}
					        	

					            b->cursorAt_inBytes -= bytesOfPrevRune;

					            if(global_platformInput.keyStates[PLATFORM_KEY_SHIFT].isDown) 
					        	{
					        		assert(editorState->selectable_state.is_active);
					        		update_select(&editorState->selectable_state, b->cursorAt_inBytes);
					        	} else {
					        		end_select(&editorState->selectable_state);
					        	}

					        }
					    }

					    if(command == PLATFORM_KEY_RIGHT) {
					    	u32 bytesOfNextRune = 1;
					    	endGapBuffer(b);
					        //NOTE: Move cursor right 
					        if(b->cursorAt_inBytes < b->bufferSize_inUse_inBytes) {
					        	if(global_platformInput.keyStates[PLATFORM_KEY_SHIFT].isDown) 
					        	{

					        		update_select(&editorState->selectable_state, b->cursorAt_inBytes);
					        	} else {
					        		end_select(&editorState->selectable_state);
					        	}

					            b->cursorAt_inBytes += bytesOfNextRune;

					            if(global_platformInput.keyStates[PLATFORM_KEY_SHIFT].isDown) 
					            {
					            	assert(editorState->selectable_state.is_active);
					            	update_select(&editorState->selectable_state, b->cursorAt_inBytes);
					            } else {
					        		end_select(&editorState->selectable_state);
					        	}
					        }
					    }       
					}
				}

				

				draw_wl_window(editorState, w, renderer, is_active, windowWidth, windowHeight, editorState->font, editorState->color_palette.standard, editorState->fontScale, i, mouse_point_top_left_origin);
			}
		} break;
	}

	
	//////////////////////////////////////////////////////// 

	// if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown) {
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
		DEBUG_draw_stats(editorState, renderer, &editorState->font, windowWidth, windowHeight);
	}
#endif

	return editorState;

}