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

#include <windows.h>
#include <xinput.h>
#include <dsound.h>
#include <math.h>
#include <strsafe.h>
#include <intrin.h>

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

#define Pi32 3.14159265359f

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

void Win32_FillSoundBufferRegion(int16 *region, DWORD regionSampleCount, win32_sound_output *soundOutput) {
  for (DWORD sampleIndex = 0; sampleIndex < regionSampleCount; sampleIndex++) {
    real32 sineValue = sinf(soundOutput->TSine);
    int16 sampleValue = (int16)(sineValue * soundOutput->ToneVolume);
    *region++ = sampleValue;
    *region++ = sampleValue;
    soundOutput->TSine += (1.0f/(real32)soundOutput->WavePeriod) * 2.0f * Pi32;
    soundOutput->RunningSampleIndex++;
  }
}

void Win32_FillSoundBuffer(win32_sound_output *soundOutput, DWORD byteToLock, DWORD bytesToWrite) {
  VOID *region1;
  DWORD region1Size;
  VOID *region2;
  DWORD region2Size;

  if (SUCCEEDED(globalSecondaryBuffer->Lock(byteToLock, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0))) {
    DWORD region1SampleCount = region1Size/soundOutput->BytesPerSample;
    DWORD region2SampleCount = region2Size/soundOutput->BytesPerSample;
    Win32_FillSoundBufferRegion((int16*)region1, region1SampleCount, soundOutput);
    Win32_FillSoundBufferRegion((int16*)region2, region2SampleCount, soundOutput);

    globalSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
  }
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

      win32_initDSound(window, soundOutput.SamplesPerSec, soundOutput.SecondaryBufferSize);
      Win32_FillSoundBuffer(&soundOutput, 0, soundOutput.WriteAheadSize);
      globalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

      LARGE_INTEGER lastCounter;
      QueryPerformanceCounter(&lastCounter);
      LARGE_INTEGER perfCounterFrequencyResult;
      QueryPerformanceFrequency(&perfCounterFrequencyResult);
      int64 perfCounterFrequency = perfCounterFrequencyResult.QuadPart;

      int64 lastCycleCount = __rdtsc();

      while (Running) {

        MSG message;
        while(PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
          if (message.message == WM_QUIT) {
            Running = false;
          }
          TranslateMessage(&message);
          DispatchMessageA(&message);
        }

        for (DWORD controllerIndex=0; controllerIndex < XUSER_MAX_COUNT; controllerIndex++) {

          XINPUT_STATE controllerState;
          if(XInputGetState(controllerIndex, &controllerState)) {
            XINPUT_GAMEPAD *pad = &controllerState.Gamepad;

            bool up = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
            bool down = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
            bool left = (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
            bool right = (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
            bool start = (pad->wButtons & XINPUT_GAMEPAD_START);
            bool back = (pad->wButtons & XINPUT_GAMEPAD_BACK);
            bool leftShould = (pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
            bool rightShoulder = (pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
            bool aButton = (pad->wButtons & XINPUT_GAMEPAD_A);
            bool bButton = (pad->wButtons & XINPUT_GAMEPAD_B);
            bool xButton = (pad->wButtons & XINPUT_GAMEPAD_X);
            bool yButton = (pad->wButtons & XINPUT_GAMEPAD_Y);

            int16 stickyX = pad->sThumbLX;
            int16 stickyY = pad->sThumbLY;
          }
          else {
          }
        }

        game_offscreen_buffer offscreenBuffer = {};
        offscreenBuffer.Memory = global_back_buffer.Memory;
        offscreenBuffer.Width = global_back_buffer.Width;
        offscreenBuffer.Height = global_back_buffer.Height;
        offscreenBuffer.Pitch = global_back_buffer.Pitch;
        GameUpdateAndRender(&offscreenBuffer, xOffset, yOffset);
        //        renderGradient(&global_back_buffer, xOffset, yOffset);

        // Test sound

        DWORD writePointer;
        DWORD bytesToWrite;

        DWORD playCursor;
        DWORD writeCursor;
        if (SUCCEEDED(globalSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor))) {

          DWORD byteToLock = (soundOutput.RunningSampleIndex*soundOutput.BytesPerSample) % soundOutput.SecondaryBufferSize;
          DWORD bytesToWrite;

          DWORD targetCursor = (playCursor + (soundOutput.WriteAheadSize * soundOutput.BytesPerSample)) % soundOutput.SecondaryBufferSize;

          // TODO: Change to use a lower latency offset from the playcursor
          // when we actually start having sound effects
          if (byteToLock > targetCursor) {
            bytesToWrite = (soundOutput.SecondaryBufferSize - byteToLock);
            bytesToWrite += targetCursor;
          }
          else {
            bytesToWrite = targetCursor - byteToLock;
          }

          Win32_FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite);
        }

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
        OutputDebugStringA(writeBuffer);

        lastCycleCount = endCycleCount;
        lastCounter = endCounter;
      }
    }
    else {

    }
  }
  else {
  }



  return(0);
}
