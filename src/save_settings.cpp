#pragma pack(push, 1)
struct Settings_To_Save {
	bool is_valid;
	float window_width;
	float window_height;

	float window_xAt;
	float window_yAt;
	


};
#pragma pack(pop)


static void save_settings(Settings_To_Save *to_save, char *utf8_full_file_name) {
	Platform_File_Handle handle = platform_begin_file_write_utf8_file_path (utf8_full_file_name);

	assert(!handle.has_errors);

	platform_write_file_data(handle, to_save, sizeof(Settings_To_Save), 0);

	platform_close_file(handle);
}	

static Settings_To_Save load_settings(char *utf8_full_file_name) {

	Settings_To_Save result = {};

	result.is_valid = false;

	u16 *file_name_wide = platform_utf8_to_wide_char(utf8_full_file_name, &globalPerFrameArena);

	if(platform_does_file_exist(file_name_wide)) {

		Settings_To_Save *data = 0;
		size_t data_size = 0;
		bool worked = Platform_LoadEntireFile_wideChar(file_name_wide, (void **)&data, &data_size);

		if(worked && data_size == (sizeof(Settings_To_Save))) {
			// data->window_width;
			result = *data;
			result.is_valid = true;

		}

		if(data) {
			platform_free_memory(data);	
		}
	}

	return result;

}	