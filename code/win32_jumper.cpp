#include <windows.h>
#include <xinput.h>
#include <xaudio2.h>

#include "win32_jumper.h"
#include <math.h>

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))


#define pi32 3.14159265359f



#define BITS_PER_SAMPLE 16
#define SAMPLES_PER_SEC 44100
#define AUDIO_BUFFER_SIZE_CYCLES 10
#define CYCLES_PER_SEC 220.0f


#define SAMPLES_PER_CYCLE (DWORD)(SAMPLES_PER_SEC / CYCLES_PER_SEC)
#define AUDIO_BUFFER_SIZE_SAMPLES  SAMPLES_PER_CYCLE * AUDIO_BUFFER_SIZE_CYCLES
#define AUDIO_BUFFER_SIZE_BYTES AUDIO_BUFFER_SIZE_SAMPLES * BITS_PER_SAMPLE / 8


global_variable bool32 running;
global_variable win32_offscreen_buffer globalBackBuffer;
global_variable bool32 pause;

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

#define X_AUDIO2_CREATE(name) HRESULT name(IXAudio2 **ppXAudio2, UINT32 Flags, XAUDIO2_PROCESSOR XAudio2Processor)
typedef X_AUDIO2_CREATE(x_audio2_create);

#define COINITIALIZE(name) HRESULT name(LPVOID pvReserved, DWORD dwCoInit)
typedef COINITIALIZE(co_initialize);

internal void
Win32InitSound(win32_audio_info* audioInfo, i32 samplesPerSecond)
{
    HMODULE xAudioLibrary = LoadLibrary("XAudio2_9.dll");

    HMODULE oleLibrary = LoadLibrary("Ole32.dll");
    if (xAudioLibrary)
    {
	x_audio2_create* xAudio2Create = (x_audio2_create*)GetProcAddress(xAudioLibrary, "XAudio2Create");
	
	
	if (xAudio2Create)
	{
	    if(xAudio2Create(&audioInfo->audioInterface, 0, XAUDIO2_DEFAULT_PROCESSOR) == S_OK)
	    {
		if (oleLibrary)
		{
		    co_initialize* coinitialize = (co_initialize*)GetProcAddress(oleLibrary, "CoInitializeEx");

		    if (coinitialize(0, COINIT_MULTITHREADED) == S_OK)
		    {
			
			if (audioInfo->audioInterface->CreateMasteringVoice(&audioInfo->audioMasterVoice,
									    XAUDIO2_DEFAULT_CHANNELS,
									    XAUDIO2_DEFAULT_SAMPLERATE,
									    0,
									    0) == S_OK)
			{
			    OutputDebugString("Audio Interface Mastering Voice created\n");
			    //Populate WAVEFORMATEX structure
			    WAVEFORMATEX wf = {};
			    wf.wFormatTag = WAVE_FORMAT_PCM;
			    wf.nChannels = 1;
			    wf.nSamplesPerSec = SAMPLES_PER_SEC;
			    wf.wBitsPerSample = BITS_PER_SAMPLE;
			    wf.nBlockAlign = wf.nChannels * BITS_PER_SAMPLE / 8;
			    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
			    wf.cbSize = 0;

			    if (audioInfo->audioInterface->CreateSourceVoice(&audioInfo->sourceVoice,
									     &wf,
									     0,
									     XAUDIO2_MAX_FREQ_RATIO) == S_OK)
			    {
				//When you are ready to play a sound, you will submit a buffer to be read
				//and then call the start audio function
				OutputDebugString("SoundSource Successfully created\n");
			    }
			}
//			DWORD lastError = GetLastError();
		    }
		}
	    }
	    else
	    {
		OutputDebugString("Error, xAudio2 device not initialized\n");	    
	    }
	}
    }
}

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

internal void
Win32ProcessKeyboardMessage(game_button_state* newState, bool32 isDown)
{
    if (newState->endedDown != isDown)
    {
	newState->endedDown = isDown;
	++newState->halfTransitionCount;
    }
}

