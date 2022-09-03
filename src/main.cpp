#include "wl_memory.h"
#include "file_helper.cpp"
#include "lex_utf8.h"
#include "color.cpp"
#include "selectable.cpp"
#include "undo_redo.cpp"
#include "wl_buffer.cpp"
#include "wl_ast.cpp"
#include "font.cpp"
#include "ui.cpp"
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


*/


#define MAX_WINDOW_COUNT 8
#define MAX_BUFFER_COUNT 256 //TODO: Allow user to open unlimited buffers

typedef enum {
	MODE_EDIT_BUFFER,
	MODE_BUFFER_SELECT,
	MODE_FIND,
	MODE_CALCULATE,
	MODE_JAVASCRIPT_REPL,
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

	//NOTE: To remember where the cursor is on a line when we're using arrow keys to move up and down
	int moveVertical_xPos; 

	//NOTE: What's selected in the buffer
	Selectable_State selectable_state;
	u32 current_save_undo_redo_id;

	EasyAst ast;
 
	WL_Buffer buffer;
} WL_Open_Buffer;

typedef struct {
	Rect2f bounds_; //This is a percentage of the window

	int buffer_index; //NOTE: index into all the active buffers 
} WL_Window;

struct Single_Search {
    WL_Buffer buffer;
    Selectable_State selectable_state;
};

typedef struct {
	bool initialized;

	Renderer renderer;

	u32 active_window_index;

	u32 window_count_used;
	WL_Window windows[MAX_WINDOW_COUNT];

	u32 buffer_count_used;
	WL_Open_Buffer buffers_loaded[MAX_BUFFER_COUNT]; 

	EditorMode mode_;

	Font font;

	Editor_Color_Palette color_palette;

	float fontScale;

	bool draw_debug_memory_stats;

	Ui_State ui_state;

	Settings_To_Save settings_to_save;

	char *save_file_location_utf8;

	float moveVertical_xPos;

	int selectionIndex_forDropDown;
	
	Single_Search searchBar;

	String_Query_Search_Results current_search_reults;
	char *lastQueryString;
	int searchIndexAt;

	float line_spacing;

} EditorState;

#include "single_search.cpp"

static void set_editor_mode(EditorState *editorState, EditorMode mode) {
	refresh_buffer(&editorState->searchBar);
	if(editorState->lastQueryString) {
		easyPlatform_freeMemory(editorState->lastQueryString);
	}
	editorState->lastQueryString = 0;
	editorState->searchIndexAt = 0;
	editorState->mode_ = mode;
}

static EditorMode get_editor_mode(EditorState *editorState) {
	return editorState->mode_;
}

#include "bufferInputController.cpp"



