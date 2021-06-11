#include "game.h"

local_persist real32 tSine = 0.0f;

internal void GameOutputSound(game_sound_output_buffer *sound_buffer, int tone_hz) {
  int16 tone_volume = 3000;
  int wave_period = sound_buffer->samples_per_second/tone_hz;

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
  uint8 *row = (uint8 *)buffer->memory;
  for (int y = 0; y < buffer->height; y++) {
    uint32 *pixel = (uint32*)row;
    for (int x = 0; x < buffer->width; x++) {
      uint8 blue = (uint8)(x + xOffset);
      uint8 green = (uint8)(y + yOffset);
      *pixel++ = ((green << 8) | blue);
    }
    row += buffer->pitch;
  }
}



void GameUpdateAndRender(game_memory              *memory,
                         game_offscreen_buffer    *buffer,
                         game_sound_output_buffer *sound_buffer,
                         game_input               *input) {
  // TODO: Allow sample offsets here for more robust platform options

  // ASSERTS
  Assert((&input->controllers[0].terminator - &input->controllers[0].buttons[0]) ==
         (ArrayCount(input->controllers[0].buttons) - 1));
   Assert(sizeof(game_state) <= memory->permanent_storage_size);
  //END ASSERTS

  game_state *state = (game_state *)memory->permanent_storage;

  if (!memory->is_initialized) {
    //    char *file_name = __FILE__;
    //debug_read_file_result bit_map_memory = DEBUGplatform_read_entire_file(file_name);
    //    if (bit_map_memory.content_size > 0) {
      //char *data = "test";
      //DEBUGplatform_write_entire_file("./test.out", data, 4);
      //DEBUGplatform_free_file_memory(bit_map_memory.contents);
    //}

    state->tone_hz = 256;
    memory->is_initialized = true;
  }

  for(int controller_index = 0; controller_index < ArrayCount(input->controllers); controller_index++) {
    game_controller_input *controller  = get_controller(input, controller_index);
    if (controller->is_analog) {
      state->tone_hz = 256 + (int)(128.0f*(controller->stick_average_x));
      state->y_offset += (int)(4.0f*(controller->stick_average_y));
    }
    else {
    }


    if (controller->move_up.ended_down) {
      state->y_offset -= 5;
    }
    if (controller->move_down.ended_down) {
      state->y_offset += 5;
    }
    if (controller->move_left.ended_down) {
      state->x_offset -= 5;
    }
    if (controller->move_right.ended_down) {
      state->x_offset += 5;
    }
  }

  GameOutputSound(sound_buffer, state->tone_hz);
  renderGradient(buffer, state->x_offset, state->y_offset);
}
