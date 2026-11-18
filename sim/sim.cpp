// Desktop simulator: renders the soundtrack to WAV and the visuals (with the
// OLED's contrast / invert / start-line tricks emulated) to raw RGB for ffmpeg.
//   sim wav out.wav        -> audio
//   sim video FPS SCALE    -> raw rgb24 frames on stdout
//   sim stats              -> per-voice levels
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static double st_peak[10], st_sq[10]; static long st_n;
#define SYNTH_STATS(...) do { int32_t v_[10] = {__VA_ARGS__}; for (int q = 0; q < 10; q++) { double a = fabs((double)v_[q]); if (a > st_peak[q]) st_peak[q] = a; st_sq[q] += a * a; } st_n++; } while (0)

#include "../include/effects.h"

static void wr32(FILE* f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void wr16(FILE* f, uint16_t v) { fwrite(&v, 2, 1, f); }

int main(int argc, char** argv) {
  effectsInit();
  synthInit();
  const char* mode = argc > 1 ? argv[1] : "stats";
  if (!strcmp(mode, "wav") || !strcmp(mode, "stats")) {
    FILE* f = !strcmp(mode, "wav") ? fopen(argv[2], "wb") : NULL;
    uint32_t n = SONG_SAMPLES;
    if (f) {
      fwrite("RIFF", 1, 4, f); wr32(f, 36 + n * 2); fwrite("WAVEfmt ", 1, 8, f);
      wr32(f, 16); wr16(f, 1); wr16(f, 1); wr32(f, SR); wr32(f, SR * 2); wr16(f, 2); wr16(f, 16);
      fwrite("data", 1, 4, f); wr32(f, n * 2);
    }
    int16_t buf[256]; long clip = 0; double sq = 0; int pk = 0;
    for (uint32_t i = 0; i < n; i += 256) {
      synthRender(buf, 256);
      for (int k = 0; k < 256; k++) { int a = abs(buf[k]); if (a > pk) pk = a; if (a >= 32000) clip++; sq += (double)a * a; }
      if (f) fwrite(buf, 2, 256, f);
    }
    if (f) fclose(f);
    const char* nm[10] = {"kick", "snare", "hat", "crash", "riser", "bass", "pad", "lead", "arp", "wet"};
    fprintf(stderr, "master peak %d  rms %.0f  near-clip samples %ld\n", pk, sqrt(sq / n), clip);
    for (int q = 0; q < 10; q++) fprintf(stderr, "  %-6s peak %6.0f rms %6.0f\n", nm[q], st_peak[q], sqrt(st_sq[q] / st_n));
    return 0;
  }
  if (!strcmp(mode, "video") || !strcmp(mode, "frame")) {
    int fps = atoi(argv[2]), S = atoi(argv[3]);
    int W = SCR_W * S, Hh = SCR_H * S;
    uint8_t* img = (uint8_t*)malloc(W * Hh * 3);
    int nframes = (int)((double)SONG_SAMPLES / SR * fps);
    int f0 = 0, f1 = nframes;
    if (!strcmp(mode, "frame")) { f0 = (int)(atof(argv[4]) * fps); f1 = f0 + 1; }
    for (int fr = f0; fr < f1; fr++) {
      uint32_t pos = (uint32_t)((double)fr * SR / fps);
      Sync s; syncAt(pos, s);
      DisplayCtl d; renderFrame(s, d);
      float br = d.contrast ? 0.18f + 0.82f * d.contrast / 255.0f : 0.0f;
      for (int y = 0; y < Hh; y++) {
        int oy = y / S, ry = (oy + d.startLine) & 63;
        for (int x = 0; x < W; x++) {
          int ox = x / S;
          bool on = ry < SCR_H ? pget(ox, ry) : false;
          if (d.invert) on = !on;
          bool gap = (x % S == S - 1) || (y % S == S - 1);
          float v = on ? br * (gap ? 0.55f : 1.0f) : 0.02f;
          uint8_t* p = img + (y * W + x) * 3;
          p[0] = (uint8_t)(235 * v); p[1] = (uint8_t)(245 * v); p[2] = (uint8_t)(255 * v);
        }
      }
      fwrite(img, 1, W * Hh * 3, stdout);
    }
    return 0;
  }
  return 1;
}
