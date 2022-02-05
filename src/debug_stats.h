struct DEBUG_memory_ptr_to_size {
	size_t size;
	void *ptr;
};

struct DEBUG_stats {
	size_t total_heap_allocated;
	size_t total_virtual_alloc;

	int render_command_count;
	int draw_call_count;

	u32 memory_block_count;
	DEBUG_memory_ptr_to_size memory_blocks[512];

};

static void DEBUG_add_memory_block_size(DEBUG_stats *stats, void *ptr, size_t size) {
	assert(stats->memory_block_count < arrayCount(stats->memory_blocks));

	stats->total_heap_allocated += size;
	DEBUG_memory_ptr_to_size *block = &stats->memory_blocks[stats->memory_block_count++];

	block->size = size;
	block->ptr = ptr;
}


static void DEUBG_remove_memory_block_size(DEBUG_stats *stats, void *ptr) {
    if(ptr) {
        for(int i = 0; i < stats->memory_block_count; ++i) {
            DEBUG_memory_ptr_to_size *block = &stats->memory_blocks[i];

            if(block->ptr == ptr) {
            	assert(stats->total_heap_allocated >= block->size);
				stats->total_heap_allocated -= block->size;
				stats->memory_block_count--;
            	break;
            }
        }
    }
}
