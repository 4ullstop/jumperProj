#include <windows.h>
#include <xinput.h> 

#include "win32_jumper.h"

global_variable bool32 running;
global_variable win32_offscreen_buffer globalBackBuffer;


#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

internal void
Win32LoadXInput(void)
{
    HMODULE XInputLibrary = LoadLibrary("xinput1_4.dll");
    if (!XInputLibrary)
    {
	XInputLibrary = LoadLibrary("xinput9_1_0.dll");
    }
    if (!XInputLibrary)
    {
	XInputLibrary = LoadLibrary("xinput1_3.dll");
    }
    if (XInputLibrary)
    {
	XInputGetState = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
	XInputSetState = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");

	OutputDebugString("Library is loaded, functions should be set\n");
    }
    else
    {
	
    }
}

//Accounting for dead zone calculations
internal r32
Win32ProcessInputStickValue(SHORT value, SHORT deadZoneThreshold)
{
    r32 result = 0;

    if (value < -deadZoneThreshold)
    {
	result = (r32)((value + deadZoneThreshold) / (32768.0f - deadZoneThreshold));
    }
    else if (value > deadZoneThreshold)
    {
	result = (r32)((value - deadZoneThreshold) / (32767.0f - deadZoneThreshold));
    }
    return(result);
}

internal void
Win32ProcessXInputDigitalButton(DWORD xInputButtonState, game_button_state* oldState, DWORD buttonBit, game_button_state* newState)
{
    newState->endedDown = ((xInputButtonState & buttonBit) == buttonBit);
    newState->halfTransitionCount = (newState->endedDown != oldState->endedDown) ? 1 : 0;
}

internal void
RenderGradient(win32_offscreen_buffer* buffer, int xOffset, int yOffset)
{
    u8* row = (u8*)buffer->memory;
    for (int y = 0; y < buffer->height; ++y)
    {
	u32* pixel = (u32*)row;
	for (int x = 0; x < buffer->width; ++x)
	{
	    u8 blue = (u8)(x + xOffset);
	    u8 green = (u8)(y + yOffset);

	    *pixel++ = ((green << 16) | blue);
	}
	row += buffer->pitch;
    }
}

