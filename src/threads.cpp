/*
This is the platform independent thread work stuff so it's not in the thread platform stuff
*/
static void thread_work_check_file_stamps(EditorState *editorState) {
    for(int i = 0; i < editorState->buffer_count_used; ++i) {
        WL_Open_Buffer *b = &editorState->buffers_loaded[i];

        //NOTE: If haven't saved to disk won't have a file name
        if(b->file_name_utf8) {
            //NOTE: Check the buffer time stamp
            u64 fileStamp = platform_get_file_time_utf8_filename(b->file_name_utf8);

            b->current_time_stamp = fileStamp;
        }
        
    }
}