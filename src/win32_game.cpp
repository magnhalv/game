/*
   GAME_INTERNAL:
   0 - Build for public realeases
   1 - Build for development

   GAME_SLOW
   0 - No slow code allowed
   1 - Slow code allowed
 */


#include "game.h"

#include <strsafe.h>
#include <windows.h>
#include <xinput.h>
#include <dsound.h>
#include <intrin.h>
#include <malloc.h>
#include <fileapi.h>

#include "win32_game.h"

global_variable win32_offscreen_buffer global_back_buffer;
global_variable bool Running;
global_variable LPDIRECTSOUNDBUFFER globalSecondaryBuffer;
global_variable win32_sound_output global_sound_output = {};
global_variable int64 global_perf_counter_frequency;
global_variable bool32 global_pause = false;

#define AUDIO_DEBUG 0

/**
    UTIL
 **/
internal void concat_strings(int a_count, const char *a, int b_count, const char *b, int dest_count, char *dest) {
  Assert ((a_count + b_count) <= dest_count);
  for (int i = 0; i < a_count; i++) {
    *dest++ = a[i];
  }

  for (int i = 0; i < a_count; i++) {
    *dest++ = b[i];
  }

  *dest++ = 0;
}

internal int  string_length(const char *string) {
  int count = 0;
  while (*string++) {
    count++;
  }
  return count;
}

/**
   Dynamic load XINPUT
**/
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(xInputGetStateStub)
{
  return ERROR_DEVICE_NOT_CONNECTED;
}

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(xInputSetStateStub)
{
  return ERROR_DEVICE_NOT_CONNECTED;
}

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);


global_variable x_input_get_state *XInputGetState_ = xInputGetStateStub;
global_variable x_input_set_state *XInputSetState_ = xInputSetStateStub;
#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_


DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory_imp) {
  if (memory) {
    VirtualFree(memory, 0, MEM_RELEASE);
  }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file_imp) {
  HANDLE file_handle = CreateFileA(file_name,
                                   GENERIC_READ,
                                   FILE_SHARE_READ,
                                   0,
                                   OPEN_EXISTING,
                                   0,
                                   0);
  debug_read_file_result result = {};
  if (file_handle != INVALID_HANDLE_VALUE) {
    LARGE_INTEGER file_size;
    if(GetFileSizeEx(file_handle, &file_size)) {
      uint32 file_size_32 = safe_truncate_uint64(file_size.QuadPart);
      result.contents = VirtualAlloc(0, file_size_32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
      if (result.contents) {
        DWORD bytes_read;
        if (ReadFile(file_handle, result.contents, file_size_32, &bytes_read, 0) && file_size_32 == bytes_read) {
          result.content_size = file_size_32;
        }
        else {
          debug_platform_free_file_memory_imp(thread, result.contents);
          result.contents = 0;
        }
      }
    }
    else {

    }
    CloseHandle(file_handle);
  }
  return result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file_imp) {
  bool32 is_success = false;
  HANDLE file_handle = CreateFileA(file_name, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
  if (file_handle != INVALID_HANDLE_VALUE) {
    //uint32 file_size_32 = safe_truncate_uint64(file_size.QuadPart);
      DWORD bytes_written;
      if (WriteFile(file_handle, data, data_size, &bytes_written, NULL)) {
        is_success = data_size == bytes_written;
      }
      else {
        is_success = false;
      }
      CloseHandle(file_handle);
  }
  return is_success;
}

internal FILETIME win32_get_last_write_time(const char *filename) {
  FILETIME last_write_time = {};
  WIN32_FILE_ATTRIBUTE_DATA attributes;
  if (GetFileAttributesEx(filename, GetFileExInfoStandard, &attributes)) {
    last_write_time = attributes.ftLastWriteTime;
  }

  return last_write_time;
}

internal win32_game_code win32_load_game_code(const char *dll_path, const char *temp_dll_path) {
  win32_game_code result = {};
  result.last_write_time = win32_get_last_write_time(dll_path);

  bool32 is_copied = CopyFile(dll_path, temp_dll_path, FALSE);

  if (is_copied) {
    result.game_code_dll = LoadLibrary(temp_dll_path);
    if (result.game_code_dll) {
      result.game_update_and_render = (game_update_and_render*)GetProcAddress(result.game_code_dll, "game_update_and_render_imp");
      result.game_get_sound_samples = (game_get_sound_samples*)GetProcAddress(result.game_code_dll, "game_get_sound_samples_imp");
      result.is_valid = (result.game_update_and_render && result.game_get_sound_samples);
    }
  }

  if (!result.is_valid) {
    result.game_update_and_render = 0;
    result.game_get_sound_samples = 0;
  }

  return result;
}

internal void win32_unload_game_code(win32_game_code *game_code) {
  if (game_code->game_code_dll) {
    FreeLibrary(game_code->game_code_dll);
    game_code->game_code_dll = 0;
  }

  game_code->is_valid = false;
  game_code->game_update_and_render = 0;
  game_code->game_get_sound_samples = 0;
}

internal void win32_loadXinput(void) {
  HMODULE xInputLibrary = LoadLibrary("xinput1_4.dll");
  if (xInputLibrary) {
    XInputGetState = (x_input_get_state*)GetProcAddress(xInputLibrary, "XInputGetState");
    XInputSetState = (x_input_set_state*)GetProcAddress(xInputLibrary, "XInputSetState");
  }
}

internal void win32_initDSound(HWND window, uint32 samplesPerSec, uint32 bufferSize) {
  // Load the library

  HMODULE dSoundLibrary = LoadLibrary("dsound.dll");
  if (dSoundLibrary) {
    direct_sound_create *directSoundCreate = (direct_sound_create*)GetProcAddress(dSoundLibrary, "DirectSoundCreate");

    LPDIRECTSOUND directSound;
    if (directSoundCreate && SUCCEEDED(directSoundCreate(0, &directSound, 0))) {
      WAVEFORMATEX waveFormat = {};
      waveFormat.wFormatTag = WAVE_FORMAT_PCM;
      waveFormat.nChannels = 2;
      waveFormat.wBitsPerSample = 16;
      waveFormat.nSamplesPerSec = samplesPerSec;
      waveFormat.nBlockAlign = (waveFormat.nChannels*waveFormat.wBitsPerSample) / 8;
      waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec*waveFormat.nBlockAlign;
      waveFormat.cbSize = 0;

      if(SUCCEEDED(directSound->SetCooperativeLevel(window, DSSCL_PRIORITY))) {
        DSBUFFERDESC bufferDescription = {};
        bufferDescription.dwSize = sizeof(bufferDescription);
        bufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

        LPDIRECTSOUNDBUFFER primaryBuffer;
        if(SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription, &primaryBuffer, 0))) {
          if (SUCCEEDED(primaryBuffer->SetFormat(&waveFormat))) {
            OutputDebugStringA("Sound initialized primary");
          }
          else {
            // TODO Diagnostic
          }
        }
        else {
          // TODO: Diagnostic

        }
      }
      else {
        // TODO: Diagnostic
      }

      ///
      DSBUFFERDESC bufferDescription = {};
      bufferDescription.dwSize = sizeof(bufferDescription);
      bufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
      bufferDescription.dwBufferBytes = bufferSize;
      bufferDescription.lpwfxFormat = &waveFormat;
      if (SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription, &globalSecondaryBuffer, 0))) {
            OutputDebugStringA("Sound initialized secondary");
      }

    }
    else {
      // TODO: Diagnostic
    }
  }
  else {
    // TODO: Diagnostic
  }
}

