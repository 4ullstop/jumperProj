#include "jumper.h"

internal void
RenderGradient(game_offscreen_buffer* buffer, int xOffset, int yOffset)
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

internal void
FillSinWaveSoundBuffer(game_sound_info* gameSoundInfo)
{
    r64 phase = 0;
    u32 bufferIndex = 0;
    while (bufferIndex < AUDIO_BUFFER_SIZE_BYTES)
    {
	phase += (2 * pi32) / SAMPLES_PER_CYCLE;
	i16 sample = (i16)(sin(phase) * INT16_MAX * 0.5f);
	gameSoundInfo->buffer[bufferIndex++] = (u8)sample;
	gameSoundInfo->buffer[bufferIndex++] = (u8)(sample >> 8);
    }
}


extern "C" GAME_GET_SOUND_DATA(GameGetSoundData)
{
    if (!soundInfo->bufferFilled)
    {
	FillSinWaveSoundBuffer(soundInfo);
	soundInfo->bufferFilled = true;
    }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    int foo = 4;

    for (int controllerIndex = 0; controllerIndex < ArrayCount(input->controllers); ++controllerIndex)
    {
	game_controller_input* controller = GetController(input, controllerIndex);

	if (controller->isAnalog)
	{

	}
	else
	{
	    if (controller->moveUp.endedDown)
	    {
		gameState->yOffset++;
	    }
	    if (controller->moveDown.endedDown)
	    {
		gameState->yOffset--;
	    }
	    if (controller->moveLeft.endedDown)
	    {
		gameState->xOffset++;
	    }
	    if (controller->moveRight.endedDown)
	    {
		gameState->xOffset--;
	    }
	}
    }

    RenderGradient(backBuffer, gameState->xOffset, gameState->yOffset);
}
