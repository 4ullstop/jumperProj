#if !defined(WIN32_JUMPER_H)



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

struct win32_game_code
{
    HMODULE gameCodeDLL;
    FILETIME dllLastWriteTime;

    game_update_and_render* UpdateAndRender;
    game_get_sound_data* GetSoundData;
    
    bool32 isValid;
};

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH

struct win32_replay_buffer
{
    HANDLE fileHandle;
    HANDLE memoryMap;
    char filename[WIN32_STATE_FILE_NAME_COUNT];
    void* memoryBlock;
};

struct win32_state
{
    u64 totalSize;
    void* gameMemoryBlock;
    win32_replay_buffer replayBuffers[4];
    
    HANDLE recordingHandle;
    i32 inputRecordingIndex;

    HANDLE playbackHandle;
    i32 inputPlayingIndex;
    
    char exeFilename[WIN32_STATE_FILE_NAME_COUNT];
    char* onePastLastExeFilenameSlash;

    bool32 scrollUpWasDown;
    bool32 scrollDownWasDown;    
};


#define WIN32_JUMPER_H
#endif
