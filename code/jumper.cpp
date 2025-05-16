#include "jumper.h"
#include "jumper_tile.cpp"

#pragma pack(push, 1)
struct bitmap_header
{
    u16 fileType;
    u32 fileSize;
    u16 reserved1;
    u16 reserved2;
    u32 bitmapOffset;
    u32 size;
    i32 width;
    i32 height;
    u16 planes;
    u16 bitsPerPixel;
    u32 compression;
    u32 sizeOfBitmap;
    i32 horzResolution;
    i32 vertResolution;
    u32 colorsUsed;
    u32 colorsImportant;

    u32 redMask;
    u32 greenMask;
    u32 blueMask;    
};
#pragma pack(pop)  

internal loaded_bitmap
DEBUGLoadBMP(thread_context* thread, debug_platform_read_entire_file* ReadEntireFile, char* filename)
{
    loaded_bitmap result = {};
    debug_read_file_result readResult = ReadEntireFile(thread, filename);

    if (readResult.contentsSize != 0)
    {
	bitmap_header* header = (bitmap_header*)readResult.contents;
	u32* pixels = (u32*)((u8*)readResult.contents + header->bitmapOffset);
	result.pixels = pixels;
	result.width = header->width;
	result.height = header->height;

	Assert(header->compression == 3);

	u32 redMask = header->redMask;
	u32 greenMask = header->greenMask;
	u32 blueMask = header->blueMask;
	u32 alphaMask = ~(redMask | greenMask | blueMask);

	bit_scan_result redShift = FindLeastSignificantSetBit(redMask);
	bit_scan_result greenShift = FindLeastSignificantSetBit(greenMask);
	bit_scan_result blueShift = FindLeastSignificantSetBit(blueMask);
	bit_scan_result alphaShift = FindLeastSignificantSetBit(alphaMask);

	Assert(redShift.found);
	Assert(greenShift.found);
	Assert(blueShift.found);
	Assert(alphaShift.found);
	
	u32* sourceDest = pixels;
	for (i32 y = 0; y < header->height; ++y)
	{
	    for (i32 x = 0; x < header->width; ++x)
	    {
		u32 c = *sourceDest;

		*sourceDest++ = ((((c >> alphaShift.index) & 0xFF) << 24) |
			       (((c >> redShift.index) & 0xFF) << 16) |
			       (((c >> greenShift.index) & 0xFF) << 8) |
			       (((c >> blueShift.index) & 0xFF) << 0));
	    }
	}
    }
    return(result);
}

internal void
DrawBitmap(game_offscreen_buffer* buffer, loaded_bitmap* bitmap,
	   r32 realX, r32 realY,
	   i32 alignX = 0, i32 alignY = 0)
{
    realX -= (r32)alignX;
    realY -= (r32)alignY;

    i32 minX = RoundReal32ToInt32(realX);
    i32 minY = RoundReal32ToInt32(realY);
    i32 maxX = RoundReal32ToInt32(realX + (r32)bitmap->width);
    i32 maxY = RoundReal32ToInt32(realY + (r32)bitmap->height);

    i32 sourceOffsetX = 0;

    if (minX < 0)
    {
	sourceOffsetX = -minX;
	minX = 0;
    }

    i32 sourceOffsetY = 0;
    if (minY < 0)
    {
	sourceOffsetY = -minY;
	minY = 0;
    }

    if (maxX > buffer->width)
    {
	maxX = buffer->width;
    }
    if (maxY > buffer->height)
    {
	maxY = buffer->height;
    }

    u32* sourceRow = bitmap->pixels + bitmap->width*(bitmap->height - 1);
    sourceRow += -bitmap->width*sourceOffsetY + sourceOffsetX;
    u8* destRow = ((u8*)buffer->memory +
		   minX * buffer->bytesPerPixel +
		   minY * buffer->pitch);

    for (i32 y = minY; y < maxY; ++y)
    {
	u32* dest = (u32*)destRow;
	u32* source = sourceRow;
	for (i32 x = minX; x < maxX; ++x)
	{
	    r32 a = ((*source >> 24) &0xFF) / 255.0f;
	    r32 SR = (r32)((*source >> 16) & 0xFF);
	    r32 SG = (r32)((*source >> 8) & 0xFF);
	    r32 SB = (r32)((*source >> 0) & 0xFF);
	    
	    r32 DR = (r32)((*dest >> 16) & 0xFF);
	    r32 DG = (r32)((*dest >> 8) & 0xFF);
	    r32 DB = (r32)((*dest >> 0) & 0xFF);

	    r32 r = (1.0f-a)*DR + a*SR;
	    r32 g = (1.0f-a)*DG + a*SG;
	    r32 b = (1.0f-a)*DB + a*SB;

	    *dest = (((u32)(r + 0.5f) << 16) |
		     ((u32)(g + 0.5f) << 8) |
		     ((u32)(b + 0.5f) << 0));

	    if ((*source >> 24) > 128)
	    {
		*dest = *source;
	    }
	    ++dest;
	    ++source;
	}
	destRow += buffer->pitch;
	sourceRow -= bitmap->width;
    }
}

