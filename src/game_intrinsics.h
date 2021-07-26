#if !defined(GAME_INTRINSICS_H)

internal int32 round_real32_to_int32(real32 real) {
  return (int32)(real + 0.5f);
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

#define GAME_INTRINSICS_H
#endif
