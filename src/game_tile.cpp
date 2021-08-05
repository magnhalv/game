inline void recanonicalize_coord(tile_map *tile_map, uint32 *tile, real32 *tile_rel) {
  int32 offset = round_real32_to_int32(*tile_rel / tile_map->tile_side_in_meters);
  *tile += offset;
  *tile_rel -= offset * tile_map->tile_side_in_meters;

  Assert(*tile_rel >= -0.5f*tile_map->tile_side_in_meters);
  Assert(*tile_rel <= 0.5f*tile_map->tile_side_in_meters);
}

inline tile_map_position recanonicalize_position(tile_map *tile_map, tile_map_position pos) {
  tile_map_position result = pos;
  recanonicalize_coord(tile_map, &result.abs_tile_x, &result.tile_rel_x);
  recanonicalize_coord(tile_map, &result.abs_tile_y, &result.tile_rel_y);
  return result;
}

inline tile_chunk* get_tile_chunk(tile_map *tile_map, uint32 x, uint32 y) {
  if ((x < tile_map->tile_chunk_count_x) && (y < tile_map->tile_chunk_count_y)) {
    return &tile_map->tile_chunks[x + y*tile_map->tile_chunk_count_x];
  }
  return NULL;
}

internal uint32 get_tile_value_unchecked(tile_map *tile_map, tile_chunk *tile_chunk, uint32 tile_x, uint32 tile_y) {
  Assert(tile_chunk);
  Assert(tile_x < tile_map->chunk_dim);
  Assert(tile_y < tile_map->chunk_dim);
  uint32 index = tile_x + tile_map->chunk_dim*tile_y;
  return tile_chunk->tiles[index];
}

internal void set_tile_value_unchecked(tile_map *tile_map, tile_chunk *tile_chunk, uint32 tile_x, uint32 tile_y, uint32 tile_value) {
  Assert(tile_chunk);
  Assert(tile_x < tile_map->chunk_dim);
  Assert(tile_y < tile_map->chunk_dim);
  uint32 index = tile_x + tile_map->chunk_dim*tile_y;
  tile_chunk->tiles[index] = tile_value;
}

internal uint32 get_tile_value(tile_map *tile_map, tile_chunk *tile_chunk, uint32 test_tile_x, uint32 test_tile_y) {
  uint32 tile_chunk_value = 0;
  if (tile_chunk) {
    tile_chunk_value = get_tile_value_unchecked(tile_map, tile_chunk, test_tile_x, test_tile_y);
  }
  return tile_chunk_value;
}

internal void set_tile_value(tile_map *tile_map, tile_chunk *tile_chunk, uint32 tile_x, uint32 tile_y, uint32 tile_value) {
  uint32 tile_chunk_value = 0;
  if (tile_chunk) {
    set_tile_value_unchecked(tile_map, tile_chunk, tile_x, tile_y, tile_value);
  }
}

inline tile_chunk_position get_chunk_position(tile_map *tile_map, uint32 abs_tile_x, uint32 abs_tile_y) {
  tile_chunk_position result;

  result.tile_chunk_x = abs_tile_x >> tile_map->chunk_shift;
  result.tile_chunk_y = abs_tile_y >> tile_map->chunk_shift;

  result.rel_tile_x = abs_tile_x & tile_map->chunk_mask;
  result.rel_tile_y = abs_tile_y & tile_map->chunk_mask;

  return result;
}

internal uint32 get_tile_value(tile_map *tile_map, uint32 abs_tile_x, uint32 abs_tile_y) {
  tile_chunk_position chunk_pos = get_chunk_position(tile_map, abs_tile_x, abs_tile_y);
  tile_chunk *tile_chunk = get_tile_chunk(tile_map, chunk_pos.tile_chunk_x, chunk_pos.tile_chunk_y);
  uint32 tile_value = get_tile_value(tile_map, tile_chunk, chunk_pos.rel_tile_x, chunk_pos.rel_tile_y);
  return tile_value;
}


internal bool32 is_tile_map_point_empty(tile_map *tile_map, tile_map_position pos) {
  uint32 tile_chunk_value = get_tile_value(tile_map, pos.abs_tile_x, pos.abs_tile_y);
  bool32 is_empty = (tile_chunk_value == 0);
  return is_empty;
}

internal void set_tile_value(memory_arena *arena,
                             tile_map *tile_map,
                             uint32 abs_tile_x,
                             uint32 abs_tile_y,
                             uint32 tile_value) {
  tile_chunk_position chunk_pos = get_chunk_position(tile_map, abs_tile_x, abs_tile_y);
  tile_chunk *tile_chunk = get_tile_chunk(tile_map, chunk_pos.tile_chunk_x, chunk_pos.tile_chunk_y);
  // TODO: On-demand tile chunk creation
  Assert(tile_chunk);

  set_tile_value(tile_map, tile_chunk, chunk_pos.rel_tile_x, chunk_pos.rel_tile_y, tile_value);
}
