#include "game.h"

/** internal void GameOutputSound(game_sound_output_buffer *sound_buffer, game_state *state) {
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
} **/

/**
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
**/

internal int32 RoundReal32ToInt32(real32 real) {
  return (int32)(real + 0.5f);
}

internal int32 truncate_real32_to_int32(real32 real) {
  return (int32)real;
}

internal void draw_rectangle(game_offscreen_buffer *buffer,
                             real32 real_min_x, real32 real_min_y, real32 real_max_x, real32 real_max_y,
                             real32 r, real32 g, real32 b) {
  int32 min_x = RoundReal32ToInt32(real_min_x);
  int32 max_x = RoundReal32ToInt32(real_max_x);
  int32 min_y = RoundReal32ToInt32(real_min_y);
  int32 max_y = RoundReal32ToInt32(real_max_y);
  if (min_x < 0) {
    min_x = 0;
  }
  if (max_x > buffer->width) {
    max_x = buffer->width;
  }

  if (min_y < 0) {
    min_y = 0;
  }
  if (max_y > buffer->height) {
    max_y = buffer->height;
  }

  uint32 color =
    (RoundReal32ToInt32(r * 255.0f) << 16)|
    (RoundReal32ToInt32(g * 255.0f) << 8) |
    (RoundReal32ToInt32(b * 255.0f));


  uint8 *row = ((uint8*)buffer->memory + min_y*buffer->pitch + min_x*buffer->bytes_per_pixel);
  for (int y = min_y; y < max_y; y++) {
    uint32 *pixel = (uint32*)row;
    for (int x = min_x; x < max_x; x++) {
      *pixel++ = color;
    }
    row += buffer->pitch;
  }
}

struct tile_map {
  uint32 dim_x;
  uint32 dim_y;
  real32 upper_left_x;
  real32 upper_left_y;;
  real32 tile_width;
  real32 tile_height;

  uint32 *tiles;
};

internal uint32 get_tile_value(tile_map *tile_map, uint32 x, uint32 y) {
  return tile_map->tiles[x + tile_map->dim_x*y];
}

internal bool32 is_tile_point_empty(tile_map *tile_map, real32 test_x, real32 test_y) {
      uint32 point_tile_x = truncate_real32_to_int32((test_x - tile_map->upper_left_x) / tile_map->tile_width);
      uint32 point_tile_y = truncate_real32_to_int32((test_y - tile_map->upper_left_y) / tile_map->tile_height);

      bool32 is_empty = false;

      if ((point_tile_x >= 0) && (point_tile_x < tile_map->dim_x) &&
          (point_tile_y >= 0) && (point_tile_y < tile_map->dim_y)) {
        uint32 tile_map_value = get_tile_value(tile_map, point_tile_x, point_tile_y);
        is_empty = tile_map_value == 0;
      }
      return is_empty;
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
    memory->is_initialized = true;
    state->player_x = 100;
    state->player_y = 100;
  }


  #define TILE_MAP_DIM_X 17
  #define TILE_MAP_DIM_Y 9
  tile_map tile_map = {};

  tile_map.dim_x = TILE_MAP_DIM_X;
  tile_map.dim_y = TILE_MAP_DIM_Y;
  tile_map.upper_left_x = -30;
  tile_map.upper_left_y = 0;
  tile_map.tile_width = 60;
  tile_map.tile_height = 60;

  uint32 tiles[TILE_MAP_DIM_X * TILE_MAP_DIM_Y] = {
    1, 1, 1, 1,  1, 1, 1, 1,  1, 0, 1, 1,  1, 1, 1, 0,  1,
    1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 1, 0, 0,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 1, 0,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
    0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 1, 0,  0,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  1,
    1, 0, 1, 1,  0, 0, 0, 0,  1, 0, 0, 0,  0, 1, 0, 0,  1,
    1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1,
  };

  tile_map.tiles = tiles;


  for(int controller_index = 0; controller_index < ArrayCount(input->controllers); controller_index++) {
    game_controller_input *controller  = get_controller(input, controller_index);
    if (controller->is_analog) {

    }
    else {
      real32 velocity = 512.0f * input->dt;
      real32 player_dx = 0.0f;
      real32 player_dy = 0.0f;
      if (controller->move_up.ended_down) {
        player_dy = -1.0f;
      }
      if (controller->move_down.ended_down) {
        player_dy = 1.0f;
      }
      if (controller->move_left.ended_down) {
        player_dx = -1.0f;
      }
      if (controller->move_right.ended_down) {
        player_dx = 1.0f;
      }

      real32 new_player_x = state->player_x + (player_dx*velocity);
      real32 new_player_y = state->player_y + (player_dy*velocity);

      if (is_tile_point_empty(&tile_map, new_player_x, new_player_y)) {
          state->player_x = new_player_x;
          state->player_y = new_player_y;
        }
    }
  }

  draw_rectangle(buffer, 0.0f, 0.0f, (real32)buffer->width, (real32)buffer->height, 0.9f, 0.5f, 1.0f);

  for (uint32 row = 0; row < tile_map.dim_y; row++) {
    for (uint32 column = 0; column < tile_map.dim_x; column++) {
      uint32 tile_value = get_tile_value(&tile_map, column, row);
      real32 gray = 0.5f;
      if (tile_value == 1) {
        gray = 1.0f;
      }
      real32 min_x = tile_map.upper_left_x + ((real32)column)*tile_map.tile_width;
      real32 min_y = tile_map.upper_left_y + ((real32)row)*tile_map.tile_height;
      real32 max_x = min_x + tile_map.tile_width;
      real32 max_y = min_y + tile_map.tile_height;
      draw_rectangle(buffer, min_x, min_y, max_x, max_y, gray, gray, gray);

    }
  }

  real32 player_r = 0.6f;
  real32 player_g = 1.0f;
  real32 player_b = 0.5f;
  real32 player_width = 0.75f*tile_map.tile_width;
  real32 player_height = 0.75f*tile_map.tile_height;
  real32 player_left = state->player_x - 0.5f*player_width;
  real32 player_top = state->player_y - player_height;


  draw_rectangle(buffer,
                 player_left, player_top, player_left + player_width, player_top + player_height,
                 player_r, player_g, player_b);

}

extern "C" GAME_GET_SOUND_SAMPLES(game_get_sound_samples_imp)
{
  game_state *state = (game_state *)memory->permanent_storage;
}
