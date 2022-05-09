#pragma once
#include <vector>

#include "tile.h"
#include "../utils/math.h"

#pragma pack(push, 1)
struct WorldTileMap {
    math::Vec2<int> size;
    uint32_t tile_count;
    std::vector<Tile> tiles;

    void serialize(void* buffer, std::size_t position = 0) {
        BinaryReader br{ buffer };
        br.skip(position);

        size = br.read<math::Vec2<int>>();
        tile_count = br.read<uint32_t>();

        /*position = br.position();
        for (uint32_t i = 0; i < tile_count; i++)
            tiles[i].serialize(buffer, position);*/
    }
};
#pragma pack(pop)