internal win32_window_dimension win32_get_window_dimension(HWND window_handle) {
  win32_window_dimension result;
  RECT clientRect;
  GetClientRect(window_handle, &clientRect);
  result.width = clientRect.right - clientRect.left;
  result.height = clientRect.bottom - clientRect.top;
  return result;
}

internal void
win32_ResizeDIBSection(win32_offscreen_buffer *buffer, int width, int height) {
  // TODO: Bulletproof this
  // Maybe don't free first, free after, then free first if that fails.

  if (buffer->memory) {
    VirtualFree(buffer->memory, 0, MEM_RELEASE);
  }

  buffer->width = width;
  buffer->height = height;

  buffer->Info.bmiHeader.biSize = sizeof(buffer->Info.bmiHeader);
  buffer->Info.bmiHeader.biWidth = buffer->width;
  buffer->Info.bmiHeader.biHeight = -buffer->height;
  buffer->Info.bmiHeader.biPlanes = 1;
  buffer->Info.bmiHeader.biBitCount = 32;
  buffer->Info.bmiHeader.biCompression = BI_RGB;

  buffer->bytes_per_pixel = 4;
  buffer->memorySize = buffer->bytes_per_pixel*(buffer->width*buffer->height);
  buffer->memory = VirtualAlloc(0, buffer->memorySize, MEM_COMMIT, PAGE_READWRITE);
  buffer->pitch = buffer->width*buffer->bytes_per_pixel;
}

internal void
win32_DisplayBufferInWindows(HDC deviceContext,
                             int window_width,
                             int window_height,
                             win32_offscreen_buffer buffer,
                             int x,
                             int y,
                             int width,
                             int height) {
  int offset = 10;
  StretchDIBits(deviceContext,
                //                x, y, width, height,
                //                x, y, width, height,
                offset, offset, buffer.width, buffer.height,
                0, 0, buffer.width, buffer.height,
                buffer.memory,
                &buffer.Info,
                DIB_RGB_COLORS,
                SRCCOPY
                    );
}

internal LRESULT CALLBACK WindowProcCallback(HWND window,
                            UINT message,
                            WPARAM wParam,
                            LPARAM lParam ) {
  LRESULT result = 0;
  switch(message) {
  case WM_SIZE: {
  } break;
  case WM_CLOSE:
    Running = false;
  case WM_DESTROY:
    Running = false;
    break;
  case WM_ACTIVATEAPP:
    if (wParam == TRUE) {
      SetLayeredWindowAttributes(window, RGB(0, 0, 0), 255, LWA_ALPHA);
    }
    else {
      SetLayeredWindowAttributes(window, RGB(0, 0, 0), 64, LWA_ALPHA);
    }
    break;

  case WM_SYSKEYDOWN:
  case WM_SYSKEYUP:
  case WM_KEYDOWN:
  case WM_KEYUP:
    Assert("Keyboard event came through a non-disptach event");
    break;
  case WM_PAINT: {
    PAINTSTRUCT paint;
    HDC deviceContext = BeginPaint(window,&paint);
    int x = paint.rcPaint.left;
    int y = paint.rcPaint.top;
    int width = paint.rcPaint.right - x;
    int height = paint.rcPaint.bottom - y;
    local_persist DWORD color = WHITENESS;
    PatBlt(deviceContext, x, y, width, height, color);

    win32_window_dimension dimension = win32_get_window_dimension(window);
    PatBlt(deviceContext, 0, 0, dimension.width, dimension.height, BLACKNESS);
    win32_DisplayBufferInWindows(deviceContext, dimension.width, dimension.height, global_back_buffer, x, y, width, height);
    if (color == WHITENESS) {
      color = BLACKNESS;
    }
    else {
      color = WHITENESS;
    }

    EndPaint(window, &paint);
  } break;
  default:
      result = DefWindowProc(window, message, wParam, lParam);
    break;
  }
  return result;
}

