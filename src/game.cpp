#include "game.h"
#include "game_random.h"
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

internal void draw_bitmap(game_offscreen_buffer *buffer, loaded_bitmap *bitmap,
                          real32 real_x, real32 real_y,
                          int32 align_x = 0, int32 align_y = 0) {
  real_x -= (real32)align_x;
  real_y -= (real32)align_y;
  int32 min_x = round_real32_to_int32(real_x);
  int32 min_y = round_real32_to_int32(real_y);
  int32 max_x = round_real32_to_int32(real_x + (real32)bitmap->width);
  int32 max_y = round_real32_to_int32(real_y + (real32)bitmap->height);

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

  // source row needs to be changed due to clipping
  uint32 *source_row = bitmap->pixels + (bitmap->width*(bitmap->height-1));
  uint8 *dest_row = ((uint8*)buffer->memory + min_y*buffer->pitch + min_x*buffer->bytes_per_pixel);
  for (int y = min_y; y < max_y; y++) {

    uint32 *dest = (uint32*) dest_row;
    uint32 *source = (uint32 *) source_row;
    for (int x = min_x; x < max_x; x++) {
      real32 alpha = (real32)((*source >> 24) & 0XFF) / 255.0f;
      real32 source_red = (real32)((*source >> 16) & 0XFF);
      real32 source_green = (real32)((*source >> 8) & 0XFF);
      real32 source_blue = (real32)((*source >> 0) & 0XFF);

      real32 dest_red = (real32)((*dest >> 16) & 0XFF);
      real32 dest_green = (real32)((*dest >> 8) & 0XFF);
      real32 dest_blue = (real32)((*dest >> 0) & 0XFF);

      real32 r = (1.0f - alpha)*dest_red + alpha*source_red;
      real32 g = (1.0f - alpha)*dest_green + alpha*source_green;
      real32 b = (1.0f - alpha)*dest_blue + alpha*source_blue;


      *dest = (((uint32)(r + 0.5f) << 16) | ((uint32)(g + 0.5f) << 8) | ((uint32)(b + 0.5f)) << 0);

      dest++;
      source++;
    }
    dest_row += buffer->pitch;
    source_row -= bitmap->width;
  }
}

#pragma pack(push, 1)
struct bitmap_header {
  uint16 file_type;
  uint32 file_size;
  uint16 reserved1;
  uint16 reserved2;
  uint32 bitmap_offset;
  uint32 size;
  int32 width;
  int32 height;
  uint16 planes;
  uint16 bits_per_pixel;
  uint32 compression;
  uint32 size_of_bitmap;
  int32 horz_resolution;
  int32 vert_resolution;
  uint32 colors_used;
  uint32 colors_important;

  uint32 red_mask;
  uint32 green_mask;
  uint32 blue_mask;
};
#pragma pack(pop)

internal loaded_bitmap DEBUG_load_bmp(thread_context *thread,
                       debug_platform_read_entire_file *read_entire_file,
                       const char *file_name) {
  loaded_bitmap result = {};

  debug_read_file_result read_result = read_entire_file(thread, file_name);
  if (read_result.content_size != 0) {
    bitmap_header *header = (bitmap_header*)read_result.contents;
    uint32 *pixels = (uint32*)((uint8 *)read_result.contents + header->bitmap_offset);
    result.pixels = pixels;
    result.width = header->width;
    result.height = header->height;

    // NOTE: Byte order in memory is determined in the header
    uint32 red_mask = header->red_mask;
    uint32 green_mask = header->green_mask;
    uint32 blue_mask = header->blue_mask;
    uint32 alpha_mask = ~(red_mask | green_mask | blue_mask);


    bit_scan_result red_shift = find_least_significant_set_bit(red_mask);
    bit_scan_result green_shift = find_least_significant_set_bit(green_mask);
    bit_scan_result blue_shift = find_least_significant_set_bit(blue_mask);
    bit_scan_result alpha_shift = find_least_significant_set_bit(alpha_mask);

    Assert(red_shift.found);
    Assert(green_shift.found);
    Assert(blue_shift.found);
    Assert(alpha_shift.found);

    uint32 *source_dest = pixels;
    for (int32 y = 0; y < header->height; y++) {
      for (int32 x = 0; x < header->width; x++) {
        uint32 c = *source_dest;

        uint32 red = (((c >> red_shift.index) & 0xFF) << 16);
        uint32 green = (((c >> green_shift.index) & 0xFF) << 8);
        uint32 blue = (((c >> blue_shift.index) & 0xFF) << 0);
        uint32 alpha = (((c >> alpha_shift.index) & 0xFF) << 24);

        *source_dest++ = alpha | red | blue | green;
      }
    }
  }
  return result;

}

