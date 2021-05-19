#if !defined(GAME_H)

// Services that the platform layer provides to the game

// Services that the game provides to the platform layer

struct game_offscreen_buffer {
  //  BITMAPINFO Info;
  void *Memory;
  int MemorySize;
  int  Width;
  int Height;
  int BytesPerPixel;
  int Pitch;
};

void GameUpdateAndRender(game_offscreen_buffer *buffer, int xOffset, int yOffset);

#define GAME_H
#endif
