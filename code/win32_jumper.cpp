#include "jumper.h"

#include <windows.h>
#include <xinput.h>
#include <xaudio2.h>
#include <stdio.h>

#include "win32_jumper.h"


global_variable bool32 running;
global_variable win32_offscreen_buffer globalBackBuffer;
global_variable bool32 pause;
global_variable i64 perfCountFrequency;
global_variable WINDOWPLACEMENT windowPosition = {sizeof(windowPosition)};

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

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
    VirtualFree(memory, 0, MEM_RESERVE);
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
    debug_read_file_result result = {};
    HANDLE fileHandle = CreateFile(filename,
				   GENERIC_READ,
				   FILE_SHARE_READ,
				   0,
				   OPEN_EXISTING,
				   0,
				   0);
    if (fileHandle != INVALID_HANDLE_VALUE)
    {
	LARGE_INTEGER fileSize;
	if (GetFileSizeEx(fileHandle, &fileSize))
	{
	    u32 fileSize32 = SafeTruncateUInt64(fileSize.QuadPart);
	    result.contents = VirtualAlloc(0, fileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	    if (result.contents)
	    {
		DWORD bytesRead;
		if (ReadFile(fileHandle,
			     result.contents,
			     fileSize32,
			     &bytesRead,
			     0) &&
		    (fileSize32 == bytesRead))
		{
		    result.contentsSize = fileSize32;
		}
		else
		{
		    DEBUGPlatformFreeFileMemory(thread, result.contents);
		    result.contents = 0;
		}
	    }
	    else
	    {

	    }
	    CloseHandle(fileHandle);
	}
	else
	{

	}
    }
    else
    {

    }
    return(result);
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
    bool32 result = 0;
    HANDLE fileHandle = CreateFile(filename,
				   GENERIC_WRITE,
				   0,
				   0,
				   CREATE_ALWAYS,
				   0,
				   0);
    if (fileHandle != INVALID_HANDLE_VALUE)
    {
	DWORD bytesWritten;
	if (WriteFile(fileHandle, memory, memorySize, &bytesWritten, 0))
	{
	    result = (bytesWritten == memorySize);
	}
	else
	{

	}
	CloseHandle(fileHandle);
    }
    else
    {
	
    }
    return(result);
}

DEBUG_PLATFORM_WRITE_TO_FILE(DEBUGPlatformWriteToFile)
{
    bool32 result = false;
    HANDLE fileHandle = (HANDLE)openedFile->handle;
    if (fileHandle != INVALID_HANDLE_VALUE)
    {

	DWORD bytesWritten;
	if (WriteFile(fileHandle, memory, memorySize, &bytesWritten, 0))
	{
	    result = (bytesWritten == memorySize);
	}
	else
	{

	}
    }
    return(result);
}

DEBUG_PLATFORM_OPEN_FILE(DEBUGPlatformOpenFile)
{
    debug_open_file_result result = {};
    HANDLE fileHandle = CreateFile(filename,
				   GENERIC_WRITE,
				   0,
				   0,
				   CREATE_ALWAYS,
				   0,
				   0);
    result.handle = fileHandle;
    result.fileOpened = fileHandle != INVALID_HANDLE_VALUE;
    return(result);
}

DEBUG_PLATFORM_CLOSE_FILE(DEBUGPlatformCloseFile)
{
    HANDLE fileHandle = (HANDLE)openedFile->handle;
    bool32 result = false;
    if (fileHandle != INVALID_HANDLE_VALUE)
    {
	result = CloseHandle(fileHandle);
    }
    return(result);
}

internal void
CatStrings(size_t sourceACount, char* sourceA,
	   size_t sourceBCount, char* sourceB,
	   size_t destCount, char* dest)
{
    for (int index = 0; index < sourceACount; ++index)
    {
	*dest++ = *sourceA++;
    }
    for (int index = 0; index < sourceBCount; ++index)
    {
	*dest++ = *sourceB++;
    }
    *dest++ = 0;
}

