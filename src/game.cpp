#include "game.h"
#include "game_intrinsics.h"

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

internal void draw_rectangle(game_offscreen_buffer *buffer,
                             real32 real_min_x, real32 real_min_y, real32 real_max_x, real32 real_max_y,
                             real32 r, real32 g, real32 b) {
  int32 min_x = round_real32_to_int32(real_min_x);
  int32 max_x = round_real32_to_int32(real_max_x);
  int32 min_y = round_real32_to_int32(real_min_y);
  int32 max_y = round_real32_to_int32(real_max_y);
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
    (round_real32_to_int32(r * 255.0f) << 16)|
    (round_real32_to_int32(g * 255.0f) << 8) |
    (round_real32_to_int32(b * 255.0f));


  uint8 *row = ((uint8*)buffer->memory + min_y*buffer->pitch + min_x*buffer->bytes_per_pixel);
  for (int y = min_y; y < max_y; y++) {
    uint32 *pixel = (uint32*)row;
    for (int x = min_x; x < max_x; x++) {
      *pixel++ = color;
    }
    row += buffer->pitch;
  }
}

internal tile_map* get_tile_map(world *world, uint32 x, uint32 y) {
  if ((x >= 0) && (x < world->world_dim_x) &&
      (y >= 0) && (y < world->world_dim_y)) {
      return &world->tile_maps[x + y*world->world_dim_x];
  }
  return NULL;
}

internal uint32 get_tile_value_unchecked(world *world, tile_map *tile_map, uint32 x, uint32 y) {
  return tile_map->tiles[x + world->tile_map_dim_x*y];
}

internal bool32 is_tile_point_empty(world *world, tile_map *tile_map, uint32 test_tile_x, uint32 test_tile_y) {
  bool32 is_empty = false;
  if (tile_map) {
    uint32 tile_map_value = get_tile_value_unchecked(world, tile_map, test_tile_x, test_tile_y);
    is_empty = tile_map_value == 0;
  }
  return is_empty;
}

inline void recanonicalize_point(world *world, int32 tile_count, int32 *tile_map, int32 *tile, real32 *tile_rel) {
  int32 tile_offset = floor_real32_to_int32(*tile_rel / world->tile_side_in_meters);
  *tile += tile_offset;
  *tile_rel -= tile_offset * world->tile_side_in_meters;

  Assert(*tile_rel >= 0);
  Assert(*tile_rel <= world->tile_side_in_meters);

  if (*tile < 0) {
    *tile += tile_count;
    *tile_map = *tile_map -1;
  }

  if (*tile >= (int32)(tile_count)) {
    *tile -= tile_count;
    *tile_map = *tile_map + 1;
  }
}

inline canonical_position recanonicalize_position(world *world, canonical_position pos) {
  canonical_position result = pos;
  recanonicalize_point(world, world->tile_map_dim_x, &result.tile_map_x, &result.tile_x, &result.tile_rel_x);
  recanonicalize_point(world, world->tile_map_dim_y, &result.tile_map_y, &result.tile_y, &result.tile_rel_y);
  return result;
}