global_variable uint32 random_number_index = 0;

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
    state->backdrop = DEBUG_load_bmp(thread, memory->debug_platform_read_entire_file, "bmp/test_background.bmp");

    hero_bitmaps *bitmap = state->hero_bitmaps;
    bitmap->head = DEBUG_load_bmp(thread, memory->debug_platform_read_entire_file, "bmp/test_hero_right_head.bmp");
    bitmap->cape = DEBUG_load_bmp(thread, memory->debug_platform_read_entire_file, "bmp/test_hero_right_cape.bmp");
    bitmap->torso = DEBUG_load_bmp(thread, memory->debug_platform_read_entire_file, "bmp/test_hero_right_torso.bmp");
    bitmap->align_x = 72;
    bitmap->align_y = 182;
    bitmap++;


    bitmap->head = DEBUG_load_bmp(thread, memory->debug_platform_read_entire_file, "bmp/test_hero_back_head.bmp");
    bitmap->cape = DEBUG_load_bmp(thread, memory->debug_platform_read_entire_file, "bmp/test_hero_back_cape.bmp");
    bitmap->torso = DEBUG_load_bmp(thread, memory->debug_platform_read_entire_file, "bmp/test_hero_back_torso.bmp");
    bitmap->align_x = 72;
    bitmap->align_y = 182;
    bitmap++;

    bitmap->head = DEBUG_load_bmp(thread, memory->debug_platform_read_entire_file, "bmp/test_hero_left_head.bmp");
    bitmap->cape = DEBUG_load_bmp(thread, memory->debug_platform_read_entire_file, "bmp/test_hero_left_cape.bmp");
    bitmap->torso = DEBUG_load_bmp(thread, memory->debug_platform_read_entire_file, "bmp/test_hero_left_torso.bmp");
    bitmap->align_x = 72;
    bitmap->align_y = 182;
    bitmap++;

    bitmap->head = DEBUG_load_bmp(thread, memory->debug_platform_read_entire_file, "bmp/test_hero_front_head.bmp");
    bitmap->cape = DEBUG_load_bmp(thread, memory->debug_platform_read_entire_file, "bmp/test_hero_front_cape.bmp");
    bitmap->torso = DEBUG_load_bmp(thread, memory->debug_platform_read_entire_file, "bmp/test_hero_front_torso.bmp");
    bitmap->align_x = 72;
    bitmap->align_y = 182;
    bitmap++;


    memory->is_initialized = true;
    state->player_position.abs_tile_x = 3;
    state->player_position.abs_tile_y = 3;
    state->player_position.offset_x = 5.0f;
    state->player_position.offset_y = 5.0f;

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

    // Note: This is set to be using 256x256 tile chunks
    tile_map->chunk_shift = 4;
    tile_map->chunk_mask = (1 << tile_map->chunk_shift) - 1;
    tile_map->chunk_dim = (1 << tile_map->chunk_shift);

    tile_map->tile_chunk_count_x = 128;
    tile_map->tile_chunk_count_y = 128;
    tile_map->tile_chunk_count_z = 2;
    tile_map->tile_chunks = PushArray(&state->world_arena,
                                      tile_map->tile_chunk_count_x*tile_map->tile_chunk_count_y*tile_map->tile_chunk_count_z,
                                      tile_chunk);

    uint32 tiles_per_height = 9;
    uint32 tiles_per_width = 17;

    uint32 screen_x = 0;
    uint32 screen_y = 0;
    uint32 abs_tile_z = 0;

    bool32 door_left = false;
    bool32 door_right = false;
    bool32 door_top = false;
    bool32 door_bottom = false;

    bool32 stairs_up = false;
    bool32 stairs_down = false;
    bool32 previous_stairs_was_up = false;

    for(uint32 screen_index = 0; screen_index < 100; screen_index++) {

      Assert(random_number_index < ArrayCount(random_number_table));
      uint32 random_choice;
      if (stairs_up || stairs_down) {
        random_choice = random_number_table[random_number_index++] % 2;
      }
      else {
        random_choice = random_number_table[random_number_index++] % 3;
      }
      bool32 created_stairs = false;
      if (random_choice == 2) {
        created_stairs = true;
        if (abs_tile_z == 0) {
          stairs_up = true;
        }
        else {
          stairs_down = true;
        }

      }
      else if (random_choice == 1) {
        door_top = true;
      }
      else {
        door_right = true;
      }

      for (uint32 tile_y = 0; tile_y < tiles_per_height; tile_y++) {
        for (uint32 tile_x = 0; tile_x < tiles_per_width; tile_x++) {
          uint32 abs_tile_x = screen_x*tiles_per_width + tile_x;
          uint32 abs_tile_y = screen_y*tiles_per_height + tile_y;

          uint32 tile_value = WALKABLE;
          if ((tile_x == 0) && (!door_left || (tile_y != (tiles_per_height/2)))) {
            tile_value = WALL;
          }
          if ((tile_x == tiles_per_width-1) && (!door_right || (tile_y != tiles_per_height/2))) {
            tile_value = WALL;
          }
          if ((tile_y == 0) && (!door_bottom || (tile_x != (tiles_per_width/2)))) {
            tile_value = WALL;
          }
          if ((tile_y == tiles_per_height-1) && (!door_top || (tile_x != (tiles_per_width/2)))) {
            tile_value = WALL;
          }
          if ((tile_x == 5) && (tile_y == 5)) {
            if (stairs_down) {
              tile_value = STAIRS_DOWN;
            }
            else if (stairs_up) {
              tile_value = STAIRS_UP;
            }
          }

          set_tile_value(&state->world_arena, tile_map, abs_tile_x, abs_tile_y, abs_tile_z, tile_value);
        }
      }
      // TODO: Random number generator
      door_left = door_right;
      door_bottom = door_top;

      if (created_stairs){
        stairs_up = !stairs_up;
        stairs_down = !stairs_down;
      }
      else {
        stairs_up = false;
        stairs_down = false;
      }

      door_right = false;
      door_top = false;
      if (random_choice == 2) {
        if (abs_tile_z == 0) {
          abs_tile_z = 1;
        }
        else {
          abs_tile_z = 0;
        }
      }
      else if (random_choice == 1) {
        screen_y++;
      }
      else {
        screen_x++;
      }
    }
  }


  world *world = state->world;
  tile_map *tile_map = world->tile_map;
  real32 player_height = 1.4f;
  real32 player_width = 0.75f*player_height;
  int32 tile_side_in_pixels = 60;
  real32 meters_to_pixels = (real32)tile_side_in_pixels / tile_map->tile_side_in_meters;

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
        state->hero_facing_direction = 1;
      }
      if (controller->move_down.ended_down) {
        player_dy = -1.0f;
        state->hero_facing_direction = 3;
      }
      if (controller->move_left.ended_down) {
        player_dx = -1.0f;
        state->hero_facing_direction = 2;
      }
      if (controller->move_right.ended_down) {
        player_dx = 1.0f;
        state->hero_facing_direction = 0;
      }

      if (controller->action_up.ended_down) {
        velocity *= 5;
      }

      tile_map_position new_player_pos = {};
      new_player_pos = state->player_position;
      new_player_pos.offset_x = state->player_position.offset_x + (player_dx*velocity);
      new_player_pos.offset_y = state->player_position.offset_y + (player_dy*velocity);
      new_player_pos = recanonicalize_position(tile_map, new_player_pos);

      tile_map_position new_player_left_pos = new_player_pos;
      new_player_left_pos.offset_x -= 0.5f*player_width;
      new_player_left_pos = recanonicalize_position(tile_map, new_player_left_pos);

      tile_map_position new_player_right_pos = new_player_pos;
      new_player_right_pos.offset_x += 0.5f*player_width;
      new_player_right_pos = recanonicalize_position(tile_map, new_player_right_pos);

      if (
          is_tile_map_point_empty(tile_map, new_player_pos) &&
          is_tile_map_point_empty(tile_map, new_player_left_pos) &&
          is_tile_map_point_empty(tile_map, new_player_right_pos)
          ) {
        if (!are_on_same_tile(&state->player_position, &new_player_pos)) {
          uint32 new_tile_value = get_tile_value(tile_map, new_player_pos);

          if (new_tile_value == STAIRS_UP) {
            new_player_pos.abs_tile_z++;
          }
          if (new_tile_value == STAIRS_DOWN) {
            new_player_pos.abs_tile_z--;
          }
        }

        state->player_position = new_player_pos;
      }
    }
  }

  //Todo: Maybe use
  //tile_chunk *tile_chunk = get_tile_chunk(tile_map, state->player_position.tile_chunk_x, state->player_position.tile_chunk_y);

  draw_rectangle(buffer, 0.0f, 0.0f, (real32)buffer->width, (real32)buffer->height, 0.9f, 0.5f, 1.0f);

  draw_bitmap(buffer, &state->backdrop, 0, 0);

  real32 screen_center_x = 0.5f*((real32)buffer->width);
  real32 screen_center_y = 0.5f*((real32)buffer->height);

  for (int32 rel_row = -10; rel_row < 10; rel_row++) {
    for (int32 rel_column = -20; rel_column < 20; rel_column++) {
      tile_map_position player_pos = state->player_position;

      uint32 column = player_pos.abs_tile_x + rel_column;
      uint32 row = player_pos.abs_tile_y + rel_row;

      uint32 tile_value = get_tile_value(tile_map, column, row, player_pos.abs_tile_z);
      if (tile_value > 1) {
        real32 gray = 0.5f;
        if (tile_value == WALL) {
          gray = 1.0f;
        }

        if (tile_value == STAIRS_UP || tile_value == STAIRS_DOWN) {
          gray = 0.1f;
        }

        if ((column == state->player_position.abs_tile_x) && (row == state->player_position.abs_tile_y)) {
          gray = 0.3f;
        }

        real32 cen_x = screen_center_x - state->player_position.offset_x*meters_to_pixels + ((real32)rel_column * tile_side_in_pixels);
        real32 cen_y = screen_center_y + state->player_position.offset_y*meters_to_pixels - ((real32)rel_row * tile_side_in_pixels);
        real32 min_x = cen_x - 0.5f*tile_side_in_pixels;
        real32 min_y = cen_y - 0.5f*tile_side_in_pixels;
        real32 max_x = cen_x + 0.5f*tile_side_in_pixels;
        real32 max_y = cen_y + 0.5f*tile_side_in_pixels;
        draw_rectangle(buffer, min_x, min_y, max_x, max_y, gray, gray, gray);
      }
    }
  }

  // DRAW Player
  real32 player_r = 0.6f;
  real32 player_g = 1.0f;
  real32 player_b = 0.5f;
  real32 player_ground_point_x = screen_center_x;
  real32 player_ground_point_y = screen_center_y;
  tile_map_position pos = state->player_position;
  real32 player_left = player_ground_point_x - 0.5f*player_width*meters_to_pixels;
  real32 player_top = player_ground_point_y - player_height*meters_to_pixels;

  uint32 facing_direction = 3;
  hero_bitmaps hero = state->hero_bitmaps[state->hero_facing_direction];
  draw_bitmap(buffer, &hero.torso, player_ground_point_x, player_ground_point_y, hero.align_x, hero.align_y);
  draw_bitmap(buffer, &hero.cape, player_ground_point_x, player_ground_point_y, hero.align_x, hero.align_y);
  draw_bitmap(buffer, &hero.head, player_ground_point_x, player_ground_point_y, hero.align_x, hero.align_y);

}

extern "C" GAME_GET_SOUND_SAMPLES(game_get_sound_samples_imp)
{
  game_state *state = (game_state *)memory->permanent_storage;
}
