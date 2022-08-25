
static void init_single_search(Single_Search *search) {
    initBuffer(&search->buffer); 

}

static void  refresh_buffer(Single_Search *search) {
    wl_emptyBuffer(&search->buffer);
}

static void draw_single_search(Single_Search *search, Renderer *renderer, Font *font, float fontScale, float4 color, float xAt, float yAt) {
    Compiled_Buffer_For_Drawing buffer_to_draw = compileBuffer_toDraw(&search->buffer, &globalPerFrameArena, &search->selectable_state);

    draw_text(renderer, font, (char *)buffer_to_draw.memory, xAt, yAt, fontScale, color);
}