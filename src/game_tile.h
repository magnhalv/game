#if !defined(GAME_TILE_H)

#include "game_math.h"

#define WALKABLE 1
#define WALL 2
#define STAIRS_DOWN 3
#define STAIRS_UP 4

struct tile_map_difference {
  v2 dxy;
  real32 dz;
};

struct tile_map_position {

  uint32 abs_tile_x;
  uint32 abs_tile_y;
  uint32 abs_tile_z;

  // NOTE: Tile offsets from center
  v2 offset;
};

struct tile_chunk {
  uint32 *tiles;
};

struct tile_chunk_position {
  uint32 tile_chunk_x;
  uint32 tile_chunk_y;
  uint32 tile_chunk_z;

  uint32 rel_tile_x;
  uint32 rel_tile_y;
};

struct tile_map {
  uint32 chunk_shift;
  uint32 chunk_mask;
  uint32 chunk_dim;

  real32 tile_side_in_meters;

  uint32 tile_chunk_count_x;
  uint32 tile_chunk_count_y;
  uint32 tile_chunk_count_z;
  tile_chunk *tile_chunks;
};

#define GAME_TILE_H
#endif