void drawAndUpdateBufferSelection(EditorState *editorState, Renderer *renderer, Font *font, float windowWidth, float windowHeight) {
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
	float4 color = editorState->color_palette.standard;

	bool *useLongName = pushArray(&globalPerFrameArena, editorState->buffer_count_used, bool);

	for(int i = 0; i < editorState->buffer_count_used; ++i) {
		for(int j = i + 1; j < editorState->buffer_count_used; ++j) {
			WL_Open_Buffer *b1 = &editorState->buffers_loaded[i];
			WL_Open_Buffer *b2 = &editorState->buffers_loaded[j];

			if(easyString_stringsMatch_nullTerminated(b1->name, b2->name)) {
				//NOTE: Files names the same so show long ones instead 
				useLongName[i] = true;
				useLongName[j] = true;
			}
		}
	}

	Rect2f *rectsToDraw = pushArray(&globalPerFrameArena, editorState->buffer_count_used, Rect2f);
	float4 *colors = pushArray(&globalPerFrameArena, editorState->buffer_count_used, float4);

	float xAt = 0;
	float yAt = -2.0f*font->fontHeight*fontScale;

	float spacing = -yAt;

	//NOTE: Draw the backing
	pushShader(renderer, &textureShader);
	pushTexture(renderer, global_white_texture, make_float3(0.5f*windowWidth, 0.5f*yAt, 1.0f), make_float2(windowWidth, -1*yAt), editorState->color_palette.background, make_float4(0, 1, 0, 1));

	//NOTE: Update the single search buffer
	process_buffer_controller(editorState, NULL, &editorState->searchBar.buffer, BUFFER_SIMPLE, &editorState->searchBar.selectable_state);
	//NOTE: Draw the search text
	char *queryString = draw_single_search(&editorState->searchBar, renderer, font, fontScale, editorState->color_palette.variable, xAt, yAt + 0.5f*spacing, editorState->color_palette.standard, "Search: ");
	
	//NOTE: We draw a cursor in draw single search so we reset it 
	pushShader(renderer, &sdfFontShader);

	//NOTE: Move down a line to now draw all the buffers
	yAt -= spacing;


	for(int i = 0; i < editorState->buffer_count_used; i++) {
		WL_Open_Buffer *b = &editorState->buffers_loaded[i];
		

		char *nameToDraw = b->name;

		if(useLongName[i] && b->file_name_utf8) {
			nameToDraw = b->file_name_utf8;
			//NOTE: If nameToDraw is null, this is an unsaved file so we don't have a file name for it
		}

		//NOTE: See if this buffer matches the search query
		bool in_search_query = easyString_string_contains_utf8(nameToDraw, queryString);
		 if(in_search_query || easyString_getSizeInBytes_utf8(queryString) == 0) {

			//NOTE: Push the outline of the box, we don't draw it since we want to batch the draw calls together
			rectsToDraw[i] = make_rect2f(0, yAt, windowWidth, yAt + spacing);


			if(editorState->ui_state.use_mouse && in_rect2f_bounds(rectsToDraw[i], make_float2(global_platformInput.mouseX, -global_platformInput.mouseY)))
			{

				//NOTE: If hovering over the box, set it to the index
				editorState->selectionIndex_forDropDown = i;

				Ui_Type uiType = WL_INTERACTION_SELECT_DROP_DOWN;
				if(global_platformInput.keyStates[PLATFORM_MOUSE_LEFT_BUTTON].pressedCount > 0) {
					//NOTE: Try begin an interaction with selecting a menu drop down
					try_begin_interaction(&editorState->ui_state, uiType, i);
				}
				
				//NOTE: see if the user released on this id 
				if(global_platformInput.keyStates[PLATFORM_MOUSE_LEFT_BUTTON].releasedCount > 0 && is_same_interaction(&editorState->ui_state, uiType, i)) {
					//NOTE: Open the buffer now to the one selected
					WL_Window *w = &editorState->windows[editorState->active_window_index];

					//NOTE: Switch buffer to the new buffer the user selected
					w->buffer_index = editorState->selectionIndex_forDropDown;

					//NOTE: Exit the dropdown when we press enter
					set_editor_mode(editorState, MODE_EDIT_BUFFER);
				}
			}
			
			if(i == editorState->selectionIndex_forDropDown) {
				//NOTE: This is the selection we're currently on so draw a different outline color
				colors[i] = editorState->color_palette.function;
			} else {
				colors[i] = editorState->color_palette.standard;
			} 

			
			
			//NOTE: Draw the text
			draw_text(renderer, font, nameToDraw, xAt, yAt + 0.5f*spacing, fontScale, color); 

			
			yAt -= spacing;
		}

	}

	pushShader(renderer, &rectOutlineShader);

	for(int i = 0; i < editorState->buffer_count_used; ++i) {
		Rect2f r = rectsToDraw[i];
		float4 color = colors[i];
		
		float2 c = get_centre_rect2f(r);
		float2 s = get_scale_rect2f(r);
		pushRectOutline(renderer, make_float3(c.x, c.y, 1.0f), s, color);
	}
}


#include "wl_window.cpp"

static bool has_utf8_BOM(u8 *txt) {
	bool result = false;
	if(txt[0] == (u8)239 && txt[1] == (u8)187 && txt[2] == (u8)191) {
		result = true;
	}

	return result;
}

