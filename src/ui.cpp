enum Ui_Type {
	WL_INTERACTION_NILL,
	WL_INTERACTION_RESIZE_WINDOW,
	WL_INTERACTION_SELECT_WINDOW
};

struct Ui_Id {
	Ui_Type type;
	int id; //-1 for no id
};

struct Ui_State {
	Ui_Id id;
};

static void try_begin_interaction(Ui_State *state, Ui_Type type, int window_id) {
	if(state->id.id < 0) { //NO interactions current
		state->id.id = window_id;
		state->id.type = type;
	}
}

static bool has_active_interaction(Ui_State *state) {
	return (state->id.id >= 0);
}

static bool is_interaction_active(Ui_State *state, Ui_Type type) {
	bool result = (state->id.id >= 0) && state->id.type == type;

	return result;
}

static bool do_interaction_ids_equal(Ui_Id id, Ui_Id id1) {
	bool result = false;
	if(id.id == id1.id && id.type == id1.type) {
		result = true;
	}
	return result;
}

static void end_interaction(Ui_State *state) {
	state->id.id = -1;
	state->id.type = WL_INTERACTION_NILL;
}