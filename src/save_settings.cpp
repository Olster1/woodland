#pragma pack(push, 1)
struct Settings_To_Save {
	float window_width;
	float window_height;
};
#pragma pack(pop)


static void save_settings(Settings_To_Save *to_save, char *utf8_full_file_name) {
	Platform_File_Handle handle = platform_begin_file_write_utf8_file_path (utf8_full_file_name, &globalPerFrameArena);

	assert(!handle.has_errors);

	platform_write_file_data(handle, to_save, sizeof(Settings_To_Save), 0);

	platform_close_file(handle);
}	