// song.h - "415 SIGNAL" soundtrack: sequencer + integer synth + visual sync
//
// One master clock: the audio sample counter. The synth and the visuals both
// read the SAME pattern functions (kickAt, snareAt, leadAt, ...), so every
// flash, zoom and explosion on screen is computed from the exact sample at
// which the corresponding sound starts.
#pragma once
#include <stdint.h>
#include <math.h>
#include <string.h>

#define SR 32000
#define ROW_SAMPLES 3750                 // 128 BPM, 16th-note rows: 32000*60/(128*4)
#define HALF_ROW (ROW_SAMPLES / 2)
#define BAR_ROWS 16
#define SONG_BARS 64
#define SONG_ROWS (SONG_BARS * BAR_ROWS)
#define SONG_SAMPLES ((uint32_t)SONG_ROWS * ROW_SAMPLES)   // 3,840,000 = 120 s

// Section starts (bars)
enum {
  SEC_DIAMOND = 0, SEC_LOGO = 4, SEC_STARS = 8, SEC_PLASMA = 16, SEC_MORPH = 24,
  SEC_SCROLL = 32, SEC_ROTO = 40, SEC_TUNNEL = 48, SEC_OUTRO = 56
};

// ----------------------------------------------------------------- arrangement
static inline bool isFillBar(int b) { return b == 15 || b == 31 || b == 47 || b == 55; }
static inline bool drumsOn(int b) { return b >= 8 && b < 60 && !(b >= 32 && b < 40); }

static bool kickAt(int row) {
  int b = row >> 4, s = row & 15;
  if (b < 4) return false;
  if (b < 8) return s == 0;                       // intro "booms"
  if (b == 60) return s == 0;                     // final hit
  if (!drumsOn(b)) return false;
  if (isFillBar(b) && s >= 12) return s == 14;    // break before the drop
  return (s & 3) == 0;                            // four on the floor
}
static bool snareAt(int row) {
  int b = row >> 4, s = row & 15;
  if (b == 7 || b == 39) return (0xF555u >> s) & 1;        // build-up roll
  if (b == 38) return (s & 3) == 0;
  if (!drumsOn(b)) return false;
  if (isFillBar(b) && s >= 12) return true;                // 16th fill
  return s == 4 || s == 12;
}
// 0 none, 1 closed, 2 open
static int hatAt(int row) {
  int b = row >> 4, s = row & 15;
  if (b == 6 || b == 7) return (s & 3) == 2 ? 2 : 0;
  if (!drumsOn(b)) return 0;
  if ((s & 3) == 2) return 2;
  if (b >= 40 && (s & 1)) return 1;                        // 16th hats in the climax
  if (b >= 16 && (s & 3) == 3) return 1;
  return 0;
}
static bool crashAt(int row) {
  int b = row >> 4, s = row & 15;
  if (s) return false;
  return b == 4 || b == 8 || b == 16 || b == 24 || b == 32 || b == 40 || b == 48 || b == 56 || b == 60;
}
// riser: returns progress 0..1 inside a riser region, or -1
static float riserAt(float barPos) {
  int b = (int)barPos;
  if (b == 3 || b == 7 || b == 31 || b == 55) return barPos - b;
  if (b == 38 || b == 39) return (barPos - 38) * 0.5f;
  return -1;
}

// Am - F - C - G  (vi-IV-I-V), the outro holds Am
static inline int chordAt(int bar) { return bar >= 60 ? 0 : (bar & 3); }
static const uint8_t CHORD_PAD[4][3] = {{57, 60, 64}, {53, 57, 60}, {55, 60, 64}, {55, 59, 62}};
static const uint8_t CHORD_ROOT[4] = {45, 41, 48, 43};      // A2 F2 C3 G2

static const int8_t BASS_PAT[16] = {-1, 0, 12, 0, -1, 0, 12, 0, -1, 0, 12, 0, -1, 12, 0, 7};
// returns midi note or -1; accent flag via *acc
static int bassAt(int row, int* acc) {
  int b = row >> 4, s = row & 15;
  *acc = 0;
  int root = CHORD_ROOT[chordAt(b)];
  if (b >= 4 && b < 8) return s == 0 ? root : -1;                 // long intro notes
  if (b == 60) return s == 0 ? root : -1;
  bool on = (b >= 8 && b < 32) || (b >= 36 && b < 60);
  if (!on) return -1;
  if (isFillBar(b) && s >= 12) return -1;
  int o = BASS_PAT[s];
  if (o < 0) return -1;
  *acc = (o == 12);
  return root + o;
}

