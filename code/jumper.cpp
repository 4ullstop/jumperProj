#include "jumper.h"
#include "jumper_tile.cpp"

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

    r32 playerSpeed = 180.0f;
    ddP *= playerSpeed;

    
    ddP += IsEntityInAir(entity) ? -2.0f * entity->dP : -35.0f*entity->dP;
    tile_map_position oldPlayerP = entity->p;

    v2 playerDelta = (0.5f * ddP * Square(dt) + entity->dP*dt);
    entity->dP.y += dt * (-9.81f * tileMap->metersToPixels);

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
		u32 tileValue = GetTileValue(tileMap, testTileP);

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
		    }		    
		}
	    }
	}
	entity->p = Offset(tileMap, entity->p, tMin*playerDelta);
	entity->dP = entity->dP - 1*Inner(entity->dP, wallNormal)*wallNormal;
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

	AddEntity(gameState);
	
	gameState->cameraP.absTileX = 33/2;
	gameState->cameraP.absTileY = 9;
	
	InitializeArena(&gameState->worldArena, memory->permanentStorageSize - sizeof(game_state),
			(u8*)memory->permanentStorage + sizeof(game_state));

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
		    u32 tileValue = 1;
		    u32 absTileX = screenX * tilesPerWidth + tileX;
		    u32 absTileY = screenY * tilesPerHeight + tileY;

		    //TODO: SetTileValue here

		    if (screenIndex == 0)
		    {
			if (tileY <= 1)
			{
			    tileValue = 2;
			}
		    }
		    if ((tileX <= 1) || (tileX == tilesPerWidth - 1) || tileX == tilesPerWidth -2)
		    {
			tileValue = 2;
		    }

		    SetTileValue(&gameState->worldArena, world->tileMap, absTileX, absTileY, absTileZ, tileValue);
 
		    serializedChunks[screenIndex][tileY][tileX] = tileValue;
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
	u32* tileValue = (u32*)result.contents;
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
	    v2 ddP = {};
	    if (controller->isAnalog)
	    {

	    }
	    else
	    {
	    
		r32 dPlayerX = 0.0f;
		r32 dPlayerY = 0.0f;

#if 0
		//basic jump code, that doesn't have any other considerations besides holding a button
		if (controller->actionDown.endedDown)
		{
		    
		    if (!(controllingEntity->dP.y < 0.0f) && !(controllingEntity->dP.y > 0.0f))
		    {
			if (controllingEntity->canJump)
			{
			    controllingEntity->dP.y += 100.0f;
			    controllingEntity->canJump = false;
			}
		    }
		}
		else
		{
		    controllingEntity->canJump = true;
		}
#endif


		if (controller->save.endedDown)
		{
		    
		}

		if (input->mouseButtons[0].endedDown)
		{
		    tile_map_position mousePos = {};
		    mousePos.absTileX = input->mouseX;
		    mousePos.absTileY = input->mouseY;
		    mousePos.absTileZ = 0;
		    //Divide the screen up based on the screen size
		    //the dimension of the tiles in pixels and the number of tiles per screen
		    mousePos.absTileX = mousePos.absTileX / tileSideInPixels;
		    mousePos.absTileY = mousePos.absTileY / tileSideInPixels;
		    u32 tileValue = GetTileValue(tileMap, mousePos);
		    SetTileValue(&gameState->worldArena, tileMap, mousePos.absTileX, mousePos.absTileY, mousePos.absTileZ, 2);
		    
		}
		

		
		bool32 movementDetected = false;
		bool32 jumpInputDetected = false;
		if (controller->moveUp.endedDown)
		{
		    ddP.y = 1.0f;
		    movementDetected = true;
		}
		if (controller->moveLeft.endedDown)
		{
		    ddP.x = -1.0f;
		    movementDetected = true;		    
		}
		if (controller->moveRight.endedDown)
		{
		    ddP.x = 1.0f;
		    movementDetected = true;		    
		}
		if (controller->moveDown.endedDown)
		{
		    //Where we implement the ability to jump
		    if (controller->actionDown.endedDown)
		    {
			if (controllingEntity->dP.y == 0.0f)
			{
			    if (controllingEntity->canJump)
			    {
				r32 dtMult = input->dTime * 60.0f;
				if (ddP.x != 0)
				{
				    r32 xVel = 10.0f;
				    if (ddP.x > 0.0f)
				    {
					controllingEntity->dP.x +=
					    ((r32)controllingEntity->framesHeld * dtMult + 10.0f);
				    }
				    else
				    {
					controllingEntity->dP.x -=
					    ((r32)controllingEntity->framesHeld * dtMult + 10.0f);
				    }
				}

				controllingEntity->dP.y += ((r32)controllingEntity->framesHeld * dtMult);
				controllingEntity->canJump = false;
			    }
			}
		    }
		    else
		    {
			controllingEntity->canJump = true;
			ddP.x = 0.0f;			
		    }
		    //currently the jump is frame rate dependent, meaning the higher the frames,
		    //the higher you can jump, you're gonna wanna fix this at some point but it works for now
		    if (controllingEntity->framesHeld == 100)
		    {
			//we can jump now
			controllingEntity->framesHeld = 0;
			
		    }
		    controllingEntity->framesHeld++;		    
		}
		else
		{
		    controllingEntity->framesHeld = 0;
		}

		MovePlayer(gameState, controllingEntity, input->dTime, ddP);
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

    

    //clear the background
//    DrawRectangle(buffer, 0.0f, 0.0f, (r32)buffer->width, (r32)buffer->height, 1.0f, 0.0f, 1.0f);

    //draw our tilemap
#if 0    
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
#endif

    r32 screenCenterX = 0.5f *(r32)buffer->width;
    r32 screenCenterY = 0.5f * (r32)buffer->height;
    for (i32 relRow =  -10; relRow < 10; ++relRow)
    {
	for (i32 relColumn = -20; relColumn < 20; ++relColumn)
	{
	    u32 column = gameState->cameraP.absTileX + relColumn;
	    u32 row = gameState->cameraP.absTileY + relRow;
	    u32 tileId = GetTileValue(tileMap, column, row, gameState->cameraP.absTileZ);

	    if (tileId >= 1)
	    {
		r32 gray = 0.5f;
		if (tileId == 2)
		{
		    gray = 1.0f;
		}
		if (tileId == 1)
		{
		    gray = 0.5f;
		}

		if (tileId > 2)
		{
		    gray = 0.25f;
		}

		v2 tileSide = {0.5f * tileSideInPixels, 0.5f * tileSideInPixels};
		v2 cen = {screenCenterX - metersToPixels * gameState->cameraP.offset.x +
		    ((r32)relColumn) * tileSideInPixels,
		    screenCenterY + metersToPixels * gameState->cameraP.offset.y -
		    ((r32)relRow) * tileSideInPixels};
		v2 min = cen - tileSide;
		v2 max = cen + tileSide;

		DrawRectangle(buffer, min, max, gray, gray, gray);
	    }
	}
    }

    //The issue with the camera centering stuff is here in the camera following code, will need to figure this out
#if 0     
    entity* cameraFollowingEntity = GetEntity(gameState, gameState->cameraFollowingEntityIndex);
    if (cameraFollowingEntity)
    {
	gameState->cameraP.absTileZ = cameraFollowingEntity->p.absTileZ;

	u32 heightFromPlayerHead = 10;
	tile_map_difference diff = Subtract(tileMap, &cameraFollowingEntity->p, &gameState->cameraP);

	if (diff.dXY.x > (9.0f*tileMap->tileSideInMeters))
	{
	    gameState->cameraP.absTileX += 17;
	}
	if (diff.dXY.x < -(9.0f*tileMap->tileSideInMeters))
	{
	    gameState->cameraP.absTileX -= 17;
	}

	if (diff.dXY.y > (5.0f*tileMap->tileSideInMeters))
	{
	    gameState->cameraP.absTileY += heightFromPlayerHead;
	}
	if (diff.dXY.y < -(5.0f*tileMap->tileSideInMeters))
	{
	    gameState->cameraP.absTileY -= heightFromPlayerHead;
	}
    }
#endif	    
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
	    DrawRectangle(buffer, playerLeftTop, playerLeftTop + metersToPixels * entityWidthHeight,
			  playerR, playerG, playerB);
	}
    }

}
