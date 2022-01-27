#include "wl_buffer.cpp"
#include "font.cpp"

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

	Font font;

} EditorState;



static EditorState *updateEditor(float dt, float windowWidth, float windowHeight) {
	EditorState *editorState = (EditorState *)global_platform.permanent_storage;
	assert(sizeof(EditorState) < global_platform.permanent_storage_size);
	if(!editorState->initialized) {
		
		editorState->initialized = true;

		initRenderer(&editorState->renderer);

		editorState->bufferAt = 0;
		editorState->mode = MODE_EDIT_BUFFER;

		editorState->font = initFont("..\\fonts\\SpaceMono.ttf");

		
	}

	WL_Window *w = &editorState->windows[editorState->bufferAt];

	WL_Buffer *b = &w->buffer;

	Renderer *renderer = &editorState->renderer;

	//NOTE: Clear the renderer out so we can start again
	clearRenderer(renderer);

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

	OutputDebugStringA((LPCSTR)str);
	OutputDebugStringA((LPCSTR)"\n");

	pushViewport(renderer, make_float4(0, 0, 0, 0));
	pushClearColor(renderer, make_float4(1, 0.5f, 0, 1)); 
	pushShader(renderer, &sdfFontShader);

	

	float16 orthoMatrix = make_ortho_matrix_top_left_corner(windowWidth, windowHeight, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE);
	pushMatrix(renderer, orthoMatrix);

	

	float fontScale = 2.0f;

	float xAt = 0;
	float yAt = 0;//-editorState->font.fontHeight*fontScale;

	u8 *at = str;

	float cursorX = 0;
	float cursorY = 0;

	//NOTE: Output the buffer
	while(*at) {

#define UTF8_ADVANCE 1
#if UTF8_ADVANCE
		u32 rune = easyUnicode_utf8_codepoint_To_Utf32_codepoint(&((char *)at), true);
#else 
		u32 rune = (u32)(*at);
		at++;

#endif

		//NOTE: DEBUG PURPOSES
		// GlyphInfo g = easyFont_getGlyph(&editorState->font, rune);
		// char string[256];
  //       sprintf(string, "%c: %d\n", rune, g.hasTexture);

  //       OutputDebugStringA((char *)string);

		

		if(rune == '\n' || rune == '\r') {
			yAt -= editorState->font.fontHeight*fontScale;
			xAt = 0;
		} else if(rune == '|') {

			cursorX = xAt;
			cursorY = yAt;
			
		} else {
			GlyphInfo g = easyFont_getGlyph(&editorState->font, rune);

			if(rune == ' ') {
				g.width = easyFont_getGlyph(&editorState->font, 'M').width;
			}

			if(g.hasTexture) {

				float4 color = make_float4(1, 1, 1, 1);
				float2 scale = make_float2(g.width*fontScale, g.height*fontScale);

				float3 pos = {};
				pos.x = xAt + g.xoffset;
				pos.y = yAt + g.yoffset;
				pos.z = 1.0f;
				pushGlyph(renderer, global_testTexture, pos, scale, color, g.uvCoords);
			}

			xAt += g.width*fontScale;

		}
	}

	// orthoMatrix = make_ortho_matrix_bottom_left_corner(windowWidth, windowHeight, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE);
	// pushMatrix(renderer, orthoMatrix);

	// float2 scale = make_float2(400, 400);
	// GlyphInfo g = easyFont_getGlyph(&editorState->font, 'M');
	// pushTexture(renderer, global_white_texture, make_float3(0.5f*windowWidth, 0.5f*windowHeight, 1.0f), scale, make_float4(1, 0, 1, 1), make_float4(0, 1, 0, 1));

	// pushTexture(renderer, global_white_texture, make_float3(0.1f*windowWidth, 0.5f*windowHeight, 1.0f), scale, make_float4(1, 1, 0, 1), make_float4(0, 1, 0, 1));

	// pushTexture(renderer, global_white_texture, make_float3(0.5f*windowWidth, 0.9f*windowHeight, 1.0f), scale, make_float4(0, 1, 1, 1), make_float4(0, 1, 0, 1));

	{
		pushShader(renderer, &textureShader);

		//NOTE: Draw the cursor
		float cursor_width = easyFont_getGlyph(&editorState->font, 'M').width;

		float2 scale = make_float2(cursor_width*fontScale, editorState->font.fontHeight*fontScale);

		pushTexture(renderer, global_white_texture, make_float3(cursorX, cursorY, 1.0f), scale, make_float4(0, 0, 1, 1), make_float4(0, 1, 0, 1));
	}

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


	

	return editorState;

}