#if !defined(GAME_H)

// Services that the platform layer provides to the game

// Services that the game provides to the platform layer

#define ArrayCount(Array) (sizeof(Array)/sizeof((Array)[0]))

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
  real32 start_x;
  real32 end_x;
  real32 min_x;
  real32 max_x;

  real32 start_y;
  real32 end_y;
  real32 min_y;
  real32 max_y;

  union
  {
    game_button_state buttons[6];
    struct {
      game_button_state up;
      game_button_state down;
      game_button_state left;
      game_button_state right;
      game_button_state left_shoulder;
      game_button_state right_shoulder;
    };
  };
};

struct game_input {
  game_controller_input controllers[4];
};

void GameUpdateAndRender(game_offscreen_buffer *buffer,
                         game_sound_output_buffer *sound_buffer,
                         game_input *input);

#define GAME_H
#endif