void win32_fill_sound_buffer(win32_sound_output *sound_output,
                           DWORD               byteToLock,
                           DWORD               bytesToWrite,
                           game_sound_output_buffer *source_buffer) {
  VOID *region1;
  DWORD region1Size;
  VOID *region2;
  DWORD region2Size;

  if (SUCCEEDED(globalSecondaryBuffer->Lock(byteToLock, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0))) {
    DWORD region1SampleCount = region1Size/sound_output->BytesPerSample;
    DWORD region2SampleCount = region2Size/sound_output->BytesPerSample;


    int16 *source_samples = source_buffer->samples;
    int16 *region1_destination = (int16*)region1;
    for (DWORD sampleIndex = 0; sampleIndex < region1SampleCount; sampleIndex++) {
      *region1_destination++ = *source_samples++;
      *region1_destination++ = *source_samples++;
      sound_output->RunningSampleIndex++;
    }

    int16 *region2_destination = (int16*)region2;
    for (DWORD sampleIndex = 0; sampleIndex < region2SampleCount; sampleIndex++) {
      *region2_destination++ = *source_samples++;
      *region2_destination++ = *source_samples++;
      sound_output->RunningSampleIndex++;
    }

    globalSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
  }
}

void win32_clear_sound_buffer(win32_sound_output *sound_output)
{
  VOID *region1;
  DWORD region1Size;
  VOID *region2;
  DWORD region2Size;

  if (SUCCEEDED(globalSecondaryBuffer->Lock(0, sound_output->SecondaryBufferSize, &region1, &region1Size, &region2, &region2Size, 0))) {
    uint8 *dest_sample = (uint8*) region1;
    for (DWORD i = 0; i < region1Size; i++) {
      *dest_sample++ = 0;
    }
    globalSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
  }
}

internal void win32_get_exe_filename(win32_state *state) {
  GetModuleFileNameA(0, state->exe_file_path, sizeof(state->exe_file_path));
  state->exe_filename = state->exe_file_path;
  for (char *scan = state->exe_file_path; *scan; scan++) {
    if (*scan == '\\') {
      state->exe_filename = scan + 1;
    }
  }
}

internal void win32_build_exe_path_filename (win32_state *state, const char *filename, int dest_count, char *dest) {
  int count_until_root_dir = (int)(state->exe_filename - state->exe_file_path);
  concat_strings(count_until_root_dir, state->exe_file_path,
                 string_length(filename), filename,
                 dest_count, dest);
}

internal void win32_get_recording_file_location(win32_state *state, bool32 isInputStream, int slot_index, int dest_path, char *dest) {
  char filename[64];
  wsprintf(filename, "recording_%d_%s.hmi", slot_index, isInputStream ? "input" : "state");
  win32_build_exe_path_filename(state, filename, dest_path, dest);
}

internal win32_replay_buffer* win32_get_replay_buffer(win32_state *state, int unsigned index) {
  Assert(index < ArrayCount(state->replay_buffers));
  win32_replay_buffer *replay_buffer = &state->replay_buffers[index];
  return replay_buffer;
}

