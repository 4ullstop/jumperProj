#if !defined(JUMPER_TILE_H)

struct tile_map_difference
{
    v2 dXY;
    r32 dZ;
};

struct tile_map_position
{
    u32 absTileX;
    u32 absTileY;
    u32 absTileZ;

    v2 offset;
};

struct tile_chunk_position
{
    u32 tileChunkX;
    u32 tileChunkY;
    u32 tileChunkZ;

    u32 relTileX;
    u32 relTileY;
};

enum e_tile_texture : u8
{
    blueBackground,
    blueBrick,
    grayBrick
};

struct tile_value
{
    bool32 collisionEnabled;
    e_tile_texture tileTexture;
};

struct tile_chunk
{
    u32* tiles;
};


#pragma pack(push, 1)
struct tile_map
{
    u32 chunkShift;
    u32 chunkMask;
    u32 chunkDim;

    r32 tileSideInMeters;

    u32 tileChunkCountX;
    u32 tileChunkCountY;
    u32 tileChunkCountZ;

    tile_chunk* tileChunks;

    r32 metersToPixels;

};
#pragma pack(pop)

#define JUMPER_TILE_H
#endif
