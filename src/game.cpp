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

internal tile_chunk* get_tile_chunk(world *world, uint32 x, uint32 y) {
  if ((x < world->tile_chunk_count_x) && (y < world->tile_chunk_count_y)) {
    return &world->tile_chunks[x + y*world->tile_chunk_count_x];
  }
  return NULL;
}


inline void recanonicalize_point(world *world, uint32 *tile, real32 *tile_rel) {
  int32 offset = floor_real32_to_int32(*tile_rel / world->tile_side_in_meters);
  *tile += offset;
  *tile_rel -= offset * world->tile_side_in_meters;

  Assert(*tile_rel >= 0);
  Assert(*tile_rel <= world->tile_side_in_meters);
}

inline world_position recanonicalize_position(world *world, world_position pos) {
  world_position result = pos;
  recanonicalize_point(world, &result.abs_tile_x, &result.tile_rel_x);
  recanonicalize_point(world, &result.abs_tile_y, &result.tile_rel_y);
  return result;
}

inline tile_chunk_position get_chunk_position(world *world, uint32 abs_tile_x, uint32 abs_tile_y) {
  tile_chunk_position result;

  result.tile_chunk_x = abs_tile_x >> world->chunk_shift;
  result.tile_chunk_y = abs_tile_y >> world->chunk_shift;

  result.rel_tile_x = abs_tile_x & world->chunk_mask;
  result.rel_tile_y = abs_tile_y & world->chunk_mask;

  return result;
}

internal uint32 get_tile_value_unchecked(world *world, tile_chunk *tile_chunk, uint32 tile_x, uint32 tile_y) {
  Assert(tile_chunk);
  Assert(tile_x < world->chunk_dim);
  Assert(tile_y < world->chunk_dim);
  uint32 index = tile_x + world->chunk_dim*tile_y;
  return tile_chunk->tiles[index];
}

internal uint32 get_tile_value(world *world, tile_chunk *tile_chunk, uint32 test_tile_x, uint32 test_tile_y) {
  uint32 tile_chunk_value = 0;
  if (tile_chunk) {
    tile_chunk_value = get_tile_value_unchecked(world, tile_chunk, test_tile_x, test_tile_y);
  }
  return tile_chunk_value;
}

internal uint32 get_tile_value(world *world, uint32 abs_tile_x, uint32 abs_tile_y) {
  tile_chunk_position chunk_pos = get_chunk_position(world, abs_tile_x, abs_tile_y);
  tile_chunk *tile_chunk = get_tile_chunk(world, chunk_pos.tile_chunk_x, chunk_pos.tile_chunk_y);
  uint32 tile_value = get_tile_value(world, tile_chunk, chunk_pos.rel_tile_x, chunk_pos.rel_tile_y);
  return tile_value;
}