internal void
Win32GetEXEFilename(win32_state* state)
{
    DWORD sizeOfFilename = GetModuleFileName(0, state->exeFilename, sizeof(state->exeFilename));
    state->onePastLastExeFilenameSlash = state->exeFilename;
    for (char* scan = state->exeFilename; *scan; ++scan)
    {
	if (*scan == '\\')
	{
	    state->onePastLastExeFilenameSlash = scan + 1;
	}
    }
}

internal int
StringLength(char* string)
{
    int count = 0;
    while (*string++)
    {
	++count;
    }
    return(count);
}

internal void
Win32BuildEXEPathFilename(win32_state* state, char* filename, int destCount, char* dest)
{
    CatStrings(state->onePastLastExeFilenameSlash - state->exeFilename, state->exeFilename,
	      StringLength(filename), filename,
	      destCount, dest);
}

inline FILETIME
Win32GetLastWriteTime(char* filename)
{
    FILETIME lastWriteTime = {};

    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesEx(filename, GetFileExInfoStandard, &data))
    {
	lastWriteTime = data.ftLastWriteTime;
    }
    return(lastWriteTime);
}

internal win32_game_code
Win32LoadGameCode(char* sourceDLLName, char* tempDLLName, char* lockFilename)
{
    win32_game_code result = {};
    WIN32_FILE_ATTRIBUTE_DATA ignored;
    if (!GetFileAttributesEx(lockFilename, GetFileExInfoStandard, &ignored))
    {
	result.dllLastWriteTime =Win32GetLastWriteTime(sourceDLLName);

	CopyFile(sourceDLLName, tempDLLName, FALSE);
	result.gameCodeDLL = LoadLibrary(tempDLLName);

	if (result.gameCodeDLL)
	{
	    result.UpdateAndRender = (game_update_and_render*)GetProcAddress(result.gameCodeDLL, "GameUpdateAndRender");
	    result.GetSoundData = (game_get_sound_data*)GetProcAddress(result.gameCodeDLL, "GameGetSoundData");
	    result.isValid = (result.UpdateAndRender && result.GetSoundData);
	}
    }
    if (!result.isValid)
    {
	result.UpdateAndRender = 0;
    }
    return(result);
}

internal void
Win32UnloadGameCode(win32_game_code* gameCode)
{
    if (gameCode->gameCodeDLL)
    {
	FreeLibrary(gameCode->gameCodeDLL);
	gameCode->gameCodeDLL = 0;
    }
    gameCode->isValid = false;
    gameCode->UpdateAndRender = 0;
    gameCode->GetSoundData = 0;
}

internal void
Win32InitSound(win32_audio_info* audioInfo, i32 samplesPerSecond)
{
    HMODULE xAudioLibrary = LoadLibrary("XAudio2_9.dll");
    HMODULE oleLibrary = LoadLibrary("Ole32.dll");

    if (xAudioLibrary && oleLibrary)
    {
	x_audio2_create* xAudio2Create = (x_audio2_create*)GetProcAddress(xAudioLibrary, "XAudio2Create");
	co_initialize* coinitialize = (co_initialize*)GetProcAddress(oleLibrary, "CoInitializeEx");	
	
	if (xAudio2Create && coinitialize)
	{
	    if(xAudio2Create(&audioInfo->audioInterface, 0, XAUDIO2_DEFAULT_PROCESSOR) == S_OK)
	    {
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
			else
			{

			}
		    }
		    else
		    {

		    }
		}
		else
		{

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
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
	u32 VKCode = (u32)wParam;
	bool32 wasDown = ((lParam & (1 << 30)) != 0);
	bool32 isDown = ((lParam & (1 << 31)) == 0);
	if (wasDown != isDown)
	{
	    if (VKCode == 'W')
	    {

	    }
	    else if (VKCode == 'A')
	    {
		
	    }
	    else if (VKCode == 'S')
	    {

	    }
	    else if (VKCode == 'D')
	    {

	    }
	    else if (VKCode == VK_UP)
	    {

	    }
	}
    }
    default:
    {
	result = DefWindowProc(hwnd, uMsg, wParam, lParam);
    } break;
    }

    return(result);
}

