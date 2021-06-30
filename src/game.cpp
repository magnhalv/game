#include "game.h"

internal void GameOutputSound(game_sound_output_buffer *sound_buffer, game_state *state) {
  int16 tone_volume = 3000;
  int wave_period = sound_buffer->samples_per_second/state->tone_hz;

  int16 *sampleOut = sound_buffer->samples;
  for (int sampleIndex = 0; sampleIndex < sound_buffer->sample_count; sampleIndex++) {
    real32 sineValue = sinf(state->t_sine);
    int16 sampleValue = (int16)(sineValue * tone_volume);
#if 0
    *sampleOut++ = sampleValue;
    *sampleOut++ = sampleValue;
#else
    *sampleOut++ = 0;
    *sampleOut++ = 0;
#endif

    state->t_sine += (1.0f/(real32)wave_period) * 2.0f * Pi32;
  }
}

void renderGradient(game_offscreen_buffer *buffer, int xOffset, int yOffset) {
  uint8 *row = (uint8 *)buffer->memory;
  for (int y = 0; y < buffer->height; y++) {
    uint32 *pixel = (uint32*)row;
    for (int x = 0; x < buffer->width; x++) {
      uint8 blue = (uint8)(x + xOffset);
      uint8 green = (uint8)(y + yOffset);
      *pixel++ = ((green << 16) | blue);
    }
    row += buffer->pitch;
  }
}

internal void render_player(game_offscreen_buffer *buffer, int player_x, int player_y, uint32 color) {
  const int size = 20;
  if (player_x < 0) {
    player_x = 0;
  }
  if (player_x > buffer->width - size) {
    player_x = buffer->width - size;
  }

  if (player_y < 0) {
    player_y = 0;
  }
  if (player_y > buffer->height - size) {
    player_y = buffer->height - size;
  }

  int top = player_y;
  int bottom = player_y + size;
  for (int y = top; y < bottom; y++) {
    uint8 *pixel = ((uint8 *)buffer->memory + y*buffer->pitch + player_x*buffer->bytes_per_pixel);
    for (int x = 0; x < size; x++) {
      *(uint32*) pixel = color;
      pixel += buffer->bytes_per_pixel;
    }
  }
}



extern "C" GAME_UPDATE_AND_RENDER(game_update_and_render_imp)
{
  // TODO: Allow sample offsets here for more robust platform options

  // ASSERTS
  Assert((&input->controllers[0].terminator - &input->controllers[0].buttons[0]) ==
         (ArrayCount(input->controllers[0].buttons) - 1));
   Assert(sizeof(game_state) <= memory->permanent_storage_size);
  //END ASSERTS

  game_state *state = (game_state *)memory->permanent_storage;

  if (!memory->is_initialized) {
    char file_name[] = __FILE__;
    debug_read_file_result bit_map_memory = memory->debug_platform_read_entire_file(thread, file_name);
    if (bit_map_memory.content_size > 0) {
      char data[] = "test";
      memory->debug_platform_write_entire_file(thread, "./test.out", &data, 4);
      memory->debug_platform_free_file_memory(thread, bit_map_memory.contents);
    }

    state->tone_hz = 256;
    state->t_sine = 0.0f;
    memory->is_initialized = true;

    state->player_x = 300;
    state->player_y = 300;
  }

  for(int controller_index = 0; controller_index < ArrayCount(input->controllers); controller_index++) {
    game_controller_input *controller  = get_controller(input, controller_index);
    if (controller->is_analog) {
      state->tone_hz = 256 + (int)(128.0f*(controller->stick_average_x));
      //state->y_offset += (int)(4.0f*(controller->stick_average_y));

      state->player_x += (int)controller->stick_average_x*2;
      state->player_y -= (int)controller->stick_average_y*2;
    }
    else {
    }


    if (controller->move_up.ended_down) {
      state->player_y += -4;
    }
    if (controller->move_down.ended_down) {
      state->player_y += 4;
      state->tone_hz = 256;
    }
    if (controller->move_left.ended_down) {
      state->player_x += -4;
      state->tone_hz = 256 - 128;
    }
    if (controller->move_right.ended_down) {
      state->player_x += 4;
      state->tone_hz = 256 + 128;
    }
  }

  renderGradient(buffer, state->x_offset, state->y_offset);
  uint32 color = 0xFFFFFFFF;
  render_player(buffer, state->player_x, state->player_y, color);

  if (input->mouse_buttons[0].ended_down) {
    color = 0xFF0000FF;
  }

  render_player(buffer, input->mouse_x, input->mouse_y, color);

}

extern "C" GAME_GET_SOUND_SAMPLES(game_get_sound_samples_imp)
{
  game_state *state = (game_state *)memory->permanent_storage;
  GameOutputSound(sound_buffer, state);
}
