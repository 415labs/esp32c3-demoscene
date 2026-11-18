// effects.h - all visual parts. Every effect is a pure function of the song
// position (Sync), so visuals can never drift from the music.
#pragma once
#include "gfx.h"
#include "song.h"

struct DisplayCtl { uint8_t contrast; bool invert; uint8_t startLine; };

// ============================================================ shared geometry
// --- diamond (brilliant-cut gem): 8 table, 8 girdle, 1 culet
static V3 DV[17];
static uint8_t DE[40][2];
static void buildDiamond() {
  for (int i = 0; i < 8; i++) {
    float a = i * PI_F / 4 + PI_F / 8, g = i * PI_F / 4;
    DV[i] = {0.5f * cosf(a), -0.55f, 0.5f * sinf(a)};
    DV[8 + i] = {cosf(g), -0.18f, sinf(g)};
  }
  DV[16] = {0, 0.95f, 0};
  int e = 0;
  for (int i = 0; i < 8; i++) { DE[e][0] = i; DE[e][1] = (i + 1) & 7; e++; }           // table
  for (int i = 0; i < 8; i++) { DE[e][0] = i; DE[e][1] = 8 + ((i + 1) & 7); e++;        // crown star
                                DE[e][0] = i; DE[e][1] = 8 + i; e++; }
  for (int i = 0; i < 8; i++) { DE[e][0] = 8 + i; DE[e][1] = 8 + ((i + 1) & 7); e++; }  // girdle
  for (int i = 0; i < 8; i++) { DE[e][0] = 8 + i; DE[e][1] = 16; e++; }                 // pavilion
}
struct Xf { float cy, sy, cx, sx, cz, sz; };
static inline Xf mkXf(float ay, float ax, float az) { return {cosf(ay), sinf(ay), cosf(ax), sinf(ax), cosf(az), sinf(az)}; }
static inline V3 xf(V3 v, const Xf& r) { return rotZ(rotX(rotY(v, r.cy, r.sy), r.cx, r.sx), r.cz, r.sz); }
static inline V3 xfInv(V3 v, const Xf& r) { return rotY(rotX(rotZ(v, r.cz, -r.sz), r.cx, -r.sx), r.cy, -r.sy); }

static void drawDiamond(float ay, float ax, float scale, float cx, float cy, int edges, int pattern = 0,
                        float explode = 0, uint32_t seed = 0) {
  Xf r = mkXf(ay, ax, 0.12f * sinf(ay * 0.7f));
  int px[17], py[17]; bool ok[17];
  V3 tv[17];
  for (int i = 0; i < 17; i++) {
    V3 v = {DV[i].x * scale, (DV[i].y - 0.2f) * scale, DV[i].z * scale};
    tv[i] = xf(v, r);
    ok[i] = project(tv[i], 4.0f, 80.0f, cx, cy, px[i], py[i]);
  }
  for (int e = 0; e < edges && e < 40; e++) {
    int a = DE[e][0], b = DE[e][1];
    if (explode > 0) {
      V3 m = {(tv[a].x + tv[b].x) * 0.5f, (tv[a].y + tv[b].y) * 0.5f, (tv[a].z + tv[b].z) * 0.5f};
      float jx = hashf(seed + e * 7) - 0.5f, jy = hashf(seed + e * 7 + 1) - 0.5f, jz = hashf(seed + e * 7 + 2) - 0.5f;
      V3 d = {m.x + jx, m.y + jy, m.z + jz};
      float sh = 1.0f / (1.0f + explode * 0.4f);
      V3 p0 = {m.x + (tv[a].x - m.x) * sh + d.x * explode, m.y + (tv[a].y - m.y) * sh + d.y * explode, m.z + (tv[a].z - m.z) * sh + d.z * explode};
      V3 p1 = {m.x + (tv[b].x - m.x) * sh + d.x * explode, m.y + (tv[b].y - m.y) * sh + d.y * explode, m.z + (tv[b].z - m.z) * sh + d.z * explode};
      int x0, y0, x1, y1;
      if (project(p0, 4.0f, 80.0f, cx, cy, x0, y0) && project(p1, 4.0f, 80.0f, cx, cy, x1, y1)) line(x0, y0, x1, y1);
      continue;
    }
    if (ok[a] && ok[b]) line(px[a], py[a], px[b], py[b], pattern);
  }
}

