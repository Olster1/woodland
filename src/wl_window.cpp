static void draw_wl_window(EditorState *editorState, WL_Window *w, Renderer *renderer, bool is_active, float windowWidth, float windowHeight, Font font, float4 font_color, float fontScale, int window_index, float2 mouse_point_top_left_origin) {
	WL_Open_Buffer *open_buffer = &editorState->buffers_loaded[w->buffer_index];

	WL_Buffer *b = &open_buffer->buffer;


	Rect2f window_bounds = make_rect2f(w->bounds_.minX*windowWidth, w->bounds_.minY*windowWidth, w->bounds_.maxX*windowWidth, w->bounds_.maxY*windowHeight);


	float handle_width = 10;
	Rect2f bounds0 = make_rect2f(window_bounds.maxX - handle_width,  window_bounds.minY, window_bounds.maxX + handle_width, window_bounds.maxY);


	if(editorState->window_count_used > 1 && window_index < (editorState->window_count_used - 1) && in_rect2f_bounds(bounds0, mouse_point_top_left_origin) && global_platformInput.keyStates[PLATFORM_MOUSE_LEFT_BUTTON].pressedCount > 0)
	{
		try_begin_interaction(&editorState->ui_state, WL_INTERACTION_RESIZE_WINDOW, window_index);
	}



	float16 orthoMatrix = make_ortho_matrix_top_left_corner(windowWidth, windowHeight, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE);
	pushMatrix(renderer, orthoMatrix);
	pushScissorsRect(renderer, window_bounds);

	pushShader(renderer, &rectOutlineShader);

	float2 window_scale = get_scale_rect2f(window_bounds);

	if(editorState->window_count_used > 1) 
	{

	 	float2 centre = get_centre_rect2f(window_bounds);

	 	float4 color = editorState->color_palette.standard;
	 	if(in_rect2f_bounds(bounds0, mouse_point_top_left_origin)) {
	 		color = editorState->color_palette.function;
	 	}

	 	
		pushRectOutline(renderer, make_float3(centre.x, -centre.y, 1.0f), window_scale, color);
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

		char *name_str = open_buffer->name;

		if(!open_buffer->is_up_to_date) {

			name_str = easy_createString_printf(&globalPerFrameArena, "%s  %s", open_buffer->name, "*");
		}

		
		float title_offset = 5;
		draw_text(renderer, &font, name_str, window_bounds.minX + title_offset, -0.5f*font.fontHeight*fontScale -window_bounds.minY - title_offset, fontScale, editorState->color_palette.standard);
	}


	// EasyAst ast = easyAst_generateAst(, &globalPerFrameArena);

	// u8 *str = compileBuffer_toNullTerminateString(b);

	Compiled_Buffer_For_Drawing buffer_to_draw = compileBuffer_toDraw(b, &globalPerFrameArena, &open_buffer->selectable_state);

	// OutputDebugStringA((LPCSTR)str);
	// OutputDebugStringA((LPCSTR)"\n");
	
	
	float startX = window_bounds.minX - open_buffer->scroll_pos.x;
	float startY = -1.0f*font.fontHeight*fontScale - buffer_title_height - window_bounds.minY + open_buffer->scroll_pos.y;

	float xAt = startX;
	float yAt = startY;

	float max_x = 0;

	float cursorX = startX;
	float cursorY = startY;

	bool newLine = true;

	bool parsing = true;
	EasyTokenizer tokenizer = lexBeginParsing((char *)buffer_to_draw.memory, EASY_LEX_OPTION_NONE);

	bool hit_start = true;
	bool hit_end = true;

	s32 memory_offset = 0;

	bool drawing = false;
	//NOTE: Output the buffer

	bool got_cursor = false;

	bool in_select = false;

	Highlight_Array *highlight_array = 0;
	if(is_active) {
		highlight_array = init_highlight_array(&globalPerFrameArena);


	}

	size_t closest_click_buffer_point = 0;
	float closest_click_distance = FLT_MAX;

	bool tried_clicking = !has_active_interaction(&editorState->ui_state) && global_platformInput.keyStates[PLATFORM_MOUSE_LEFT_BUTTON].pressedCount > 0;

	while(parsing) {
		bool renderToken = true;

		EasyToken token = lexGetNextToken(&tokenizer);

		if(token.type == TOKEN_NULL_TERMINATOR) {
			memory_offset = (s32)((char *)token.at - (char *)buffer_to_draw.memory); 
			parsing = false;
			renderToken = false;
			break;
		}

		float4 text_color = editorState->color_palette.standard;

		if(token.type == TOKEN_STRING || token.type == TOKEN_INTEGER || token.type == TOKEN_FLOAT) {		
			text_color = editorState->color_palette.variable;
		} else if(token.type == TOKEN_OPEN_BRACKET || token.type == TOKEN_CLOSE_BRACKET || token.type == TOKEN_OPEN_SQUARE_BRACKET || token.type == TOKEN_CLOSE_SQUARE_BRACKET) {		
			text_color = editorState->color_palette.bracket;
		} else if(token.type == TOKEN_FUNCTION) {		
			text_color = editorState->color_palette.function;
		} else if(token.isKeyword || token.isType) {		
			text_color = editorState->color_palette.keyword;
		} else if(token.type == TOKEN_COMMENT) {		
			text_color = editorState->color_palette.comment;
		} else if(token.type == TOKEN_PREPROCESSOR) {		
			text_color = editorState->color_palette.preprocessor;
		}


		char *at = token.at;
		char *start_token = token.at;
		while((at - start_token) < token.size && renderToken) {

			memory_offset = (s32)((char *)at - (char *)buffer_to_draw.memory); 

			u32 rune = easyUnicode_utf8_codepoint_To_Utf32_codepoint(&((char *)at), true);

			//NOTE: DEBUG PURPOSES
			// GlyphInfo g = easyFont_getGlyph(&font, rune);
			// char string[256];
	  //       sprintf(string, "%c: %d\n", rune, g.hasTexture);

	  //       OutputDebugStringA((char *)string);


			float factor = 1.0f;

			if(yAt < 0 && yAt >= -(window_bounds.maxY + font.fontHeight)) {
				drawing = true;


				if(tried_clicking) {
					float2 distance = make_float2(xAt - mouse_point_top_left_origin.x, yAt + mouse_point_top_left_origin.y); 
					float distance_sqr = distance.x*distance.x + distance.y*distance.y;

					if(distance_sqr <= closest_click_distance) {
						closest_click_distance = distance_sqr;
						closest_click_buffer_point = memory_offset;
					}
				}

			} else {
				drawing = false;
			}

			//NOTE: Draw selectable overlay
			{
				if(is_active && &open_buffer->selectable_state.is_active) {
					if(memory_offset == buffer_to_draw.shift_begin) {
						in_select = true;
					}

					if(memory_offset == buffer_to_draw.shift_end) {
						in_select = false;
					}
				}
			}

			if(memory_offset == buffer_to_draw.cursor_at) {
				cursorX = xAt;
				cursorY = yAt;
				got_cursor = true; 
			}


			if(rune == '\n' || rune == '\r') {
				yAt -= font.fontHeight*fontScale;
				xAt = startX;
				newLine = true;

				//NOTE: Newline token so we have to check the cursor again and skip the extra newline
				if(isNewlineTokenWindowsType(token)) {
					 assert(token.size == 2);

					//NOTE: Check the cursor location again
					memory_offset = (s32)((char *)at - (char *)buffer_to_draw.memory); 
					if(memory_offset == buffer_to_draw.cursor_at) {
					 	cursorX = xAt;
					 	cursorY = yAt;
					 	got_cursor = true; 
					}

					//NOTE: Move past the newline character
					at++;
				}

				if(yAt < -window_bounds.maxY) {
					drawing = false;
				}
				
			} else if(drawing) {

				if(rune == '\t') {
					rune = (u32)' ';
					factor = 4.0f;
				} 
				
				GlyphInfo g = easyFont_getGlyph(&font, rune);	

				assert(g.unicodePoint == rune);

				// if(rune == ' ') {
				// 	g.width = 0.5f*easyFont_getGlyph(&font, 'l').width;
				// }

				// char buffer[245] = {};
				// sprintf(buffer, "%d\n", g.unicodePoint);
				// OutputDebugStringA(buffer);

				

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
					pushGlyph(renderer, g.handle, pos, scale, text_color, g.uvCoords);

					if(in_select) {
						float2 selectScale = scale;

						// if(selectScale.y < font.fontHeight) { selectScale.y = font.fontHeight; } 
						highlight_push_rectangle(highlight_array, make_rect2f_center_dim(pos.xy, selectScale), yAt);
					}

				}

				
				xAt += (g.width + g.xoffset)*fontScale*factor;

				if((xAt) > max_x) {
					max_x = xAt;
				}

				newLine = false;

			}
		}
	}

	open_buffer->max_scroll_bounds.x = max_x - startX;

	//both should be positive versions
	open_buffer->max_scroll_bounds.y = get_abs_value(yAt - startY);


	//NOTE: Draw the cursor
	if(is_active) {

		if(tried_clicking) {
			if(closest_click_distance != FLT_MAX) {
				b->cursorAt_inBytes = convert_compiled_byte_point_to_buffer_byte_point(b, closest_click_buffer_point);

				end_select(&open_buffer->selectable_state);

				update_select(&open_buffer->selectable_state, b->cursorAt_inBytes);
			}
		}
		

		if(memory_offset == buffer_to_draw.cursor_at) {
			assert(!got_cursor);
			cursorX = xAt;
			cursorY = yAt;

			got_cursor = true;
		}

		assert(got_cursor);

		pushShader(renderer, &textureShader);

		//NOTE: Draw the cursor
		float cursor_width = easyFont_getGlyph(&font, 'M').width;

		float2 scale = make_float2(cursor_width*fontScale, font.fontHeight*fontScale);

		pushTexture(renderer, global_white_texture, make_float3(cursorX, cursorY + 0.25f*font.fontHeight*fontScale, 1.0f), scale, editorState->color_palette.standard, make_float4(0, 1, 0, 1));

		open_buffer->scroll_target_pos = make_float2(open_buffer->scroll_pos.x, open_buffer->scroll_pos.y);

		if(open_buffer->should_scroll_to) { 
			if(cursorX < window_bounds.minX || cursorX > window_bounds.maxX) {
				open_buffer->scroll_target_pos = make_float2(cursorX - startX - 0.5f*window_scale.x, open_buffer->scroll_target_pos.y);
			}

			if(cursorY > -window_bounds.minY || cursorY < -window_bounds.maxY) {
				open_buffer->scroll_target_pos = make_float2(open_buffer->scroll_target_pos.x, get_abs_value(cursorY - startY) - 0.5f*window_scale.y);
			}
		} 


		//NOTE: This is drawing the selectable overlay 
		if(open_buffer->selectable_state.is_active) {

			for(int i = 0; i < highlight_array->number_of_rects; ++i) {
				Hightlight_Rect *r = highlight_get_rectangle(highlight_array, i); 
					
				float2 p = get_centre_rect2f(r->rect);
				float2 s = get_scale_rect2f(r->rect);	

				float draw_y = p.y + 0.25f*font.fontHeight*fontScale;

				pushShader(renderer, &textureShader);

				float4 color = editorState->color_palette.standard;

				color.w = 0.3f;
				pushTexture(renderer, global_white_texture, make_float3(p.x, draw_y, 1.0f), s, color, make_float4(0, 0, 1, 1));
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

	// platform_free_memory(str);

}