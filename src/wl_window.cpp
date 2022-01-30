static void draw_wl_window(WL_Window *w, Renderer *renderer, bool is_active, float windowWidth, float windowHeight, Font font, float4 font_color, float fontScale) {
	WL_Buffer *b = &w->buffer;

	pushScissorsRect(renderer, w->bounds);

	u8 *str = compileBuffer_toNullTerminateString(b);

	// OutputDebugStringA((LPCSTR)str);
	// OutputDebugStringA((LPCSTR)"\n");
	
	pushShader(renderer, &sdfFontShader);

	float16 orthoMatrix = make_ortho_matrix_top_left_corner(windowWidth, windowHeight, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE);
	pushMatrix(renderer, orthoMatrix);


	float xAt = 0;
	float yAt = -0.5f*font.fontHeight*fontScale;

	u8 *at = str;

	float cursorX = 0;
	float cursorY = 0;

	bool newLine = true;

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
		GlyphInfo g = easyFont_getGlyph(&font, rune);
		char string[256];
        sprintf(string, "%c: %d\n", rune, g.hasTexture);

        OutputDebugStringA((char *)string);

		
		float factor = 1.0f;

		if(rune == '\n' || rune == '\r') {
			yAt -= font.fontHeight*fontScale;
			xAt = 0;
			newLine = true;
		} else if(rune == '|') {

			cursorX = xAt;
			cursorY = yAt;

		} else if(rune == '#') {

			//NOTE: Skip
			factor = 0.0f;
			
		} else {

			GlyphInfo g = easyFont_getGlyph(&font, rune);

			assert(g.unicodePoint == rune);

			if(rune == ' ') {
				g.width = easyFont_getGlyph(&font, 'M').width;
			}

			if(g.hasTexture) {


				float4 color = font_color;//make_float4(0, 0, 0, 1);
				float2 scale = make_float2(g.width*fontScale, g.height*fontScale);

				if(newLine) {
					xAt = 0.5f*scale.x;
				}

				float offsetY = -0.5f*scale.y;

				float3 pos = {};
				pos.x = xAt + fontScale*g.xoffset;
				pos.y = yAt + -fontScale*g.yoffset	 + offsetY;
				pos.z = 1.0f;
				pushGlyph(renderer, g.handle, pos, scale, font_color, g.uvCoords);
			}

			xAt += (g.width + g.xoffset)*fontScale*factor;

			newLine = false;

		}
	}

	//NOTE: Draw the cursor
	if(is_active) {
		pushShader(renderer, &textureShader);

		//NOTE: Draw the cursor
		float cursor_width = easyFont_getGlyph(&font, 'M').width;

		float2 scale = make_float2(cursor_width*fontScale, font.fontHeight*fontScale);

		pushTexture(renderer, global_white_texture, make_float3(cursorX, cursorY, 1.0f), scale, make_float4(0, 0, 1, 1), make_float4(0, 1, 0, 1));
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

	Win32HeapFree(str);

}