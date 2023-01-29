struct DoubleClickWordResult {
	bool isInWord;
	size_t shift_start;
	size_t shift_end;
};

static bool isValidDoubleClickCharacter(char a) {
	if(lexIsAlphaNumeric(a) || lexIsNumeric(a) || lexInnerAlphaNumericCharacter(a) || (int)a > 127) { //NOTE: > 255 for unicode characters 
		return true;
	} else {
		return false;
	}
} 

static DoubleClickWordResult isDoubleClickInWord(size_t memory_offset, char *text) {
	DoubleClickWordResult result = {};

	char *str = text + memory_offset;

	if(isValidDoubleClickCharacter(str[0])) {
		result.isInWord = true;

		char *forward = str;
		//NOTE: Walk forward
		while(*forward && isValidDoubleClickCharacter(forward[0])) {	
			forward++;
		}

		char *back = str;
		//NOTE: Walk back
		while(isValidDoubleClickCharacter(back[0]) && back > text) {	
			back--;
		}

		//NOTE: Know this stopped because we weren't at the end of the buffer, so actually move forward one byte so we didn't over step the edge
		if(back > text) {
			back++;
		}

		result.shift_end = forward - text;
		result.shift_start = back - text;
	} else {
		//NOTE: Could just select this one? 
	} 
	
	return result;
}

//NOTE: This got pulled out of the main funciton becuase this needs to be run twice, one in the main loop and one if we get a null terminator  
static void check_if_clicking_of_dragging_nearby(size_t memory_offset, bool tried_clicking, bool mouseIsDown, float xAt, float yAt, float2 mouse_point_top_left_origin, float2 *closest_click_distance, size_t *closest_click_buffer_point) {
	if(tried_clicking || mouseIsDown) {
		float2 distance = make_float2(xAt - mouse_point_top_left_origin.x, get_abs_value(yAt) - mouse_point_top_left_origin.y); 
		distance.x = get_abs_value(distance.x);
		distance.y = get_abs_value(distance.y);
		//NOTE: We test the y distance first since we want to give preference to lines
		if(distance.y <= closest_click_distance->y) {
			bool differentLine = distance.y < closest_click_distance->y;
			if(distance.x <= closest_click_distance->x || differentLine) {
				*closest_click_distance = distance;
				*closest_click_buffer_point = memory_offset;
			}
		} 
	}
}

