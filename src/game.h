#if !defined(GAME_H)

// Services that the platform layer provides to the game

// Services that the game provides to the platform layer

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

#if GAME_INTERNAL

struct debug_read_file_result {
  uint32 content_size;
  void *contents;
};

debug_read_file_result DEBUGplatform_read_entire_file(char *file_name);
void DEBUGplatform_free_file_memory(void *memory);
bool32 DEBUGplatform_write_entire_file(char *file_name, void *data, uint32 data_size);
#endif;


struct game_offscreen_buffer {
  //  BITMAPINFO Info;
  void *Memory;
  int   MemorySize;
  int   Width;
  int   Height;
  int   BytesPerPixel;
  int   Pitch;
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
  game_controller_input controllers[5]; // 4 + keyboard
  //  game_clocks clocks;
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

};

void GameUpdateAndRender(game_memory *memory,
                         game_offscreen_buffer *buffer,
                         game_sound_output_buffer *sound_buffer,
                         game_input *input);

struct game_state {
  int x_offset;
  int y_offset;
  int tone_hz;
};

#define GAME_H
#endif
