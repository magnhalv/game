#include <windows.h>
#include <stdint.h>
#include <xinput.h>

#define local_persist static
#define global_variable static
#define internal static

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

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(xInputGetStateStub)
{
  return 0;
}

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(xInputSetStateStub)
{
  return 0;
}

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

global_variable win32_offscreen_buffer global_back_buffer;
global_variable bool Running;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

internal win32_window_dimension win32_get_window_dimension(HWND window_handle) {
  win32_window_dimension result;
  RECT clientRect;
  GetClientRect(window_handle, &clientRect);
  result.width = clientRect.right - clientRect.left;
  result.height = clientRect.bottom - clientRect.top;
  return result;
}

internal void renderGradient(win32_offscreen_buffer buffer, int xOffset, int yOffset) {
  uint8 *row = (uint8 *)buffer.Memory;
  for (int y = 0; y < buffer.Height; y++) {
    uint32 *pixel = (uint32*)row;
    for (int x = 0; x < buffer.Width; x++) {
      uint8 blue = (x + xOffset);
      uint8 green = (y + yOffset);
      *pixel++ = ((green << 8) | blue);
    }
    row += buffer.Pitch;
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


  renderGradient(*buffer, 0, 0);
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
    OutputDebugStringA("DEFAULT\n");
      result = DefWindowProc(window, message, wParam, lParam);
    break;
  }
  return result;
}


internal int CALLBACK WinMain(HINSTANCE instance,
	HINSTANCE PrevInstance,
	LPSTR CommandLine,
	int ShowCode)
{

  WNDCLASS windowClass = {};

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
      int xOffset = 0;
      int yOffset = 0;
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

        renderGradient(global_back_buffer, xOffset, yOffset);
        xOffset++;
        yOffset++;

        HDC deviceContext = GetDC(window);
        win32_window_dimension dimension = win32_get_window_dimension(window);
        win32_DisplayBufferInWindows(deviceContext, dimension.width, dimension.height, global_back_buffer, 0, 0, dimension.width, dimension.height);
        ReleaseDC(window, deviceContext);

      }
    }
    else {

    }
  }
  else {
  }


  return(0);
}