// --- vector logo "415 / LABS" on a 5x7 stroke grid (0..4, 0..6)
struct Seg { uint8_t x0, y0, x1, y1; };
static const Seg LOGO_SEGS[] = {
  // 4
  {0,0,0,4},{0,4,4,4},{3,0,3,6},
  // 1
  {1,1,2,0},{2,0,2,6},{1,6,3,6},
  // 5
  {4,0,0,0},{0,0,0,3},{0,3,3,3},{3,3,4,4},{4,4,4,5},{4,5,3,6},{3,6,0,6},
  // L
  {0,0,0,6},{0,6,4,6},
  // A
  {0,6,0,2},{0,2,2,0},{2,0,4,2},{4,2,4,6},{0,4,4,4},
  // B
  {0,0,0,6},{0,0,3,0},{3,0,4,1},{4,1,4,2},{4,2,3,3},{0,3,3,3},{3,3,4,4},{4,4,4,5},{4,5,3,6},{3,6,0,6},
  // S
  {4,1,3,0},{3,0,1,0},{1,0,0,1},{0,1,0,2},{0,2,1,3},{1,3,3,3},{3,3,4,4},{4,4,4,5},{4,5,3,6},{3,6,1,6},{1,6,0,5},
};
static const uint8_t LOGO_START[8] = {0, 3, 6, 13, 15, 20, 30, 41};
// two layouts: A = two lines, big; B = one line, small
static const float LOGO_A[7][3] = {{21,10.5f,2.4f},{36,10.5f,2.4f},{51,10.5f,2.4f},{13.5f,29.5f,2.4f},{28.5f,29.5f,2.4f},{43.5f,29.5f,2.4f},{58.5f,29.5f,2.4f}};
static const float LOGO_B[7][3] = {{7.2f,9,1.8f},{16.8f,9,1.8f},{26.4f,9,1.8f},{36,9,1.8f},{45.6f,9,1.8f},{55.2f,9,1.8f},{64.8f,9,1.8f}};

// morph 0=A 1=B; rotation around Y; explode scatters letters; letters = how many to show
static void drawLogo(float morph, float ay, float ax, float zoom, float explode, int bold, int letters = 7) {
  float cy_ = cosf(ay), sy_ = sinf(ay), cx_ = cosf(ax), sx_ = sinf(ax);
  for (int l = 0; l < letters; l++) {
    float lx = LOGO_A[l][0] + (LOGO_B[l][0] - LOGO_A[l][0]) * morph;
    float ly = LOGO_A[l][1] + (LOGO_B[l][1] - LOGO_A[l][1]) * morph;
    float sc = (LOGO_A[l][2] + (LOGO_B[l][2] - LOGO_A[l][2]) * morph) * zoom;
    float ex = 0, ey = 0, ez = 0, rot = 0;
    if (explode > 0) {
      ex = (hashf(l * 3 + 11) - 0.5f) * 90 * explode;
      ey = (hashf(l * 3 + 12) - 0.5f) * 60 * explode;
      ez = -hashf(l * 3 + 13) * 120 * explode;
      rot = (hashf(l * 5) - 0.5f) * 6 * explode;
    }
    float cr = cosf(rot), sr = sinf(rot);
    for (int k = LOGO_START[l]; k < LOGO_START[l + 1]; k++) {
      const Seg& g = LOGO_SEGS[k];
      int sx[2], sy[2]; bool vis = true;
      for (int p = 0; p < 2; p++) {
        float gx = ((p ? g.x1 : g.x0) - 2.0f) * sc, gy = ((p ? g.y1 : g.y0) - 3.0f) * sc;
        float rx = gx * cr - gy * sr, ry = gx * sr + gy * cr;
        V3 v = {(lx - 36) * zoom + rx + ex, (ly - 20) * zoom + ry + ey, ez};
        v = rotY(v, cy_, sy_);
        v = rotX(v, cx_, sx_);
        vis &= project(v, 120.0f, 120.0f, 36, 20, sx[p], sy[p]);
      }
      if (!vis) continue;
      line(sx[0], sy[0], sx[1], sy[1]);
      if (bold) line(sx[0] + 1, sy[0], sx[1] + 1, sy[1]);
    }
  }
}