// Lead melody, 8 bars, 16th rows. 0 = hold, 1 = note off. (played 12 lower)
static const uint8_t LEAD[128] = {
  81,0,0,0, 76,0,81,0, 84,0,83,0, 81,0,76,0,   // Am
  77,0,0,0, 81,0,84,0,  0,0,81,0, 79,0,77,0,   // F
  76,0,0,0, 79,0,84,0,  0,0,86,0, 84,0,79,0,   // C
  83,0,0,0,  0,0,0,0,  86,0,84,0, 83,0,79,0,   // G
  81,0,0,0, 84,0,88,0,  0,0,86,0, 84,0,83,0,   // Am
  84,0,0,0, 81,0,77,0,  0,0,81,0, 84,0,86,0,   // F
  88,0,0,0, 86,0,84,0, 79,0,0,0,  84,0,86,0,   // C
  83,0,0,0,  0,0,0,0,   0,0,0,0,  79,0,83,1,   // G
};
static inline bool leadOn(int b) { return b >= 16 && b < 60; }
// returns midi note (>1), 1 = note off, 0 = nothing new
static int leadAt(int row) {
  int b = row >> 4;
  if (!leadOn(b)) return (b == 60 && (row & 15) == 0) ? 1 : 0;
  int n = LEAD[(row - 16 * 16) & 127];
  if (n > 1) n += (b >= 48 && b < 56) ? 0 : -12;   // climax an octave up
  return n;
}
static inline bool arpOn(int b) { return b < 62; }

