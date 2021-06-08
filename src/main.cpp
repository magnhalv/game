/*

  TODO: Platform layer

  - saved game locations
  - getting a handle to our executable file
  - asset loading path
  - threading (launch a thread)
  - raw input ( support for multiple keyboards)
  - Sleep/timeBeginperiod
  - clipCursor() (multi monitor)
  - fullscreen support
  - VM_SETCURSOR (control cursor visibility)
  - QueryCancelAutoPlay
  - WM_ACTIVATEAPP (for when we are not the active application)
  - Blit speed improvements
  - Hardware acceleration
  - GetKeyboardLayout

 */
#include <stdint.h>
#include <math.h>
#include <strsafe.h>
#include <windows.h>
#include <xinput.h>
#include <dsound.h>
#include <intrin.h>
#include <malloc.h>
#include <fileapi.h>

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

typedef int32 bool32;

typedef float real32;
typedef double real64;

#include "game.cpp"



struct win32_offscreen_buffer {
  BITMAPINFO Info;
  void *Memory;
  int MemorySize;
  int  Width;
  int Height;
  int BytesPerPixel;
  int Pitch;
};

struct win32_window_dimension {
  int width;
  int height;
};

struct win32_sound_output {
  int SamplesPerSec;
  int ToneHz;
  int ToneVolume;
  uint32 RunningSampleIndex;
  int WavePeriod;
  int BytesPerSample;
  int SecondaryBufferSize;
  bool IsSoundPlaying;
  real32 TSine;
  int WriteAheadSize;
};

global_variable win32_offscreen_buffer global_back_buffer;
global_variable bool Running;
global_variable LPDIRECTSOUNDBUFFER globalSecondaryBuffer;
global_variable win32_sound_output soundOutput = {};

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