internal void
Win32ProcessKeyboardMessage(game_button_state* newState, bool32 isDown, bool32 wasDown)
{
    if (newState->endedDown != isDown)
    {
	newState->endedDown = isDown;
	++newState->halfTransitionCount;
    }
}

internal void
Win32GetInputFileLocation(win32_state* state, bool32 inputStream, int slotIndex, int destCount, char* dest)
{
    char temp[64];
    wsprintf(temp, "loop_edit_%d_%s.hmi", slotIndex, inputStream ? "input" : "state");
    Win32BuildEXEPathFilename(state, temp, destCount, dest);
}

internal win32_replay_buffer*
Win32GetReplayBuffer(win32_state* win32State, int unsigned index)
{
    //TODO: put assert here when you have asserts
    win32_replay_buffer* result = &win32State->replayBuffers[index];
    return(result);
}

internal void
Win32BeginRecordingInput(win32_state* win32State, int inputRecordingIndex)
{
    win32_replay_buffer* replayBuffer = Win32GetReplayBuffer(win32State, inputRecordingIndex);
    if (replayBuffer->memoryBlock)
    {
	win32State->inputRecordingIndex = inputRecordingIndex;

	char filename[WIN32_STATE_FILE_NAME_COUNT];
	Win32GetInputFileLocation(win32State, true,
				  inputRecordingIndex, sizeof(filename),
				  filename);
	win32State->recordingHandle = CreateFile(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

	CopyMemory(replayBuffer->memoryBlock, win32State->gameMemoryBlock, win32State->totalSize);
    }
}

internal void
Win32EndRecordingInput(win32_state* win32State)
{
    CloseHandle(win32State->recordingHandle);
    win32State->inputRecordingIndex = 0;
}

internal void
Win32BeginInputPlayback(win32_state* win32State, int inputPlayingIndex)
{
    win32_replay_buffer* replayBuffer = Win32GetReplayBuffer(win32State, inputPlayingIndex);
    if (replayBuffer->memoryBlock)
    {
	win32State->inputPlayingIndex = inputPlayingIndex;
	char filename[WIN32_STATE_FILE_NAME_COUNT];
	Win32GetInputFileLocation(win32State, true, inputPlayingIndex, sizeof(filename), filename);
	win32State->playbackHandle = CreateFile(filename, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

	CopyMemory(win32State->gameMemoryBlock, replayBuffer->memoryBlock, win32State->totalSize);
    }
}

internal void
Win32EndInputPlayback(win32_state* win32State)
{
    CloseHandle(win32State->playbackHandle);
    win32State->inputPlayingIndex = 0;
}

internal void
Win32RecordInput(win32_state* win32State, game_input* newInput)
{
    DWORD bytesWritten;
    WriteFile(win32State->recordingHandle, newInput, sizeof(*newInput), &bytesWritten, 0);
}

internal void
Win32PlaybackInput(win32_state* win32State, game_input* newInput)
{
    DWORD bytesRead = 0;
    if (ReadFile(win32State->playbackHandle, newInput, sizeof(*newInput), &bytesRead, 0))
    {
	if (bytesRead == 0)
	{
	    int playingIndex = win32State->inputPlayingIndex;
	    Win32EndInputPlayback(win32State);
	    Win32BeginInputPlayback(win32State, playingIndex);
	    ReadFile(win32State->playbackHandle, newInput, sizeof(*newInput), &bytesRead, 0);
	}
    }
}

internal void
ToggleFullscreen(HWND hwnd)
{
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
    if (dwStyle & WS_OVERLAPPEDWINDOW)
    {
	MONITORINFO mi = {sizeof(mi)};
	if (GetWindowPlacement(hwnd, &windowPosition) &&
	    GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY),
			   &mi))
	{
	    SetWindowLong(hwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
	    SetWindowPos(hwnd, HWND_TOP,
			  mi.rcMonitor.left, mi.rcMonitor.top,
			  mi.rcMonitor.right - mi.rcMonitor.left,
			  mi.rcMonitor.bottom - mi.rcMonitor.top,
			  SWP_NOOWNERZORDER|SWP_FRAMECHANGED);
	}
    }
    else
    {
	SetWindowLong(hwnd, GWL_STYLE, dwStyle|WS_OVERLAPPEDWINDOW);
	SetWindowPlacement(hwnd, &windowPosition);
	SetWindowPos(hwnd, 0, 0, 0, 0, 0,
		     SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|
		     SWP_NOOWNERZORDER|SWP_FRAMECHANGED);
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
	case WM_KEYUP:	    
	case WM_KEYDOWN:
	{
	    u32 VKCode = (u32)msg.wParam;
	    bool32 wasDown = ((msg.lParam & (1 << 30)) != 0);
	    bool32 isDown = ((msg.lParam & (1 << 31)) == 0);
	    //TODO: Change the properties here bc I don't think this will work for the setup you are trying to achieve with the precise inputs determining things like how long a button is pressed (although half transition count might be this)
	    if (wasDown != isDown)
	    {
		if (VKCode == 'W')
		{
		    Win32ProcessKeyboardMessage(&keyboardController->moveUp, isDown, wasDown);
		}
		else if (VKCode == 'A')
		{
		    Win32ProcessKeyboardMessage(&keyboardController->moveLeft, isDown, wasDown);
		}
		else if (VKCode == 'S')
		{
		    Win32ProcessKeyboardMessage(&keyboardController->moveDown, isDown, wasDown);
		}
		else if (VKCode == 'D')
		{
		    Win32ProcessKeyboardMessage(&keyboardController->moveRight, isDown, wasDown);
		}
		else if (VKCode == VK_ESCAPE)
		{
		    running = false;
		}
		else if (VKCode == 'J')
		{
		    Win32ProcessKeyboardMessage(&keyboardController->actionDown, isDown, wasDown);
		}
		else if (VKCode == VK_SHIFT)
		{
		    Win32ProcessKeyboardMessage(&keyboardController->start, isDown, wasDown);
		}
#if JUMPER_INTERNAL
		else if (VKCode == 'P')
		{
		    if (isDown)
		    {
			pause = !pause;
		    }
		}
		else if (VKCode == 'L')
		{
		    if (isDown)
		    {
			if (win32State->inputPlayingIndex == 0)
			{
			    if (win32State->inputRecordingIndex == 0)
			    {
				Win32BeginRecordingInput(win32State, 1);
			    }
			    else
			    {
				Win32EndRecordingInput(win32State);
				Win32BeginInputPlayback(win32State, 1);
			    }
			}
			else
			{
			    Win32EndInputPlayback(win32State);
			}
		    }
		}

		else if (VKCode == 'V')
		{
		    if (isDown)
		    {
			Win32ProcessKeyboardMessage(&keyboardController->scrollUp, isDown, wasDown);
			keyboardController->scrollUp.started = true;
		    }
		    else
		    {
			keyboardController->scrollUp.started = false;
		    }
		    
		}
		else if (VKCode == 'C')
		{
		    if (isDown)
		    {
			Win32ProcessKeyboardMessage(&keyboardController->scrollDown, isDown, wasDown);
		    }
		    else
		    {
			keyboardController->scrollDown.started = false;
		    }
		}
		if (isDown)
		{
		    bool32 altKeyWasDown = ((msg.lParam & (1 << 29)) != 0);
		    if ((VKCode == 'S') && altKeyWasDown)
		    {
			Win32ProcessKeyboardMessage(&keyboardController->save, isDown, wasDown);		
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

		    if ((VKCode == VK_RETURN) && altKeyWasDown)
		    {
			if (msg.hwnd)
			{
			    ToggleFullscreen(msg.hwnd);
			}
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

internal void
Win32PlayAndSubmitSound(win32_audio_info* audioInfo, u8* audioBuffer)
{

    XAUDIO2_BUFFER audioDataBuffer = {};
    audioDataBuffer.Flags = XAUDIO2_END_OF_STREAM;
    audioDataBuffer.AudioBytes = AUDIO_BUFFER_SIZE_BYTES;
    audioDataBuffer.pAudioData = (BYTE*)audioBuffer;
    audioDataBuffer.LoopCount = XAUDIO2_LOOP_INFINITE;
	    
    audioInfo->sourceVoice->SubmitSourceBuffer(&audioDataBuffer);
    audioInfo->sourceVoice->Start(0);
}

inline LARGE_INTEGER
Win32GetWallClock(void)
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return(result);
}

inline r32
Win32GetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
    r32 result = ((r32)(end.QuadPart - start.QuadPart) / (r32)perfCountFrequency);
    return(result);
}



/*
  WINMAIN
  WINMAIN
  WINMAIN
 */
int CALLBACK WinMain(HINSTANCE hInstance,
		     HINSTANCE hPrevInstance,
		     LPSTR lpCmdLine,
		     int nCmdShow)
{
    LARGE_INTEGER perfCountFrequencyResult;
    QueryPerformanceFrequency(&perfCountFrequencyResult);
    perfCountFrequency = perfCountFrequencyResult.QuadPart;

    win32_state win32State = {};
    Win32GetEXEFilename(&win32State);
    
    char sourceGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFilename(&win32State, "jumper.dll", sizeof(sourceGameCodeDLLFullPath), sourceGameCodeDLLFullPath);

    char tempCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFilename(&win32State, "jumper_temp.dll", sizeof(tempCodeDLLFullPath), tempCodeDLLFullPath);

    char gameCodeLockFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFilename(&win32State, "lock.tmp", sizeof(gameCodeLockFullPath), gameCodeLockFullPath);

    UINT desiredSchedulerMs = 1;
    bool32 sleepIsGranular = (timeBeginPeriod(desiredSchedulerMs) == TIMERR_NOERROR);
    
    win32_audio_info audioInfo;
    Win32InitSound(&audioInfo, 48000);
    game_sound_info soundInfo = {};

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
	    i32 monitorRefreshHz = 60;

#if 1 
	    HDC refreshDC = GetDC(window);
	    i32 win32RefreshRate = GetDeviceCaps(refreshDC, VREFRESH);
	    ReleaseDC(window, refreshDC);
	    if (win32RefreshRate > 1)
	    {
		monitorRefreshHz = win32RefreshRate;
	    }
#endif
	    r32 gameUpdateHz = (monitorRefreshHz / 2.0f);
	    r32 targetSecondsPerFrame = 1.0f / (r32)gameUpdateHz;

	    game_state gameState = {};
#if 0
	    //NOTE: This is the code for writing a sine wave, everthing is packed but will need
	    //more attention when the time comes for abstracting the game layer and such

	    r64 phase = 0;
	    u32 bufferIndex = 0;
	    u8 audioBuffer[AUDIO_BUFFER_SIZE_BYTES];
	    //The action of filling the buffer
	    while (bufferIndex < AUDIO_BUFFER_SIZE_BYTES)
	    {
		phase += (2 * pi32) / SAMPLES_PER_CYCLE;
		i16 sample = (i16)(sin(phase) * INT16_MAX * 0.5f); //last value here is our volume, I'm just too lazy to make  variable atm
		audioBuffer[bufferIndex++] = (u8)sample;
		audioBuffer[bufferIndex++] = (u8)(sample >> 8);
	    }

	    Win32PlayAndSubmitSound(&audioInfo, audioBuffer);
#endif	    
	    running = true;


	    char* sourceDLLName = "jumper.dll";
	    win32_game_code game = Win32LoadGameCode(sourceGameCodeDLLFullPath, tempCodeDLLFullPath, gameCodeLockFullPath);



#if HANDMADE_INTERNAL
	    LPVOID baseAddress = (LPVOID)Terabytes((u64)2);
#else
	    LPVOID baseAddress = 0;
#endif
	    
	    game_memory gameMemory = {};
	    gameMemory.permanentStorageSize = Megabytes(64);
	    gameMemory.permanentStorageSize = Gigabytes(1);
	    gameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
	    gameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
	    gameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;
	    gameMemory.DEBUGPlatformOpenFile = DEBUGPlatformOpenFile;
	    gameMemory.DEBUGPlatformCloseFile = DEBUGPlatformCloseFile;
	    gameMemory.DEBUGPlatformWriteToFile = DEBUGPlatformWriteToFile;
	    
	    win32State.totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;
	    win32State.gameMemoryBlock = VirtualAlloc(baseAddress, (size_t)win32State.totalSize,
						       MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	    gameMemory.permanentStorage = win32State.gameMemoryBlock;
	    gameMemory.transientStorage = ((u8*)gameMemory.permanentStorage + gameMemory.permanentStorageSize);

	    for (int replayIndex = 0; replayIndex < ArrayCount(win32State.replayBuffers); ++replayIndex)
	    {
		win32_replay_buffer* replayBuffer = &win32State.replayBuffers[replayIndex];

		Win32GetInputFileLocation(&win32State, false,
					  replayIndex, sizeof(replayBuffer->filename),
					  replayBuffer->filename);
		replayBuffer->fileHandle =
		    CreateFile(replayBuffer->filename, GENERIC_READ|GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

		DWORD maxSizeHigh = (win32State.totalSize >> 32);
		DWORD maxSizeLow = (win32State.totalSize & 0xFFFFFFFF);

		replayBuffer->memoryMap = CreateFileMapping(replayBuffer->fileHandle, 0,
							    PAGE_READWRITE,
							    maxSizeHigh,
							    maxSizeLow,
							    0);
		DWORD error = GetLastError();

		replayBuffer->memoryBlock = MapViewOfFile(replayBuffer->memoryMap,
							  FILE_MAP_ALL_ACCESS, 0, 0,
							  win32State.totalSize);

		if (replayBuffer->memoryBlock)
		{
		    
		}
		else
		{
		    
		}
	    }

	    if (gameMemory.permanentStorage && gameMemory.transientStorage)
	    {

		i32 xOffset = 0;
		game_input input[2] = {};
		game_input* newInput = &input[0];
		game_input* oldInput = &input[1];


		LARGE_INTEGER lastCounter = Win32GetWallClock();
		u64 lastCycleCount = __rdtsc();
		LARGE_INTEGER flipWallClock = Win32GetWallClock();

		u32 loadCounter = 120;

		/*
		  Start of the main game loop
		*/
		
		while (running)
		{
		    newInput->dTime = targetSecondsPerFrame;
		    FILETIME newDLLWriteTime = Win32GetLastWriteTime(sourceGameCodeDLLFullPath);
		    if (CompareFileTime(&newDLLWriteTime, &game.dllLastWriteTime) != 0)
		    {
			Win32UnloadGameCode(&game);
			game = Win32LoadGameCode(sourceGameCodeDLLFullPath, tempCodeDLLFullPath, gameCodeLockFullPath);
			loadCounter = 0;
		    }
		
		    DWORD maxControllerCount = XUSER_MAX_COUNT;

		    game_controller_input* oldKeyboardController = GetController(oldInput, 0);
		    game_controller_input* newKeyboardController = GetController(newInput, 0);
		    *newKeyboardController = {};
		    newKeyboardController->isConnected = true;
		    for (int buttonIndex = 0; buttonIndex < ArrayCount(newKeyboardController->buttons); ++buttonIndex)
		    {
			newKeyboardController->buttons[buttonIndex].endedDown =
			    oldKeyboardController->buttons[buttonIndex].endedDown;
			newKeyboardController->buttons[buttonIndex].started =
			    oldKeyboardController->buttons[buttonIndex].started;			
		    }
		    Win32ProcessPendingMessages(&win32State, newKeyboardController);

		    POINT mouseP;
		    GetCursorPos(&mouseP);
		    ScreenToClient(window, &mouseP);
		    newInput->mouseX = mouseP.x;
		    newInput->mouseY = mouseP.y;
		    Win32ProcessKeyboardMessage(&newInput->mouseButtons[0],
						GetKeyState(VK_LBUTTON) & (1 << 15), 0);
		    Win32ProcessKeyboardMessage(&newInput->mouseButtons[1],
						GetKeyState(VK_RBUTTON) & (1 << 15), 0);		    
		    
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

		    game_offscreen_buffer goBuffer = {};
		    goBuffer.memory = globalBackBuffer.memory;
		    goBuffer.width = globalBackBuffer.width;
		    goBuffer.height = globalBackBuffer.height;
		    goBuffer.pitch = globalBackBuffer.pitch;
		    goBuffer.bytesPerPixel = globalBackBuffer.bytesPerPixel;

		    if (win32State.inputRecordingIndex)
		    {
			Win32RecordInput(&win32State, newInput);
		    }

		    if (win32State.inputPlayingIndex)
		    {
			Win32PlaybackInput(&win32State, newInput);
		    }

		    thread_context thread = {};
		    
		    if (game.UpdateAndRender)
		    {
			game.UpdateAndRender(&thread, &goBuffer, &gameMemory, newInput);
		    }

		    if (game.GetSoundData)
		    {
			if (!soundInfo.bufferFilled)
			{
			    game.GetSoundData(&soundInfo);
			    Win32PlayAndSubmitSound(&audioInfo, soundInfo.buffer);
			}
		    }

		    LARGE_INTEGER workCounter = Win32GetWallClock();
		    r32 workSecondsElapsed = Win32GetSecondsElapsed(lastCounter, workCounter);

		    r32 secondsElapsedForFrame = workSecondsElapsed;
		    if (secondsElapsedForFrame < targetSecondsPerFrame)
		    {
			DWORD sleepMs = 0;

			while (secondsElapsedForFrame < targetSecondsPerFrame)
			{
			    if (sleepIsGranular)
			    {
				sleepMs = (DWORD)(1000.0f * (targetSecondsPerFrame - secondsElapsedForFrame));
				if (sleepMs > 0)
				{
				    Sleep(sleepMs - 1);
				}
			    }
			    r32 testSecondsElapsedForFrame = Win32GetSecondsElapsed(lastCounter, Win32GetWallClock());
			    secondsElapsedForFrame = Win32GetSecondsElapsed(lastCounter, Win32GetWallClock());
			    if (testSecondsElapsedForFrame < targetSecondsPerFrame)
			    {
			    
			    }
			}
		    }

		    LARGE_INTEGER endCounter = Win32GetWallClock();
		    r32 msPerFrame = (1000.0f * (Win32GetSecondsElapsed(lastCounter, endCounter)));
		    lastCounter = endCounter;
		
		    win32_window_dimension dimension = Win32GetWindowDimension(window);
		
		    HDC deviceContext = GetDC(window);
		    Win32DisplayBufferWindow(&globalBackBuffer, deviceContext, 0, 0, dimension.width, dimension.height);
		    ReleaseDC(window, deviceContext);

		    flipWallClock = Win32GetWallClock();

		    u64 endCycleCount = __rdtsc();
		    u64 cyclesElapsed = endCycleCount - lastCycleCount;

		    r64 FPS = 0;
		    r64 MCPF = (r64)(cyclesElapsed / (1000 * 1000));

#if 1
		    char fpsBuffer[250];
		    sprintf_s(fpsBuffer, "%.02fms/f, %ff/s, %.02fmc/f\n", msPerFrame, FPS, MCPF);
		    OutputDebugString(fpsBuffer);
#endif
		    lastCycleCount = endCycleCount;
		
		    game_input* temp = newInput;
		    newInput = oldInput;
		    oldInput = temp;
		}
	    }
	}
	
    }
}
  
