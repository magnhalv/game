#include "game.h"



void renderGradient(game_offscreen_buffer *buffer, int xOffset, int yOffset) {
  uint8 *row = (uint8 *)buffer->Memory;
  for (int y = 0; y < buffer->Height; y++) {
    uint32 *pixel = (uint32*)row;
    for (int x = 0; x < buffer->Width; x++) {
      uint8 blue = (x + xOffset);
      uint8 green = (y + yOffset);
      *pixel++ = ((green << 8) | blue);
    }
    row += buffer->Pitch;
  }
}

void GameUpdateAndRender(game_offscreen_buffer *buffer, int xOffset, int yOffset) {
  renderGradient(buffer, xOffset, yOffset);
}
