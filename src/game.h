#if !defined(GAME_H)

// Services that the platform layer provides to the game

// Services that the game provides to the platform layer

struct game_offscreen_buffer {
  //  BITMAPINFO Info;
  void *Memory;
  int MemorySize;
  int Width;
  int Height;
  int BytesPerPixel;
  int Pitch;
};

struct game_sound_output_buffer {
  int samples_per_second;
  int sample_count;
  int16* samples;
  int tone_hz;
};

void GameUpdateAndRender(game_offscreen_buffer    *buffer,
                         int                       xOffset,
                         int                       yOffset,
                         game_sound_output_buffer *sound_buffer);

#define GAME_H
#endif
