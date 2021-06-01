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
global_variable int xOffset, yOffset;
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


internal debug_read_file_result DEBUGplatform_read_entire_file(char *file_name) {
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

internal void DEBUGplatform_free_file_memory(void *memory) {
  if (memory) {
    VirtualFree(memory, 0, MEM_RELEASE);
  }
}

internal bool32 DEBUGplatform_write_entire_file(char *file_name, void *data, uint32 data_size) {
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

internal void renderGradient(win32_offscreen_buffer *buffer, int xOffset, int yOffset) {
  uint8 *row = (uint8 *)buffer->Memory;
  for (int y = 0; y < buffer->Height; y++) {
    uint32 *pixel = (uint32*)row;
    for (int x = 0; x < buffer->Width; x++) {
      uint8 blue = (x + xOffset);
      uint8 green = (y + yOffset);
      *pixel++ = ((green << 8) | blue);
    }
    row += buffer->Pitch;
  }
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


  renderGradient(buffer, 0, 0);
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
    {
      uint32 VKCode = wParam;
      if (VKCode == 'W') {
      }
      if (VKCode == 'A') {
      }
      if (VKCode == 'S') {
      }
      if (VKCode == 'D') {
      }
      if (VKCode == VK_UP) {
        yOffset += 5;
        soundOutput.ToneHz += 50;
        soundOutput.WavePeriod = soundOutput.SamplesPerSec/soundOutput.ToneHz;
      }
      if (VKCode == VK_DOWN) {
        yOffset -= 5;
        soundOutput.ToneHz -= 50;
        soundOutput.WavePeriod = soundOutput.SamplesPerSec/soundOutput.ToneHz;
      }
      if (VKCode == VK_RIGHT) {
        xOffset -= 5;
      }
      if (VKCode == VK_LEFT) {
        xOffset += 5;
      }
      if (VKCode == VK_ESCAPE) {
        Running = false;
      }
      if (VKCode == VK_SPACE) {
      }
    }
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

void win32_fill_sound_buffer(win32_sound_output *soundOutput,
                           DWORD               byteToLock,
                           DWORD               bytesToWrite,
                           game_sound_output_buffer *source_buffer) {
  VOID *region1;
  DWORD region1Size;
  VOID *region2;
  DWORD region2Size;

  if (SUCCEEDED(globalSecondaryBuffer->Lock(byteToLock, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0))) {
    DWORD region1SampleCount = region1Size/soundOutput->BytesPerSample;
    DWORD region2SampleCount = region2Size/soundOutput->BytesPerSample;


    int16 *source_samples = source_buffer->samples;
    int16 *region1_destination = (int16*)region1;
    for (DWORD sampleIndex = 0; sampleIndex < region1SampleCount; sampleIndex++) {
      *region1_destination++ = *source_samples++;
      *region1_destination++ = *source_samples++;
      soundOutput->RunningSampleIndex++;
    }

    int16 *region2_destination = (int16*)region2;
    for (DWORD sampleIndex = 0; sampleIndex < region2SampleCount; sampleIndex++) {
      *region2_destination++ = *source_samples++;
      *region2_destination++ = *source_samples++;
      soundOutput->RunningSampleIndex++;
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

internal int CALLBACK WinMain(HINSTANCE instance,
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
      xOffset = 0;
      yOffset = 0;

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
        game_input input[2];
        game_input *new_input = &input[0];
        game_input *old_input = &input[0];

        while (Running) {

          MSG message;
          while(PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
              Running = false;
            }
            TranslateMessage(&message);
            DispatchMessageA(&message);
          }

          int max_controller_count = XUSER_MAX_COUNT;
          if (max_controller_count > ArrayCount(new_input->controllers)) {
            max_controller_count = ArrayCount(new_input->controllers);
          }


          for (DWORD controllerIndex=0; controllerIndex < max_controller_count; controllerIndex++) {

            game_controller_input *old_controller = &old_input->controllers[controllerIndex];
            game_controller_input *new_controller = &new_input->controllers[controllerIndex];

            XINPUT_STATE controllerState;
            if(XInputGetState(controllerIndex, &controllerState)) {
              XINPUT_GAMEPAD *pad = &controllerState.Gamepad;


              new_controller->is_analog = true;
              new_controller->start_x = old_controller->end_x;
              new_controller->start_y = old_controller->end_y;

              real32 x;
              if(pad->sThumbLX < 0) {
                x = (real32)pad->sThumbLX / 32768.0f;
              }
              else {
                x = (real32)pad->sThumbLX / 32767.0f;
              }

              new_controller->min_x = new_controller->max_x = new_controller->end_x = x;

              real32 y;
              if(pad->sThumbLY < 0) {
                y = (real32)pad->sThumbLY / 32768.0f;
              }
              else {
                y = (real32)pad->sThumbLY / 32767.0f;
              }

              new_controller->min_y = new_controller->max_y = new_controller->end_y = y;

              win32_process_x_input_button(pad->wButtons, &old_controller->down, XINPUT_GAMEPAD_A, &new_controller->down);
              win32_process_x_input_button(pad->wButtons, &old_controller->right, XINPUT_GAMEPAD_B, &new_controller->right);
              win32_process_x_input_button(pad->wButtons, &old_controller->left, XINPUT_GAMEPAD_X, &new_controller->left);
              win32_process_x_input_button(pad->wButtons, &old_controller->up, XINPUT_GAMEPAD_Y, &new_controller->up);
              win32_process_x_input_button(pad->wButtons, &old_controller->left_shoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &new_controller->up);
              win32_process_x_input_button(pad->wButtons, &old_controller->left_shoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &new_controller->left_shoulder);
              win32_process_x_input_button(pad->wButtons, &old_controller->right_shoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &new_controller->right_shoulder);
            }
            else {

            }
          }

          DWORD playCursor;
          DWORD writeCursor;

          DWORD byteToLock;
          DWORD bytes_to_write;
          DWORD targetCursor;
          bool32 is_sound_valid = false;
          // TODO: Tighten up sound logic so that we know where we should be
          // writing to and can anticipate the time spent in the game update.
          if (SUCCEEDED(globalSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor))) {

            byteToLock = (soundOutput.RunningSampleIndex*soundOutput.BytesPerSample) % soundOutput.SecondaryBufferSize;
            targetCursor = (playCursor + (soundOutput.WriteAheadSize * soundOutput.BytesPerSample)) % soundOutput.SecondaryBufferSize;

            // TODO: Change to use a lower latency offset from the playcursor
            // when we actually start having sound effects
            if (byteToLock > targetCursor) {
              bytes_to_write = (soundOutput.SecondaryBufferSize - byteToLock);
              bytes_to_write += targetCursor;
            }
            else {
              bytes_to_write = targetCursor - byteToLock;
            }

            is_sound_valid = true;
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

          if (is_sound_valid) {
            win32_fill_sound_buffer(&soundOutput, byteToLock, bytes_to_write, &sound_buffer);
          }

          // Test sound
          // TODO: Check for reuse
          HDC deviceContext = GetDC(window);
          win32_window_dimension dimension = win32_get_window_dimension(window);
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
          int32 fps = (perfCounterFrequency/counterElapsed);
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