debug_read_file_result DEBUGplatform_read_entire_file(char *file_name) {
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
          DEBUGplatform_free_file_memory(result.contents);
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

void DEBUGplatform_free_file_memory(void *memory) {
  if (memory) {
    VirtualFree(memory, 0, MEM_RELEASE);
  }
}

bool32 DEBUGplatform_write_entire_file(char *file_name, void *data, uint32 data_size) {
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
      bufferDescription.dwFlags = 0;
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

  if (buffer->Memory) {
    VirtualFree(buffer->Memory, 0, MEM_RELEASE);
  }

  buffer->Width = width;
  buffer->Height = height;

  buffer->Info.bmiHeader.biSize = sizeof(buffer->Info.bmiHeader);
  buffer->Info.bmiHeader.biWidth = buffer->Width;
  buffer->Info.bmiHeader.biHeight = -buffer->Height;
  buffer->Info.bmiHeader.biPlanes = 1;
  buffer->Info.bmiHeader.biBitCount = 32;
  buffer->Info.bmiHeader.biCompression = BI_RGB;

  buffer->BytesPerPixel = 4;
  buffer->MemorySize = buffer->BytesPerPixel*(buffer->Width*buffer->Height);
  buffer->Memory = VirtualAlloc(0, buffer->MemorySize, MEM_COMMIT, PAGE_READWRITE);
  buffer->Pitch = buffer->Width*buffer->BytesPerPixel;
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
  StretchDIBits(deviceContext,
                //                x, y, width, height,
                //                x, y, width, height,
                0, 0, window_width, window_height,
                0, 0, buffer.Width, buffer.Height,
                buffer.Memory,
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
    OutputDebugStringA("WM_ACTIVATEAPP\n");
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

internal void win32_process_x_input_button(DWORD x_input_button_state,
                                           game_button_state *old_state,
                                           DWORD button_bit,
                                           game_button_state *new_state) {
  new_state->ended_down = ((x_input_button_state & button_bit) == button_bit);
  new_state->half_transition_count = (old_state->ended_down != new_state->ended_down) ? 1 : 0;
}

internal void win32_process_keyboard_message(game_button_state *new_state,
                                             bool32 is_down) {
  Assert(new_state->ended_down != is_down);
  new_state->ended_down = is_down;
  new_state->half_transition_count++;
}

internal void win32_process_pending_messages(game_controller_input *keyboard_controller) {
  MSG message;
  while(PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
    switch (message.message) {
    case WM_QUIT:
      {
        Running = false;
      }
      break;
    case  WM_SYSKEYDOWN:
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

int CALLBACK WinMain(HINSTANCE instance,
	HINSTANCE PrevInstance,
	LPSTR CommandLine,
	int ShowCode)
{

  WNDCLASS windowClass = {};
  win32_loadXinput();
  windowClass.style = CS_HREDRAW|CS_VREDRAW;
  windowClass.lpfnWndProc = WindowProcCallback;
  windowClass.hInstance = instance;
  //  WindowClass.hIcon;
  windowClass.lpszClassName = "GameWindowClass";

  if (RegisterClass(&windowClass)) {
    HWND window =
      CreateWindowExA(0,
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
      Running = true;

      soundOutput.SamplesPerSec = 48000;
      soundOutput.ToneHz = 256;
      soundOutput.ToneVolume = 500;
      soundOutput.RunningSampleIndex = 0;
      soundOutput.WavePeriod = soundOutput.SamplesPerSec/soundOutput.ToneHz;
      soundOutput.BytesPerSample = sizeof(int16)*2;
      soundOutput.SecondaryBufferSize = soundOutput.SamplesPerSec*soundOutput.BytesPerSample;
      soundOutput.TSine = 0;
      soundOutput.WriteAheadSize = soundOutput.SamplesPerSec / 15;

      int16 *samples = (int16 *)VirtualAlloc(0, soundOutput.SecondaryBufferSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

#if GAME_INTERNAL
      LPVOID base_address = (LPVOID)Terabytes((uint64)2);
#else
      LPVOID base_address = 0;
#endif

      game_memory game_memory = {};
      game_memory.permanent_storage_size = Megabytes(64);
      game_memory.transient_storage_size = Gigabytes((uint64)4);
      uint64 total_size = game_memory.permanent_storage_size + game_memory.transient_storage_size;
      game_memory.permanent_storage = VirtualAlloc(base_address,
                                                   total_size,
                                                   MEM_RESERVE|MEM_COMMIT,
                                                   PAGE_READWRITE);

      game_memory.transient_storage = ((uint8 *)game_memory.permanent_storage + game_memory.permanent_storage_size);


      win32_initDSound(window, soundOutput.SamplesPerSec, soundOutput.SecondaryBufferSize);
      win32_clear_sound_buffer(&soundOutput);
      globalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

      LARGE_INTEGER lastCounter;
      QueryPerformanceCounter(&lastCounter);
      LARGE_INTEGER perfCounterFrequencyResult;
      QueryPerformanceFrequency(&perfCounterFrequencyResult);
      int64 perfCounterFrequency = perfCounterFrequencyResult.QuadPart;

      int64 lastCycleCount = __rdtsc();

      if (samples && game_memory.permanent_storage) {
        game_input input[2] = {{0}};
        game_input *new_input = &input[0];
        game_input *old_input = &input[1];


        while (Running) {

          game_controller_input *old_keyboard_controller = get_controller(old_input, 0);
          game_controller_input *new_keyboard_controller = get_controller(new_input, 0);
          game_controller_input zero_controller = {};
          *new_keyboard_controller = zero_controller; // TODO: FIX THIS
          for(int button_index = 0; button_index < ArrayCount(new_keyboard_controller->buttons); button_index++) {
            new_keyboard_controller->buttons[button_index].ended_down
              = old_keyboard_controller->buttons[button_index].ended_down;

            if (new_keyboard_controller->buttons[button_index].ended_down) {
              OutputDebugStringA("It is down!\n");
            }
          }


          win32_process_pending_messages(new_keyboard_controller);

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


              new_controller->is_analog = true;
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

          DWORD playCursor;
          DWORD writeCursor;

          DWORD byte_to_lock = 0;
          DWORD bytes_to_write;
          DWORD targetCursor;
          // TODO: Tighten up sound logic so that we know where we should be
          // writing to and can anticipate the time spent in the game update.
          if (SUCCEEDED(globalSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor))) {

            byte_to_lock = (soundOutput.RunningSampleIndex*soundOutput.BytesPerSample) % soundOutput.SecondaryBufferSize;
            targetCursor = (playCursor + (soundOutput.WriteAheadSize * soundOutput.BytesPerSample)) % soundOutput.SecondaryBufferSize;

            // TODO: Change to use a lower latency offset from the playcursor
            // when we actually start having sound effects
            if (byte_to_lock > targetCursor) {
              bytes_to_write = (soundOutput.SecondaryBufferSize - byte_to_lock);
              bytes_to_write += targetCursor;
            }
            else {
              bytes_to_write = targetCursor - byte_to_lock;
            }

            game_sound_output_buffer sound_buffer = {};
            sound_buffer.samples_per_second = soundOutput.SamplesPerSec;
            sound_buffer.sample_count = bytes_to_write / soundOutput.BytesPerSample;
            sound_buffer.samples = samples;
            sound_buffer.tone_hz = soundOutput.ToneHz;

            game_offscreen_buffer offscreenBuffer = {};
            offscreenBuffer.Memory = global_back_buffer.Memory;
            offscreenBuffer.Width = global_back_buffer.Width;
            offscreenBuffer.Height = global_back_buffer.Height;
            offscreenBuffer.Pitch = global_back_buffer.Pitch;
            GameUpdateAndRender(&game_memory, &offscreenBuffer, &sound_buffer, new_input);
            //        renderGradient(&global_back_buffer, xOffset, yOffset);

            win32_fill_sound_buffer(&soundOutput, byte_to_lock, bytes_to_write, &sound_buffer);
          }

          // Test sound
          // TODO: Check for reuse
          HDC deviceContext = GetDC(window);
          win32_DisplayBufferInWindows(deviceContext, dimension.width, dimension.height, global_back_buffer, 0, 0, dimension.width, dimension.height);
          ReleaseDC(window, deviceContext);


          // GET TIMING
          int64 endCycleCount = __rdtsc();

          LARGE_INTEGER endCounter;
          QueryPerformanceCounter(&endCounter);


          // CALC TIMING

          int64 cyclesElapsed = endCycleCount - lastCycleCount;

          int64 counterElapsed = endCounter.QuadPart - lastCounter.QuadPart;
          int32 msPerFrame = (int32)((1000*counterElapsed)/perfCounterFrequency);
          int32 fps = (int32)(perfCounterFrequency/counterElapsed);
          int32 mcpf = (int32)(cyclesElapsed/(1000*1000));

          char writeBuffer[256];
          StringCbPrintfA(writeBuffer, sizeof(writeBuffer), "%d ms/f. %d f/s. %d mc/f\n", msPerFrame, fps, mcpf);
          //        OutputDebugStringA(writeBuffer);

          lastCycleCount = endCycleCount;
          lastCounter = endCounter;

          game_input *temp = new_input;
          new_input = old_input;
          old_input = temp;
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