static int open_new_backing_buffer(EditorState *editorState) {
	assert(editorState->buffer_count_used < MAX_BUFFER_COUNT);
		
	int buffer_index_to_use = editorState->buffer_count_used++;

	WL_Open_Buffer *open_buffer = &editorState->buffers_loaded[buffer_index_to_use];
	
	open_buffer->moveVertical_xPos = -1; //NOTE: For unitiliased

	initBuffer(&open_buffer->buffer);

	open_buffer->should_scroll_to = false;

	open_buffer->name = "untitled";
	open_buffer->file_name_utf8 = 0;
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

static WL_Open_Buffer *open_file_and_add_to_window(EditorState *editorState, char *file_name_wide_char, Open_File_Into_Buffer_Type open_type) {

	WL_Open_Buffer *result = 0;

	size_t data_size = 0;
	void *data = 0;
	if(Platform_LoadEntireFile_wideChar(file_name_wide_char, &data, &data_size)) {

		WL_Window *w = 0;
		if(open_type == OPEN_FILE_INTO_NEW_WINDOW) {
			w = open_new_window(editorState);	
			//NOTE: Make this the active window now
			editorState->active_window_index = editorState->window_count_used - 1;
		} else if(open_type == OPEN_FILE_INTO_CURRENT_WINDOW) {
			w = &editorState->windows[editorState->active_window_index];
			w->buffer_index = open_new_backing_buffer(editorState);
		} else {
			assert(!"invalid code path"); 
		}
		
		
		WL_Open_Buffer *open_buffer = &editorState->buffers_loaded[w->buffer_index];

		WL_Buffer *b = &open_buffer->buffer;

		char *start_of_text = (char *)data;

		if(data_size > 3 && has_utf8_BOM((u8 *)start_of_text)) {
			start_of_text += 3;
			//NOTE: Is utf8 
		} 
		//TODO: eventually we'll want to check all different encodings

		bool should_add_to_history = false; //NOTE: Shouldn't add this to the history since we are opening a file, and don't want to undo this from the buffer
		addTextToBuffer(b, start_of_text, b->cursorAt_inBytes, should_add_to_history);

		b->cursorAt_inBytes = 0;

		open_buffer->file_name_utf8 = (char *)platform_wide_char_to_utf8_allocates_on_heap(file_name_wide_char);
		open_buffer->name = getFileLastPortion(open_buffer->file_name_utf8);
		open_buffer->is_up_to_date = true;

		platform_free_memory(data);

		result = open_buffer;

		// open_buffer->ast = easyAst_generateAst((char *)b->bufferMemory, &global_long_term_arena);

	} else {
		// assert(!"Couldn't open file");
	}

	return result;
}


#include "unitTests.cpp"

static void DEBUG_draw_stats(EditorState *editorState, Renderer *renderer, Font *font, float windowWidth, float windowHeight, float dt) {

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
	// DEBUG_draw_stats_FLOAT_MACRO("Start at: ", editorState->selectable_state.start_pos.x, editorState->selectable_state.start_pos.y);
	// DEBUG_draw_stats_FLOAT_MACRO("Target Scroll: ", w->scroll_target_pos.x, w->scroll_target_pos.y);

	DEBUG_draw_stats_FLOAT_MACRO("mouse scroll x ", global_platformInput.mouseX / windowWidth, global_platformInput.mouseY / windowHeight);
	DEBUG_draw_stats_FLOAT_MACRO("dt for frame ", dt, dt);

}


static EditorState *updateEditor(float dt, float windowWidth, float windowHeight, bool should_save_settings, char *save_file_location_utf8_only_use_on_inititalize, Settings_To_Save save_settings_only_use_on_inititalize) {
	EditorState *editorState = (EditorState *)global_platform.permanent_storage;
	assert(sizeof(EditorState) < global_platform.permanent_storage_size);
	if(!editorState->initialized) {

		editorState->initialized = true;

		initRenderer(&editorState->renderer);

		editorState->active_window_index = 0;
		set_editor_mode(editorState, MODE_EDIT_BUFFER);

		#if DEBUG_BUILD
		editorState->font = initFont("..\\fonts\\liberation-mono.ttf");
		#else
		editorState->font = initFont(".\\fonts\\liberation-mono.ttf");
		#endif

		editorState->fontScale = 0.6f;
		editorState->buffer_count_used = 0;

		editorState->draw_debug_memory_stats = false;
		
		open_new_window(editorState);

		editorState->save_file_location_utf8 = save_file_location_utf8_only_use_on_inititalize;
		editorState->settings_to_save = save_settings_only_use_on_inititalize;

		//NOTE: Init the single search bar
		init_single_search(&editorState->searchBar);

		editorState->lastQueryString = 0;
		editorState->searchIndexAt = 0;
 		
		editorState->ui_state.id.id = -1;

		editorState->line_spacing = 1.6f; //NOTE: How big it is between lines

		{
			editorState->color_palette.background = color_hexARGBTo01(0xFF161616);
			editorState->color_palette.standard =  color_hexARGBTo01(0xFFA08563);
			editorState->color_palette.variable = color_hexARGBTo01(0xFF6B8E23);
			editorState->color_palette.bracket = color_hexARGBTo01(0xFFDAB98F);
			editorState->color_palette.function = color_hexARGBTo01(0xFF008563);
			editorState->color_palette.keyword = color_hexARGBTo01(0xFFCD950C);
			editorState->color_palette.comment = color_hexARGBTo01(0xFF7D7D7D);
			editorState->color_palette.preprocessor = color_hexARGBTo01(0xFFDAB98F);
		}

#if DEBUG_BUILD
		DEBUG_runUnitTestForLookBackTokens();
		DEBUG_runUnitTestForLookForwardTokens();
		DEBUG_runUnitTests();
#endif
		

	} else {
		releaseMemoryMark(&global_perFrameArenaMark);
		global_perFrameArenaMark = takeMemoryMark(&globalPerFrameArena);
	}

	if(should_save_settings) {
		editorState->settings_to_save.window_width = windowWidth;
		editorState->settings_to_save.window_height = windowHeight;

		float2 xy = platform_get_window_xy_pos();

		editorState->settings_to_save.window_xAt = xy.x;
		editorState->settings_to_save.window_yAt = xy.y;

		char *file_path = concatInArena(editorState->save_file_location_utf8, "user.settings", &globalPerFrameArena);

#if !DEBUG_BUILD
		save_settings(&editorState->settings_to_save, file_path);
#endif
	}

	Renderer *renderer = &editorState->renderer;

	//NOTE: Clear the renderer out so we can start again
	clearRenderer(renderer);

	/////////KEYBOARD COMMANDS BELOW /////////////

	if(global_platformInput.drop_file_name_wide_char_need_to_free != 0) {
		open_file_and_add_to_window(editorState, global_platformInput.drop_file_name_wide_char_need_to_free, OPEN_FILE_INTO_NEW_WINDOW);

		platform_free_memory(global_platformInput.drop_file_name_wide_char_need_to_free);
		global_platformInput.drop_file_name_wide_char_need_to_free = 0;

		// end_select(&editorState->selectable_state);
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

		if(editorState->fontScale < 0.5f) {
			editorState->fontScale *= 1.1f;
		} else {
			editorState->fontScale += 0.25f;
		}

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
		if(editorState->fontScale < 0.5f) {
			editorState->fontScale *= 0.9f;
		} else {
			editorState->fontScale -= 0.25f;	
		}
		

		
	}

	//NOTE: Ctrl X -new window
	if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_FULL_STOP].pressedCount > 0 && !is_interaction_active(&editorState->ui_state, WL_INTERACTION_RESIZE_WINDOW)) 
	{
		open_new_window(editorState);
	}


	//NOTE: Ctrl O -> open file 
	if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_O].pressedCount > 0) 
	{	
		//NOTE: Make sure free the string
		char *fileNameToOpen_utf16 = (char *)Platform_OpenFile_withDialog_wideChar(&globalPerFrameArena);

		open_file_and_add_to_window(editorState, fileNameToOpen_utf16, OPEN_FILE_INTO_CURRENT_WINDOW);

		// end_select(&editorState->selectable_state);
		
	}


	//NOTE: Ctrl + S -> save file 
	if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_S].pressedCount > 0) 
	{	

		WL_Window *w = &editorState->windows[editorState->active_window_index];

		WL_Open_Buffer *open_buffer = &editorState->buffers_loaded[w->buffer_index];

		WL_Buffer *b = &open_buffer->buffer;

		if(!open_buffer->file_name_utf8) {
			//NOTE: Open a Save Dialog window
			u16 *fileNameToOpen_utf16 = (u16 *)Platform_SaveFile_withDialog_wideChar(&globalPerFrameArena);

			if(fileNameToOpen_utf16) {
				open_buffer->file_name_utf8 = (char *)platform_wide_char_to_utf8_allocates_on_heap(fileNameToOpen_utf16);
			}

			

		}

		if(open_buffer->file_name_utf8) { //NOTE: If the user cancels the save dialog box

			Compiled_Buffer_For_Save compiled_buffer = compile_buffer_to_save_format(b, &globalPerFrameArena);
			
			Platform_File_Handle handle = platform_begin_file_write_utf8_file_path (open_buffer->file_name_utf8);
			assert(!handle.has_errors);
			platform_write_file_data(handle, compiled_buffer.memory, compiled_buffer.size_in_bytes, 0);

			platform_close_file(handle);

			open_buffer->name = getFileLastPortion(open_buffer->file_name_utf8);
			open_buffer->is_up_to_date = true;

			//NOTE: Update the save position in the redo buffer so we know when we're back to a save position
			open_buffer->current_save_undo_redo_id = open_buffer->buffer.undo_redo_state.idAt;
		}
	}



	pushViewport(renderer, make_float4(0, 0, 0, 0));
	renderer_defaultScissors(renderer, windowWidth, windowHeight);
	pushClearColor(renderer, editorState->color_palette.background);

	float2 mouse_point_top_left_origin = make_float2(global_platformInput.mouseX, global_platformInput.mouseY);	
	float2 mouse_point_top_left_origin_01 = make_float2(global_platformInput.mouseX / windowWidth, global_platformInput.mouseY / windowHeight);
	
	//NOTE: Update the active buffer no matter if in different mode
	WL_Window *w = &editorState->windows[editorState->active_window_index];
	WL_Open_Buffer *open_buffer = &editorState->buffers_loaded[w->buffer_index];
	WL_Buffer *b = &open_buffer->buffer;
	{
		open_buffer->scroll_pos = lerp_float2(open_buffer->scroll_pos, open_buffer->scroll_target_pos, 0.3f);
	}
	

	switch(editorState->mode_) {
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

			//NOTE: Update the active buffer
			{	

				//NOTE: Check if the user scrolled, if so stop trying to target a cursor position
				bool user_scrolled = false;
				if(get_abs_value(global_platformInput.mouseScrollX) > 0 || get_abs_value(global_platformInput.mouseScrollY) > 0) {
					open_buffer->should_scroll_to = false;
					user_scrolled = true;
				}
		

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
				
				process_buffer_controller(editorState, open_buffer, b, BUFFER_ALL, &open_buffer->selectable_state);
			}
		} break;
		case MODE_FIND: {
			WL_Window *w = &editorState->windows[editorState->active_window_index];

			WL_Open_Buffer *open_buffer = &editorState->buffers_loaded[w->buffer_index];

			WL_Buffer *b = &open_buffer->buffer;


			if(global_platformInput.keyStates[PLATFORM_KEY_ENTER].pressedCount > 0) {

				endGapBuffer(b);

				if(editorState->current_search_reults.byteOffsetCount > 0) {
					//NOTE: Jump to next query point
					editorState->searchIndexAt++;

					if(editorState->searchIndexAt >= editorState->current_search_reults.byteOffsetCount) {
						editorState->searchIndexAt = 0;
					}

					int offset = editorState->current_search_reults.byteOffsets[editorState->searchIndexAt];

					b->cursorAt_inBytes = offset;
					open_buffer->should_scroll_to = true;
				}

				
			}

			if(global_platformInput.keyStates[PLATFORM_KEY_ESCAPE].pressedCount > 0) {
				//NOTE: Escape exits the buffer we're on
				set_editor_mode(editorState, MODE_EDIT_BUFFER);
			}

			//NOTE: Draw the name of the file
			pushShader(renderer, &sdfFontShader);

			float16 orthoMatrix = make_ortho_matrix_top_left_corner(windowWidth, windowHeight, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE);
			pushMatrix(renderer, orthoMatrix);

			float xAt = 0;
			float yAt = -2.0f*editorState->font.fontHeight*editorState->fontScale;
			float spacing = -yAt;

		
			//NOTE: Update the single search buffer
			process_buffer_controller(editorState, NULL, &editorState->searchBar.buffer, BUFFER_SIMPLE, &editorState->searchBar.selectable_state);
			//NOTE: Draw the search text
			char *queryString = draw_single_search(&editorState->searchBar, renderer, &editorState->font, editorState->fontScale, editorState->color_palette.variable, xAt, yAt + 0.5f*spacing, editorState->color_palette.standard, "Find: ");
			if(*queryString) { //NOTE: Check if it is not an empty string
				//NOTE: OnChanged Event
				if(!editorState->lastQueryString || !easyString_stringsMatch_nullTerminated(queryString, editorState->lastQueryString)) {
					if(editorState->lastQueryString) {
						easyPlatform_freeMemory(editorState->lastQueryString);
					}
					Compiled_Buffer_For_Drawing buffer_to_draw = compileBuffer_toDraw(b, &globalPerFrameArena, &open_buffer->selectable_state);
					//NOTE: Cache the query string
					editorState->lastQueryString = easyPlatform_allocateStringOnHeap_nullTerminated(queryString);
					editorState->current_search_reults = string_utf8_find_sub_string((char *)buffer_to_draw.memory, editorState->lastQueryString);
					if(editorState->current_search_reults.byteOffsetCount > 0) {
						editorState->current_search_reults.sub_string_width = font_getStringDimensions(renderer, &editorState->font, editorState->lastQueryString);
					}
				}
			} else {
				editorState->current_search_reults.byteOffsetCount = 0;
			}
		} break;
		case MODE_BUFFER_SELECT: {
			if(global_platformInput.keyStates[PLATFORM_KEY_UP].pressedCount > 0) {
				editorState->selectionIndex_forDropDown--;
				editorState->selectionIndex_forDropDown = max(editorState->selectionIndex_forDropDown, 0);

				editorState->ui_state.use_mouse = false;
			}

			if(global_platformInput.keyStates[PLATFORM_KEY_DOWN].pressedCount > 0) {
				editorState->selectionIndex_forDropDown++;
				editorState->selectionIndex_forDropDown = min(editorState->selectionIndex_forDropDown, (editorState->buffer_count_used - 1));
				
				editorState->ui_state.use_mouse = false;
			}

			float2 mouseP = make_float2(global_platformInput.mouseX, global_platformInput.mouseY);

			if(!editorState->ui_state.use_mouse && !are_float2_equal(editorState->ui_state.last_mouse_pos, mouseP)) {
				editorState->ui_state.use_mouse = true;

			}

			//NOTE: Get the latest mouse position
			editorState->ui_state.last_mouse_pos = mouseP;

			if(global_platformInput.keyStates[PLATFORM_KEY_ENTER].pressedCount > 0) {
				//NOTE: Open the buffer now to the one selected
				WL_Window *w = &editorState->windows[editorState->active_window_index];

				//NOTE: Switch buffer to the new buffer the user selected
				w->buffer_index = editorState->selectionIndex_forDropDown;

				//NOTE: Exit the dropdown when we press enter
				set_editor_mode(editorState, MODE_EDIT_BUFFER);
			}

			if(global_platformInput.keyStates[PLATFORM_KEY_ESCAPE].pressedCount > 0) {
				//NOTE: Escape exits the buffer we're on
				set_editor_mode(editorState, MODE_EDIT_BUFFER);

				if(is_interaction_active(&editorState->ui_state, WL_INTERACTION_SELECT_DROP_DOWN)) {
					//NOTE: Make sure we don't have an interaction anymore if we were trying to interact with a box
					end_interaction(&editorState->ui_state);
				}
			}

			drawAndUpdateBufferSelection(editorState, renderer, &editorState->font, windowWidth, windowHeight);
		} break;
	}

	//NOTE: Clamp the buffer position to the ends so doesnt shoot off screen. 
	//		We do it here because we want this to apply to even when we're not in a buffer edit mode
	{
		
		Rect2f window_bounds = make_rect2f(w->bounds_.minX*windowWidth, w->bounds_.minY*windowWidth, w->bounds_.maxX*windowWidth, w->bounds_.maxY*windowHeight);
		
		//NOTE: Clamp the buffer position
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

	}
	
	for(int i = 0; i < editorState->window_count_used; ++i) {
		WL_Window *w = &editorState->windows[i];
		

		WL_Open_Buffer *open_buffer = &editorState->buffers_loaded[w->buffer_index];

		WL_Buffer *b = &open_buffer->buffer;

		bool is_active = (i == editorState->active_window_index);

		draw_wl_window(editorState, w, renderer, is_active, windowWidth, windowHeight, editorState->font, editorState->color_palette.standard, editorState->fontScale, i, mouse_point_top_left_origin);

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
		DEBUG_draw_stats(editorState, renderer, &editorState->font, windowWidth, windowHeight, dt);
	}
#endif

	return editorState;

}