#if !defined(WIN32_GAME_H)


struct win32_offscreen_buffer {
  BITMAPINFO Info;
  void *memory;
  int memorySize;
  int  width;
  int height;
  int bytes_per_pixel;
  int pitch;
};

struct win32_window_dimension {
  int width;
  int height;
};

struct win32_sound_output {
  int SamplesPerSec;
  int ToneHz;
  real32 t_sine;
  int ToneVolume;
  uint32 RunningSampleIndex;
  int WavePeriod;
  int BytesPerSample;
  DWORD SecondaryBufferSize;
  bool IsSoundPlaying;
  real32 TSine;
  DWORD safety_bytes;
};

struct win32_debug_time_marker {
  DWORD output_play_cursor;
  DWORD output_write_cursor;
  DWORD output_location;
  DWORD output_byte_count;

  DWORD expected_flip_play_cursor;
  DWORD flip_play_cursor;
  DWORD flip_write_cursor;
};

struct win32_game_code {
  HMODULE game_code_dll;
  game_update_and_render *game_update_and_render;
  game_get_sound_samples *game_get_sound_samples;
  bool32 is_valid;
  FILETIME last_write_time;
};

struct win32_recorded_input {

};


#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH

struct win32_state {
  uint64 total_size;
  void *game_memory_block;

  HANDLE recording_handle;
  int input_recording_index;
  HANDLE playback_handle;
  int input_playback_index;

  char exe_file_path[WIN32_STATE_FILE_NAME_COUNT];
  char *exe_filename;
  char recording_file_path[WIN32_STATE_FILE_NAME_COUNT];
};


#define WIN32_GAME_H
#endif