internal void
Win32ProcessPendingMessages(win32_state* win32State, game_controller_input* keyboardController)
{
    MSG msg;
    while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
    {
	switch(msg.message)
	{
	case WM_QUIT:
	{
	    running = false;
	} break;
	case WM_SETCURSOR:
	{
	    
	} break;
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP:
	{
	    u32 VKCode = (u32)msg.wParam;
	    bool32 wasDown = ((msg.lParam & (1 << 30)) != 0);
	    bool32 isDown = ((msg.lParam & (1 << 31)) == 0);
	    //TODO: Change the properties here bc I don't think this will work for the setup you are trying to achieve with the precise inputs determining things like how long a button is pressed (although half transition count might be this)
	    if (wasDown != isDown)
	    {
		if (VKCode == 'W')
		{
		    Win32ProcessKeyboardMessage(&keyboardController->moveUp, isDown);
		}
		else if (VKCode == 'A')
		{
		    Win32ProcessKeyboardMessage(&keyboardController->moveLeft, isDown);
		}
		else if (VKCode == 'S')
		{
		    Win32ProcessKeyboardMessage(&keyboardController->moveDown, isDown);
		}
		else if (VKCode == 'D')
		{
		    Win32ProcessKeyboardMessage(&keyboardController->moveRight, isDown);
		}
		else if (VKCode == 'Q')
		{
		    Win32ProcessKeyboardMessage(&keyboardController->rightShoulder, isDown);
		}
		else if (VKCode == VK_UP)
		{
		    Win32ProcessKeyboardMessage(&keyboardController->actionUp, isDown);
		}
		else if (VKCode == VK_DOWN)
		{
		    Win32ProcessKeyboardMessage(&keyboardController->actionDown, isDown);
		}
		else if (VKCode == VK_LEFT)
		{
		    Win32ProcessKeyboardMessage(&keyboardController->actionLeft, isDown);
		}
		else if (VKCode == VK_RIGHT)
		{
		    Win32ProcessKeyboardMessage(&keyboardController->actionRight, isDown);
		}
		else if (VKCode == VK_ESCAPE)
		{
		    running = false;
		}
		else if (VKCode == VK_SPACE)
		{
		    Win32ProcessKeyboardMessage(&keyboardController->start, isDown);
		}
#if JUMPER_INTERNAL
		else if (VKCode == 'P')
		{
		    if (isDown)
		    {
			pause = !pause;
		    }
		}
#endif
		if (isDown)
		{
		    bool32 altKeyWasDown = ((msg.lParam & (1 << 29)) != 0);
		    if ((VKCode == VK_F4) && altKeyWasDown)
		    {
			running = false;
		    }
		}
	    }
	} break;
	default:
	{
	    TranslateMessage(&msg);
	    DispatchMessage(&msg);
	} break;
	}
    }
}

int CALLBACK WinMain(HINSTANCE hInstance,
		     HINSTANCE hPrevInstance,
		     LPSTR lpCmdLine,
		     int nCmdShow)
{
    win32_audio_info audioInfo;
    Win32InitSound(&audioInfo, 48000);

    win32_state win32State = {};
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
	    i32 xOffset = 0;
	    game_input input[2] = {};
	    game_input* newInput = &input[0];
	    game_input* oldInput = &input[1];


	    r64 phase{};
	    u32 bufferIndex{};
	    byte audioBuffer[AUDIO_BUFFER_SIZE_BYTES];
	    //The action of filling the buffer
	    while (bufferIndex < AUDIO_BUFFER_SIZE_BYTES)
	    {
		phase += (2 * pi32) / SAMPLES_PER_CYCLE;
		i16 sample = (i16)(sin(phase) * INT16_MAX * 0.5f); //last value here is our volume, I'm just too lazy to make  variable atm
		audioBuffer[bufferIndex++] = (byte)sample;
		audioBuffer[bufferIndex++] = (byte)(sample >> 8);
	    }
	    
	    XAUDIO2_BUFFER audioDataBuffer = {};
	    audioDataBuffer.Flags = XAUDIO2_END_OF_STREAM;
	    audioDataBuffer.AudioBytes = AUDIO_BUFFER_SIZE_BYTES;
	    audioDataBuffer.pAudioData = (BYTE*)&audioBuffer;
	    audioDataBuffer.LoopCount = XAUDIO2_LOOP_INFINITE;
	    
	    audioInfo.sourceVoice->SubmitSourceBuffer(&audioDataBuffer);
	    audioInfo.sourceVoice->Start(0);
	    
	    running = true;
	    while (running)
	    {
		DWORD maxControllerCount = XUSER_MAX_COUNT;

		//TODO: ProcessPendingMessages here
		game_controller_input* oldKeyboardController = GetController(oldInput, 0);
		game_controller_input* newKeyboardController = GetController(newInput, 0);
		*newKeyboardController = {};
		newKeyboardController->isConnected = true;
		for (int buttonIndex = 0; buttonIndex < ArrayCount(newKeyboardController->buttons); ++buttonIndex)
		{
		    newKeyboardController->buttons[buttonIndex].endedDown =
			oldKeyboardController->buttons[buttonIndex].endedDown;
		}
		Win32ProcessPendingMessages(&win32State, newKeyboardController);
		
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

			Win32ProcessXInputDigitalButton(pad->wButtons,
							&oldController->actionDown, XINPUT_GAMEPAD_A,
							&newController->actionDown);
			Win32ProcessXInputDigitalButton(pad->wButtons,
							&oldController->actionRight, XINPUT_GAMEPAD_B,
							&newController->actionRight);
			Win32ProcessXInputDigitalButton(pad->wButtons,
							&oldController->actionLeft, XINPUT_GAMEPAD_X,
							&newController->actionLeft);
			Win32ProcessXInputDigitalButton(pad->wButtons,
							&oldController->actionUp, XINPUT_GAMEPAD_Y,
							&newController->actionUp);

			Win32ProcessXInputDigitalButton(pad->wButtons,
							&oldController->leftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER,
							&newController->leftShoulder);
			Win32ProcessXInputDigitalButton(pad->wButtons,
							&oldController->rightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER,
							&newController->rightShoulder);
			Win32ProcessXInputDigitalButton(pad->wButtons,
							&oldController->back, XINPUT_GAMEPAD_BACK,
							&newController->back);
			Win32ProcessXInputDigitalButton(pad->wButtons,
							&oldController->start, XINPUT_GAMEPAD_START,
							&newController->start);
			
		    }
		    else
		    {
			newController->isConnected = false;
		    }
		}


		game_controller_input* cont = GetController(newInput, 0);
		if (cont->moveUp.endedDown)
		{
		    xOffset += 1;
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
  