// ----------------------------------------------------------------- synth
// mixer gains, Q8 (256 = 1.0)
#define G_KICK 135
#define G_SNARE 95
#define G_HAT 150
#define G_CRASH 80
#define G_RISER 200
#define G_BASS 320
#define G_PAD 256
#define G_LEAD 300
#define G_ARP 170
#define G_WET 220
#define DLY_LEN (ROW_SAMPLES * 3)          // dotted-eighth echo
static inline uint32_t decayK(float tauSamples) { return (uint32_t)(65536.0f * (1.0f - 1.0f / tauSamples)); }
static inline int32_t envStep(int32_t e, uint32_t k) { return (int32_t)(((uint32_t)e * k) >> 16); }
static inline int32_t clamp32(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

struct Synth {
  uint32_t pos;          // absolute sample counter
  uint32_t songPos;      // position in song
  uint32_t rowCtr;       // samples into current row
  uint32_t noteInc[128];
  int16_t sinT[1024];
  uint32_t rnd;
  // bass
  uint32_t bPh, bInc; int32_t bEnv, bFenv, bLow, bBand, bFamt; uint32_t bDec;
  // lead
  uint32_t lPh, lInc, lTarget, lLfo, lVib; int32_t lEnv, lLp; int lGate, lAtk; uint32_t lHeld;
  // arp
  uint32_t aPh, aInc; int32_t aEnv; int aStep;
  // pad
  uint32_t pPh[6], pInc[6]; int32_t pLp, pLp2, pVol, pVolT, pCut;
  // drums
  uint32_t kPh; int32_t kEnv, kPEnv, kClick;
  uint32_t sPh; int32_t sEnv, sTEnv;
  int32_t hEnv, hLp; uint32_t hDec;
  int32_t cEnv, cLp;
  int32_t rLp, rAmt;
  // fx
  int16_t dly[DLY_LEN]; int dIdx; int32_t dWet;
  int32_t arpVol;
};
static Synth S;

static void synthInit() {
  memset(&S, 0, sizeof(S));
  for (int n = 0; n < 128; n++) S.noteInc[n] = (uint32_t)(440.0 * pow(2.0, (n - 69) / 12.0) / SR * 4294967296.0);
  for (int i = 0; i < 1024; i++) S.sinT[i] = (int16_t)lrintf(32767 * sinf(i * 2 * 3.14159265f / 1024));
  S.rnd = 0x1234567;
  S.bDec = decayK(3000);
  S.hDec = decayK(500);
  S.pCut = 1500;
}
static inline int32_t isin(uint32_t ph) { return S.sinT[ph >> 22]; }
static inline int32_t noise() { S.rnd ^= S.rnd << 13; S.rnd ^= S.rnd >> 17; S.rnd ^= S.rnd << 5; return (int32_t)S.rnd >> 16; }

static void setChord(int c) {
  for (int i = 0; i < 3; i++) {
    uint32_t inc = S.noteInc[CHORD_PAD[c][i]];
    S.pInc[i * 2] = inc + (inc >> 9);
    S.pInc[i * 2 + 1] = inc - (inc >> 9);
  }
}

static void onArp() {
  int row = S.songPos / ROW_SAMPLES, b = row >> 4;
  if (!arpOn(b)) return;
  static const uint8_t pat[8] = {0, 1, 2, 3, 4, 5, 3, 2};
  const uint8_t* ch = CHORD_PAD[chordAt(b)];
  int k = pat[S.aStep++ & 7];
  int n = ch[k % 3] + 12 + (k >= 3 ? 12 : 0);
  S.aInc = S.noteInc[n];
  S.aEnv = 32767;
}

static void onRow(int row) {
  int b = row >> 4, s = row & 15;
  if (s == 0) {
    setChord(chordAt(b));
    // pad level per section (Q15)
    int32_t v = 9000;
    if (b < 8) v = 16000;
    else if (b >= 32 && b < 40) v = 18000;
    else if (b >= 60) v = 14000;
    if (b >= 62) v = 0;
    S.pVolT = v;
    S.dWet = (b >= 32 && b < 40) ? 22000 : 12000;
    S.arpVol = (b < 2) ? 3000 + b * 3000 : (b >= 32 && b < 40 ? 6000 : 7000);
    if (b >= 60) S.arpVol = 5000;
  }
  if (row == 0) { S.aStep = 0; S.pVol = 0; }
  if (kickAt(row)) { S.kEnv = 32767; S.kPEnv = 32767; S.kClick = 32767; }
  if (snareAt(row)) { S.sEnv = (b == 7 || b == 39) ? 14000 + s * 1100 : 32767; S.sTEnv = 32767; }
  int h = hatAt(row);
  if (h) { S.hEnv = h == 2 ? 26000 : 16000; S.hDec = decayK(h == 2 ? 2600 : 450); }
  if (crashAt(row)) S.cEnv = 32767;
  int acc;
  int bn = bassAt(row, &acc);
  if (bn >= 0) {
    S.bInc = S.noteInc[bn];
    S.bEnv = 32767; S.bFenv = 32767;
    bool longNote = (b < 8 || b >= 60);
    S.bDec = decayK(longNote ? 40000 : (acc ? 4200 : 2600));
    // breakdown re-entry: filter slowly opens over bars 36-39
    int32_t amt = acc ? 15000 : 9000;
    if (b >= 36 && b < 40) amt = amt * (1 + (row - 36 * 16)) / 64;
    if (longNote) amt = 6000;
    S.bFamt = amt;
  }
  int ln = leadAt(row);
  if (ln > 1) {
    S.lTarget = S.noteInc[ln];
    if (!S.lGate) S.lInc = S.lTarget;
    S.lGate = 1; S.lAtk = 1; S.lHeld = 0;
  } else if (ln == 1) S.lGate = 0;
}

static void synthRender(int16_t* out, int n) {
  for (int i = 0; i < n; i++) {
    // ---- sequencer clock
    if (S.rowCtr == 0) onRow(S.songPos / ROW_SAMPLES);
    if (S.rowCtr == 0 || S.rowCtr == HALF_ROW) onArp();
    int32_t nz = noise();

    // ---- kick: pitch-swept sine + click
    int32_t kick = 0;
    if (S.kEnv) {
      uint32_t inc = S.noteInc[31] + ((S.noteInc[54] - S.noteInc[31]) >> 15) * S.kPEnv;
      S.kPh += inc;
      kick = (isin(S.kPh) * S.kEnv) >> 15;
      kick += (nz * S.kClick) >> 17;
      S.kEnv = envStep(S.kEnv, 65524);   // ~170 ms
      S.kPEnv = envStep(S.kPEnv, 65400); // fast sweep
      S.kClick = envStep(S.kClick, 64800);
    }
    int32_t duck = 32767 - ((S.kEnv * 24000) >> 15);   // sidechain pump

    // ---- snare: noise + 185 Hz body
    int32_t snare = 0;
    if (S.sEnv) {
      S.sPh += S.noteInc[54];
      snare = ((nz * S.sEnv) >> 15) * 3 / 4 + ((isin(S.sPh) * S.sTEnv) >> 16);
      S.sEnv = envStep(S.sEnv, 65517);
      S.sTEnv = envStep(S.sTEnv, 65480);
    }
    // ---- hats + crash: high-passed noise
    S.hLp += (nz - S.hLp) >> 1;
    int32_t hp = nz - S.hLp;
    int32_t hat = (hp * S.hEnv) >> 17;
    S.hEnv = envStep(S.hEnv, S.hDec);
    int32_t crash = (hp * S.cEnv) >> 17;
    S.cEnv = envStep(S.cEnv, 65532);

    // ---- riser: filtered noise sweeping up
    int32_t riser = 0;
    if ((S.rowCtr & 63) == 0) {
      float r = riserAt(S.songPos / (float)(ROW_SAMPLES * BAR_ROWS));
      S.rAmt = r < 0 ? 0 : (int32_t)(r * r * 30000);
    }
    if (S.rAmt) {
      int32_t k = 600 + S.rAmt;
      S.rLp += ((nz - S.rLp) * (k >> 4)) >> 11;
      riser = (S.rLp * S.rAmt) >> 17;
    }

    // ---- acid bass: saw -> resonant SVF
    int32_t bass = 0;
    if (S.bEnv) {
      S.bPh += S.bInc;
      int32_t in = (int32_t)(S.bPh >> 19) - 4096;
      int32_t f = 900 + ((S.bFenv * S.bFamt) >> 15);
      if (f > 22000) f = 22000;
      S.bLow += (f * S.bBand) >> 15;
      int32_t hi = in - S.bLow - ((9000 * S.bBand) >> 15);
      S.bBand += (f * hi) >> 15;
      S.bBand = clamp32(S.bBand, -60000, 60000);
      S.bLow = clamp32(S.bLow, -60000, 60000);
      bass = (S.bLow * S.bEnv) >> 15;
      S.bEnv = envStep(S.bEnv, S.bDec);
      S.bFenv = envStep(S.bFenv, 65500);
    }

    // ---- lead: PWM pulse, glide, delayed vibrato
    int32_t lead = 0;
    if (S.lEnv || S.lGate) {
      S.lInc += (int32_t)(S.lTarget - S.lInc) >> 9;
      S.lLfo += 94000;            // ~0.7 Hz PWM
      S.lVib += 738000;           // ~5.5 Hz vibrato
      uint32_t inc = S.lInc;
      if (S.lHeld > 5000) inc += ((int32_t)(S.lInc >> 12) * isin(S.lVib)) >> 10;
      S.lHeld++;
      S.lPh += inc;
      uint32_t duty = 0x80000000u + (uint32_t)(isin(S.lLfo) * 40000);
      int32_t x = S.lPh < duty ? 4096 : -4096;
      S.lLp += (x - S.lLp) >> 2;
      if (S.lAtk) { S.lEnv += 600; if (S.lEnv >= 32767) { S.lEnv = 32767; S.lAtk = 0; } }
      else {
        int32_t sus = S.lGate ? 18000 : 0;
        S.lEnv = sus + (int32_t)(((int64_t)(S.lEnv - sus) * (S.lGate ? 65520 : 65480)) >> 16);
        if (!S.lGate && S.lEnv < 8) S.lEnv = 0;
      }
      lead = (S.lLp * S.lEnv) >> 15;
    }

    // ---- arp pluck: square
    int32_t arp = 0;
    if (S.aEnv) {
      S.aPh += S.aInc;
      arp = ((S.aPh >> 31) ? 2048 : -2048) * S.aEnv >> 15;
      arp = (arp * S.arpVol) >> 12;
      S.aEnv = envStep(S.aEnv, 65490);
    }

    // ---- pad: 6 detuned saws -> 2-pole LP
    int32_t pad = 0;
    S.pVol += (S.pVolT - S.pVol) >> 13;
    if (S.pVol > 4) {
      int32_t sum = 0;
      for (int v = 0; v < 6; v++) { S.pPh[v] += S.pInc[v]; sum += (int32_t)(S.pPh[v] >> 21) - 1024; }
      int32_t cut = S.songPos < SONG_SAMPLES / 16 ? 300 + (int32_t)(S.songPos / 160) : 3600;
      S.pLp += ((sum - S.pLp) * cut) >> 15;
      S.pLp2 += ((S.pLp - S.pLp2) * cut) >> 15;
      pad = (S.pLp2 * S.pVol) >> 13;
      pad = (pad * duck) >> 15;
    }

    // ---- echo on lead + arp
    int32_t wet = S.dly[S.dIdx];
    int32_t din = lead + (arp >> 1) + ((wet * 15000) >> 15);
    S.dly[S.dIdx] = (int16_t)clamp32(din, -32767, 32767);
    if (++S.dIdx >= DLY_LEN) S.dIdx = 0;

    // ---- mix (gains in Q8)
    bass = (bass * duck) >> 15;
    wet = (wet * S.dWet) >> 15;
#ifdef SYNTH_STATS
    SYNTH_STATS(kick, snare, hat, crash, riser, bass, pad, lead, arp, wet);
#endif
    int32_t mix = (kick * G_KICK + snare * G_SNARE + hat * G_HAT + crash * G_CRASH + riser * G_RISER
                 + bass * G_BASS + pad * G_PAD + lead * G_LEAD + arp * G_ARP + wet * G_WET) >> 8;
    // soft clip
    if (mix > 20000) mix = 20000 + ((mix - 20000) >> 2);
    if (mix < -20000) mix = -20000 + ((mix + 20000) >> 2);
    out[i] = (int16_t)clamp32(mix, -32767, 32767);

    // ---- advance clocks
    S.pos++;
    if (++S.rowCtr >= ROW_SAMPLES) S.rowCtr = 0;
    if (++S.songPos >= SONG_SAMPLES) { S.songPos = 0; S.rowCtr = 0; }
  }
}

// ----------------------------------------------------------------- visual sync
struct Sync {
  uint32_t pos;     // sample position in song
  int row, bar, step;
  float rows;       // fractional row position
  float bars;       // fractional bar position
  float t;          // seconds
  float kick, snare, hat, crash, lead;   // 0..1 envelopes
  int kickSince, snareSince;             // samples since last hit (large if none)
  int chord, leadNote, beatIndex;        // beatIndex = kicks counted
  float leadAge;                          // s since last lead note-on
};

template <typename F>
static int samplesSince(uint32_t pos, F pred, int maxBack) {
  int row = pos / ROW_SAMPLES;
  for (int r = row; r >= 0 && r > row - maxBack; r--)
    if (pred(r)) return (int)(pos - (uint32_t)r * ROW_SAMPLES);
  return 1 << 24;
}
static bool kickPred(int r) { return kickAt(r); }
static bool snarePred(int r) { return snareAt(r); }
static bool hatPred(int r) { return hatAt(r) != 0; }
static bool crashPred(int r) { return crashAt(r); }
static bool leadPred(int r) { return leadAt(r) > 1; }

static void syncAt(uint32_t pos, Sync& s) {
  pos %= SONG_SAMPLES;
  s.pos = pos;
  s.row = pos / ROW_SAMPLES;
  s.bar = s.row >> 4;
  s.step = s.row & 15;
  s.rows = pos / (float)ROW_SAMPLES;
  s.bars = s.rows / 16.0f;
  s.t = pos / (float)SR;
  s.kickSince = samplesSince(pos, kickPred, 64);
  s.snareSince = samplesSince(pos, snarePred, 32);
  s.kick = expf(-s.kickSince / (0.11f * SR));
  s.snare = expf(-s.snareSince / (0.09f * SR));
  s.hat = expf(-samplesSince(pos, hatPred, 16) / (0.05f * SR));
  s.crash = expf(-samplesSince(pos, crashPred, 128) / (0.25f * SR));
  int ls = samplesSince(pos, leadPred, 32);
  s.leadAge = ls / (float)SR;
  s.lead = leadOn(s.bar) ? expf(-s.leadAge * 5) : 0;
  s.leadNote = 0;
  for (int r = s.row; r >= 0 && r > s.row - 32; r--) { int n = leadAt(r); if (n > 1) { s.leadNote = n; break; } }
  s.chord = chordAt(s.bar);
  s.beatIndex = s.row >> 2;
}
