#if !defined(WIN32_JUMPER_H)

#include <stdint.h>

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

typedef float r32;
typedef double r64;

struct win32_offscreen_buffer
{
    BITMAPINFO info;
    void* memory;
    int height;
    int width;
    int pitch;
    int bytesPerPixel;
};

struct win32_window_dimension
{
    int width;
    int height;
};

struct win32_audio_info
{
    IXAudio2* audioInterface;
    IXAudio2MasteringVoice* audioMasterVoice;
    IXAudio2SourceVoice* sourceVoice;
};

struct game_button_state
{
    int halfTransitionCount;
    bool32 endedDown;
};

struct game_controller_input
{
    bool32 isAnalog;
    bool32 isConnected;

    r32 stickAverageX;
    r32 stickAverageY;

    union
    {
	game_button_state buttons[12];
	struct
	{
	    game_button_state moveUp;
	    game_button_state moveDown;
	    game_button_state moveRight;
	    game_button_state moveLeft;

	    game_button_state actionUp;
	    game_button_state actionDown;
	    game_button_state actionLeft;
	    game_button_state actionRight;

	    game_button_state leftShoulder;
	    game_button_state rightShoulder;

	    game_button_state back;
	    game_button_state start;

	    game_button_state terminator;
	};
    };
};

struct game_input
{
    game_button_state mouseButtons[5];
    i32 mouseX, mouseY, mouseZ;

    r32 dTime;
    game_controller_input controllers[5];
};

struct win32_state
{
    u64 totalSize;
    void* gameMemoryBlock;

    HANDLE recordingIndex;
    i32 inputRecordingIndex;
};

inline game_controller_input* GetController(game_input* input, int unsigned controllerIndex)
{
    game_controller_input* result = &input->controllers[controllerIndex];
    return(result);
}

#define WIN32_JUMPER_H
#endif