static void draw_wl_window(EditorState *editorState, WL_Window *w, Renderer *renderer, bool is_active, float windowWidth, float windowHeight, Font font, float4 font_color, float fontScale, int window_index, float2 mouse_point_top_left_origin, float dt) {
	WL_Open_Buffer *open_buffer = &editorState->buffers_loaded[w->buffer_index];

	WL_Buffer *b = &open_buffer->buffer;


	Rect2f window_bounds = make_rect2f(w->bounds_.minX*windowWidth, w->bounds_.minY*windowWidth, w->bounds_.maxX*windowWidth, w->bounds_.maxY*windowHeight);
	

	float handle_width = 10;
	Rect2f bounds0 = make_rect2f(window_bounds.maxX - handle_width,  window_bounds.minY, window_bounds.maxX + handle_width, window_bounds.maxY);

	
	float buffer_title_height = font.fontHeight*fontScale;
	
	float16 orthoMatrix = make_ortho_matrix_top_left_corner(windowWidth, windowHeight, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE);
	pushMatrix(renderer, orthoMatrix);
	pushScissorsRect(renderer, window_bounds);


	float2 window_scale = get_scale_rect2f(window_bounds);
	{
		float scroll_handle_width = 25;
		float window_height = (window_bounds.maxY - window_bounds.minY - buffer_title_height);
		float scroll_handle_height = 0;
		if(window_height < open_buffer->max_scroll_bounds.y) {
			scroll_handle_height = min(1, window_height / open_buffer->max_scroll_bounds.y)*window_height;
		} else {
			scroll_handle_height = window_height;
		}

		float scroll_handle_start_y = (open_buffer->max_scroll_bounds.y > 0) ? (open_buffer->scroll_pos.y / open_buffer->max_scroll_bounds.y)*(window_height - scroll_handle_height) : 0;
		Rect2f handle_bounds = make_rect2f(window_bounds.maxX - scroll_handle_width,  scroll_handle_start_y + buffer_title_height, window_bounds.maxX, scroll_handle_start_y + scroll_handle_height + buffer_title_height);

		float4 handle_color = editorState->color_palette.function;

		//NOTE: Draw the scroll bar
		if(editorState->mode_ == MODE_EDIT_BUFFER && in_rect2f_bounds(handle_bounds, mouse_point_top_left_origin))
		{
			handle_color = editorState->color_palette.keyword;
			if(global_platformInput.keyStates[PLATFORM_MOUSE_LEFT_BUTTON].pressedCount > 0) {
				float2 dragOffset = make_float2(0, mouse_point_top_left_origin.y - scroll_handle_start_y);
				try_begin_interaction(&editorState->ui_state, WL_INTERACTION_SCROLL_WINDOW_Y, window_index, dragOffset);
			}
			
		}

		
		if(is_same_interaction(&editorState->ui_state, WL_INTERACTION_SCROLL_WINDOW_Y, window_index)) {
			handle_color = editorState->color_palette.variable;
			float mouseY = mouse_point_top_left_origin.y - editorState->ui_state.dragOffset.y;

			//NOTE: Update scroll handle movement
			float percentage = min(1, mouseY / (window_height - scroll_handle_height));
			open_buffer->scroll_pos.y = percentage * open_buffer->max_scroll_bounds.y;

			if(open_buffer->scroll_pos.y < 0) { open_buffer->scroll_pos.y = 0; }
			if(open_buffer->scroll_pos.y > open_buffer->max_scroll_bounds.y) { open_buffer->scroll_pos.y = open_buffer->max_scroll_bounds.y; }

		} 

		//NOTE: Only draw the handle if it is less then the screen height
		if(scroll_handle_height < window_height) {
			//NOTE: Update the scroll y position
			scroll_handle_start_y = (open_buffer->max_scroll_bounds.y > 0) ? (open_buffer->scroll_pos.y / open_buffer->max_scroll_bounds.y)*(window_height - scroll_handle_height) : 0;
			handle_bounds = make_rect2f(window_bounds.maxX - scroll_handle_width,  scroll_handle_start_y + buffer_title_height, window_bounds.maxX, scroll_handle_start_y + scroll_handle_height + buffer_title_height);


			float2 centre = get_centre_rect2f(handle_bounds);
			float2 scale = get_scale_rect2f(handle_bounds);
			
			//NOTE: Draw the scroll handle
			pushShader(renderer, &textureShader);
			pushRectOutline(renderer, make_float3(centre.x, -centre.y, 1.0f), scale, handle_color);
		}
	}


	if(editorState->mode_ == MODE_EDIT_BUFFER && editorState->window_count_used > 1 && window_index < (editorState->window_count_used - 1) && in_rect2f_bounds(bounds0, mouse_point_top_left_origin) && global_platformInput.keyStates[PLATFORM_MOUSE_LEFT_BUTTON].pressedCount > 0)
	{
		try_begin_interaction(&editorState->ui_state, WL_INTERACTION_RESIZE_WINDOW, window_index);
	}
	
	pushShader(renderer, &rectOutlineShader);
	if(editorState->window_count_used > 1) 
	{

	 	float2 centre = get_centre_rect2f(window_bounds);

	 	float4 color = editorState->color_palette.standard;
	 	if(in_rect2f_bounds(bounds0, mouse_point_top_left_origin)) {
	 		color = editorState->color_palette.function;
	 	}
	 	
		pushRectOutline(renderer, make_float3(centre.x, -centre.y, 1.0f), window_scale, color);
	}

	if(open_buffer->type == OPEN_BUFFER_TEXT_EDITOR) {

	
		pushShader(renderer, &sdfFontShader);


		// EasyAst ast = easyAst_generateAst(, &globalPerFrameArena);

		// u8 *str = compileBuffer_toNullTerminateString(b);

		Compiled_Buffer_For_Drawing buffer_to_draw = compileBuffer_toDraw(b, &globalPerFrameArena, &open_buffer->selectable_state);

		// OutputDebugStringA((LPCSTR)str);
		// OutputDebugStringA((LPCSTR)"\n");


		float default_hight_light_height = 1.3f*buffer_title_height;

		float leftMargin = 0.8f*font.fontHeight*fontScale;
		float startX = window_bounds.minX - open_buffer->scroll_pos.x + leftMargin; 
		float startY = -1.0f*font.fontHeight*fontScale - buffer_title_height - window_bounds.minY + open_buffer->scroll_pos.y;

		float xAt = startX;
		float yAt = startY;

		float max_x = 0;

		float cursorX = startX;
		float cursorY = startY;

		float newLineIncrement = font.fontHeight*fontScale*editorState->line_spacing;

		bool parsing = true;
		EasyTokenizer tokenizer = lexBeginParsing((char *)buffer_to_draw.memory, EASY_LEX_OPTION_NONE);

		bool hit_start = true;
		bool hit_end = true;

		s32 memory_offset = 0;

		bool drawing = false;
		//NOTE: Output the buffer

		bool got_cursor = false;

		bool in_select = false;

		//NOTE: Null if this isn't the active buffer
		int searchBufferAt = 0;
		String_Query_Search_Results *search_query = 0;

		float2 *rectsToDraw_forSearch = 0;

		Highlight_Array *highlight_array = 0;
		highlight_array = init_highlight_array(&globalPerFrameArena);

		if(is_active) {
			open_buffer->cursor_blink_time += 2*dt;

			if(open_buffer->cursor_blink_time > 1.0f) {
				open_buffer->cursor_blink_time = -1.0f;
			}
			//NOTE: Get the highlight array ready
			if(get_editor_mode(editorState) == MODE_FIND && editorState->current_search_reults.byteOffsetCount > 0) {
				search_query = &editorState->current_search_reults;
				rectsToDraw_forSearch = pushArray(&globalPerFrameArena, editorState->current_search_reults.byteOffsetCount, float2);
			}
		}
		

		size_t closest_click_buffer_point = 0;

		//NOTE: We do a float2 because we want to test closest y-line first then x so when we click on a line we go to the end of the line even if there aren't any glyphs there 
		float2 closest_click_distance = make_float2(FLT_MAX, FLT_MAX);

		bool tried_clicking = editorState->mode_ == MODE_EDIT_BUFFER && !has_active_interaction(&editorState->ui_state) && (global_platformInput.keyStates[PLATFORM_MOUSE_LEFT_BUTTON].pressedCount > 0 || global_platformInput.doubleClicked);
		bool mouseIsDown = open_buffer->selectable_state.is_active && editorState->mode_ == MODE_EDIT_BUFFER && !has_active_interaction(&editorState->ui_state) && (global_platformInput.keyStates[PLATFORM_MOUSE_LEFT_BUTTON].isDown && global_platformInput.keyStates[PLATFORM_MOUSE_LEFT_BUTTON].pressedCount == 0);

		while(parsing) {
			bool isNotNullTerminator = true;

			EasyToken token = lexGetNextToken(&tokenizer);

			if(token.type == TOKEN_NULL_TERMINATOR) {
				memory_offset = (s32)((char *)token.at - (char *)buffer_to_draw.memory); 

				check_if_clicking_of_dragging_nearby(memory_offset, tried_clicking, mouseIsDown, xAt, yAt, mouse_point_top_left_origin, &closest_click_distance, &closest_click_buffer_point);

				parsing = false;
				isNotNullTerminator = false;
				break;
			}

			float4 text_color = editorState->color_palette.standard;

			if(drawing) {
				if(token.type == TOKEN_INTEGER || token.type == TOKEN_FLOAT) {		
					text_color = editorState->color_palette.variable;
				} else if(token.type == TOKEN_STRING) {
					text_color = editorState->color_palette.string;
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
			}


			char *at = token.at;
			char *start_token = token.at;
			while((at - start_token) < token.size && isNotNullTerminator) {

				memory_offset = (s32)((char *)at - (char *)buffer_to_draw.memory); 

				u32 rune = easyUnicode_utf8_codepoint_To_Utf32_codepoint(&((char *)at), true);

				//NOTE: For scaling tabs glyphs
				float factor = 1.0f;

				if(yAt < 0 && yAt >= -(window_bounds.maxY + font.fontHeight)) {
					drawing = true;


					check_if_clicking_of_dragging_nearby(memory_offset, tried_clicking, mouseIsDown, xAt, yAt, mouse_point_top_left_origin, &closest_click_distance, &closest_click_buffer_point);

				} else {
					drawing = false;
				}

				

				//NOTE: Draw selectable overlay
				{
					if(&open_buffer->selectable_state.is_active) {
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


				if(rune == '\n' || rune == '\r' || (editorState->should_wrap_text && xAt > window_bounds.maxX)) {
					yAt -= newLineIncrement;
					xAt = startX;

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

					//NOTE: Gone below the window view
					if(yAt < -window_bounds.maxY) {
						drawing = false;
						if (!w->needToGetTotalBounds) {
							parsing = false;
						}	
						
					}
					
				} else {

					bool wasTabRune = false;
					if(rune == '\t') {
						rune = (u32)' ';
						factor = 4.0f;
						wasTabRune = true;
					} 

					GlyphInfo g = easyFont_getGlyph(&font, rune);	

					assert(g.unicodePoint == rune);

					// if(rune == ' ') {
					// 	g.width = 0.5f*easyFont_getGlyph(&font, 'l').width;
					// }

					// char buffer[245] = {};
					// sprintf(buffer, "%d\n", g.unicodePoint);
					// OutputDebugStringA(buffer);

					if(drawing) {
						
						float2 scale = make_float2(g.width*fontScale, g.height*fontScale);

						float offsetY = -0.5f*scale.y;

						float3 pos = {};
						pos.x = xAt + fontScale*g.xoffset;
						pos.y = yAt + -fontScale*g.yoffset + offsetY;
						pos.z = 1.0f;

						//NOTE: If highlighting the text
						if(in_select) {
							float2 selectScale = scale;
							selectScale.y = default_hight_light_height;

							float2 highlight_pos = pos.xy;
							highlight_pos.y = yAt;

							if(wasTabRune) {
								//NOTE: Was a tab rune so we want to move our position to center more
								selectScale.x = (g.width + g.xoffset)*fontScale*factor;
								
								//NOTE: Move over a bit to get in the center
								highlight_pos.x += 0.3f*selectScale.x;
							}

							//NOTE: This unions the rectangles together
							highlight_push_rectangle(highlight_array, make_rect2f_center_dim(highlight_pos, selectScale), yAt);
						}

						if(g.hasTexture) {

							float4 color = font_color;//make_float4(0, 0, 0, 1);
							
							pushGlyph(renderer, g.handle, pos, scale, text_color, g.uvCoords);
						}

						{
							if(search_query) {
								//NOTE: Draw the highlighted search results
								if(searchBufferAt < search_query->byteOffsetCount && isInSearchArray(memory_offset, search_query)) {
									//NOTE: Push the outline of the box, we don't draw it since we want to batch the draw calls together
									rectsToDraw_forSearch[searchBufferAt] = make_float2(xAt + fontScale*g.xoffset, yAt);
									searchBufferAt++;
									
								}
							}
					
						}
					} //NOTE: if(drawing) block

					
					//NOTE: For drawing and not drawing glyphs we need to find out wide we are for the wrap text
					xAt += (g.width + g.xoffset)*fontScale*factor;

					//NOTE: Only check this if we are actually drawing 
					if(drawing && (xAt) > max_x) {
						max_x = xAt;
					}
				}
			}
		}

		open_buffer->max_scroll_bounds.x = max_x - startX;


		if (w->needToGetTotalBounds) {
			//both should be positive versions
			open_buffer->max_scroll_bounds.y = get_abs_value(yAt - startY);

			w->needToGetTotalBounds = false;
		}

		//NOTE: Update cursor postion
		if(memory_offset == buffer_to_draw.cursor_at) {
			assert(!got_cursor);
			cursorX = xAt;
			cursorY = yAt;

			got_cursor = true;
		}

		//assert(got_cursor);

		//NOTE:Just active buffer logic
		if(is_active) {
			
			if(open_buffer->should_scroll_to) { 
				if(cursorX < window_bounds.minX || cursorX > window_bounds.maxX) {
					float factor = 0.5f*window_scale.x;
					if(cursorX < window_bounds.minX) { factor *= -1; }
					open_buffer->scroll_target_pos = make_float2(open_buffer->scroll_pos.x + factor, open_buffer->scroll_target_pos.y);
				}

				if(cursorY > -window_bounds.minY || cursorY < -window_bounds.maxY) {
					float factor = -0.5f*window_scale.y;
					if(cursorY < -window_bounds.maxY) { factor *= -1; }
					open_buffer->scroll_target_pos = make_float2(open_buffer->scroll_target_pos.x, open_buffer->scroll_pos.y + factor);
				}
			} else {
				open_buffer->scroll_target_pos = make_float2(open_buffer->scroll_pos.x, open_buffer->scroll_pos.y);
			}

			if(tried_clicking) {
				if(closest_click_distance.x != FLT_MAX) { //NOTE: See if this is a valid position 
					endGapBuffer(b);
					b->cursorAt_inBytes = closest_click_buffer_point;

					//NOTE: Reset the blink rate
					open_buffer->cursor_blink_time = 0.0f;

					end_select(&open_buffer->selectable_state);

					//NOTE: User double clicked on a glyph, so see if clicked a word
					if(global_platformInput.doubleClicked) {
						DoubleClickWordResult doubleClickResult = isDoubleClickInWord(closest_click_buffer_point, (char *)buffer_to_draw.memory);
						if(doubleClickResult.isInWord) {

							size_t shiftEnd = convert_compiled_byte_point_to_buffer_byte_point(b, doubleClickResult.shift_end);
							size_t shiftStart = convert_compiled_byte_point_to_buffer_byte_point(b, doubleClickResult.shift_start);

							//NOTE: Update the highlight with the new bytes. Update select will set the start and end based on if this is a new active select
							update_select(&open_buffer->selectable_state, shiftStart);
							update_select(&open_buffer->selectable_state, shiftEnd);

							b->cursorAt_inBytes = shiftEnd;
						}
					} else {
						//NOTE: Just reset the highlight text when we try clicking and prepare the select if the user is 
						//		going to drag and highlight 
						update_select(&open_buffer->selectable_state, b->cursorAt_inBytes);
					}
				}
			}

			//NOTE: Drag select
			if(mouseIsDown && open_buffer->selectable_state.is_active) {
				endGapBuffer(b);
				b->cursorAt_inBytes = closest_click_buffer_point;

				//NOTE: We are dragging 
				update_select(&open_buffer->selectable_state, b->cursorAt_inBytes);

				//NOTE: How far to jump when we're trying to scroll and get to the edges
				float scroll_factor = 0.05f;
				float2 jump_distance = make_float2(scroll_factor*window_scale.x, scroll_factor*window_scale.y);

				//NOTE: Check if we should the page up or down or left or right
				float offsetMargin = 50;
				if(mouse_point_top_left_origin.y < window_bounds.minY + offsetMargin) {
					//NOTE: Trying to move up
					open_buffer->scroll_target_pos = make_float2(open_buffer->scroll_target_pos.x, open_buffer->scroll_pos.y - jump_distance.y);
				}
				if(mouse_point_top_left_origin.y > window_bounds.maxY - offsetMargin) {
					//NOTE: Trying to move down
					open_buffer->scroll_target_pos = make_float2(open_buffer->scroll_target_pos.x, open_buffer->scroll_pos.y + jump_distance.y);
					
				}
				if(mouse_point_top_left_origin.x < window_bounds.minX + offsetMargin) {
					//NOTE: Trying to move right
					open_buffer->scroll_target_pos = make_float2(open_buffer->scroll_pos.x - jump_distance.x, open_buffer->scroll_target_pos.y);
				}
				if(mouse_point_top_left_origin.x > window_bounds.maxX - offsetMargin) {
					//NOTE: Trying to move left
					open_buffer->scroll_target_pos = make_float2(open_buffer->scroll_pos.x + jump_distance.x, open_buffer->scroll_target_pos.y);
				}
				
			}
			
			pushShader(renderer, &textureShader);
			
			if(rectsToDraw_forSearch) {
				//NOTE: Get size of search string
				float width = editorState->current_search_reults.sub_string_width*fontScale;
				
				float height = font.fontHeight*fontScale;
				float4 color = editorState->color_palette.variable;

				color.w = 0.5f;
				//NOTE: Draw the highlighted search rectangles
				for(int i = 0; i < editorState->current_search_reults.byteOffsetCount; ++i) {
					float2 p = rectsToDraw_forSearch[i];

					Rect2f r = make_rect2f(p.x, p.y, p.x + width, p.y + height);
					
					float2 c = get_centre_rect2f(r);
					float2 s = get_scale_rect2f(r);
					pushRectOutline(renderer, make_float3(c.x, c.y, 1.0f), s, color);
				}
			}
		}

		pushShader(renderer, &textureShader);
		
		{
			//NOTE: Draw the cursor
			float cursor_width = easyFont_getGlyph(&font, 'M').width;

			float2 scale = make_float2(cursor_width*fontScale, font.fontHeight*fontScale);

			float4 cursorColor = editorState->color_palette.standard;

			if(is_active) {
				cursorColor = editorState->color_palette.variable;

				
			}
			
			if(open_buffer->cursor_blink_time >= 0) {
				cursorColor.z = 0.5f;

				cursorX -= 0.5f*scale.x;
				scale.x = 3;

				pushTexture(renderer, global_white_texture, make_float3(cursorX, cursorY + 0.25f*font.fontHeight*fontScale, 1.0f), scale, cursorColor, make_float4(0, 1, 0, 1));
			}
		}
		//NOTE: This is drawing the selectable overlay 
		if(open_buffer->selectable_state.is_active) {
			
			for(int i = 0; i < highlight_array->number_of_rects; ++i) {
				Hightlight_Rect *r = highlight_get_rectangle(highlight_array, i); 
					
				float2 p = get_centre_rect2f(r->rect);
				float2 s = get_scale_rect2f(r->rect);	

				float draw_y = p.y + 0.25f*font.fontHeight*fontScale;

				float4 color = editorState->color_palette.function;

				color.w = 0.3f;
				pushTexture(renderer, global_white_texture, make_float3(p.x, draw_y, 1.0f), s, color, make_float4(0, 0, 1, 1));
			}
			
		}

		if(get_editor_mode(editorState) == MODE_EDIT_BUFFER) {		
			//NOTE: Draw the outline around the file name
			float2 scale = window_scale;
			float2 centre = get_centre_rect2f(window_bounds);
			scale.y = buffer_title_height;
			centre.y = -0.5f*buffer_title_height;
			pushRectOutline(renderer, make_float3(centre.x, centre.y, 1.0f), scale, editorState->color_palette.background);
			
			pushShader(renderer, &rectOutlineShader);
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
	} else if(open_buffer->type == OPEN_BUFFER_PROJECT_TREE) {
		draw_wl_project_viewer(editorState, w, renderer, is_active, windowWidth, windowHeight, font, font_color, fontScale, mouse_point_top_left_origin);
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