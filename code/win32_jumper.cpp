#include "win32_jumper.h"



#define local_persist static
#define global_variable static
#define internal static

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef i32 bool32;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

global_variable bool32 running;
global_variable win32_offscreen_buffer globalBackBuffer;

//Update buffer
//Edit buffer

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
	    running = true;
	    //Virtual alloc here
	    //TODO: evetually we will want this to align at some desired spot so we can get the same addresses each time the game is run.
	    
	    while (running)
	    {
		MSG msg;
		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
		    DispatchMessage(&msg);
		    TranslateMessage(&msg);		    
		}

		win32_window_dimension dimension = Win32GetWindowDimension(window);

		RenderGradient(&globalBackBuffer, 0, 0);
		
		HDC deviceContext = GetDC(window);
		Win32DisplayBufferWindow(&globalBackBuffer, deviceContext, 0, 0, dimension.width, dimension.height);
		ReleaseDC(window, deviceContext);
	    }
	}
    }
}
  