internal void
DrawBackgroundTile(game_offscreen_buffer* buffer, loaded_bitmap* bitmap, v2 vMin, v2 vMax)
{
    i32 minX = RoundReal32ToInt32(vMin.x);
    i32 minY = RoundReal32ToInt32(vMin.y);
    i32 maxX = RoundReal32ToInt32(vMax.x);
    i32 maxY = RoundReal32ToInt32(vMax.y);


    i32 sourceOffsetX = 0;

    if (minX < 0)
    {
	sourceOffsetX = -minX;
	minX = 0;
    }

    i32 sourceOffsetY = 0;
    if (minY < 0)
    {
	sourceOffsetY = -minY;
	minY = 0;
    }
    if (maxX > buffer->width)
    {
	maxX = buffer->width;
    }
    if (maxY > buffer->height)
    {
	maxY = buffer->height;
    }

    u32* sourceRow = bitmap->pixels + bitmap->width*(bitmap->height - 1);
    sourceRow += -bitmap->width*sourceOffsetY + sourceOffsetX;
    u8* destRow = ((u8*)buffer->memory +
		   minX * buffer->bytesPerPixel +
		   minY * buffer->pitch);

    for (i32 y = minY; y < maxY; ++y)
    {
	u32* dest = (u32*)destRow;
	u32* source = sourceRow;
	for (i32 x = minX; x < maxX; ++x)
	{
	    r32 a = ((*source >> 24) &0xFF) / 255.0f;
	    r32 SR = (r32)((*source >> 16) & 0xFF);
	    r32 SG = (r32)((*source >> 8) & 0xFF);
	    r32 SB = (r32)((*source >> 0) & 0xFF);
	    
	    r32 DR = (r32)((*dest >> 16) & 0xFF);
	    r32 DG = (r32)((*dest >> 8) & 0xFF);
	    r32 DB = (r32)((*dest >> 0) & 0xFF);

	    r32 r = (1.0f-a)*DR + a*SR;
	    r32 g = (1.0f-a)*DG + a*SG;
	    r32 b = (1.0f-a)*DB + a*SB;

	    *dest = (((u32)(r + 0.5f) << 16) |
		     ((u32)(g + 0.5f) << 8) |
		     ((u32)(b + 0.5f) << 0));

	    if ((*source >> 24) > 128)
	    {
		*dest = *source;
	    }
	    ++dest;
	    ++source;
	}
	destRow += buffer->pitch;
	sourceRow -= bitmap->width;
    }    
}

