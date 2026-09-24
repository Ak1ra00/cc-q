// QUASAR - small math helpers: angle tables, fast rng, float helpers.
#pragma once
#include <stdint.h>

#define PI_F        3.14159265f
#define TAU_F       6.28318531f

// sine for angles in 1/256 of a turn, result scaled by 16384
extern const int16_t g_sin256[256];
static inline int isin256(int a) { return g_sin256[a & 255]; }
static inline int icos256(int a) { return g_sin256[(a + 64) & 255]; }

// float sin/cos on "turns" (1.0 = full circle); table based, good enough for games
float fsin_t(float turns);
float fcos_t(float turns);
float fatan2_t(float y, float x);           // returns turns in [-0.5, 0.5)
float fsqrt(float x);

static inline float fclampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline int iclamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float fabsf_(float v) { return v < 0 ? -v : v; }
static inline int iabs(int v) { return v < 0 ? -v : v; }
static inline int fround(float v) { return (int)(v < 0 ? v - 0.5f : v + 0.5f); }
static inline float flerp(float a, float b, float t) { return a + (b - a) * t; }

int isqrt(uint32_t v);

// xorshift rng
typedef struct { uint32_t s; } rng_t;
extern rng_t g_rng;         // gameplay (deterministic per run)
extern rng_t g_fxrng;       // visual only
uint32_t rng_next(rng_t *r);
static inline int rnd(int n) { return n > 0 ? (int)(rng_next(&g_rng) % (uint32_t)n) : 0; }
static inline int rndfx(int n) { return n > 0 ? (int)(rng_next(&g_fxrng) % (uint32_t)n) : 0; }
float rndf(void);           // [0,1)
float rndfxf(void);         // [0,1) visual rng
static inline float rndr(float lo, float hi) { return lo + (hi - lo) * rndf(); }
static inline float rndfxr(float lo, float hi) { return lo + (hi - lo) * rndfxf(); }

// value noise (tileable over 256), result 0..255
int noise2(int x, int y, int seed);