internal void win32_begin_recording_input(win32_state *state, int input_recording_index) {
  Assert(input_recording_index >= 1);
  win32_replay_buffer *replay_buffer = win32_get_replay_buffer(state, input_recording_index);
  if (replay_buffer->state_memory_block) {
    state->input_recording_index = input_recording_index;

    char filename[WIN32_STATE_FILE_NAME_COUNT];
    win32_get_recording_file_location(state, true, input_recording_index, sizeof(filename), filename);
    state->recording_handle = CreateFileA(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    CopyMemory(replay_buffer->state_memory_block, state->game_memory_block, state->total_size);
  }
}

internal void win32_end_recording_input(win32_state *state) {
  CloseHandle(state->recording_handle);
  state->input_recording_index = 0;
}

internal void win32_begin_playback_input(win32_state *state, int input_playback_index) {
  Assert(input_playback_index >= 1);
  win32_replay_buffer *replay_buffer = win32_get_replay_buffer(state, input_playback_index);
  if (replay_buffer->state_memory_block) {
    state->input_playback_index = input_playback_index;

    char filename[WIN32_STATE_FILE_NAME_COUNT];
    win32_get_recording_file_location(state, true, input_playback_index, sizeof(filename), filename);
    state->playback_handle = CreateFileA(filename, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    CopyMemory(state->game_memory_block, replay_buffer->state_memory_block, state->total_size);
  }
}

internal void win32_end_playback_input(win32_state *state) {
  CloseHandle(state->playback_handle);
  state->input_playback_index = 0;
}

internal void win32_signal_end_playback_input(win32_state *state) {
  state->input_playback_index = -1;
}

internal void win32_record_input(win32_state *state, game_input *input) {
  DWORD bytes_written;
  WriteFile(state->recording_handle, input, sizeof(*input), &bytes_written, 0);
}

internal void win32_playback_input(win32_state *state, game_input *input) {
  DWORD bytes_read;
  if (ReadFile(state->playback_handle, input, sizeof(*input), &bytes_read, 0)) {
    if (bytes_read == 0) {
      int playing_index = state->input_playback_index;
      win32_end_playback_input(state);
      win32_begin_playback_input(state, playing_index);
      ReadFile(state->playback_handle, input, sizeof(*input), &bytes_read, 0);
    }
  }
  else {

  }
}

internal void win32_process_x_input_button(DWORD x_input_button_state,
                                           game_button_state *old_state,
                                           DWORD button_bit,
                                           game_button_state *new_state) {
  new_state->ended_down = ((x_input_button_state & button_bit) == button_bit);
  new_state->half_transition_count = (old_state->ended_down != new_state->ended_down) ? 1 : 0;
}

internal void win32_process_keyboard_message(game_button_state *new_state,
                                             bool32 is_down) {
  if (new_state->ended_down != is_down) {
    new_state->ended_down = is_down;
    new_state->half_transition_count++;
  }
}



internal void win32_process_pending_messages(win32_state *win32_state, game_controller_input *keyboard_controller) {
  MSG message;
  while(PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
    switch (message.message) {
    case WM_QUIT:
      {
        Running = false;
      }
      break;
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
      Assert("Keyboard event came through a non-disptach event");
      {
        uint32 vk_code = (uint32)message.wParam;
        bool32 was_down = ((message.lParam & (1 << 30)) != 0);
        bool32 is_down = ((message.lParam & (1 << 31)) == 0);
        if (was_down != is_down) {
          if (vk_code == 'W') {
            win32_process_keyboard_message(&keyboard_controller->move_up, is_down);
          }
          if (vk_code == 'A') {
            win32_process_keyboard_message(&keyboard_controller->move_left, is_down);
          }
          if (vk_code == 'S') {
            win32_process_keyboard_message(&keyboard_controller->move_down, is_down);
          }
          if (vk_code == 'D') {
            win32_process_keyboard_message(&keyboard_controller->move_right, is_down);

          }
          if (vk_code == VK_UP) {

          }
          if (vk_code == VK_DOWN) {

          }
          if (vk_code == VK_RIGHT) {
          }
          if (vk_code == VK_LEFT) {
          }
          if (vk_code == VK_ESCAPE) {
            Running = false;
          }
          if (vk_code == VK_SPACE) {
          }
#if GAME_INTERNAL
          if (vk_code == 'P' && is_down) {
            global_pause = !global_pause;
          }
          if (vk_code == 'R' && is_down) {
            if (win32_state->input_recording_index == 0) {
              win32_begin_recording_input(win32_state, 1);
            }
            else {
              win32_end_recording_input(win32_state);
            }
          }
          if (vk_code == 'T' && is_down) {
            if (win32_state->input_recording_index == 0 && win32_state->input_playback_index == 0) {
              win32_begin_playback_input(win32_state, 1);
            }
            else if (win32_state->input_playback_index) {
              win32_signal_end_playback_input(win32_state);
            }

          }
#endif

        }
      }
      break;
    default:
      TranslateMessage(&message);
      DispatchMessageA(&message);
    }
  }
}

internal real32 win32_process_x_input_stick_value(SHORT stick_value, SHORT dead_zone) {
  if(stick_value < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
    return (real32)stick_value / 32768.0f;
  }
  else if (stick_value > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
    return (real32)stick_value / 32767.0f;
  }
  else {
    return 0;
  }
}

inline LARGE_INTEGER win32_get_wall_clock() {
  LARGE_INTEGER result;
  QueryPerformanceCounter(&result);
  return result;
}

inline real32 win32_get_seconds_elapsed(LARGE_INTEGER start, LARGE_INTEGER end) {
  return ((real32)(end.QuadPart - start.QuadPart) / (real32)global_perf_counter_frequency);
}

inline void win32_debug_draw_vertical(win32_offscreen_buffer *back_buffer, int x, int top, int bottom, uint32 color) {
  if (top <= 0) {
    top = 0;
  }

  if (bottom >= back_buffer->height) {
    bottom = back_buffer->height;
  }

  if ((x >= 0) && (x < back_buffer->width)) {
    uint8 *pixel = (uint8 *)back_buffer->memory + x*back_buffer->bytes_per_pixel + top*back_buffer->pitch;
    for (int y = top; y < bottom; y++) {
      *(uint32 *)pixel = color;
      pixel += back_buffer->pitch;
    }
  }
}

inline void win32_draw_sound_buffer_marker(win32_offscreen_buffer *back_buffer,
                                           win32_sound_output *sound_output,
                                           real32 c, int pad_x, int top, int bottom,
                                           DWORD value, uint32 color) {
    int x = pad_x + (int)(c * (real32)value);
    win32_debug_draw_vertical(back_buffer, x, top, bottom, color);
}

inline void  win32_debug_sync_display(win32_offscreen_buffer *back_buffer,
                                      int nof_markers,
                                      win32_debug_time_marker *markers,
                                      int current_marker_index,
                                      win32_sound_output *sound_output,
                                      real32 target_seconds_per_frame
                                      ) {
  int pad_x = 16;
  int pad_y = 16;

  int line_height = 64;


  real32 c = (real32)(back_buffer->width- 2*pad_x) / (real32)sound_output->SecondaryBufferSize;
  for(int marker_index = 0; marker_index < nof_markers; marker_index++) {
    int top = pad_y;
    int bottom = pad_y + line_height;
    DWORD play_color = 0xFFFFFFFF;
    DWORD write_color = 0xFFFF0000;
    DWORD expected_flip_color = 0xFFFFFF00;
    DWORD play_window_color = 0xFFFF00FF;

    win32_debug_time_marker *current_marker = &markers[marker_index];
    Assert(current_marker->output_play_cursor < sound_output->SecondaryBufferSize);
    Assert(current_marker->output_write_cursor < sound_output->SecondaryBufferSize);
    Assert(current_marker->output_location < sound_output->SecondaryBufferSize);
    Assert(current_marker->output_byte_count < sound_output->SecondaryBufferSize);
    Assert(current_marker->flip_play_cursor < sound_output->SecondaryBufferSize);
    Assert(current_marker->flip_write_cursor < sound_output->SecondaryBufferSize);

    if (marker_index == current_marker_index) {
      top += line_height + pad_y;
      bottom += line_height + pad_y;
      int first_top = bottom;

      win32_draw_sound_buffer_marker(back_buffer, sound_output, c, pad_x, top, bottom, current_marker->output_play_cursor, play_color);
      win32_draw_sound_buffer_marker(back_buffer, sound_output, c, pad_x, top, bottom, current_marker->output_write_cursor, write_color);

      top += line_height + pad_y;
      bottom += line_height + pad_y;

      win32_draw_sound_buffer_marker(back_buffer, sound_output, c, pad_x, top, bottom, current_marker->output_location, play_color);
      win32_draw_sound_buffer_marker(back_buffer, sound_output, c, pad_x, top, bottom, current_marker->output_location + current_marker->output_byte_count, write_color);

      top += line_height + pad_y;
      bottom += line_height + pad_y;

      win32_draw_sound_buffer_marker(back_buffer, sound_output, c, pad_x, first_top, bottom, current_marker->expected_flip_play_cursor, expected_flip_color);
    }

    win32_draw_sound_buffer_marker(back_buffer, sound_output, c, pad_x, top, bottom, current_marker->flip_play_cursor, play_color);
    win32_draw_sound_buffer_marker(back_buffer, sound_output, c, pad_x, top, bottom, current_marker->flip_play_cursor + 480*sound_output->BytesPerSample, play_window_color);
    win32_draw_sound_buffer_marker(back_buffer, sound_output, c, pad_x, top, bottom, current_marker->flip_write_cursor, write_color);

  }
}

int CALLBACK WinMain(HINSTANCE instance,
	HINSTANCE PrevInstance,
	LPSTR CommandLine,
	int ShowCode)
{

  win32_state win32_state = {};
  win32_get_exe_filename(&win32_state);
  char dll_path[WIN32_STATE_FILE_NAME_COUNT];
  win32_build_exe_path_filename(&win32_state, "game.dll", sizeof(dll_path), dll_path);
  char temp_dll_path[WIN32_STATE_FILE_NAME_COUNT];
  win32_build_exe_path_filename(&win32_state, "game_temp.dll", sizeof(temp_dll_path), temp_dll_path);

  LARGE_INTEGER global_perf_counter_frequencyResult;
  QueryPerformanceFrequency(&global_perf_counter_frequencyResult);
  global_perf_counter_frequency = global_perf_counter_frequencyResult.QuadPart;

  // NOTE: Set the windows scheduler granularity to 1 ms so that our sleep can be more granular.
  UINT desired_scheduler_ms = 1;
  bool32 is_sleep_granular = timeBeginPeriod(desired_scheduler_ms) == TIMERR_NOERROR;
  is_sleep_granular = false;

  WNDCLASS windowClass = {};
  win32_loadXinput();
  windowClass.style = CS_HREDRAW|CS_VREDRAW;
  windowClass.lpfnWndProc = WindowProcCallback;
  windowClass.hInstance = instance;
  //  WindowClass.hIcon;
  windowClass.lpszClassName = "GameWindowClass";

  if (RegisterClass(&windowClass)) {
    HWND window =
      CreateWindowExA(WS_EX_TOPMOST|WS_EX_LAYERED,
                      windowClass.lpszClassName,
                      "Game",
                      WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                      CW_USEDEFAULT,
                      CW_USEDEFAULT,
                      CW_USEDEFAULT,
                      CW_USEDEFAULT,
                      0,
                      0,
                      instance,
                      0);
    win32_window_dimension dimension = win32_get_window_dimension(window);
    win32_ResizeDIBSection(&global_back_buffer, 1280, 720);
    if (window) {
      HDC refresh_dc = GetDC(window);
      int monitor_refresh_hz = 60;
      int win32_refresh_rate = GetDeviceCaps(refresh_dc, VREFRESH);
      ReleaseDC(window, refresh_dc);
      if (win32_refresh_rate > 1) {
        monitor_refresh_hz = win32_refresh_rate;
      }
      real32 game_update_hz = (monitor_refresh_hz / 2.0f);
      real32 target_seconds_per_frame = 1.0f / (real32)monitor_refresh_hz;

      // Setup memory, state, and audio
#if GAME_INTERNAL
      LPVOID base_address = (LPVOID)Terabytes((uint64)2);
#else
      LPVOID base_address = 0;
#endif

      thread_context thread = {};
      game_memory game_memory = {};
      game_memory.permanent_storage_size = Megabytes(64);
      game_memory.transient_storage_size = Gigabytes((uint64)1);

      win32_state.total_size = game_memory.permanent_storage_size + game_memory.transient_storage_size;
      win32_state.game_memory_block = VirtualAlloc(base_address,
                                                   win32_state.total_size,
                                                   MEM_RESERVE|MEM_COMMIT,
                                                   PAGE_READWRITE);

      game_memory.permanent_storage = win32_state.game_memory_block;
      game_memory.debug_platform_read_entire_file = debug_platform_read_entire_file_imp;
      game_memory.debug_platform_free_file_memory = debug_platform_free_file_memory_imp;
      game_memory.debug_platform_write_entire_file = debug_platform_write_entire_file_imp;
      global_sound_output.SamplesPerSec = 48000;
      global_sound_output.ToneHz = 256;
      global_sound_output.ToneVolume = 500;
      global_sound_output.RunningSampleIndex = 0;
      global_sound_output.WavePeriod = global_sound_output.SamplesPerSec/global_sound_output.ToneHz;
      global_sound_output.BytesPerSample = sizeof(int16)*2;
      global_sound_output.SecondaryBufferSize = global_sound_output.SamplesPerSec*global_sound_output.BytesPerSample;
      global_sound_output.TSine = 0;
      global_sound_output.safety_bytes = (int)(((real32)global_sound_output.SamplesPerSec*(real32)global_sound_output.BytesPerSample)/game_update_hz)/3;
      game_memory.transient_storage = ((uint8 *)game_memory.permanent_storage + game_memory.permanent_storage_size);

      for(int replay_index = 0; replay_index < ArrayCount(win32_state.replay_buffers); replay_index++) {
        win32_replay_buffer *replay_buffer = &win32_state.replay_buffers[replay_index];
        win32_get_recording_file_location(&win32_state, false, replay_index+1, sizeof(replay_buffer->file_path), replay_buffer->file_path);
        replay_buffer->file_handle = CreateFileA(replay_buffer->file_path, GENERIC_WRITE|GENERIC_READ, 0, 0, CREATE_ALWAYS, 0, 0);
        replay_buffer->memory_map = CreateFileMapping(replay_buffer->file_handle,
                                                       0,
                                                      PAGE_READWRITE,
                                                      (win32_state.total_size >> 32),
                                                      (win32_state.total_size & 0xFFFFFFFF),
                                                      0);

        replay_buffer->state_memory_block = MapViewOfFile(replay_buffer->memory_map, FILE_MAP_ALL_ACCESS, 0, 0, win32_state.total_size);
        Assert(replay_buffer->state_memory_block);
      }

      // used by game sound output
      int16 *samples = (int16 *)VirtualAlloc(0, global_sound_output.SecondaryBufferSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

      win32_initDSound(window, global_sound_output.SamplesPerSec, global_sound_output.SecondaryBufferSize);
      win32_clear_sound_buffer(&global_sound_output);
      globalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

      if (samples && game_memory.permanent_storage) {
        game_input input[2] = {{0}};
        game_input *new_input = &input[0];
        game_input *old_input = &input[1];
        new_input->dt = target_seconds_per_frame;

        LARGE_INTEGER last_counter = win32_get_wall_clock();
        uint64 last_cycle_count = __rdtsc();

        DWORD audio_latency_in_bytes = 0;
        real32 audio_latency_in_seconds = 0;

        int debug_time_marker_index = 0;
        win32_debug_time_marker debug_time_markers[30] = {};
        bool32 sound_is_valid = false;
        LARGE_INTEGER flip_wall_clock = win32_get_wall_clock();

        win32_game_code game_code = win32_load_game_code(dll_path, temp_dll_path);

        Running = true;

        while (Running) {
          FILETIME new_dll_write_time = win32_get_last_write_time(dll_path);
          if (CompareFileTime(&new_dll_write_time, &game_code.last_write_time) != 0) {
            win32_unload_game_code(&game_code);
            game_code = win32_load_game_code(dll_path, temp_dll_path);
          }


          game_controller_input *old_keyboard_controller = get_controller(old_input, 0);
          game_controller_input *new_keyboard_controller = get_controller(new_input, 0);
          game_controller_input zero_controller = {};
          *new_keyboard_controller = zero_controller; // TODO: FIX THIS
          for(int button_index = 0; button_index < ArrayCount(new_keyboard_controller->buttons); button_index++) {
            new_keyboard_controller->buttons[button_index].ended_down
              = old_keyboard_controller->buttons[button_index].ended_down;
          }


          win32_process_pending_messages(&win32_state, new_keyboard_controller);

          POINT mouse_point;
          GetCursorPos(&mouse_point);
          ScreenToClient(window, &mouse_point);
          new_input->mouse_x = mouse_point.x;
          new_input->mouse_y = mouse_point.y;
          new_input->mouse_z = 0;
          win32_process_keyboard_message(&new_input->mouse_buttons[0], GetKeyState(VK_LBUTTON) & (1 << 15));

          // TODO: Avoid polling disconnected controllers to avoid xinput framerate hit on older libraries
          DWORD max_controller_count = XUSER_MAX_COUNT;
          if (max_controller_count > (ArrayCount(new_input->controllers)-1)) {
            max_controller_count = ArrayCount(new_input->controllers)-1;
          }


          for (DWORD controller_index=0; controller_index < max_controller_count; controller_index++) {
            int our_controller_index = controller_index + 1; // skip keyboard at index 0
            game_controller_input *old_controller = get_controller(old_input, our_controller_index);
            game_controller_input *new_controller = get_controller(new_input, our_controller_index);

            XINPUT_STATE controllerState;
            if(XInputGetState(controller_index, &controllerState) != ERROR_DEVICE_NOT_CONNECTED) {

              new_controller->is_connected = true;
              XINPUT_GAMEPAD *pad = &controllerState.Gamepad;
              new_controller->is_analog = old_controller->is_analog;
              new_controller->stick_average_x = win32_process_x_input_stick_value(pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
              new_controller->stick_average_y = win32_process_x_input_stick_value(pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

              if ((new_controller->stick_average_x != 0.0f) || (new_controller->stick_average_y != 0.0f)) {
                new_controller->is_analog = true;
              }

              if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) {
                new_controller->stick_average_y = -1.0f;
                new_controller->is_analog = false;
              }
              if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
                new_controller->stick_average_y = 1.0f;
                new_controller->is_analog = false;
              }
              if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
                new_controller->stick_average_x = -1.0f;
                new_controller->is_analog = false;
              }
              if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
                new_controller->stick_average_x = 1.0f;
                new_controller->is_analog = false;
              }


              real32 threshold = 0.5f;
              win32_process_x_input_button((new_controller->stick_average_x < -threshold) ? 1 : 0,
                                           &old_controller->move_left,
                                           1,
                                           &new_controller->move_left);

              win32_process_x_input_button((new_controller->stick_average_x > threshold) ? 1 : 0,
                                           &old_controller->move_right,
                                           1,
                                           &new_controller->move_right);

              win32_process_x_input_button((new_controller->stick_average_y < -threshold) ? 1 : 0,
                                           &old_controller->move_up,
                                           1,
                                           &new_controller->move_up);

              win32_process_x_input_button((new_controller->stick_average_y > threshold) ? 1 : 0,
                                           &old_controller->move_down,
                                           1,
                                           &new_controller->move_down);


              win32_process_x_input_button(pad->wButtons, &old_controller->action_down, XINPUT_GAMEPAD_A, &new_controller->action_down);
              win32_process_x_input_button(pad->wButtons, &old_controller->action_right, XINPUT_GAMEPAD_B, &new_controller->action_right);
              win32_process_x_input_button(pad->wButtons, &old_controller->action_left, XINPUT_GAMEPAD_X, &new_controller->action_left);
              win32_process_x_input_button(pad->wButtons, &old_controller->action_up, XINPUT_GAMEPAD_Y, &new_controller->action_up);
              win32_process_x_input_button(pad->wButtons, &old_controller->left_shoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &new_controller->left_shoulder);
              win32_process_x_input_button(pad->wButtons, &old_controller->right_shoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &new_controller->right_shoulder);

              win32_process_x_input_button(pad->wButtons, &old_controller->start, XINPUT_GAMEPAD_START, &new_controller->start);
              win32_process_x_input_button(pad->wButtons, &old_controller->back, XINPUT_GAMEPAD_BACK, &new_controller->back);
            }
            else {
              new_controller->is_connected = false;
            }
          }

          if (global_pause) {
            continue;
          }



          game_offscreen_buffer offscreenBuffer = {};
          offscreenBuffer.memory = global_back_buffer.memory;
          offscreenBuffer.memory_size = global_back_buffer.memorySize;
          offscreenBuffer.width = global_back_buffer.width;
          offscreenBuffer.height = global_back_buffer.height;
          offscreenBuffer.pitch = global_back_buffer.pitch;
          offscreenBuffer.bytes_per_pixel = global_back_buffer.bytes_per_pixel;

          if (win32_state.input_recording_index) {
            win32_record_input(&win32_state, new_input);
          }

          if (win32_state.input_playback_index == -1) {
            // TODO: Find a better way to do this.
            // If you stop the playback in the middle of it, the previous playback_input will dominate,
            // and get stuck on that. Perhaps rewrite keyboard handling.
            *new_input = {};
            win32_end_playback_input(&win32_state);
          }

          if (win32_state.input_playback_index) {
            win32_playback_input(&win32_state, new_input);
          }

          if (game_code.game_update_and_render) {
            game_code.game_update_and_render(&thread, &game_memory, &offscreenBuffer, new_input);
          }


          /*

            Sound oupput computation

            We define a safety value thjat is the number of samples we think our game update loop may vary by.
            (let's say up to 2 ms).

            When we wake up to write audio, we will look and see what the play cursor
            position is and we will forecast ahead where we think the play cursor will be
            on the next frame boundry.

            We will then look to see if the write cursor is before that by at least our safety value.
            If it is, the target fill position is the frame boundry plus one frame.

            This gives us us perfect audio sync in the case of a card that has low enough latency.

            If the write cursor is _after_ that safety margin, then we assume we can never sync
            the audio perfectly, so we will write one frame's worth of audio plus the safety margin's worth of guard samples.

          */

          LARGE_INTEGER audio_wall_clock = win32_get_wall_clock();
          real32 from_begin_to_audio_seconds = win32_get_seconds_elapsed(flip_wall_clock, audio_wall_clock);

          DWORD play_cursor = 0;
          DWORD write_cursor = 0;
          if (SUCCEEDED(globalSecondaryBuffer->GetCurrentPosition(&play_cursor, &write_cursor))) {
            if (!sound_is_valid) {
              global_sound_output.RunningSampleIndex = write_cursor / global_sound_output.BytesPerSample;
              sound_is_valid = true;
            }

            DWORD byte_to_lock = (global_sound_output.RunningSampleIndex*global_sound_output.BytesPerSample) % global_sound_output.SecondaryBufferSize;

            DWORD expected_sound_bytes_per_frame = (DWORD)(((real32)global_sound_output.SamplesPerSec*(real32)global_sound_output.BytesPerSample) / game_update_hz);
            real32 seconds_left_until_flip = (target_seconds_per_frame - from_begin_to_audio_seconds);
            DWORD expected_bytes_until_flip = (DWORD)((seconds_left_until_flip/target_seconds_per_frame)*(real32)expected_sound_bytes_per_frame);
            DWORD expected_frame_boundry_byte = play_cursor + expected_bytes_until_flip;

            DWORD safe_write_cursor = write_cursor;
            if (safe_write_cursor < play_cursor) {
              safe_write_cursor += global_sound_output.SecondaryBufferSize;
            }
            Assert(safe_write_cursor > play_cursor);
            safe_write_cursor += global_sound_output.safety_bytes;

            bool32 is_audio_card_low_latency = safe_write_cursor < expected_frame_boundry_byte;
            DWORD target_cursor;
            if (is_audio_card_low_latency) {
              target_cursor = (expected_frame_boundry_byte + expected_sound_bytes_per_frame);
            }
            else {
               target_cursor = (write_cursor + expected_sound_bytes_per_frame + global_sound_output.safety_bytes);
            }
            target_cursor = target_cursor % global_sound_output.SecondaryBufferSize;

            DWORD bytes_to_write = 0;
            if (byte_to_lock > target_cursor) {
              bytes_to_write = (global_sound_output.SecondaryBufferSize - byte_to_lock);
              bytes_to_write += target_cursor;
            }
            else {
              bytes_to_write = target_cursor - byte_to_lock;
            }

            game_sound_output_buffer sound_buffer = {};
            sound_buffer.samples_per_second = global_sound_output.SamplesPerSec;
            sound_buffer.sample_count = bytes_to_write / global_sound_output.BytesPerSample;
            sound_buffer.samples = samples;
            sound_buffer.tone_hz = global_sound_output.ToneHz;

            if (game_code.game_get_sound_samples) {
              game_code.game_get_sound_samples(&thread, &game_memory, &sound_buffer);
            }

#if AUDIO_DEBUG
            {
              win32_debug_time_marker *marker = &debug_time_markers[debug_time_marker_index];
              marker->output_play_cursor = play_cursor;
              marker->output_write_cursor = write_cursor;
              marker->output_location = byte_to_lock;
              marker->output_byte_count = bytes_to_write;
              marker->expected_flip_play_cursor = expected_frame_boundry_byte;

              DWORD unwrapped_write_cursor = write_cursor;
              if (unwrapped_write_cursor < play_cursor) {
                unwrapped_write_cursor += global_sound_output.SecondaryBufferSize;
              }
              audio_latency_in_bytes = unwrapped_write_cursor - play_cursor;
              audio_latency_in_seconds = (((real32)audio_latency_in_bytes / (real32)global_sound_output.BytesPerSample) / (real32)global_sound_output.SamplesPerSec);

              char soundDebugBuffer[256];
              StringCbPrintfA(soundDebugBuffer,
                              sizeof(soundDebugBuffer),
                              "BTL:%u TC:%u BTW:%u - PC:%u WC:%u - DELTA:%u (%fs)\n",
                              byte_to_lock, target_cursor, bytes_to_write, play_cursor, write_cursor, audio_latency_in_bytes, audio_latency_in_seconds);
              OutputDebugStringA(soundDebugBuffer);
            }
#endif
            win32_fill_sound_buffer(&global_sound_output, byte_to_lock, bytes_to_write, &sound_buffer);
          }
          else {
            sound_is_valid = false;
          }





          // GET TIMING
          LARGE_INTEGER work_counter = win32_get_wall_clock();
          real32 seconds_elapsed_for_work = win32_get_seconds_elapsed(last_counter, work_counter);
          real32 seconds_elapsed_for_frame = seconds_elapsed_for_work;
          if (seconds_elapsed_for_frame < target_seconds_per_frame) {
            // TODO: Figure out why granularity doesnt work
            if (is_sleep_granular) {
              DWORD sleep_ms = (DWORD)((target_seconds_per_frame - seconds_elapsed_for_frame) * 1000.0f);
              if (sleep_ms > 0) {
                Sleep(sleep_ms);
              }
            }

            real32 test_seconds_elapsed_for_frame = win32_get_seconds_elapsed(last_counter, win32_get_wall_clock());
            if (test_seconds_elapsed_for_frame <= target_seconds_per_frame) {
              // TODO: Log missed
            }

            while (seconds_elapsed_for_frame < target_seconds_per_frame) {
              seconds_elapsed_for_frame = win32_get_seconds_elapsed(last_counter, win32_get_wall_clock());
            }
          }
          else {
            // TODO: Missed frame rate
          }


          uint64 end_cycle_count = __rdtsc();
          uint64 cycles_elapsed =  end_cycle_count - last_cycle_count;
          last_cycle_count = end_cycle_count;

#if AUDIO_DEBUG
          win32_debug_sync_display(&global_back_buffer, ArrayCount(debug_time_markers), debug_time_markers, debug_time_marker_index-1, &global_sound_output, target_seconds_per_frame);
#endif
          HDC device_context = GetDC(window);
          win32_DisplayBufferInWindows(device_context, dimension.width, dimension.height, global_back_buffer, 0, 0, dimension.width, dimension.height);
          ReleaseDC(window, device_context);

          flip_wall_clock = win32_get_wall_clock();

#if AUDIO_DEBUG
          {
            Assert(debug_time_marker_index < ArrayCount(debug_time_markers))
            globalSecondaryBuffer->GetCurrentPosition(&play_cursor, &write_cursor);

            debug_time_markers[debug_time_marker_index].flip_play_cursor = play_cursor;
            debug_time_markers[debug_time_marker_index].flip_write_cursor = write_cursor;
          }
#endif
          game_input *temp = new_input;
          new_input = old_input;
          old_input = temp;

          LARGE_INTEGER end_counter = win32_get_wall_clock();
          real64 msPerFrame = 1000.0f * win32_get_seconds_elapsed(last_counter, end_counter);
          real64 fps = 0.0f;
          real64 mcpf = (real64)(cycles_elapsed/(1000*1000));

          char fpsBuffer[256];
          StringCbPrintfA(fpsBuffer, sizeof(fpsBuffer), "%.02f ms/f. %.02f f/s. %.02f mc/f\n", msPerFrame, fps, mcpf);
          OutputDebugStringA(fpsBuffer);
          last_counter = end_counter;
#if AUDIO_DEBUG
          debug_time_marker_index++;
          if (debug_time_marker_index >= ArrayCount(debug_time_markers)) {
            debug_time_marker_index = 0;
          }
#endif

        }
      }
      else {
      }

    }
    else {

    }
  }
  else {
  }
  return(0);
}