internal bool32 is_world_point_empty(world *world, world_position pos) {
  uint32 tile_chunk_value = get_tile_value(world, pos.abs_tile_x, pos.abs_tile_y);
  bool32 is_empty = (tile_chunk_value == 0);
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
    state->player_position.abs_tile_x = 3;
    state->player_position.abs_tile_y = 3;
    state->player_position.tile_rel_x = 5.0f;
    state->player_position.tile_rel_y = 5.0f;
  }


  #define TILE_MAP_DIM_X 256
  #define TILE_MAP_DIM_Y 256
  tile_chunk tile_chunks[2][2] = {{0}};

  world world = {};
  world.tile_side_in_meters = 1.4f;
  world.tile_side_in_pixels = 64;
  world.meters_to_pixels = (real32)world.tile_side_in_pixels / world.tile_side_in_meters;

  // Note: This is set to be using 256x256 tile chunks
  world.chunk_shift = 8;
  world.chunk_mask = (1 << world.chunk_shift) - 1;
  world.chunk_dim = 256;

  uint32 temp_tiles[TILE_MAP_DIM_Y][TILE_MAP_DIM_X] = {
    {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
    {1, 1, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
    {1, 1, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 1, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
    {1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
    {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
    {1, 1, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  0, 1, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
    {1, 0, 0, 0,  0, 1, 0, 0,  1, 0, 0, 0,  1, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
    {1, 1, 1, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 1, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
    {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
    {1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 1, 1, 1, 1},
    {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
    {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
    {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
    {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
    {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
    {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
    {1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
    {1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1, 1},
  };

  tile_chunk tile_chunk;
  tile_chunk.tiles = (uint32*) temp_tiles;
  world.tile_chunks = &tile_chunk;
  world.tile_chunk_count_x = 1;
  world.tile_chunk_count_y = 1;

  real32 player_height = 1.4f;
  real32 player_width = 0.75f*player_height;
  real32 lower_left_x = 0;
  real32 lower_left_y = (real32)buffer->height;

  for(int controller_index = 0; controller_index < ArrayCount(input->controllers); controller_index++) {
    game_controller_input *controller  = get_controller(input, controller_index);
    if (controller->is_analog) {

    }
    else {
      real32 velocity = 10.0f * input->dt;
      real32 player_dx = 0.0f;
      real32 player_dy = 0.0f;
      if (controller->move_up.ended_down) {
        player_dy = 1.0f;
      }
      if (controller->move_down.ended_down) {
        player_dy = -1.0f;
      }
      if (controller->move_left.ended_down) {
        player_dx = -1.0f;
      }
      if (controller->move_right.ended_down) {
        player_dx = 1.0f;
      }

      world_position player_pos = {};
      player_pos = state->player_position;
      player_pos.tile_rel_x = state->player_position.tile_rel_x + (player_dx*velocity);
      player_pos.tile_rel_y = state->player_position.tile_rel_y + (player_dy*velocity);
      player_pos = recanonicalize_position(&world, player_pos);

      world_position player_left_pos = player_pos;
      player_left_pos.tile_rel_x -= 0.5f*player_width;
      player_left_pos = recanonicalize_position(&world, player_left_pos);

      world_position player_right_pos = player_pos;
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

  //TODO: Maybe use
  //tile_chunk *tile_chunk = get_tile_chunk(&world, state->player_position.tile_chunk_x, state->player_position.tile_chunk_y);

  draw_rectangle(buffer, 0.0f, 0.0f, (real32)buffer->width, (real32)buffer->height, 0.9f, 0.5f, 1.0f);

  real32 center_x = 0.5f*((real32)buffer->width);
  real32 center_y = 0.5f*((real32)buffer->height);

  for (int32 rel_row = -10; rel_row < 10; rel_row++) {
    for (int32 rel_column = -20; rel_column < 20; rel_column++) {
      world_position player_pos = state->player_position;

      uint32 column = player_pos.abs_tile_x + rel_column;
      uint32 row = player_pos.abs_tile_y + rel_row;

      uint32 tile_value = get_tile_value(&world, column, row);
      real32 gray = 0.5f;
      if (tile_value == 1) {
        gray = 1.0f;
      }

      if ((column == state->player_position.abs_tile_x) && (row == state->player_position.abs_tile_y)) {
        gray = 0.3f;
      }

      real32 min_x = center_x - state->player_position.tile_rel_x*world.meters_to_pixels + ((real32)rel_column * world.tile_side_in_pixels);
      real32 min_y = center_y + state->player_position.tile_rel_y*world.meters_to_pixels - ((real32)rel_row * world.tile_side_in_pixels);
      real32 max_x = min_x + world.tile_side_in_pixels;
      real32 max_y = min_y - world.tile_side_in_pixels;
      draw_rectangle(buffer, min_x, max_y, max_x, min_y, gray, gray, gray);

    }
  }

  // DRAW Player
  real32 player_r = 0.6f;
  real32 player_g = 1.0f;
  real32 player_b = 0.5f;
  world_position pos = state->player_position;
  real32 player_left =
    center_x
    - 0.5f*player_width*world.meters_to_pixels;
  real32 player_top =
    center_y
    - player_height*world.meters_to_pixels;

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
