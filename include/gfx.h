// gfx.h - 1-bit framebuffer helpers for a 72x40 SSD1306 (page layout)
#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>

#define SCR_W 72
#define SCR_H 40
#define FB_SIZE (SCR_W * SCR_H / 8)

#ifndef PI_F
#define PI_F 3.14159265f
#endif

static uint8_t fb[FB_SIZE];
static int8_t SIN8[256];          // -127..127
static uint8_t ISQRT[SCR_W * SCR_W + SCR_H * SCR_H + 1];

static const uint8_t BAYER4[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

static inline void cls() { memset(fb, 0, FB_SIZE); }
static inline void pset(int x, int y) {
  if ((unsigned)x < SCR_W && (unsigned)y < SCR_H) fb[(y >> 3) * SCR_W + x] |= 1 << (y & 7);
}
static inline void pclr(int x, int y) {
  if ((unsigned)x < SCR_W && (unsigned)y < SCR_H) fb[(y >> 3) * SCR_W + x] &= ~(1 << (y & 7));
}
static inline bool pget(int x, int y) {
  if ((unsigned)x < SCR_W && (unsigned)y < SCR_H) return fb[(y >> 3) * SCR_W + x] >> (y & 7) & 1;
  return false;
}
// level 0..16, ordered dither
static inline void pdith(int x, int y, int lvl) {
  if (lvl > BAYER4[((y & 3) << 2) | (x & 3)]) pset(x, y);
}
static inline void invertAll() { for (int i = 0; i < FB_SIZE; i++) fb[i] ^= 0xFF; }

static void line(int x0, int y0, int x1, int y1, int pattern = 0) {
  // trivial reject
  if ((x0 < 0 && x1 < 0) || (x0 >= SCR_W && x1 >= SCR_W) || (y0 < 0 && y1 < 0) || (y0 >= SCR_H && y1 >= SCR_H)) return;
  if (x0 < -500 || x0 > 500 || x1 < -500 || x1 > 500 || y0 < -500 || y0 > 500 || y1 < -500 || y1 > 500) return;
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
  int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
  int err = dx + dy, n = 0;
  for (;;) {
    if (!pattern || ((n++ & pattern) == 0)) pset(x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}
static void fillRect(int x, int y, int w, int h) {
  for (int j = y; j < y + h; j++)
    for (int i = x; i < x + w; i++) pset(i, j);
}
// horizontal glitch: shift rows [y0,y1) by dx (wraps)
static void shiftRows(int y0, int y1, int dx) {
  uint8_t row[SCR_W];
  for (int y = y0; y < y1; y++) {
    if (y < 0 || y >= SCR_H) continue;
    for (int x = 0; x < SCR_W; x++) row[x] = pget(x, y);
    for (int x = 0; x < SCR_W; x++) {
      int sx = ((x - dx) % SCR_W + SCR_W) % SCR_W;
      if (row[sx]) pset(x, y); else pclr(x, y);
    }
  }
}

// ---------- 5x7 font (columns, LSB = top) ----------
static const char FONT_CHARS[] = " !*+,-./0123456789:<>?ABCDEFGHIJKLMNOPQRSTUVWXYZ#";
static const uint8_t FONT5[][5] = {
  {0x00,0x00,0x00,0x00,0x00}, // space
  {0x00,0x00,0x5F,0x00,0x00}, // !
  {0x14,0x08,0x3E,0x08,0x14}, // *
  {0x08,0x08,0x3E,0x08,0x08}, // +
  {0x00,0x50,0x30,0x00,0x00}, // ,
  {0x08,0x08,0x08,0x08,0x08}, // -
  {0x00,0x60,0x60,0x00,0x00}, // .
  {0x20,0x10,0x08,0x04,0x02}, // /
  {0x3E,0x51,0x49,0x45,0x3E}, // 0
  {0x00,0x42,0x7F,0x40,0x00}, // 1
  {0x42,0x61,0x51,0x49,0x46}, // 2
  {0x21,0x41,0x45,0x4B,0x31}, // 3
  {0x18,0x14,0x12,0x7F,0x10}, // 4
  {0x27,0x45,0x45,0x45,0x39}, // 5
  {0x3C,0x4A,0x49,0x49,0x30}, // 6
  {0x01,0x71,0x09,0x05,0x03}, // 7
  {0x36,0x49,0x49,0x49,0x36}, // 8
  {0x06,0x49,0x49,0x29,0x1E}, // 9
  {0x00,0x36,0x36,0x00,0x00}, // :
  {0x08,0x14,0x22,0x41,0x00}, // <
  {0x00,0x41,0x22,0x14,0x08}, // >
  {0x02,0x01,0x51,0x09,0x06}, // ?
  {0x7E,0x11,0x11,0x11,0x7E}, // A
  {0x7F,0x49,0x49,0x49,0x36}, // B
  {0x3E,0x41,0x41,0x41,0x22}, // C
  {0x7F,0x41,0x41,0x22,0x1C}, // D
  {0x7F,0x49,0x49,0x49,0x41}, // E
  {0x7F,0x09,0x09,0x09,0x01}, // F
  {0x3E,0x41,0x49,0x49,0x7A}, // G
  {0x7F,0x08,0x08,0x08,0x7F}, // H
  {0x00,0x41,0x7F,0x41,0x00}, // I
  {0x20,0x40,0x41,0x3F,0x01}, // J
  {0x7F,0x08,0x14,0x22,0x41}, // K
  {0x7F,0x40,0x40,0x40,0x40}, // L
  {0x7F,0x02,0x0C,0x02,0x7F}, // M
  {0x7F,0x04,0x08,0x10,0x7F}, // N
  {0x3E,0x41,0x41,0x41,0x3E}, // O
  {0x7F,0x09,0x09,0x09,0x06}, // P
  {0x3E,0x41,0x51,0x21,0x5E}, // Q
  {0x7F,0x09,0x19,0x29,0x46}, // R
  {0x46,0x49,0x49,0x49,0x31}, // S
  {0x01,0x01,0x7F,0x01,0x01}, // T
  {0x3F,0x40,0x40,0x40,0x3F}, // U
  {0x1F,0x20,0x40,0x20,0x1F}, // V
  {0x3F,0x40,0x38,0x40,0x3F}, // W
  {0x63,0x14,0x08,0x14,0x63}, // X
  {0x07,0x08,0x70,0x08,0x07}, // Y
  {0x61,0x51,0x49,0x45,0x43}, // Z
  {0x0C,0x1E,0x3C,0x1E,0x0C}, // # = heart
};
static inline const uint8_t* glyph(char c) {
  for (int i = 0; FONT_CHARS[i]; i++) if (FONT_CHARS[i] == c) return FONT5[i];
  return FONT5[0];
}
// draw char at (x,y) top-left, scale sx/sy; dith: 0 = solid, 1 = 50% checker
static void drawChar(int x, int y, char c, int sx = 1, int sy = 1, int dith = 0) {
  const uint8_t* g = glyph(c);
  for (int cx = 0; cx < 5; cx++)
    for (int cy = 0; cy < 7; cy++)
      if (g[cx] >> cy & 1)
        for (int a = 0; a < sx; a++)
          for (int b = 0; b < sy; b++) {
            int px = x + cx * sx + a, py = y + cy * sy + b;
            if (!dith || ((px + py) & 1)) pset(px, py);
          }
}
static void drawText(int x, int y, const char* s, int sx = 1, int sy = 1) {
  for (; *s; s++, x += 6 * sx) drawChar(x, y, *s, sx, sy);
}

// ---------- 3D ----------
struct V3 { float x, y, z; };
static inline V3 rotX(V3 v, float c, float s) { return {v.x, v.y * c - v.z * s, v.y * s + v.z * c}; }
static inline V3 rotY(V3 v, float c, float s) { return {v.x * c + v.z * s, v.y, -v.x * s + v.z * c}; }
static inline V3 rotZ(V3 v, float c, float s) { return {v.x * c - v.y * s, v.x * s + v.y * c, v.z}; }
// perspective projection: returns false if behind camera
static inline bool project(V3 v, float dist, float focal, float cx, float cy, int& sx, int& sy) {
  float z = v.z + dist;
  if (z < 0.05f) return false;
  float k = focal / z;
  sx = (int)lrintf(cx + v.x * k);
  sy = (int)lrintf(cy + v.y * k);
  return true;
}

static void gfxInit() {
  for (int i = 0; i < 256; i++) SIN8[i] = (int8_t)lrintf(127.0f * sinf(i * 2 * PI_F / 256));
  for (unsigned i = 0; i < sizeof(ISQRT); i++) ISQRT[i] = (uint8_t)sqrtf((float)i);
}
static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline float smooth(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }
static inline uint32_t hash32(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x;
}
static inline float hashf(uint32_t x) { return (hash32(x) & 0xFFFF) / 65535.0f; }  // 0..1