internal void
DrawRectangle(game_offscreen_buffer* buffer, v2 vMin, v2 vMax,
	      r32 r, r32 g, r32 b)
{
    i32 minX = RoundReal32ToInt32(vMin.x);
    i32 minY = RoundReal32ToInt32(vMin.y);
    i32 maxX = RoundReal32ToInt32(vMax.x);
    i32 maxY = RoundReal32ToInt32(vMax.y);

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

inline entity*
GetEntity(game_state* gameState, u32 index)
{
    entity* entity = 0;
    if ((index > 0) && (index < ArrayCount(gameState->entities)))
    {
	entity = &gameState->entities[index];
    }
    return(entity);
}

internal u32
AddEntity(game_state* gameState)
{
    u32 entityIndex = gameState->entityCount++;
    Assert(gameState->entityCount < ArrayCount(gameState->entities));
    entity* result = &gameState->entities[entityIndex];
    return(entityIndex);
}

internal void
InitializePlayer(game_state* gameState, u32 entityIndex)
{
    entity* entity = GetEntity(gameState, entityIndex);
    entity->exists = true;
    entity->p.absTileX = 17/2;
    entity->p.absTileY = 3;
    entity->p.offset.x = 0.0f;
    entity->p.offset.y = 0.0f;
    entity->height = 0.5f;
    entity->width = 1.0f;
    entity->canJump = true;
    if (!GetEntity(gameState, gameState->cameraFollowingEntityIndex))
    {
	gameState->cameraFollowingEntityIndex = entityIndex;
    }
}

internal bool32
TestWall(r32 wallX, r32 relX, r32 relY, r32 playerDeltaX, r32 playerDeltaY, r32* tMin,
	 r32 minY, r32 maxY)
{
    bool32 hit = false;
    r32 epsilon = 0.0001f;

    if (playerDeltaX != 0.0f)
    {
	r32 tResult = (wallX - relX) / playerDeltaX;
	r32 y = relY + tResult * playerDeltaY;
	if ((tResult >= 0.0f) && (*tMin > tResult))
	{
	    if ((y >= minY) && (y <= maxY))
	    {
		*tMin = Maximum(0.0f, tResult - epsilon);
		hit = true;
	    }
	}
    }
    return(hit);
}

internal void
MovePlayer(game_state* gameState, entity* entity, r32 dt, v2 ddP)
{
    tile_map* tileMap = gameState->world->tileMap;

    r32 ddPLength = LengthSq(ddP);
    if (ddPLength > 1.0f)
    {
	ddP *= 1.0f / SquareRoot(ddPLength);
    }

    r32 playerSpeed = 300.0f;
    ddP *= playerSpeed;
    
    ddP += IsEntityInAir(entity) ? -2.0f * entity->dP : -35.0f*entity->dP;
    //ddP += -35.0f*entity->dP;

    tile_map_position oldPlayerP = entity->p;

    v2 playerDelta = (0.5f * ddP * Square(dt) + entity->dP*dt);
    r32 gravity = IsEntityInAir(entity) ? -10.0f : -9.81f;
    
    entity->dP.y += dt * (gravity * tileMap->metersToPixels);

    entity->dP = ddP * dt + entity->dP;

    tile_map_position newPlayerP = Offset(tileMap, oldPlayerP, playerDelta);

    u32 minTileX = Minimum(oldPlayerP.absTileX, newPlayerP.absTileX);
    u32 minTileY = Minimum(oldPlayerP.absTileY, newPlayerP.absTileY);
    u32 maxTileX = Maximum(oldPlayerP.absTileX, newPlayerP.absTileX);
    u32 maxTileY = Maximum(oldPlayerP.absTileY, newPlayerP.absTileY);

    u32 entityTileWidth = CeilReal32ToInt32(entity->width / tileMap->tileSideInMeters);
    u32 entityTileHeight = CeilReal32ToInt32(entity->height / tileMap->tileSideInMeters);

    minTileX -= entityTileWidth;
    minTileY -= entityTileHeight;
    maxTileX += entityTileWidth;
    maxTileY += entityTileHeight;

    u32 absTileZ = entity->p.absTileZ;

    r32 tRemaining = 1.0f;
    r32 tMin = 1.0f;

    bool32 isFloor = false;
    for (u32 iteration = 0; (iteration < 4) && (tRemaining > 0.0f); ++iteration)
    {
	tMin = 1.0f;
	v2 wallNormal = {};

	Assert((maxTileX - minTileX) < 32);
	Assert((maxTileY - minTileY) < 32);

	for (u32 absTileY = minTileY; absTileY <= maxTileY; ++absTileY)
	{
	    for (u32 absTileX = minTileX; absTileX <= maxTileX; ++absTileX)
	    {
		tile_map_position testTileP = CenteredTilePoint(absTileX, absTileY, absTileZ);
		tile_value tileValue = GetTileValue(tileMap, testTileP);

		if (!IsTileValueEmpty(tileValue))
		{
		    r32 diameterW = tileMap->tileSideInMeters + entity->width;
		    r32 diameterH = tileMap->tileSideInMeters + entity->height;
		    v2 minCorner = -0.5f*v2{diameterW, diameterH};
		    v2 maxCorner = 0.5f*v2{diameterW, diameterH};

		    tile_map_difference relOldPlayerP = Subtract(tileMap, &entity->p, &testTileP);
		    v2 rel = relOldPlayerP.dXY;
		    
		    if (TestWall(minCorner.x, rel.x, rel.y, playerDelta.x, playerDelta.y,
				 &tMin, minCorner.y, maxCorner.y))
		    {
			wallNormal = v2{-1, 0};
		    }
		    if (TestWall(maxCorner.x, rel.x, rel.y, playerDelta.x, playerDelta.y,
				 &tMin, minCorner.y, maxCorner.y))
		    {
			wallNormal = v2{1, 0};
		    }
		    if (TestWall(minCorner.y, rel.y, rel.x, playerDelta.y, playerDelta.x,
				 &tMin, minCorner.x, maxCorner.x))
		    {
			wallNormal = v2{0, -1};
		    }
		    if (TestWall(maxCorner.y, rel.y, rel.x, playerDelta.y, playerDelta.x,
				 &tMin, minCorner.x, maxCorner.x))
		    {
			wallNormal = v2{0, 1};
			isFloor = true;
		    }		    
		}
	    }
	}
	entity->p = Offset(tileMap, entity->p, tMin*playerDelta);
	i32 bounceValue = (isFloor || (!IsEntityInAir(entity))) ? 1 : 2;
	entity->dP = entity->dP - bounceValue*Inner(entity->dP, wallNormal)*wallNormal;
	playerDelta = playerDelta - 1 * Inner(playerDelta, wallNormal)*wallNormal;
	tRemaining -= tMin;
    }

    if ((entity->dP.x == 0) && (entity->dP.y == 0))
    {
	
    }
    else if (AbsoluteValue(entity->dP.x) > AbsoluteValue(entity->dP.y))
    {
	if (entity->dP.x > 0)
	{
	    entity->facingDirection = 0;
	}
	else
	{
	    entity->facingDirection = 2;
	}
    }
    else
    {
	if (entity->dP.x > 0)
	{
	    entity->facingDirection = 1;
	}
	else
	{
	    entity->facingDirection = 3;
	}
    }
}

extern "C" GAME_GET_SOUND_DATA(GameGetSoundData)
{
#if 0
    if (!soundInfo->bufferFilled)
    {
	FillSinWaveSoundBuffer(soundInfo);
	soundInfo->bufferFilled = true;
    }
#endif    
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert((&input->controllers[0].terminator - &input->controllers[0].buttons[0]) ==
	   (ArrayCount(input->controllers[0].buttons)));
    Assert(sizeof(game_state) <= memory->permanentStorageSize);

    game_state* gameState = (game_state*)memory->permanentStorage;

#if JUMPER_INTERNAL    
    u32 serializedChunks[100][9][33];
#else
    u32 serializedChunks = 0;
#endif
    
    if (!memory->isInitialized)
    {

	background_bitmaps* background;
	background = gameState->backgroundBitmaps;
	background->blueTile = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "BMP/blue_background.bmp");
	background->tileValue.collisionEnabled = true;
	background->tileValue.tileTexture = e_tile_texture::blueBackground;
	background++;

	background->blueTile = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "BMP/blue_brick_wall.bmp");
	background->tileValue.collisionEnabled = false;
	background->tileValue.tileTexture = e_tile_texture::blueBrick;

	gameState->currSelectedTileIndex = 1;

	
	player_bitmap* player;
	player = gameState->playerBitmaps;
	player->bitmap = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "BMP/player_face_forward.bmp");
	player->alignX = 15;
	player->alignY = 23;

	
	
	AddEntity(gameState);
	
	gameState->cameraP.absTileX = 33/2;
	gameState->cameraP.absTileY = 9;
	
	InitializeArena(&gameState->worldArena, memory->permanentStorageSize - sizeof(game_state),
			(u8*)memory->permanentStorage + sizeof(game_state));


	gameState->playerAnimations[0].bitmap = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "BMP/player_idle.bmp");
	gameState->playerAnimations[0].alignX = 15;
	gameState->playerAnimations[0].alignY = 23;
	
	gameState->playerAnimations[1].bitmap = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "BMP/player_idle.bmp");
	gameState->playerAnimations[1].alignX = 15;
	gameState->playerAnimations[1].alignY = 23;
	
	gameState->playerAnimations[2].bitmap = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "BMP/player_launch_01.bmp");
	gameState->playerAnimations[2].alignX = 15;
	gameState->playerAnimations[2].alignY = 18;
	
	gameState->playerAnimations[3].bitmap = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "BMP/player_launch_02.bmp");
	gameState->playerAnimations[3].alignX = 15;
	gameState->playerAnimations[3].alignY = 17;
	
	gameState->playerAnimations[4].bitmap = DEBUGLoadBMP(thread, memory->DEBUGPlatformReadEntireFile, "BMP/player_in_air.bmp");
	gameState->playerAnimations[4].alignX = 15;
	gameState->playerAnimations[4].alignY = 23;		

	gameState->currentPlayerBitmap = &gameState->playerAnimations[0];

	gameState->world = PushStruct(&gameState->worldArena, world);
	world* world = gameState->world;
	world->tileMap = PushStruct(&gameState->worldArena, tile_map);

	tile_map* tileMap = world->tileMap;

	tileMap->chunkShift = 4;
	tileMap->chunkMask = (1 << tileMap->chunkShift) - 1;
	tileMap->chunkDim = (1 << tileMap->chunkShift);

	tileMap->tileChunkCountX = 128;
	tileMap->tileChunkCountY = 128;
	tileMap->tileChunkCountZ = 2;

	tileMap->tileChunks = PushArray(&gameState->worldArena,
					tileMap->tileChunkCountX * tileMap->tileChunkCountY *
					tileMap->tileChunkCountZ,
					tile_chunk);

	tileMap->tileSideInMeters = 1.4f;
	

	// the number of tiles per chunk
	u32 tilesPerWidth = 33;
	u32 tilesPerHeight = 9;

	u32 screenX = 0;
	u32 screenY = 0;
	u32 absTileZ = 0;

