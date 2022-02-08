//NOTE: This is what stores the selectable text
struct Selectable_State {
	s8 start_offset_in_bytes;
	s8 end_offset_in_bytes;

	float2 start_pos;
	float2 end_pos;

	bool is_active;

};


static void update_select(Selectable_State *state, s8 new_end_offset_in_bytes) {
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

