#if !defined(GAME_H)

// Services that the platform layer provides to the game

// Services that the game provides to the platform layer

#include <stdint.h>
#include <math.h>

#define Pi32 3.14159265359f

#define local_persist static
#define global_variable static
#define internal static


typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef size_t memory_index;

typedef int32 bool32;

typedef float real32;
typedef double real64;

#if GAME_SLOW
#define Assert(expression) if(!(expression)) { *(int *)0 = 0;}
#else
#define Assert(expression)
#endif

#define Kilobytes(value) ((value)*1024)
#define Megabytes(value) (Kilobytes(value)*1024)
#define Gigabytes(value) (Megabytes(value)*1024)
#define Terabytes(value) (Gigabytes(value)*1024)

#define ArrayCount(Array) (sizeof(Array)/sizeof((Array)[0]))

inline uint32 safe_truncate_uint64(uint64 value) {
  Assert(value <= 0xFFFFFFFF);
  return (uint32) value;
}

struct thread_context {
  int place_holder;
};

#if GAME_INTERNAL

struct debug_read_file_result {
  uint32 content_size;
  void *contents;
};

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(thread_context *thread, void *memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(thread_context *thread, const char *file_name)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(thread_context *thread, const char *file_name, void *data, uint32 data_size)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

#endif;


struct game_offscreen_buffer {
  //  BITMAPINFO Info;
  void *memory;
  int   memory_size;
  int   width;
  int   height;
  int   bytes_per_pixel;
  int   pitch;
};

struct game_sound_output_buffer {
  int    samples_per_second;
  int    sample_count;
  int16 *samples;
  int    tone_hz;
};

struct game_button_state {
  int half_transition_count;
  bool32 ended_down;
};

struct game_controller_input {
  bool32 is_analog;
  bool32 is_connected;

  real32 stick_average_x;
  real32 stick_average_y;

  union
  {
    game_button_state buttons[13];
    struct {
      game_button_state move_up;
      game_button_state move_down;
      game_button_state move_left;
      game_button_state move_right;
      game_button_state action_up;
      game_button_state action_down;
      game_button_state action_left;
      game_button_state action_right;

      game_button_state left_shoulder;
      game_button_state right_shoulder;

      game_button_state back;
      game_button_state start;

      //Note: All buttons must be added above this line
      game_button_state terminator;
    };
  };
};

struct game_clocks {
  real32 seconds_elapsed;
};

struct game_input {
  uint32 mouse_bottons;
  int32 mouse_x;
  int32 mouse_y;
  int32 mouse_z;
  game_button_state mouse_buttons[2];
  game_controller_input controllers[5]; // 4 + keyboard
  real32 dt;
};
inline game_controller_input *get_controller(game_input *input, int controller_index) {
  Assert(controller_index < ArrayCount(input->controllers));
  return &input->controllers[controller_index];
}

struct game_memory {
  bool32 is_initialized;
  uint64 permanent_storage_size;
  void *permanent_storage; // NOTE: Must be cleared to zero
  uint64 transient_storage_size;
  void *transient_storage;

  debug_platform_read_entire_file *debug_platform_read_entire_file;
  debug_platform_free_file_memory *debug_platform_free_file_memory;
  debug_platform_write_entire_file *debug_platform_write_entire_file;
};

/**
    Dynamic load Game Code
 **/

#define GAME_UPDATE_AND_RENDER(name) void name(thread_context *thread, game_memory *memory, game_offscreen_buffer *buffer, game_input *input)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);


#define GAME_GET_SOUND_SAMPLES(name) void name(thread_context *thread, game_memory *memory, game_sound_output_buffer *sound_buffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

/** Memory **/
struct memory_arena {
  memory_index size;
  uint8 *base;
  memory_index used;
};


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

/** END Memory **/

#include "game_intrinsics.h"
#include "game_tile.h"

struct world {
  tile_map *tile_map;
};

struct loaded_bitmap {
  int32 width;
  int32 height;
  uint32 *pixels;
};

struct hero_bitmaps {
  int32 align_x;
  int32 align_y;
  loaded_bitmap head;
  loaded_bitmap cape;
  loaded_bitmap torso;
};

struct game_state {
  world *world;

  tile_map_position camera_position;
  tile_map_position player_position;
  memory_arena world_arena;

  loaded_bitmap backdrop;
  hero_bitmaps hero_bitmaps[4];
  uint32 hero_facing_direction;
};


#define GAME_H
#endif


//void game_update_and_render(game_memory *memory, game_offscreen_buffer *buffer, game_input *input);
