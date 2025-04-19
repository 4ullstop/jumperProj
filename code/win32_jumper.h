#if !defined(WIN32_JUMPER_H)
#include <windows.h>
#include <stdint.h>


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

#define WIN32_JUMPER_H
#endif