//initializing our tilemap
	//TODO: Make this all read from a map file you create

	//Open the file
	//Write tile info
	//Read from the file
#if 0

	//100 chunks of 9x33 tiles

	debug_open_file_result openedFile = memory->DEBUGPlatformOpenFile("tilemap_test.map");
	bool32 isA = true;
	for (u32 screenIndex = 0; screenIndex < 100; ++screenIndex)
	{

	    for (u32 tileY = 0; tileY < tilesPerHeight; ++tileY)
	    {
		for (u32 tileX = 0; tileX < tilesPerWidth; ++tileX)
		{
		    tile_value tileValue = {};
		    tileValue.collisionEnabled = false;
		    tileValue.tileTexture = e_tile_texture::blueBackground;
		    u32 absTileX = screenX * tilesPerWidth + tileX;
		    u32 absTileY = screenY * tilesPerHeight + tileY;


		    if (screenIndex == 0)
		    {
			if (tileY <= 1)
			{
			    tileValue.tileTexture = e_tile_texture::blueBrick;
			    tileValue.collisionEnabled = true;
			}
		    }
		    if ((tileX <= 1) || (tileX == tilesPerWidth - 1) || tileX == tilesPerWidth -2)
		    {
			tileValue.tileTexture = e_tile_texture::blueBrick;
			tileValue.collisionEnabled = true;
		    }

		    SetTileValue(&gameState->worldArena, world->tileMap, absTileX, absTileY, absTileZ, tileValue);
 
		    if (openedFile.fileOpened)
		    {
			memory->DEBUGPlatformWriteToFile(&openedFile, &tileValue, sizeof(tileValue));
		    }

		}
	    }
	    //set it so we only have chunks going up atm, just to test things out
	    screenY++;
	}

	if (openedFile.fileOpened)
	{
	    memory->DEBUGPlatformCloseFile(&openedFile);
	}