internal win32_window_dimension
Win32GetWindowDimension(HWND window)
{
    win32_window_dimension result;
    RECT clientRect;
    GetClientRect(window, &clientRect);
    result.width = clientRect.right - clientRect.left;
    result.height = clientRect.bottom - clientRect.top;

    return(result);
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer* buffer, int width, int height)
{
    if (buffer->memory)
    {
	VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }

    buffer->width = width;
    buffer->height = height;

    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = buffer->width;
    buffer->info.bmiHeader.biHeight = -buffer->height;
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;
    buffer->info.bmiHeader.biCompression = BI_RGB;

    buffer->bytesPerPixel = 4;

    int bitmapMemorySize = (buffer->width * buffer->height) * buffer->bytesPerPixel;
    buffer->memory = VirtualAlloc(0, bitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    buffer->pitch = width * buffer->bytesPerPixel;
}

internal void
Win32DisplayBufferWindow(win32_offscreen_buffer* buffer,
			 HDC deviceContext,
			 int x, int y,
			 int windowWidth, int windowHeight)
{
    if ((windowWidth >= buffer->width * 2) &&
	(windowHeight >= buffer->height * 2))
    {
	StretchDIBits(deviceContext,
		      0, 0, 2*buffer->width, 2*buffer->height,
		      0, 0, buffer->width, buffer->height,
		      buffer->memory,
		      &buffer->info,
		      DIB_RGB_COLORS,
		      SRCCOPY);
    }
    else
    {
	int offsetY = 10;
	int offsetX = 10;

	PatBlt(deviceContext, 0, 0, windowWidth, offsetY, BLACKNESS);
	PatBlt(deviceContext, 0, offsetY + buffer->height, windowWidth, windowHeight, BLACKNESS);
	PatBlt(deviceContext, 0, 0, offsetX, windowHeight, BLACKNESS);
	PatBlt(deviceContext, offsetX + buffer->width, 0, windowWidth, windowHeight, BLACKNESS);

	StretchDIBits(deviceContext,
		      offsetX, offsetY, buffer->width, buffer->height,
		      0, 0, buffer->width, buffer->height,
		      buffer->memory,
		      &buffer->info,
		      DIB_RGB_COLORS,
		      SRCCOPY);
	
    }
}

LRESULT CALLBACK Win32MainWindowProc(HWND hwnd,
				     UINT uMsg,
				     WPARAM wParam,
				     LPARAM lParam)
{
    LRESULT result = 0;
    switch(uMsg)
    {
    case WM_QUIT:
    {
	running = false;
	OutputDebugStringA("quitting program\n");
    } break;
    case WM_SIZE:
    {
	OutputDebugStringA("window size changed\n");
	RECT clientRect;
	GetClientRect(hwnd, &clientRect);
	int width = clientRect.right - clientRect.left;
	int height = clientRect.bottom - clientRect.top;
	
    } break;
    case WM_DESTROY:
    {
	running = false;
	OutputDebugStringA("window destroyed\n");
    } break;
    case WM_ACTIVATEAPP:
    {
	OutputDebugStringA("activated the app\n");
    } break;
    case WM_PAINT:
    {
	//will come in use later
	PAINTSTRUCT paint;
	HDC deviceContext = BeginPaint(hwnd, &paint);

	int x = paint.rcPaint.left;
	int y = paint.rcPaint.top;

	win32_window_dimension dimension = Win32GetWindowDimension(hwnd);

	Win32DisplayBufferWindow(&globalBackBuffer, deviceContext, x, y, dimension.width, dimension.height);
	EndPaint(hwnd, &paint);
    } break;
    default:
    {
	result = DefWindowProc(hwnd, uMsg, wParam, lParam);
    } break;
    }

    return(result);
}

int CALLBACK WinMain(HINSTANCE hInstance,
		     HINSTANCE hPrevInstance,
		     LPSTR lpCmdLine,
		     int nCmdShow)
{

    Win32LoadXInput();
    
    Win32ResizeDIBSection(&globalBackBuffer, 960, 540);
    WNDCLASS wc = {};
    wc.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    wc.lpfnWndProc = Win32MainWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "JumperGame Window Class";
    
    if (RegisterClassA(&wc))
    {
	HWND window = CreateWindowEx(
	    0,
	    wc.lpszClassName,
	    "Jumper Game",
	    WS_OVERLAPPEDWINDOW|WS_VISIBLE,
	    CW_USEDEFAULT,
	    CW_USEDEFAULT,
	    CW_USEDEFAULT,
	    CW_USEDEFAULT,
	    0,
	    0,
	    hInstance,
	    0);
	if (window)
	{

	    game_input input[2] = {};
	    game_input* newInput = &input[0];
	    game_input* oldInput = &input[1];
	    
	    running = true;
	    while (running)
	    {
		MSG msg;
		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
		    DispatchMessage(&msg);
		    TranslateMessage(&msg);		    
		}

		DWORD maxControllerCount = XUSER_MAX_COUNT;

		
		for (DWORD controllerIndex = 0; controllerIndex <  maxControllerCount; ++controllerIndex)
		{
		    int ourControllerIndex = controllerIndex + 1;
		    game_controller_input* oldController = GetController(oldInput, ourControllerIndex);
		    game_controller_input* newController = GetController(newInput, ourControllerIndex);
		    XINPUT_STATE controllerState;

		    if (XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS)
		    {
			newController->isConnected = true;
			newController->isAnalog = oldController->isAnalog;

			XINPUT_GAMEPAD* pad = &controllerState.Gamepad;

			newController->isAnalog = true;
			newController->stickAverageX = Win32ProcessInputStickValue(pad->sThumbLX,
										   XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
			newController->stickAverageY = Win32ProcessInputStickValue(pad->sThumbLY,
										   XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
			if ((newController->stickAverageX != 0.0f) ||
			    (newController->stickAverageY != 0.0f))
			{
			    newController->isAnalog = true;
			}
			if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
			{
			    newController->stickAverageY = 1.0f;
			    newController->isAnalog = false;
			}
			if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
			{
			    newController->stickAverageY = -1.0f;
			    newController->isAnalog = false;
			}
			if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
			{
			    newController->stickAverageX = -1.0f;
			    newController->isAnalog = false;
			}
			if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
			{
			    newController->stickAverageX = 1.0f;
			    newController->isAnalog = false;
			}
			r32 threshold = 0.5f;

			Win32ProcessXInputDigitalButton((newController->stickAverageX < -threshold) ? 1 : 0,
							&oldController->moveLeft, 1,
							&newController->moveLeft);
			Win32ProcessXInputDigitalButton((newController->stickAverageX > threshold) ? 1 : 0,
							&oldController->moveRight, 1,
							&newController->moveRight);
			Win32ProcessXInputDigitalButton((newController->stickAverageY < -threshold) ? 1 : 0,
							&oldController->moveDown, 1,
							&newController->moveDown);
			Win32ProcessXInputDigitalButton((newController->stickAverageY > threshold) ? 1 : 0,
							&oldController->moveUp, 1,
							&newController->moveUp);			
			
		    }
		}

		i32 xOffset = 0;
		game_controller_input* cont = GetController(newInput, 1);
		if (cont->moveUp.endedDown)
		{
		    xOffset += 10;
		    OutputDebugString("controller up button pressed\n");
		}
		
		win32_window_dimension dimension = Win32GetWindowDimension(window);
		RenderGradient(&globalBackBuffer, xOffset, 0);
		
		HDC deviceContext = GetDC(window);
		Win32DisplayBufferWindow(&globalBackBuffer, deviceContext, 0, 0, dimension.width, dimension.height);
		ReleaseDC(window, deviceContext);

		game_input* temp = newInput;
		newInput = oldInput;
		oldInput = temp;
	    }
	}
    }
}
  