// --- point-cloud shapes for the morph part
#define NP 128
enum { SH_SPHERE, SH_TORUS, SH_CUBE, SH_LOGO, SH_HELIX, SH_GEM, SH_COUNT };
static V3 SHAPES[SH_COUNT][NP];
static void buildShapes() {
  for (int i = 0; i < NP; i++) {
    float y = 1 - 2 * (i + 0.5f) / NP, r = sqrtf(1 - y * y), ph = i * 2.39996f;
    SHAPES[SH_SPHERE][i] = {cosf(ph) * r, y, sinf(ph) * r};
    float u = (i % 16) * 2 * PI_F / 16, v = (i / 16) * 2 * PI_F / 8;
    SHAPES[SH_TORUS][i] = {(0.72f + 0.3f * cosf(v)) * cosf(u), 0.3f * sinf(v), (0.72f + 0.3f * cosf(v)) * sinf(u)};
    // cube edges
    static const int8_t CV[8][3] = {{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},{-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
    static const uint8_t CE[12][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
    int e = i % 12; float t = ((i / 12) + 0.5f) / 11.0f;
    const int8_t *a = CV[CE[e][0]], *b = CV[CE[e][1]];
    SHAPES[SH_CUBE][i] = {0.62f * (a[0] + (b[0] - a[0]) * t), 0.62f * (a[1] + (b[1] - a[1]) * t), 0.62f * (a[2] + (b[2] - a[2]) * t)};
    float hy = -1 + 2 * (i >> 1) / 64.0f, ha = (i >> 1) / 64.0f * 4 * PI_F + (i & 1) * PI_F;
    SHAPES[SH_HELIX][i] = {0.55f * cosf(ha), hy, 0.55f * sinf(ha)};
    int ge = i % 40; float gt = ((i / 40) + 0.5f) / 4.0f;
    V3 ga = DV[DE[ge][0]], gb = DV[DE[ge][1]];
    SHAPES[SH_GEM][i] = {ga.x + (gb.x - ga.x) * gt, ga.y + (gb.y - ga.y) * gt - 0.2f, ga.z + (gb.z - ga.z) * gt};
  }
  // logo: sample strokes uniformly
  float total = 0;
  for (int l = 0; l < 7; l++)
    for (int k = LOGO_START[l]; k < LOGO_START[l + 1]; k++) {
      const Seg& g = LOGO_SEGS[k];
      total += sqrtf((float)((g.x1 - g.x0) * (g.x1 - g.x0) + (g.y1 - g.y0) * (g.y1 - g.y0)));
    }
  int idx = 0; float acc = 0;
  for (int l = 0; l < 7 && idx < NP; l++)
    for (int k = LOGO_START[l]; k < LOGO_START[l + 1] && idx < NP; k++) {
      const Seg& g = LOGO_SEGS[k];
      float len = sqrtf((float)((g.x1 - g.x0) * (g.x1 - g.x0) + (g.y1 - g.y0) * (g.y1 - g.y0)));
      while (idx < NP && (idx + 0.5f) * total / NP < acc + len) {
        float t = ((idx + 0.5f) * total / NP - acc) / len;
        float gx = g.x0 + (g.x1 - g.x0) * t, gy = g.y0 + (g.y1 - g.y0) * t;
        float X = LOGO_A[l][0] + (gx - 2) * 2.4f - 36, Y = LOGO_A[l][1] + (gy - 3) * 2.4f - 20;
        SHAPES[SH_LOGO][idx++] = {X / 19.0f, Y / 19.0f, 0};
      }
      acc += len;
    }
  while (idx < NP) { SHAPES[SH_LOGO][idx] = SHAPES[SH_LOGO][idx - 1]; idx++; }
}

// --- tunnel tables (2x screen so the centre can wander)
#define TW 144
#define TH 80
static uint8_t TANG[TW * TH], TDEP[TW * TH];
static void buildTunnel() {
  for (int y = 0; y < TH; y++)
    for (int x = 0; x < TW; x++) {
      float dx = x - TW / 2 + 0.5f, dy = y - TH / 2 + 0.5f;
      float a = atan2f(dy, dx);
      float d = sqrtf(dx * dx + dy * dy);
      TANG[y * TW + x] = (uint8_t)((int)(a * 128 / PI_F) & 255);
      int dep = (int)(1400.0f / (d + 2.0f));
      TDEP[y * TW + x] = dep > 255 ? 255 : dep;
    }
}

// --- rotozoom texture: "415" on a 32x16 tile
static uint8_t TEX[16][32];
static void buildTex() {
  memset(TEX, 0, sizeof TEX);
  const char* t = "415";
  for (int c = 0; c < 3; c++) {
    const uint8_t* g = glyph(t[c]);
    for (int x = 0; x < 5; x++)
      for (int y = 0; y < 7; y++)
        if (g[x] >> y & 1) TEX[4 + y][7 + c * 6 + x] = 1;
  }
  for (int x = 4; x < 27; x++) TEX[13][x] = 1;
}

static void effectsInit() {
  gfxInit();
  buildDiamond();
  buildShapes();
  buildTunnel();
  buildTex();
}

// ============================================================ helpers
static inline float secsSinceBar(const Sync& s, int bar) { return (s.pos - (float)bar * 16 * ROW_SAMPLES) / SR; }
static inline int shakeLine(const Sync& s, float amount) {
  int a = (int)(s.kick * amount + 0.5f);
  if (!a) return 0;
  return ((s.kickSince / 1100) & 1) ? a : (64 - a) & 63;
}
static void glitch(const Sync& s, float strength) {
  if (s.snareSince > 2200) return;
  uint32_t h = hash32(s.pos / 600 + 77);
  int bands = 1 + (h & 3);
  for (int i = 0; i < bands; i++) {
    uint32_t hh = hash32(h + i * 31);
    int y0 = hh % SCR_H, hgt = 2 + (hh >> 8) % 7;
    int dx = (int)(((int)((hh >> 16) % 25) - 12) * strength);
    shiftRows(y0, y0 + hgt, dx);
  }
}
static void flashRamp(float p) {  // dithered white-out, p 0..1
  int lvl = (int)(p * 17);
  for (int y = 0; y < SCR_H; y++)
    for (int x = 0; x < SCR_W; x++) pdith(x, y, lvl);
}

// ============================================================ parts
// Bars 0-3: the gem is built vertex by vertex, edge by edge - one per arp pluck
static void fxIntro(const Sync& s, DisplayCtl& d) {
  float B = s.bars;
  int pl = s.pos / HALF_ROW;                         // arp pluck index
  int sincePl = s.pos % HALF_ROW;
  float spin = B > 3 ? powf(B - 3, 3) * 16 : 0;
  float ay = 1.4f * s.t + spin;
  float ax = 0.42f + 0.12f * sinf(s.t * 0.9f);
  float sc = 0.35f + 0.65f * smooth(B * 0.8f);
  if (B > 3) sc *= 1 + 9 * powf(B - 3, 4);
  // twinkling background stars, re-rolled on every pluck
  for (int j = 0; j < 14; j++) {
    uint32_t h = hash32(j * 977 + (pl >> 1) * 13);
    if ((h & 3) == 0) pset(h % SCR_W, (h >> 8) % SCR_H);
  }
  int verts = pl + 1 > 17 ? 17 : pl + 1;
  int edges = pl - 16; if (edges < 0) edges = 0;
  Xf r = mkXf(ay, ax, 0.12f * sinf(ay * 0.7f));
  for (int i = 0; i < verts; i++) {
    V3 v = {DV[i].x * sc, (DV[i].y - 0.2f) * sc, DV[i].z * sc};
    int x, y;
    if (project(xf(v, r), 4, 80, 36, 20, x, y)) {
      pset(x, y);
      if (edges < 40 && i == (pl % 17) && sincePl < 1400) {  // glint on the plucked vertex
        pset(x - 1, y); pset(x + 1, y); pset(x, y - 1); pset(x, y + 1);
        pset(x - 2, y); pset(x + 2, y); pset(x, y - 2); pset(x, y + 2);
      }
    }
  }
  drawDiamond(ay, ax, sc, 36, 20, edges);
  if (B > 3.8f) flashRamp((B - 3.8f) / 0.2f);
  d.contrast = (uint8_t)(255 * clampf(B * 4 + 0.15f, 0, 1));
}

// Bars 4-7: logo slams in on the first boom, wobbles, collapses to one line,
// "PRESENTS" is typed by the hi-hats, snare roll glitches it apart
static void fxLogo(const Sync& s, DisplayCtl& d) {
  float lb = s.bars - 4;
  float sec = secsSinceBar(s, 4);
  float morph = smooth((lb - 2.0f) / 0.5f);
  float zoom = 1 + 6 * expf(-sec * 9) + 0.07f * s.kick;
  float ay = 0.38f * sinf(s.t * 1.6f) * (1 - 0.6f * morph) + 2.5f * expf(-sec * 5);
  float ax = 0.15f * sinf(s.t * 1.1f);
  float explode = lb > 3.75f ? powf((lb - 3.75f) * 4, 2) : 0;
  drawLogo(morph, ay, ax, zoom, explode, morph < 0.5f);
  if (lb >= 2) {
    int hats = 0;
    for (int r = 6 * 16; r <= s.row; r++) if (hatAt(r)) hats++;
    const char* txt = "PRESENTS";
    for (int i = 0; i < hats && i < 8; i++) drawChar(12 + i * 6, 25, txt[i]);
    if (hats > 0 && hats <= 8 && (s.pos / 2400) & 1) fillRect(12 + hats * 6, 25, 5, 7);  // cursor
  }
  if (lb >= 3) glitch(s, 1 + (lb - 3) * 2);
  if (lb > 3.9f) flashRamp((lb - 3.9f) / 0.1f);
  d.contrast = (uint8_t)(110 + 145 * s.kick);
  d.startLine = shakeLine(s, 4);
  d.invert = s.crash > 0.8f;
}

// Bars 8-15: warp starfield; every kick launches a diamond shockwave
static void fxStars(const Sync& s, DisplayCtl& d) {
  float sec = secsSinceBar(s, 8);
  float beats = (s.rows - 8 * 16) / 4.0f;
  float travel = 0.9f * sec + 0.55f * ((int)beats + (1 - expf(-s.kickSince / (0.12f * SR))));
  float roll = 0.35f * sinf(s.t * 0.55f) + 0.2f * sinf(s.t * 0.21f);
  float cr = cosf(roll), sr = sinf(roll);
  float cx = 36 + 8 * sinf(s.t * 0.7f), cy = 20 + 5 * sinf(s.t * 0.93f);
  float streak = 0.08f + 0.5f * s.kick;
  for (int i = 0; i < 80; i++) {
    float x = hashf(i * 3) * 2 - 1, y = hashf(i * 3 + 1) * 2 - 1;
    float z = fmodf(hashf(i * 3 + 2) * 4 - travel, 4.0f);
    if (z <= 0) z += 4;
    z += 0.05f;
    float rx = (x * cr - y * sr) * 1.8f, ry = (x * sr + y * cr) * 1.2f;
    int x0 = (int)(cx + rx * 20 / z), y0 = (int)(cy + ry * 20 / z);
    int x1 = (int)(cx + rx * 20 / (z + streak)), y1 = (int)(cy + ry * 20 / (z + streak));
    line(x0, y0, x1, y1);
  }
  for (int k = 0; k < 3; k++) {  // shockwaves from the last 3 kicks
    float since = (s.kickSince + k * 4 * ROW_SAMPLES) / (float)SR;
    if (s.rows - 8 * 16 < k * 4) break;
    float rr = 3 + since * since * 260 + since * 30;
    if (rr > 80) continue;
    int a = (int)rr, b = (int)(rr * 0.6f);
    int X = (int)cx, Y = (int)cy;
    int pat = k ? 1 : 0;
    line(X - a, Y, X, Y - b, pat); line(X, Y - b, X + a, Y, pat);
    line(X + a, Y, X, Y + b, pat); line(X, Y + b, X - a, Y, pat);
  }
  if (s.bar == 15) glitch(s, 1.5f);
  d.contrast = (uint8_t)(70 + 185 * s.kick);
  d.invert = s.crash > 0.8f;
}

// Bars 16-23: dithered plasma; every lead note drops a ripple at its pitch
static void fxPlasma(const Sync& s, DisplayCtl& d) {
  int T = s.pos >> 7;
  int fx = 5 - (int)(2.5f * s.kick);
  int cx = 36 + (int)(18 * sinf(s.t * 0.6f)), cy = 20 + (int)(10 * sinf(s.t * 0.8f));
  int rx = 6 + (s.leadNote - 64) * 60 / 24, ry = 20 + (int)(8 * sinf(s.leadNote * 1.7f));
  int rr = (int)(s.leadAge * 70);
  int ramp = (int)(220 * expf(-s.leadAge * 2.5f));
  int pal = s.chord * 40;
  for (int y = 0; y < SCR_H; y++) {
    int dy = y - cy, dy2 = (y - ry) * (y - ry);
    for (int x = 0; x < SCR_W; x++) {
      int dx = x - cx;
      int v = SIN8[(x * fx + T) & 255] + SIN8[(y * 7 - T * 2 / 3) & 255]
            + SIN8[((x + y) * 3 + T / 2) & 255] + SIN8[(ISQRT[dx * dx + dy * dy] * 9 - T) & 255];
      int ddx = x - rx; int dd = ddx * ddx + dy2;
      if (dd < (int)sizeof(ISQRT)) { int rd = ISQRT[dd] - rr; if (rd > -3 && rd < 3) v += ramp; }
      int lvl = abs(((v + 512 + pal) >> 2 & 63) - 32) >> 1;
      pdith(x, y, lvl);
    }
  }
  if (s.bar == 23) glitch(s, 1);
  d.contrast = (uint8_t)(80 + 175 * s.kick);
  d.invert = s.snare > 0.75f || s.crash > 0.8f;
}

// Bars 24-31: 128-point cloud morphing on every downbeat
static const uint8_t MORPH_SEQ[9] = {SH_SPHERE, SH_SPHERE, SH_TORUS, SH_CUBE, SH_LOGO, SH_HELIX, SH_GEM, SH_TORUS, SH_LOGO};
static void fxMorph(const Sync& s, DisplayCtl& d) {
  float mb = s.bars - 24;
  int bi = (int)mb;
  float m = smooth((mb - bi) * 2.2f);
  int from = MORPH_SEQ[bi], to = MORPH_SEQ[bi + 1];
  if (bi == 0) from = -1;  // first bar: burst from a single point
  Xf r = mkXf(s.t * 1.15f, 0.35f + 0.4f * sinf(s.t * 0.7f), 0.25f * sinf(s.t * 0.5f));
  Xf wob = mkXf(0.45f * sinf(s.t * 1.7f), 0.15f * sinf(s.t * 1.3f), 0);
  float sc = 1 + 0.28f * s.kick;
  float burst = sinf(PI_F * m) * 0.7f;
  for (int i = 0; i < NP; i++) {
    V3 a = from < 0 ? V3{0, 0, 0} : SHAPES[from][i];
    V3 b = SHAPES[to][i];
    if (from == SH_LOGO) a = xfInv(xf(a, wob), r);   // logo always faces the viewer
    if (to == SH_LOGO) b = xfInv(xf(b, wob), r);
    float j = 1 + burst * (hashf(i) - 0.3f);
    V3 p = {(a.x + (b.x - a.x) * m) * j * sc, (a.y + (b.y - a.y) * m) * j * sc, (a.z + (b.z - a.z) * m) * j * sc};
    p = xf(p, r);
    int x, y;
    if (!project(p, 3.5f, 62, 36, 20, x, y)) continue;
    pset(x, y);
    if (p.z < -0.25f) { pset(x + 1, y); pset(x, y + 1); pset(x + 1, y + 1); }
  }
  if (s.bar == 31) glitch(s, 2);
  d.contrast = (uint8_t)(70 + 185 * s.kick);
  d.invert = s.crash > 0.8f;
}

// Bars 32-39: breakdown. Sine scroller whose bounce follows the lead,
// with a dithered "echo" copy trailing exactly one delay-time behind
static const char SCROLL_TXT[] = "415LABS PRESENTS   A TINY DEMO FOR A TINY CHIP  *  72X40 PIXELS  *  ONE GPIO OF SOUND  *  GREETINGS TO ALL SCENERS #   ";
static void drawScroll(const Sync& s, float x0, float amp, int dith) {
  int len = sizeof(SCROLL_TXT) - 1;
  int T = s.pos >> 8;
  for (int i = 0; i < len; i++) {
    int xc = (int)x0 + i * 12;
    if (xc < -12 || xc >= SCR_W) continue;
    const uint8_t* g = glyph(SCROLL_TXT[i]);
    for (int c = 0; c < 5; c++)
      for (int w = 0; w < 2; w++) {
        int px = xc + c * 2 + w;
        if ((unsigned)px >= SCR_W) continue;
        int py = 13 + (SIN8[(px * 5 + T * 3) & 255] * (int)amp >> 7);
        for (int b = 0; b < 7; b++)
          if (g[c] >> b & 1) {
            int y0 = py + b * 2;
            if (!dith || ((px + y0) & 1)) pset(px, y0);
            if (!dith || ((px + y0 + 1) & 1)) pset(px, y0 + 1);
          }
      }
  }
}
static void fxScroll(const Sync& s, DisplayCtl& d) {
  float total = (sizeof(SCROLL_TXT) - 1) * 12.0f + SCR_W;
  float span = 8.0f * 16 * ROW_SAMPLES;
  float prog = (s.pos - 32.0f * 16 * ROW_SAMPLES) / span;
  float speed = total / span;  // px per sample
  // parallax stars
  for (int j = 0; j < 24; j++) {
    float sp = (j & 1) ? 9.0f : 22.0f;
    int x = (int)(hashf(j * 5) * 200 - s.t * sp) % SCR_W; if (x < 0) x += SCR_W;
    pset(x, (int)(hashf(j * 5 + 1) * SCR_H));
  }
  float rise = s.bar >= 38 ? (s.bars - 38) / 2 : 0;
  float amp = 3 + 6 * s.lead + 6 * rise;
  drawScroll(s, SCR_W - prog * total + speed * DLY_LEN, amp * 0.8f, 1);   // the echo
  drawScroll(s, SCR_W - prog * total, amp, 0);
  if (s.bar >= 38) glitch(s, 1 + rise * 2);
  if (s.bars > 39.85f) flashRamp((s.bars - 39.85f) / 0.15f);
  d.contrast = (uint8_t)(90 + 165 * s.lead);
}

// Bars 40-47: rotozoomer over a "415" texture, zoom punched by the kick
static void fxRoto(const Sync& s, DisplayCtl& d) {
  float a = 0.55f * s.t + 0.7f * sinf(s.t * 0.33f);
  float z = 0.5f + 0.22f * sinf(s.t * 0.8f) - 0.2f * s.kick;
  int32_t ca = (int32_t)(cosf(a) * z * 65536), sa = (int32_t)(sinf(a) * z * 65536);
  int32_t u0 = (int32_t)((s.t * 22) * 65536) - 36 * ca + 20 * sa;
  int32_t v0 = (int32_t)((s.t * 9) * 65536) - 36 * sa - 20 * ca;
  for (int y = 0; y < SCR_H; y++) {
    int32_t u = u0 - y * sa, v = v0 + y * ca;
    for (int x = 0; x < SCR_W; x++) {
      int iu = u >> 16, iv = v >> 16;
      int bit = TEX[iv & 15][iu & 31] ^ (((iu >> 6) ^ (iv >> 5)) & 1);
      if (bit) pset(x, y);
      u += ca; v += sa;
    }
  }
  if (s.bar == 47) glitch(s, 2);
  d.contrast = (uint8_t)(70 + 185 * s.kick);
  d.invert = s.snare > 0.8f || s.crash > 0.8f;
}

// Bars 48-55: wandering tunnel, travel speed kicked forward by each beat
static void fxTunnel(const Sync& s, DisplayCtl& d) {
  float sec = secsSinceBar(s, 48);
  float beats = (s.rows - 48 * 16) / 4.0f;
  float boost = s.bar == 55 ? powf(s.bars - 55, 3) * 900 : 0;
  int travel = (int)(sec * 70 + 40 * ((int)beats + (1 - expf(-s.kickSince / (0.1f * SR)))) + boost);
  int rot = (int)(s.t * 25 + 60 * sinf(s.t * 0.37f));
  int cx = 36 + (int)(16 * sinf(s.t * 0.9f)), cy = 20 + (int)(9 * sinf(s.t * 1.3f));
  int flash = s.kick > 0.6f;
  for (int y = 0; y < SCR_H; y++) {
    const uint8_t* A = &TANG[(y - cy + TH / 2) * TW - cx + TW / 2];
    const uint8_t* D = &TDEP[(y - cy + TH / 2) * TW - cx + TW / 2];
    for (int x = 0; x < SCR_W; x++) {
      int dep = D[x];
      int bit = (((A[x] + rot) >> 5) ^ ((dep + travel) >> 4)) & 1;
      int fog = 19 - dep / 9; if (fog < 0) fog = 0; if (fog > 16) fog = 16;
      int lvl = bit ? fog : (flash ? fog >> 2 : 0);
      pdith(x, y, lvl);
    }
  }
  if (s.bar == 55) glitch(s, 2);
  if (s.bars > 55.85f) flashRamp((s.bars - 55.85f) / 0.15f);
  d.contrast = (uint8_t)(80 + 175 * s.kick);
  d.startLine = shakeLine(s, 2);
  d.invert = s.crash > 0.8f;
}

// Bars 56-63: the gem returns with motion trails, shatters on the final hit,
// logo + thanks, fade to black
static void fxOutro(const Sync& s, DisplayCtl& d) {
  float ob = s.bars - 56;
  float ay = 1.6f * s.t, ax = 0.4f + 0.15f * sinf(s.t);
  if (ob < 4) {
    float sc = 1.05f + 0.2f * s.kick;
    drawDiamond(ay - 0.30f, ax, sc, 36, 20, 40, 3);
    drawDiamond(ay - 0.15f, ax, sc, 36, 20, 40, 1);
    drawDiamond(ay, ax, sc, 36, 20, 40);
    d.contrast = (uint8_t)(90 + 165 * s.kick);
  } else {
    float sec = secsSinceBar(s, 60);
    float ay60 = 1.6f * (60 * 16 * ROW_SAMPLES / (float)SR);
    if (sec < 2.5f) drawDiamond(ay60 + sec * 0.8f, ax, 1.25f, 36, 20, 40, 0, sec * sec * 3 + sec * 2.5f, 99);
    if (sec > 0.9f) {
      int letters = (int)((sec - 0.9f) / 0.12f) + 1;
      drawLogo(1, 0.25f * sinf(s.t * 1.2f), 0, 1, 0, 0, letters > 7 ? 7 : letters);
    }
    if (sec > 2.0f) {
      const char* t = "THANK YOU";
      int n = (int)((sec - 2.0f) / 0.1f) + 1;
      for (int i = 0; i < n && i < 9; i++) drawChar(9 + i * 6, 26, t[i]);
    }
    float fade = ob > 6 ? 1 - (ob - 6) / 2 : 1;
    d.contrast = (uint8_t)(255 * clampf(fade, 0, 1));
  }
  d.invert = s.crash > 0.8f;
}

static void renderFrame(const Sync& s, DisplayCtl& d) {
  cls();
  d.contrast = 255; d.invert = false; d.startLine = 0;
  int b = s.bar;
  if (b < SEC_LOGO) fxIntro(s, d);
  else if (b < SEC_STARS) fxLogo(s, d);
  else if (b < SEC_PLASMA) fxStars(s, d);
  else if (b < SEC_MORPH) fxPlasma(s, d);
  else if (b < SEC_SCROLL) fxMorph(s, d);
  else if (b < SEC_ROTO) fxScroll(s, d);
  else if (b < SEC_TUNNEL) fxRoto(s, d);
  else if (b < SEC_OUTRO) fxTunnel(s, d);
  else fxOutro(s, d);
}
