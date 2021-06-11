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
  int ToneVolume;
  uint32 RunningSampleIndex;
  int WavePeriod;
  int BytesPerSample;
  DWORD SecondaryBufferSize;
  bool IsSoundPlaying;
  real32 TSine;
  int WriteAheadSize;
};

struct win32_debug_time_marker {
  DWORD play_cursor;
  DWORD write_cursor;
};

#define WIN32_GAME_H
#endif
