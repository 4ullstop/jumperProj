#include "jumper_tile.h"

inline void
SetTileValueUnchecked(tile_map* tileMap, tile_chunk* tileChunk, u32 tileX, u32 tileY, u32 tileValue)
{
    //Directly set the value of the tile specified here
    Assert(tileChunk);
    Assert(tileX < tileMap->chunkDim);
    Assert(tileY < tileMap->chunkDim);

    tileChunk->tiles[tileY * tileMap->chunkDim + tileX] = tileValue;
}

inline u32
SetTileValue(tile_map* tileMap, tile_chunk* tileChunk,
	     u32 testTileX, u32 testTileY,
	     u32 tileValue)
{
    u32 tileChunkValue = 0;

    if (tileChunk && tileChunk->tiles)
    {
	SetTileValueUnchecked(tileMap, tileChunk, testTileX, testTileY, tileValue);
    }
    return(tileChunkValue);
}

inline tile_chunk_position
GetChunkPositionFor(tile_map* tileMap, u32 absTileX, u32 absTileY, u32 absTileZ)
{
    tile_chunk_position result;
    //remember that we packed all of the information about the chunks
    //inside of one variable. This means the location of the chunks according to the world
    //are set in the higher bits and the position of the tile in the chunk is set in the lower bit
    result.tileChunkX = absTileX >> tileMap->chunkShift;
    result.tileChunkY = absTileY >> tileMap->chunkShift;
    result.tileChunkZ = absTileZ = absTileZ;
    result.relTileX = absTileX & tileMap->chunkMask;
    result.relTileY = absTileY & tileMap->chunkMask;
    return(result);
}

inline tile_chunk*
GetTileChunk(tile_map* tileMap, u32 tileChunkX, u32 tileChunkY, u32 tileChunkZ)
{
    tile_chunk* tileChunk  = 0;
    if ((tileChunkX >= 0) && (tileChunkX < tileMap->tileChunkCountX) &&
	(tileChunkY >= 0) && (tileChunkY < tileMap->tileChunkCountY) &&
	(tileChunkZ >= 0) && (tileChunkZ < tileMap->tileChunkCountZ))
    {
	//remeber the equation in this function is the getter for the rows and columns
	//this will be useful later when you setup file storage for these chunks
	tileChunk = &tileMap->tileChunks[
	    tileChunkZ * tileMap->tileChunkCountY * tileMap->tileChunkCountX +
	    tileChunkY * tileMap->tileChunkCountX +
	    tileChunkX];
    }
    return(tileChunk);
}

inline u32
GetTileChunkValueUnchecked(tile_map* tileMap, tile_chunk* tileChunk, u32 tileX, u32 tileY)
{
    Assert(tileChunk);
    Assert(tileX < tileMap->chunkDim);
    Assert(tileY < tileMap->chunkDim);
    
    u32 result = tileChunk->tiles[tileY * tileMap->chunkDim + tileX];
    return(result);
}
				  
internal u32
GetTileValue(tile_map* tileMap, tile_chunk* tileChunk, u32 testTileX, u32 testTileY)
{
    u32 tileChunkValue = 0;

    if (tileChunk && tileChunk->tiles)
    {
	tileChunkValue = GetTileChunkValueUnchecked(tileMap, tileChunk, testTileX, testTileY);
    }

    return(tileChunkValue);
}

internal u32
GetTileValue(tile_map* tileMap, u32 absTileX, u32 absTileY, u32 absTileZ)
{
    tile_chunk_position chunkPos = GetChunkPositionFor(tileMap, absTileX, absTileY, absTileZ);
    tile_chunk* tileChunk = GetTileChunk(tileMap, chunkPos.tileChunkX, chunkPos.tileChunkY, chunkPos.tileChunkZ);
    u32 tileChunkValue = GetTileValue(tileMap, tileChunk, chunkPos.relTileX, chunkPos.relTileY);

    return(tileChunkValue);
}

internal u32
GetTileValue(tile_map* tileMap, tile_map_position pos)
{
    u32 tileChunkValue = GetTileValue(tileMap, pos.absTileX, pos.absTileY, pos.absTileZ);
    return(tileChunkValue);
}

internal void
SetTileValue(memory_arena* arena, tile_map* tileMap,
		u32 absTileX, u32 absTileY, u32 absTileZ,
		u32 tileValue)
{
    //first we get the position of the chunk in the world
    tile_chunk_position chunkPos = GetChunkPositionFor(tileMap, absTileX, absTileY, absTileZ);
    tile_chunk* tileChunk = GetTileChunk(tileMap, chunkPos.tileChunkX, chunkPos.tileChunkY, chunkPos.tileChunkZ);

    Assert(tileChunk);

    //create the chunk if we can't find that it already exists
    if (!tileChunk->tiles)
    {
	u32 tileCount = tileMap->chunkDim * tileMap->chunkDim;
	tileChunk->tiles = PushArray(arena, tileCount, u32);
	for (u32 tileIndex = 0; tileIndex < tileCount; ++tileIndex)
	{
	    tileChunk->tiles[tileIndex] = 1;
	}
    }
    SetTileValue(tileMap, tileChunk, chunkPos.relTileX, chunkPos.relTileY, tileValue);
}
