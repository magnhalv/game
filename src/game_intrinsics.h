#if !defined(GAME_INTRINSICS_H)

internal int32 round_real32_to_int32(real32 real) {
  return (int32)roundf(real);
}

#include "math.h"
internal int32 floor_real32_to_int32(real32 real) {
  if (real < 0) {
    int x = 5;
  }
  int32 result = (int32)floorf(real);
  return result;
}

inline real32 sin(real32 angle) {
  return sinf(angle);
}

inline real32 cos(real32 angle) {
  return cosf(angle);
}

inline real32 a_tan_2(real32 x, real32 y) {
  return atan2f(y, x);
}

// TODO: MOve this into the instrinsics and call the MSVC version

struct bit_scan_result {
  bool32 found;
  uint32 index;
};

internal bit_scan_result find_least_significant_set_bit(uint32 value) {
  bit_scan_result result = {};

#if COMPILER_MSVC
  result.found = _BitScanForward((unsigned long*)&result.index, value);
#else
  for (uint32 test = 0; test < 32; test++) {
    if (value & (1 << test)) {
      result.index = test;
      result.found = true;
      break;
    }
  }
#endif
  return result;
}

#define GAME_INTRINSICS_H
#endif
