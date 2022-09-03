
static void init_single_search(Single_Search *search) {
    initBuffer(&search->buffer); 

}

static void  refresh_buffer(Single_Search *search) {
    wl_emptyBuffer(&search->buffer);
}

static char *draw_single_search(Single_Search *search, Renderer *renderer, Font *font, float fontScale, float4 color, float xAt, float yAt, float4 cursorColor, char *extraWord) {
    Compiled_Buffer_For_Drawing buffer_to_draw = compileBuffer_toDraw(&search->buffer, &globalPerFrameArena, &search->selectable_state);

    char *fullString = easy_createString_printf(&globalPerFrameArena, "%s%s", extraWord, (char *)buffer_to_draw.memory);

    

    pushShader(renderer, &sdfFontShader);
    float2 cursor_pos = draw_text(renderer, font, fullString, xAt, yAt, fontScale, color, buffer_to_draw.cursor_at).cursorP;

    //NOTE: Draw the cursor now
    pushShader(renderer, &textureShader);

    //NOTE: Draw the cursor
    float cursor_width = easyFont_getGlyph(font, 'M').width;

    float2 scale = make_float2(cursor_width*fontScale, font->fontHeight*fontScale);

    pushTexture(renderer, global_white_texture, make_float3(cursor_pos.x, cursor_pos.y + 0.25f*font->fontHeight*fontScale, 1.0f), scale, cursorColor, make_float4(0, 1, 0, 1));

    //NOTE: Return the query string
    return (char *)buffer_to_draw.memory;

}