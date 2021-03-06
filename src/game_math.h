#if !defined(GAME_MATH_H)

union v2 {
  // Makes it possible to both access as x and y, and as an array.
  struct {
    real32 x, y;
  };
  real32 E[2];
};


inline v2 operator-(v2 a) {
  v2 result;
  result.x = -a.x;
  result.y = -a.y;
  return result;
}

inline v2 operator*(real32 a, v2 b) {
  v2 result;
  result.x = a*b.x;
  result.y = a*b.y;
  return result;
}

inline v2 operator*(v2 a, real32 b) {
  return b*a;
}

inline v2 operator*=(v2 &a, real32 b) {
  a = a * b;
  return a;
}

inline v2 operator+(v2 a, v2 b) {
  v2 result;

  result.x = a.x + b.x;
  result.y = a.y + b.y;
  return result;
}

inline v2 operator+=(v2 &a, v2 b) {
  a = a + b;
  return a;
}


inline v2 operator-(v2 a, v2 b) {
  v2 result;

  result.x = a.x - b.x;
  result.y = a.y - b.y;
  return result;
}

inline real32 square(real32 a) {
  real32 result = a*a;
  return result;
}

inline real32 dot(v2 a, v2 b) {
  real32 result = a.x*b.x + a.y*b.y;
  return result;
}



#define GAME_MATH_H
#endif
