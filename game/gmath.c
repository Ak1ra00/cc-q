// QUASAR - math helpers.
#include "gmath.h"

const int16_t g_sin256[256] = {
         0,    402,    804,   1205,   1606,   2006,   2404,   2801,   3196,   3590,   3981,   4370,   4756,   5139,   5520,   5897,
      6270,   6639,   7005,   7366,   7723,   8076,   8423,   8765,   9102,   9434,   9760,  10080,  10394,  10702,  11003,  11297,
     11585,  11866,  12140,  12406,  12665,  12916,  13160,  13395,  13623,  13842,  14053,  14256,  14449,  14635,  14811,  14978,
     15137,  15286,  15426,  15557,  15679,  15791,  15893,  15986,  16069,  16143,  16207,  16261,  16305,  16340,  16364,  16379,
     16384,  16379,  16364,  16340,  16305,  16261,  16207,  16143,  16069,  15986,  15893,  15791,  15679,  15557,  15426,  15286,
     15137,  14978,  14811,  14635,  14449,  14256,  14053,  13842,  13623,  13395,  13160,  12916,  12665,  12406,  12140,  11866,
     11585,  11297,  11003,  10702,  10394,  10080,   9760,   9434,   9102,   8765,   8423,   8076,   7723,   7366,   7005,   6639,
      6270,   5897,   5520,   5139,   4756,   4370,   3981,   3590,   3196,   2801,   2404,   2006,   1606,   1205,    804,    402,
         0,   -402,   -804,  -1205,  -1606,  -2006,  -2404,  -2801,  -3196,  -3590,  -3981,  -4370,  -4756,  -5139,  -5520,  -5897,
     -6270,  -6639,  -7005,  -7366,  -7723,  -8076,  -8423,  -8765,  -9102,  -9434,  -9760, -10080, -10394, -10702, -11003, -11297,
    -11585, -11866, -12140, -12406, -12665, -12916, -13160, -13395, -13623, -13842, -14053, -14256, -14449, -14635, -14811, -14978,
    -15137, -15286, -15426, -15557, -15679, -15791, -15893, -15986, -16069, -16143, -16207, -16261, -16305, -16340, -16364, -16379,
    -16384, -16379, -16364, -16340, -16305, -16261, -16207, -16143, -16069, -15986, -15893, -15791, -15679, -15557, -15426, -15286,
    -15137, -14978, -14811, -14635, -14449, -14256, -14053, -13842, -13623, -13395, -13160, -12916, -12665, -12406, -12140, -11866,
    -11585, -11297, -11003, -10702, -10394, -10080,  -9760,  -9434,  -9102,  -8765,  -8423,  -8076,  -7723,  -7366,  -7005,  -6639,
     -6270,  -5897,  -5520,  -5139,  -4756,  -4370,  -3981,  -3590,  -3196,  -2801,  -2404,  -2006,  -1606,  -1205,   -804,   -402,
};

rng_t g_rng = { 0x1234567u };
rng_t g_fxrng = { 0x89abcdefu };

uint32_t rng_next(rng_t *r)
{
    uint32_t x = r->s;
    if(!x) x = 0x6d2b79f5u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->s = x;
    return x;
}

float rndf(void)
{
    return (float)(rng_next(&g_rng) >> 8) * (1.0f / 16777216.0f);
}

float rndfxf(void)
{
    return (float)(rng_next(&g_fxrng) >> 8) * (1.0f / 16777216.0f);
}

float fsin_t(float turns)
{
    // linear interpolation in the 256 entry table
    float p = turns * 256.0f;
    int i = (int)p;
    if(p < 0 && (float)i != p) i--;
    float f = p - (float)i;
    int a = g_sin256[i & 255];
    int b = g_sin256[(i + 1) & 255];
    return ((float)a + (float)(b - a) * f) * (1.0f / 16384.0f);
}

float fcos_t(float turns)
{
    return fsin_t(turns + 0.25f);
}

float fatan2_t(float y, float x)
{
    // polynomial approximation, max error ~0.001 turns
    float ax = fabsf_(x), ay = fabsf_(y);
    if(ax < 1e-9f && ay < 1e-9f) return 0;
    float mn = ax < ay ? ax : ay;
    float mx = ax < ay ? ay : ax;
    float a = mn / mx;
    float s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if(ay > ax) r = 1.57079637f - r;
    if(x < 0) r = 3.14159274f - r;
    if(y < 0) r = -r;
    return r * (1.0f / TAU_F);
}

float fsqrt(float x)
{
    if(x <= 0) return 0;
#if defined(__ARM_FP)
    float r;
    __asm__("vsqrt.f32 %0, %1" : "=t"(r) : "t"(x));
    return r;
#else
    return __builtin_sqrtf(x);
#endif
}

int isqrt(uint32_t v)
{
    uint32_t r = 0, bit = 1u << 30;
    while(bit > v) bit >>= 2;
    while(bit) {
        if(v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return (int)r;
}

static inline uint32_t hash3(uint32_t x, uint32_t y, uint32_t s)
{
    uint32_t h = x * 0x8da6b343u ^ y * 0xd8163841u ^ s * 0xcb1ab31fu;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}

int noise2(int x, int y, int seed)
{
    // bilinear value noise on a 16px lattice, wraps every 256 px
    int xi = (x >> 4) & 15, yi = (y >> 4) & 15;
    int xf = x & 15, yf = y & 15;
    int x1 = (xi + 1) & 15, y1 = (yi + 1) & 15;
    int a = (int)(hash3((uint32_t)xi, (uint32_t)yi, (uint32_t)seed) & 255);
    int b = (int)(hash3((uint32_t)x1, (uint32_t)yi, (uint32_t)seed) & 255);
    int c = (int)(hash3((uint32_t)xi, (uint32_t)y1, (uint32_t)seed) & 255);
    int d = (int)(hash3((uint32_t)x1, (uint32_t)y1, (uint32_t)seed) & 255);
    // smoothstep weights, t = k/16, scaled by 256
    static const uint8_t ss[16] = { 0, 3, 11, 24, 40, 59, 81, 104, 128, 152, 175, 197, 216, 232, 245, 253 };
    int wx = ss[xf], wy = ss[yf];
    int top = a + (((b - a) * wx) >> 8);
    int bot = c + (((d - c) * wx) >> 8);
    int v = top + (((bot - top) * wy) >> 8);
    return v < 0 ? 0 : (v > 255 ? 255 : v);
}
