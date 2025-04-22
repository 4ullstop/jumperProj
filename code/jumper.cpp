#include "jumper.h"

internal void
DrawRectangle(game_offscreen_buffer* buffer,
	      r32 realMinX, r32 realMinY,
	      r32 realMaxX, r32 realMaxY,
	      r32 r, r32 g, r32 b)
{
    i32 minX = RoundReal32ToInt32(realMinX);
    i32 minY = RoundReal32ToInt32(realMinY);
    i32 maxX = RoundReal32ToInt32(realMaxX);
    i32 maxY = RoundReal32ToInt32(realMaxY);

    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;

    if (maxX > buffer->width) maxX = buffer->width;
    if (maxY > buffer->height) maxY = buffer->height;

    u32 color = ((RoundReal32ToUInt32(r * 255.0f) << 16) |
		 (RoundReal32ToUInt32(g * 255.0f) << 8) |
		 (RoundReal32ToUInt32(b * 255.0f) << 0));

    u8* row = ((u8*)buffer->memory + minX * buffer->bytesPerPixel + minY * buffer->pitch);

    for (int y = minY; y < maxY; ++y)
    {
	u32* pixel = (u32*)row;
	for (int x = minX; x < maxX; ++x)
	{
	    *(u32*)pixel++ = color;
	}
	row += buffer->pitch;
    }
    
}

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
    Assert((&input->controllers[0].terminator - &input->controllers[0].buttons[0]) ==
	   (ArrayCount(input->controllers[0].buttons)));
    Assert(sizeof(game_state) <= memory->permanentStorageSize);

//    game_state* gameState = (game_state*)memory->permanentStorage;

    if (!memory->isInitialized)
    {
	memory->isInitialized = true;
    }
    
    for (int controllerIndex = 0; controllerIndex < ArrayCount(input->controllers); ++controllerIndex)
    {
	game_controller_input* controller = GetController(input, controllerIndex);

	if (controller->isAnalog)
	{

	}
	else
	{
	    r32 dPlayerX = 0.0f;
	    r32 dPlayerY = 0.0f;

	    if (controller->moveUp.endedDown)
	    {
		dPlayerY = -1.0f;
	    }
	    if (controller->moveDown.endedDown)
	    {
		dPlayerY = 1.0f;
	    }
	    if (controller->moveLeft.endedDown)
	    {
		dPlayerX = -1.0f;
	    }
	    if (controller->moveRight.endedDown)
	    {
		dPlayerX = 1.0f;
	    }

	    dPlayerX *= 128.0f;
	    dPlayerY *= 128.0f;

	    gameState->playerX += input->dTime * dPlayerX;
	    gameState->playerY += input->dTime * dPlayerY;	    
	}
    }

    u32 tileMap[9][17] =
    {
	{0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
	{0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
	{0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
	{0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
	{0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
	{0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
	{0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
	{0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},
	{0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, 1},	
    };

    r32 upperLeftX = -30;
    r32 upperLeftY = 0;
    r32 tileWidth = 60;
    r32 tileHeight = 60;

    //clear the background
    DrawRectangle(buffer, 0.0f, 0.0f, (r32)buffer->width, (r32)buffer->height, 1.0f, 0.0f, 1.0f);

    //draw our tilemap
    for (int row = 0; row < 9; ++row)
    {
	for (int column = 0; column < 17; ++column)
	{
	    u32 tileId = tileMap[row][column];
	    r32 gray = 0.5f;
	    if (tileId == 1)
	    {
		gray = 1.0f;
	    }

	    r32 minX = upperLeftX + ((r32)column) * tileWidth;
	    r32 minY = upperLeftY + ((r32)row) * tileHeight;
	    r32 maxX = minX + tileWidth;
	    r32 maxY = minY + tileHeight;

	    DrawRectangle(buffer, minX, minY, maxX, maxY, gray, gray, gray);
	}
    }


    //draw our player
    r32 playerR = 1.0f;
    r32 playerG = 1.0f;
    r32 playerB = 0.0f;

    r32 playerWidth = 0.75f*(r32)tileWidth;
    r32 playerHeight = (r32)tileHeight;

    r32 playerTop = gameState->playerY - playerHeight;
    r32 playerLeft = gameState->playerX - 0.5f * playerWidth;

    DrawRectangle(buffer, playerLeft, playerTop,
		  playerLeft + playerWidth,
		  playerTop + playerHeight,
		  playerR, playerG, playerB);
}
