static void draw_wl_window(EditorState *editorState, WL_Window *w, Renderer *renderer, bool is_active, float windowWidth, float windowHeight, Font font, float4 font_color, float fontScale) {
	WL_Buffer *b = &editorState->buffers_loaded[w->buffer_index];

	Rect2f window_bounds = make_rect2f(w->bounds_.minX*windowWidth, w->bounds_.minY*windowWidth, w->bounds_.maxX*windowWidth, w->bounds_.maxY*windowHeight);

	float16 orthoMatrix = make_ortho_matrix_top_left_corner(windowWidth, windowHeight, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE);
	pushMatrix(renderer, orthoMatrix);
	pushScissorsRect(renderer, window_bounds);

	pushShader(renderer, &rectOutlineShader);

	float2 window_scale = get_scale_rect2f(window_bounds);

	if(editorState->window_count_used > 1) 
	{

	 	float2 centre = get_centre_rect2f(window_bounds);
	 	
		pushRectOutline(renderer, make_float3(centre.x, -centre.y, 1.0f), window_scale, editorState->color_palette.standard);
	}

	
		
	float buffer_title_height = font.fontHeight*fontScale;
	{		
		//NOTE: Draw the outline around the file name
		float2 scale = window_scale;
		float2 centre = get_centre_rect2f(window_bounds);
		scale.y = buffer_title_height;
		centre.y = -0.5f*buffer_title_height;
		pushRectOutline(renderer, make_float3(centre.x, centre.y, 1.0f), scale, editorState->color_palette.standard);

		//NOTE: Draw the name of the file
		pushShader(renderer, &sdfFontShader);

		//  

		char *name_str = w->name;

		if(!w->is_up_to_date) {

			name_str = easy_createString_printf(&globalPerFrameArena, "%s  %s", w->name, "*");
		}

		
		float title_offset = 5;
		draw_text(renderer, &font, name_str, window_bounds.minX + title_offset, -window_bounds.minY - title_offset, fontScale, editorState->color_palette.standard);
	}

	u8 *str = compileBuffer_toNullTerminateString(b);

	// OutputDebugStringA((LPCSTR)str);
	// OutputDebugStringA((LPCSTR)"\n");
	
	
	float startX = window_bounds.minX - w->scroll_pos.x;
	float startY = -1.0f*font.fontHeight*fontScale - buffer_title_height - window_bounds.minY + w->scroll_pos.y;

	float xAt = startX;
	float yAt = startY;

	float max_x = 0;

	u8 *at = str;

	float cursorX = 0;
	float cursorY = 0;

	bool newLine = true;

	bool parsing = true;
	EasyTokenizer tokenizer = lexBeginParsing(str, EASY_LEX_OPTION_EAT_WHITE_SPACE);

	bool drawing = false;
	//NOTE: Output the buffer
	while(*at) {

	// while(parsing) {
		// EasyToken token = lexGetNextToken(&tokenizer);

#define UTF8_ADVANCE 1
#if UTF8_ADVANCE
		u32 rune = easyUnicode_utf8_codepoint_To_Utf32_codepoint(&((char *)at), true);
#else 
		u32 rune = (u32)(*at);
		at++;

#endif	
		//NOTE: DEBUG PURPOSES
		// GlyphInfo g = easyFont_getGlyph(&font, rune);
		// char string[256];
  //       sprintf(string, "%c: %d\n", rune, g.hasTexture);

  //       OutputDebugStringA((char *)string);

		
		float factor = 1.0f;

		if(yAt < 0 && yAt >= -(window_bounds.maxY + font.fontHeight)) {
			drawing = true;
		} else {
			drawing = false;
		}



		if(rune == '\n' || rune == '\r') {
			yAt -= font.fontHeight*fontScale;
			xAt = startX;
			newLine = true;
		} else if(rune == '|') {

			cursorX = xAt;
			cursorY = yAt;

		// } else if(rune == '#') {

		// 	//NOTE: Skip
		// 	factor = 0.0f;
			
		} else if(drawing) {

			GlyphInfo g = easyFont_getGlyph(&font, rune);

			assert(g.unicodePoint == rune);

			// if(rune == ' ') {
			// 	g.width = 0.5f*easyFont_getGlyph(&font, 'l').width;
			// }

			if(g.hasTexture) {


				float4 color = font_color;//make_float4(0, 0, 0, 1);
				float2 scale = make_float2(g.width*fontScale, g.height*fontScale);

				if(newLine) {
					xAt = 0.6f*scale.x + startX;
				}

				float offsetY = -0.5f*scale.y;

				float3 pos = {};
				pos.x = xAt + fontScale*g.xoffset;
				pos.y = yAt + -fontScale*g.yoffset + offsetY;
				pos.z = 1.0f;
				pushGlyph(renderer, g.handle, pos, scale, font_color, g.uvCoords);
			}

			xAt += (g.width + g.xoffset)*fontScale*factor;

			if((xAt) > max_x) {
				max_x = xAt;
			}

			newLine = false;

		}
	}

	w->max_scroll_bounds.x = max_x - startX;

	//both should be positive versions
	w->max_scroll_bounds.y = get_abs_value(yAt - startY);


	//NOTE: Draw the cursor
	if(is_active) {
		pushShader(renderer, &textureShader);

		//NOTE: Draw the cursor
		float cursor_width = easyFont_getGlyph(&font, 'M').width;

		float2 scale = make_float2(cursor_width*fontScale, font.fontHeight*fontScale);

		pushTexture(renderer, global_white_texture, make_float3(cursorX, cursorY, 1.0f), scale, editorState->color_palette.standard, make_float4(0, 1, 0, 1));

		w->scroll_target_pos = make_float2(w->scroll_pos.x, w->scroll_pos.y);

		if(w->should_scroll_to) { 
			if(cursorX < window_bounds.minX || cursorX > window_bounds.maxX) {
				w->scroll_target_pos = make_float2(cursorX - startX - 0.5f*window_scale.x, w->scroll_target_pos.y);
			}

			if(cursorY < window_bounds.minY || cursorY > window_bounds.maxY) {
				w->scroll_target_pos = make_float2(w->scroll_target_pos.x, get_abs_value(cursorY - startY) - 0.5f*window_scale.y);
			}
		} 
	}


	//NOTE: DEBUG - draw the font atlas in the middle of the screen
#if 0
	{
		pushShader(renderer, &sdfFontShader);
		float16 orthoMatrix = make_ortho_matrix_origin_center(windowWidth, windowHeight, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE);
		pushMatrix(renderer, orthoMatrix);

		pushTexture(renderer, easyFont_getGlyph(&font, 'M').handle, make_float3(0, 0, 1.0f), make_float2(300, 300), make_float4(1, 1, 1, 1), make_float4(0, 0, 1, 1));
	}
#endif

	platform_free_memory(str);

}