internal bool32 is_world_point_empty(world *world, canonical_position pos) {
  tile_map *tile_map = get_tile_map(world, pos.tile_map_x, pos.tile_map_y);
  bool32 is_empty = false;
  is_empty = is_tile_point_empty(world, tile_map, pos.tile_x, pos.tile_y);
  if (!is_empty) {
    int32 x = 5;
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
    state->player_position.tile_map_x = 0;
    state->player_position.tile_map_y = 0;
    state->player_position.tile_x = 5;
    state->player_position.tile_y = 5;
  }


  #define TILE_MAP_DIM_X 17
  #define TILE_MAP_DIM_Y 9
  tile_map tile_maps[2][2] = {{0}};

  world world = {};
  world.tile_side_in_meters = 1.4f;
  world.tile_side_in_pixels = 50;
  world.meters_to_pixels = (real32)world.tile_side_in_pixels / world.tile_side_in_meters;
  world.tile_map_dim_x = TILE_MAP_DIM_X;
  world.tile_map_dim_y = TILE_MAP_DIM_Y;
  world.upper_left_x = 0;
  world.upper_left_y = 0;
  world.world_dim_x = 2;
  world.world_dim_y = 2;
  real32 player_height = 1.4f;
  real32 player_width = 0.75f*player_height;


  uint32 tiles00[TILE_MAP_DIM_X * TILE_MAP_DIM_Y] = {
    1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1,
    1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 1, 0, 0,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  1,
    1, 0, 1, 1,  0, 0, 0, 0,  1, 0, 0, 0,  0, 1, 0, 0,  1,
    1, 1, 1, 1,  1, 1, 1, 0,  1, 1, 1, 1,  1, 1, 1, 1,  1,
  };

  uint32 tiles01[TILE_MAP_DIM_X * TILE_MAP_DIM_Y] = {
    1, 1, 1, 1,  1, 1, 1, 0,  1, 1, 1, 1,  1, 1, 1, 1,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 1, 0,  1,
    1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
    1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 0, 1, 0,  0,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 1, 0,  0, 0, 0, 0,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
    1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1,
  };

    uint32 tiles10[TILE_MAP_DIM_X * TILE_MAP_DIM_Y] = {
    1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 1, 0,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 1, 0,  0, 0, 0, 0,  1,
    0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 1, 0,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 1, 0,  1, 1, 0, 0,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
    1, 1, 1, 1,  1, 1, 1, 0,  1, 1, 1, 1,  1, 1, 1, 1,  1,
  };

  uint32 tiles11[TILE_MAP_DIM_X * TILE_MAP_DIM_Y] = {
    1, 1, 1, 1,  1, 1, 1, 0,  1, 1, 1, 1,  1, 1, 1, 1,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 1, 0,  1,
    1, 0, 1, 1,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0,  1,
    0, 0, 0, 1,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 1, 0,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  1,
    1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  1,
    1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1,
  };

  tile_maps[0][0].tiles = tiles00;
  tile_maps[1][0].tiles = tiles01;
  tile_maps[0][1].tiles = tiles10;
  tile_maps[1][1].tiles = tiles11;

  world.tile_maps = reinterpret_cast<tile_map *>(tile_maps);

  for(int controller_index = 0; controller_index < ArrayCount(input->controllers); controller_index++) {
    game_controller_input *controller  = get_controller(input, controller_index);
    if (controller->is_analog) {

    }
    else {
      real32 velocity = 10.0f * input->dt;
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

      canonical_position player_pos = {};
      player_pos = state->player_position;
      player_pos.tile_rel_x = state->player_position.tile_rel_x + (player_dx*velocity);
      player_pos.tile_rel_y = state->player_position.tile_rel_y + (player_dy*velocity);
      player_pos = recanonicalize_position(&world, player_pos);

      canonical_position player_left_pos = player_pos;
      player_left_pos.tile_rel_x -= 0.5f*player_width;
      player_left_pos = recanonicalize_position(&world, player_left_pos);

      canonical_position player_right_pos = player_pos;
      player_right_pos.tile_rel_x += 0.5f*player_width;
      player_right_pos = recanonicalize_position(&world, player_right_pos);

      if (
          is_world_point_empty(&world, player_pos) &&
          is_world_point_empty(&world, player_left_pos) &&
          is_world_point_empty(&world, player_right_pos)
          ) {

        state->player_position = player_pos;
      }
    }
  }

  tile_map *tile_map = get_tile_map(&world, state->player_position.tile_map_x, state->player_position.tile_map_y);
  Assert(tile_map);

  real32 player_r = 0.6f;
  real32 player_g = 1.0f;
  real32 player_b = 0.5f;
  canonical_position pos = state->player_position;
  real32 player_left = world.upper_left_x
    + pos.tile_x*world.tile_side_in_pixels
    + pos.tile_rel_x*world.meters_to_pixels
    - 0.5f*player_width*world.meters_to_pixels;
  real32 player_top = world.upper_left_y
    + pos.tile_y*world.tile_side_in_pixels
    + pos.tile_rel_y*world.meters_to_pixels
    - player_height*world.meters_to_pixels;

  draw_rectangle(buffer, 0.0f, 0.0f, (real32)buffer->width, (real32)buffer->height, 0.9f, 0.5f, 1.0f);


  for (uint32 row = 0; row < world.tile_map_dim_y; row++) {
    for (uint32 column = 0; column < world.tile_map_dim_x; column++) {
      uint32 tile_value = get_tile_value_unchecked(&world, tile_map, column, row);
      real32 gray = 0.5f;
      if (tile_value == 1) {
        gray = 1.0f;
      }

      if ((column == (uint32)state->player_position.tile_x) && (row == (uint32)state->player_position.tile_y)) {
        gray = 0.0f;
      }

      real32 min_x = world.upper_left_x + ((real32)column)*world.tile_side_in_pixels;
      real32 min_y = world.upper_left_y + ((real32)row)*world.tile_side_in_pixels;
      real32 max_x = min_x + world.tile_side_in_pixels;
      real32 max_y = min_y + world.tile_side_in_pixels;
      draw_rectangle(buffer, min_x, min_y, max_x, max_y, gray, gray, gray);

    }
  }

  draw_rectangle(buffer,
                 player_left,
                 player_top,
                 player_left + player_width*world.meters_to_pixels,
                 player_top + player_height*world.meters_to_pixels,
                 player_r, player_g, player_b);

}

extern "C" GAME_GET_SOUND_SAMPLES(game_get_sound_samples_imp)
{
  game_state *state = (game_state *)memory->permanent_storage;
}
