//NOTE: This is what stores the selectable text
struct Selectable_State {
	//TODO: This should be size_t values 
	s32 start_offset_in_bytes;
	s32 end_offset_in_bytes;

	bool is_active;

};

struct Selectable_Diff
{
	s32 start;
	s64 size;
};

static Selectable_Diff selectable_get_bytes_diff(Selectable_State *state) {
	Selectable_Diff r = {};

	r.size = state->end_offset_in_bytes - state->start_offset_in_bytes;

	r.start = state->start_offset_in_bytes;

	if(r.size < 0) {
		r.size = state->start_offset_in_bytes - state->end_offset_in_bytes;
		r.start = state->end_offset_in_bytes;
	}

	return r;
}



static void update_select(Selectable_State *state, s64 new_end_offset_in_bytes) {
	if(!state->is_active) {
		//NOTE: Just beginning
		state->is_active = true;
		state->start_offset_in_bytes = new_end_offset_in_bytes;
		state->end_offset_in_bytes = new_end_offset_in_bytes;
	} else {
		assert(new_end_offset_in_bytes >= 0);
		state->end_offset_in_bytes = new_end_offset_in_bytes;
	}
}

static void end_select(Selectable_State *state) {
	state->is_active = false;
}

//NOTE: This is an array for storing the highlight rectangles in

struct Hightlight_Rect {
	Rect2f rect;	
	int lineNumber;
};


struct Highlight_Array {
	u8 *memory;
	size_t used_memory;
	size_t total_memory;

	Memory_Arena *arena;

	int number_of_rects;
}; 


static Highlight_Array *init_highlight_array(Memory_Arena *arena) {
	Highlight_Array *array = (Highlight_Array *)pushSize(arena, sizeof(Highlight_Array));

	int default_rects = 4;
	array->total_memory = default_rects*sizeof(Hightlight_Rect);

	array->memory = (u8 *)pushSize(arena, array->total_memory);

	array->used_memory = 0;

	array->number_of_rects = 0;

	array->arena = arena;


	return array;
}

static Hightlight_Rect *highlight_get_rectangle(Highlight_Array *array, int index) {
	Hightlight_Rect *result = (Hightlight_Rect *)(array->memory + index*sizeof(Hightlight_Rect));
	return result;
}

//NOTE: This function either adds one or unions is to an exisiting line
static void highlight_push_rectangle(Highlight_Array *array, Rect2f rect_dim, int lineNumber) {
	Hightlight_Rect *rect = 0; 

	if(array->number_of_rects > 0) {

		for(int i = 0; i < array->number_of_rects && !rect; ++i) {
			Hightlight_Rect *r = (Hightlight_Rect *)(array->memory + i*sizeof(Hightlight_Rect));

			if(r->lineNumber == lineNumber) {
				rect = r;

				rect->rect = rect2f_union(rect->rect, rect_dim);
				break;
			}
		}

	}

	if(!rect) {

		if(array->used_memory >= array->total_memory) {
			assert(array->used_memory == array->total_memory);

			int default_rects = 4;

			size_t new_mem_size = array->total_memory + default_rects*sizeof(Hightlight_Rect);
			u8 *new_mem = (u8 *)pushSize(array->arena, new_mem_size);

			memcpy(new_mem, array->memory, array->used_memory);

			array->total_memory = new_mem_size;
			array->memory = new_mem;

		}

		//NOTE: Make sure there is enough room for a highlight rect
		assert(array->used_memory + sizeof(Hightlight_Rect) <= array->total_memory);
		rect = (Hightlight_Rect	*)(array->memory + array->used_memory);
		array->used_memory += sizeof(Hightlight_Rect);
		array->number_of_rects++;

		rect->lineNumber = lineNumber; 
		rect->rect = rect_dim;
	}
}

