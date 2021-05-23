#include "game.h"

local_persist real32 tSine = 0.0f;

internal void GameOutputSound(game_sound_output_buffer *sound_buffer) {
  int16 tone_volume = 3000;
  int wave_period = sound_buffer->samples_per_second/sound_buffer->tone_hz;

  int16 first, second, slast, last;

  int16 *sampleOut = sound_buffer->samples;
  for (int sampleIndex = 0; sampleIndex < sound_buffer->sample_count; sampleIndex++) {



    real32 sineValue = sinf(tSine);
    int16 sampleValue = (int16)(sineValue * tone_volume);
    *sampleOut++ = sampleValue;
    *sampleOut++ = sampleValue;

    tSine += (1.0f/(real32)wave_period) * 2.0f * Pi32;
  }
}

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

void GameUpdateAndRender(game_offscreen_buffer *buffer, int xOffset, int yOffset, game_sound_output_buffer *sound_buffer) {
  // TODO: Allow sample offsets here for more robust platform options
  GameOutputSound(sound_buffer);
  renderGradient(buffer, xOffset, yOffset);
}
