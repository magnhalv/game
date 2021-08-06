#if !defined(GAME_TILE_H)

#define WALKABLE 1
#define WALL 2

struct tile_map_position {

  uint32 abs_tile_x;
  uint32 abs_tile_y;

  // NOTE: tile relative x,y
  real32 tile_rel_x;
  real32 tile_rel_y;
};

struct tile_chunk {
  uint32 *tiles;
};

struct tile_chunk_position {
  uint32 tile_chunk_x;
  uint32 tile_chunk_y;

  uint32 rel_tile_x;
  uint32 rel_tile_y;
};

struct tile_map {
  uint32 chunk_shift;
  uint32 chunk_mask;
  uint32 chunk_dim;

  real32 tile_side_in_meters;
  int32 tile_side_in_pixels;
  real32 meters_to_pixels;

  uint32 tile_chunk_count_x;
  uint32 tile_chunk_count_y;
  tile_chunk *tile_chunks;
};

#define GAME_TILE_H
#endif
