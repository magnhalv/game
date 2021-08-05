#include "game.h"
#include "game_tile.cpp"

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

internal void initialize_arena(memory_arena *arena, memory_index size, uint8 *base) {
  arena->size = size;
  arena->base = base;
  arena->used = 0;
}

#define PushStruct(arena, type) (type *)push_size_(arena, sizeof(type))
#define PushArray(arena, count, type) (type *)push_size_(arena, (count*sizeof(type)))
void *push_size_(memory_arena *arena, memory_index size) {

  //TODO: Clear to zero
  Assert((arena->used + size) <= arena->size);
  void *result = arena->base + arena->used;
  arena->used += size;

  return result;
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

    initialize_arena(&state->world_arena,
                     memory->permanent_storage_size - sizeof(game_state),
                     (uint8*)memory->permanent_storage + sizeof(game_state));

    state->world = PushStruct(&state->world_arena, world);
    world *world = state->world;
    world->tile_map = PushStruct(&state->world_arena, tile_map);
    tile_map *tile_map = world->tile_map;


#define TILE_MAP_DIM_X 256
#define TILE_MAP_DIM_Y 256
    tile_map->tile_side_in_meters = 1.4f;
    tile_map->tile_side_in_pixels = 64;
    tile_map->meters_to_pixels = (real32)tile_map->tile_side_in_pixels / tile_map->tile_side_in_meters;

    // Note: This is set to be using 256x256 tile chunks
    tile_map->chunk_shift = 8;
    tile_map->chunk_mask = (1 << tile_map->chunk_shift) - 1;
    tile_map->chunk_dim = (1 << tile_map->chunk_shift);

    tile_map->tile_chunk_count_x = 64;
    tile_map->tile_chunk_count_y = 64;
    tile_map->tile_chunks = PushArray(&state->world_arena,
                                      tile_map->tile_chunk_count_x*tile_map->tile_chunk_count_y,
                                      tile_chunk);

    for (uint32 y = 0; y < tile_map->tile_chunk_count_y; y++) {
        for (uint32 x = 0; x < tile_map->tile_chunk_count_x; x++) {
        tile_map->tile_chunks[y*tile_map->tile_chunk_count_x + x].tiles =
          PushArray(&state->world_arena, tile_map->chunk_dim*tile_map->chunk_dim, uint32);
      }
    }

    uint32 tiles_per_height = 9;
    uint32 tiles_per_width = 17;

    for (uint32 screen_y = 0; screen_y < 32; screen_y++) {
      for (uint32 screen_x = 0; screen_x < 32; screen_x++) {
        for (uint32 tile_y = 0; tile_y < tiles_per_height; tile_y++) {
          for (uint32 tile_x = 0; tile_x < tiles_per_width; tile_x++) {
            uint32 abs_tile_x = screen_x*tiles_per_width + tile_x;
            uint32 abs_tile_y = screen_y*tiles_per_height + tile_y;

            uint32 tile_value = 0;
            if (tile_x == 0 || tile_x == (tiles_per_width - 1)) {
              tile_value = 1;
            }

            if (tile_y == 0 || tile_y == (tiles_per_height - 1)) {
              tile_value = 1;
            }

            set_tile_value(&state->world_arena, tile_map, abs_tile_x, abs_tile_y, tile_value);
          }
        }
      }
    }
  }

  world *world = state->world;
  tile_map *tile_map = world->tile_map;
  real32 player_height = 1.4f;
  real32 player_width = 0.75f*player_height;

  real32 lower_left_x = 0;
  real32 lower_left_y = (real32)buffer->height;

  for(int controller_index = 0; controller_index < ArrayCount(input->controllers); controller_index++) {
    game_controller_input *controller  = get_controller(input, controller_index);
    if (controller->is_analog) {

    }
    else {
      real32 velocity = 5.0f * input->dt;
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

      if (controller->action_up.ended_down) {
        velocity *= 5;
      }

      tile_map_position player_pos = {};
      player_pos = state->player_position;
      player_pos.tile_rel_x = state->player_position.tile_rel_x + (player_dx*velocity);
      player_pos.tile_rel_y = state->player_position.tile_rel_y + (player_dy*velocity);
      player_pos = recanonicalize_position(tile_map, player_pos);

      tile_map_position player_left_pos = player_pos;
      player_left_pos.tile_rel_x -= 0.5f*player_width;
      player_left_pos = recanonicalize_position(tile_map, player_left_pos);

      tile_map_position player_right_pos = player_pos;
      player_right_pos.tile_rel_x += 0.5f*player_width;
      player_right_pos = recanonicalize_position(tile_map, player_right_pos);

      if (
          is_tile_map_point_empty(tile_map, player_pos) &&
          is_tile_map_point_empty(tile_map, player_left_pos) &&
          is_tile_map_point_empty(tile_map, player_right_pos)
          ) {

        state->player_position = player_pos;
      }
    }
  }

  //Todo: Maybe use
  //tile_chunk *tile_chunk = get_tile_chunk(tile_map, state->player_position.tile_chunk_x, state->player_position.tile_chunk_y);

  draw_rectangle(buffer, 0.0f, 0.0f, (real32)buffer->width, (real32)buffer->height, 0.9f, 0.5f, 1.0f);

  real32 screen_center_x = 0.5f*((real32)buffer->width);
  real32 screen_center_y = 0.5f*((real32)buffer->height);

  for (int32 rel_row = -10; rel_row < 10; rel_row++) {
    for (int32 rel_column = -20; rel_column < 20; rel_column++) {
      tile_map_position player_pos = state->player_position;

      uint32 column = player_pos.abs_tile_x + rel_column;
      uint32 row = player_pos.abs_tile_y + rel_row;

      uint32 tile_value = get_tile_value(tile_map, column, row);
      real32 gray = 0.5f;
      if (tile_value == 1) {
        gray = 1.0f;
      }

      if ((column == state->player_position.abs_tile_x) && (row == state->player_position.abs_tile_y)) {
        gray = 0.3f;
      }

      real32 cen_x = screen_center_x - state->player_position.tile_rel_x*tile_map->meters_to_pixels + ((real32)rel_column * tile_map->tile_side_in_pixels);
      real32 cen_y = screen_center_y + state->player_position.tile_rel_y*tile_map->meters_to_pixels - ((real32)rel_row * tile_map->tile_side_in_pixels);
      real32 min_x = cen_x - 0.5f*tile_map->tile_side_in_pixels;
      real32 min_y = cen_y - 0.5f*tile_map->tile_side_in_pixels;
      real32 max_x = cen_x + 0.5f*tile_map->tile_side_in_pixels;
      real32 max_y = cen_y + 0.5f*tile_map->tile_side_in_pixels;
      draw_rectangle(buffer, min_x, min_y, max_x, max_y, gray, gray, gray);

    }
  }

  // DRAW Player
  real32 player_r = 0.6f;
  real32 player_g = 1.0f;
  real32 player_b = 0.5f;
  tile_map_position pos = state->player_position;
  real32 player_left =
    screen_center_x
    - 0.5f*player_width*tile_map->meters_to_pixels;
  real32 player_top =
    screen_center_y
    - player_height*tile_map->meters_to_pixels;

  draw_rectangle(buffer,
                 player_left,
                 player_top,
                 player_left + player_width*tile_map->meters_to_pixels,
                 player_top + player_height*tile_map->meters_to_pixels,
                 player_r, player_g, player_b);

}

extern "C" GAME_GET_SOUND_SAMPLES(game_get_sound_samples_imp)
{
  game_state *state = (game_state *)memory->permanent_storage;
}