#endif		

#if 1 	

	debug_read_file_result result = memory->DEBUGPlatformReadEntireFile(thread, "tilemap_test.map");
	tile_value* tileValue = (tile_value*)result.contents;
	for (u32 screenIndex = 0; screenIndex < 100; ++screenIndex)
	{
	    for (u32 tileY = 0; tileY < tilesPerHeight; ++tileY)
	    {
		for (u32 tileX = 0; tileX < tilesPerWidth; ++tileX)
		{
		    u32 absTileX = screenX * tilesPerWidth + tileX;
		    u32 absTileY = screenY * tilesPerHeight + tileY;

		    SetTileValue(&gameState->worldArena, world->tileMap, absTileX, absTileY, absTileZ, *tileValue);

		    tileValue++;

		}
	    }
	    screenY++;
	}
#endif
	gameState->cameraChunkY = 18;
	gameState->prevCameraChunkY = 18;
	gameState->cameraFollowingEntity = true;
	memory->isInitialized = true;
    }


    world* world = gameState->world;
    tile_map* tileMap = world->tileMap;
    

    i32 tileSideInPixels = 30;
    r32 metersToPixels = tileSideInPixels / tileMap->tileSideInMeters;
    tileMap->metersToPixels = metersToPixels;
    
    
    for (int controllerIndex = 0; controllerIndex < ArrayCount(input->controllers); ++controllerIndex)
    {
	game_controller_input* controller = GetController(input, controllerIndex);

	entity* controllingEntity = GetEntity(gameState, gameState->playerIndexForController[controllerIndex]);


	if (controllingEntity)
	{
	    bool32 jumpAnimDetected = false;
	    bool32 moveAnimDetected = false;	    
	    v2 ddP = {};
	    if (controller->isAnalog)
	    {

	    }
	    else
	    {
	    
		r32 dPlayerX = 0.0f;
		r32 dPlayerY = 0.0f;

		if (controller->scrollUp.endedDown && (controller->scrollUp.started == false))
		{
		    gameState->cameraFollowingEntity = false;
		    gameState->cameraP.absTileY += 18;
		    gameState->cameraChunkY += 18;
		    controller->scrollUp.started = true;
		}

		if (controller->scrollDown.endedDown && (controller->scrollDown.started == false))
		{
		    if (gameState->cameraP.absTileY > 18)
		    {
			gameState->cameraFollowingEntity = false;
			gameState->cameraP.absTileY -= 18;
			gameState->cameraChunkY -= 18;

		    }
		    controller->scrollDown.started = true;		    
		}

		if (controller->save.endedDown)
		{
		    u32 screenYVal = 0;
		    u32 screenXVal = 0;
		    u32 tilesPerWidth = 33;
		    u32 tilesPerHeight = 9;
		    debug_open_file_result unsavedMapFile = memory->DEBUGPlatformOpenFile("tilemap_test.map");
		    for (u32 screenIndex = 0; screenIndex < 100; ++screenIndex)
		    {
			for (u32 tileY = 0; tileY < tilesPerHeight; ++tileY)
			{
			    for (u32 tileX = 0; tileX < tilesPerWidth; ++tileX)
			    {
				u32 absTileX = screenXVal * tilesPerWidth + tileX;
				u32 absTileY = screenYVal * tilesPerHeight + tileY;

				tile_value tileValue = GetTileValue(tileMap, absTileX, absTileY, 0);
				if (unsavedMapFile.fileOpened)
				{
				    memory->DEBUGPlatformWriteToFile(&unsavedMapFile, &tileValue, sizeof(tileValue));
				}
			    }
			}
			screenYVal++;			
		    }
		    if (unsavedMapFile.fileOpened)
		    {
			memory->DEBUGPlatformCloseFile(&unsavedMapFile);
		    }
		}

		if (input->mouseButtons[0].endedDown)
		{
		    tile_value tile = {};
		    tile.collisionEnabled = true;
		    tile.tileTexture = gameState->backgroundBitmaps[gameState->currSelectedTileIndex].tileValue.tileTexture;
		    SetTileValueFromMouse(input, buffer, tileMap, gameState, tile);
		}

		if (input->mouseButtons[1].endedDown)
		{
		    tile_value tile = {};
		    tile.collisionEnabled = false;
		    tile.tileTexture = gameState->backgroundBitmaps[gameState->currSelectedTileIndex].tileValue.tileTexture;

		    SetTileValueFromMouse(input, buffer, tileMap, gameState, tile);
		}

		if (controller->actionLeft.endedDown)
		{
		    gameState->currSelectedTileIndex++;
		    if (gameState->currSelectedTileIndex > ArrayCount(gameState->backgroundBitmaps) - 1) gameState->currSelectedTileIndex = 0;
		    
		}

		if (controller->actionRight.endedDown)
		{
		    gameState->currSelectedTileIndex--;
		    if (gameState->currSelectedTileIndex < 0) gameState->currSelectedTileIndex = 0;
			
		}
		bool32 movementDetected = false;
		bool32 jumpInputDetected = false;
		if (!IsEntityInAir(controllingEntity))
		{
		    if (controller->moveLeft.endedDown)
		    {
			ddP.x = -1.0f;
			movementDetected = true;
			moveAnimDetected = true;
			gameState->cameraFollowingEntity = true;
		    }
		    if (controller->moveRight.endedDown)
		    {
			ddP.x = 1.0f;
			movementDetected = true;
			moveAnimDetected = true;			
			gameState->cameraFollowingEntity = true;			
		    }
		}

		bool32 jumpButtonDetected = false;
		if (controller->moveDown.endedDown)
		{
		    //Where we implement the ability to jump
		    if (controller->actionDown.endedDown)
		    {
			if (controllingEntity->dP.y == 0.0f)
			{
			    if (controllingEntity->canJump)
			    {
				r32 dtMult = input->dTime * 40.0f;
				if (ddP.x != 0)
				{
				    r32 xVel = 10.0f;
				    if (ddP.x > 0.0f)
				    {
					controllingEntity->dP.x +=
					    ((r32)controllingEntity->framesHeld * dtMult);
				    }
				    else
				    {
					controllingEntity->dP.x -=
					    ((r32)controllingEntity->framesHeld * dtMult);
				    }
				}

				controllingEntity->dP.y += ((r32)controllingEntity->framesHeld * dtMult + 30.0f);
				controllingEntity->canJump = false;

			    }
			}
			jumpAnimDetected = false;
			moveAnimDetected = true;
		    }
		    else
		    {
			controllingEntity->canJump = true;
			jumpAnimDetected = true;			
			ddP.x = 0.0f;			
		    }
		    if (controllingEntity->framesHeld == 100)
		    {
			//we can jump now
			controllingEntity->framesHeld = 0;
			
		    }
		    else if (controllingEntity->framesHeld == 5)
		    {
			gameState->currentPlayerBitmap = &gameState->playerAnimations[2];
		    }
		    else if (controllingEntity->framesHeld == 60)
		    {
			gameState->currentPlayerBitmap = &gameState->playerAnimations[3];
		    }
		    controllingEntity->framesHeld++;

		}
		else
		{
		    controllingEntity->framesHeld = 0;
		}

		MovePlayer(gameState, controllingEntity, input->dTime, ddP);
	    }
	    

	    if ((!jumpAnimDetected) && (moveAnimDetected))
	    {
		gameState->currentPlayerBitmap = &gameState->playerAnimations[0];
	    }
	    
	}
	else
	{
	    if (controller->start.endedDown)
	    {
		u32 entityIndex = AddEntity(gameState);
		InitializePlayer(gameState, entityIndex);
		gameState->playerIndexForController[controllerIndex] = entityIndex;
	    }
	}
	
    }

    r32 upperLeftX = -30;
    r32 upperLeftY = 0;
    r32 tileWidth = 60;
    r32 tileHeight = 60;



    entity* cameraFollowingEntity = GetEntity(gameState, gameState->cameraFollowingEntityIndex);
    if (cameraFollowingEntity && gameState->cameraFollowingEntity)
    {
	if ((cameraFollowingEntity->p.absTileY > gameState->cameraChunkY))
	{
	    gameState->cameraP.absTileY += 18;
	    gameState->prevCameraChunkY = gameState->cameraChunkY;
	    gameState->cameraChunkY += 18;
	}
	else if ((cameraFollowingEntity->p.absTileY < gameState->cameraChunkY) && (cameraFollowingEntity->p.absTileY <= gameState->prevCameraChunkY))
	{
	    gameState->cameraP.absTileY -= 18;
	    gameState->prevCameraChunkY = gameState->cameraChunkY;	    
	    gameState->cameraChunkY -= 18;
	}
    }
    
    r32 screenCenterX = 0.5f *(r32)buffer->width;
    r32 screenCenterY = 0.5f * (r32)buffer->height;
    for (i32 relRow =  -10; relRow < 10; ++relRow)
    {
	for (i32 relColumn = -20; relColumn < 20; ++relColumn)
	{
	    u32 column = gameState->cameraP.absTileX + relColumn;
	    u32 row = gameState->cameraP.absTileY + relRow;
	    tile_value tileId = GetTileValue(tileMap, column, row, gameState->cameraP.absTileZ);

	    v2 tileSide = {0.5f * tileSideInPixels, 0.5f * tileSideInPixels};
	    v2 cen = {screenCenterX - metersToPixels * gameState->cameraP.offset.x +
		((r32)relColumn) * tileSideInPixels,
		screenCenterY + metersToPixels * gameState->cameraP.offset.y -
		((r32)relRow) * tileSideInPixels};
	    v2 min = cen - tileSide;
	    v2 max = cen + tileSide;

#if 0	    
	    if (tileId >= 1)
	    {
		r32 gray = 0.5f;
		if (tileId == 2)
		{
		    gray = 1.0f;
		    v2 tileLoc = {screenCenterX - metersToPixels * gameState->cameraP.offset.x +
			((r32)relColumn) * tileSideInPixels,
			screenCenterY + metersToPixels * gameState->cameraP.offset.y -
			((r32)relRow) * tileSideInPixels};		    
		    background_bitmaps* background = &gameState->backgroundBitmaps[0];
		    DrawBackgroundTile(buffer, &background->blueTile, min, max);
		}
		else
		{
		    if (tileId == 1)
		    {
			gray = 0.5f;
		    }

		    if (tileId > 2)
		    {
			gray = 0.25f;
		    }
	
		    DrawRectangle(buffer, min, max, gray, gray, gray);
		}
	    }
#else

	    v2 tileLoc = {screenCenterX - metersToPixels * gameState->cameraP.offset.x +
		((r32)relColumn) * tileSideInPixels,
		screenCenterY + metersToPixels * gameState->cameraP.offset.y -
		((r32)relRow) * tileSideInPixels};		    
	    background_bitmaps* background = &gameState->backgroundBitmaps[tileId.tileTexture];
	    DrawBackgroundTile(buffer, &background->blueTile, min, max);
#endif	    
	}
    }

    //draw our player
    r32 playerWidth = 0.75f*(r32)tileWidth;
    r32 playerHeight = (r32)tileHeight;

    r32 playerTop = gameState->playerY - playerHeight;
    r32 playerLeft = gameState->playerX - 0.5f * playerWidth;

    entity* entity = gameState->entities;
    for (u32 entityIndex = 0; entityIndex < gameState->entityCount; ++entityIndex, ++entity)
    {
	if (entity->exists)
	{
	    tile_map_difference diff = Subtract(tileMap, &entity->p, &gameState->cameraP);

	    r32 playerR = 1.0f;
	    r32 playerG = 1.0f;
	    r32 playerB = 0.0f;

	    r32 playerGroundPointX = screenCenterX + metersToPixels * diff.dXY.x;
	    r32 playerGroundPointY = screenCenterY - metersToPixels * diff.dXY.y;

	    v2 playerLeftTop = {playerGroundPointX - 0.5f * metersToPixels * entity->width,
		playerGroundPointY - 0.5f * metersToPixels * entity->height};
	    v2 entityWidthHeight = {entity->width, entity->height};
//	    DrawRectangle(buffer, playerLeftTop, playerLeftTop + metersToPixels * entityWidthHeight,
//			  playerR, playerG, playerB);

//	    player_bitmap* player = &gameState->playerBitmaps[0];

	    player_bitmap* player = gameState->currentPlayerBitmap;
	    DrawBitmap(buffer, &player->bitmap, playerGroundPointX, playerGroundPointY, player->alignX, player->alignY);
	}
    }
}
