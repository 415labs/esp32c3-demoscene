/*
  415 SIGNAL - a 2-minute demo for ESP32-C3 + 0.42" 72x40 OLED + 1-pin audio
  ---------------------------------------------------------------------------
  Build  : PlatformIO, `pio run -t upload` (see platformio.ini; uses the
           pioarduino platform = Arduino-ESP32 core 3.x)
  Display: built-in SSD1306 72x40, I2C SDA=GPIO5, SCL=GPIO6
  Audio  : speaker / headphones between AUDIO_PIN and GND.
           Recommended: GPIO -> 100R -> 10uF (+ toward GPIO) -> speaker -> GND
           (see README.md for the 3-part filter / transistor options)

  Audio is a 32 kHz software synth streamed by DMA through the I2S peripheral
  in PDM "DAC" mode: the pin carries a ~6 MHz 1-bit pulse-density signal that the
  speaker (plus optional RC) low-passes back into sound. No timers, no jitter.

  Sync: the audio DMA is the master clock. The visuals compute the exact
  sample currently leaving the pin and render the frame for that instant.
*/
#include <Arduino.h>
#include <Wire.h>
#include "esp_timer.h"

#if !defined(ESP_ARDUINO_VERSION_MAJOR) || ESP_ARDUINO_VERSION_MAJOR < 3
#error "Needs Arduino-ESP32 core 3.x: use the pioarduino platform from platformio.ini"
#endif
#include "driver/i2s_pdm.h"

#include "gfx.h"
#include "song.h"
#include "effects.h"

// ------------------------------------------------------------------ config
#define AUDIO_PIN 3          // any free GPIO: 0,1,2,3,4,7,10,20,21 (avoid 5/6 = OLED, 8 = LED, 9 = BOOT)
#define PDM_CLK_SPARE_PIN 10 // only used if the driver refuses "no clock pin"
#define PIN_SDA 5
#define PIN_SCL 6
#define OLED_ADDR 0x3C
#define OLED_XOFF 28         // the 72 visible columns start at RAM column 28 on these panels
#define I2C_HZ 800000        // SSD1306 is happy at 800 kHz-1 MHz on short traces
#define AUDIO_CHUNK 256
#define DMA_DESC 4
// samples queued between the synth and the speaker (DMA ring)
#define AUDIO_LATENCY (DMA_DESC * AUDIO_CHUNK)

// ------------------------------------------------------------------ audio
static i2s_chan_handle_t txChan;
static portMUX_TYPE clkMux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t g_written = 0;    // samples handed to DMA
static volatile int64_t g_writtenUs = 0;   // when

static void audioTask(void*) {
  static int16_t buf[AUDIO_CHUNK];
  for (;;) {
    synthRender(buf, AUDIO_CHUNK);
    size_t w = 0;
    i2s_channel_write(txChan, buf, sizeof(buf), &w, portMAX_DELAY);
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&clkMux);
    g_written += w / 2;
    g_writtenUs = now;
    portEXIT_CRITICAL(&clkMux);
  }
}

static void audioBegin() {
  i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  cc.dma_desc_num = DMA_DESC;
  cc.dma_frame_num = AUDIO_CHUNK;
  cc.auto_clear = true;
  ESP_ERROR_CHECK(i2s_new_channel(&cc, &txChan, NULL));

#ifdef I2S_PDM_TX_SLOT_DAC_DEFAULT_CONFIG
  // "DAC line mode": a plain pulse-density stream meant for an RC filter / speaker
  i2s_pdm_tx_clk_config_t clk = I2S_PDM_TX_CLK_DAC_DEFAULT_CONFIG(SR);
  i2s_pdm_tx_slot_config_t slot = I2S_PDM_TX_SLOT_DAC_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
#else
  i2s_pdm_tx_clk_config_t clk = I2S_PDM_TX_CLK_DEFAULT_CONFIG(SR);
  i2s_pdm_tx_slot_config_t slot = I2S_PDM_TX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
#endif
  i2s_pdm_tx_config_t pc;
  memset(&pc, 0, sizeof(pc));
  pc.clk_cfg = clk;
  pc.slot_cfg = slot;
  pc.gpio_cfg.clk = I2S_GPIO_UNUSED;        // the speaker doesn't need the PDM clock
  pc.gpio_cfg.dout = (gpio_num_t)AUDIO_PIN;
#if SOC_I2S_PDM_MAX_TX_LINES > 1
  pc.gpio_cfg.dout2 = I2S_GPIO_UNUSED;
#endif
  if (i2s_channel_init_pdm_tx_mode(txChan, &pc) != ESP_OK) {
    // some IDF builds insist on a clock pin: park it on a spare GPIO
    pc.gpio_cfg.clk = (gpio_num_t)PDM_CLK_SPARE_PIN;
    ESP_ERROR_CHECK(i2s_channel_init_pdm_tx_mode(txChan, &pc));
  }
  ESP_ERROR_CHECK(i2s_channel_enable(txChan));
  xTaskCreate(audioTask, "synth", 4096, NULL, configMAX_PRIORITIES - 2, NULL);
}

// the sample that is leaving the pin right now
static uint32_t playPos() {
  portENTER_CRITICAL(&clkMux);
  uint32_t w = g_written; int64_t t = g_writtenUs;
  portEXIT_CRITICAL(&clkMux);
  int64_t dt = esp_timer_get_time() - t;
  uint32_t extra = (uint32_t)(dt * SR / 1000000);
  if (extra > AUDIO_CHUNK) extra = AUDIO_CHUNK;
  uint32_t p = w + extra;
  return p > AUDIO_LATENCY ? p - AUDIO_LATENCY : 0;
}

// ------------------------------------------------------------------ OLED
static void oledCmd(const uint8_t* c, int n) {
  Wire.beginTransmission(OLED_ADDR);
  Wire.write(0x00);
  Wire.write(c, n);
  Wire.endTransmission();
}
static void oledBegin() {
  Wire.begin(PIN_SDA, PIN_SCL, I2C_HZ);
  static const uint8_t init[] = {
    0xAE,             // off
    0xD5, 0x80,       // clock
    0xA8, 0x27,       // multiplex 40
    0xD3, 0x00,       // offset
    0x40,             // start line 0
    0x8D, 0x14,       // charge pump on
    0x20, 0x00,       // horizontal addressing
    0xA1, 0xC8,       // segment remap / COM scan dir
    0xDA, 0x12,       // COM pins
    0x81, 0xCF,       // contrast
    0xD9, 0xF1,       // precharge
    0xDB, 0x20,       // VCOMH
    0xAD, 0x30,       // internal IREF (0.42" panels)
    0xA4, 0xA6, 0xAF  // resume, normal, on
  };
  oledCmd(init, sizeof(init));
  // clear the whole 128x64 RAM: rows 40-63 must be black for the start-line shake trick
  static const uint8_t all[] = {0x21, 0, 127, 0x22, 0, 7};
  oledCmd(all, sizeof(all));
  uint8_t z[33] = {0x40};
  for (int i = 0; i < 1024 / 32; i++) {
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(z, 33);
    Wire.endTransmission();
  }
}
static void oledPush(const DisplayCtl& d) {
  static DisplayCtl last = {0, false, 0xFF};
  uint8_t c[16]; int n = 0;
  if (d.contrast != last.contrast) { c[n++] = 0x81; c[n++] = d.contrast; }
  if (d.invert != last.invert) c[n++] = d.invert ? 0xA7 : 0xA6;
  if (d.startLine != last.startLine) c[n++] = 0x40 | (d.startLine & 63);
  c[n++] = 0x21; c[n++] = OLED_XOFF; c[n++] = OLED_XOFF + SCR_W - 1;
  c[n++] = 0x22; c[n++] = 0; c[n++] = SCR_H / 8 - 1;
  oledCmd(c, n);
  last = d;
  for (int p = 0; p < SCR_H / 8; p++) {   // one page (72 bytes) per transaction
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x40);
    Wire.write(fb + p * SCR_W, SCR_W);
    Wire.endTransmission();
  }
}

// ------------------------------------------------------------------ main
void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(160);
  oledBegin();
  effectsInit();
  synthInit();
  audioBegin();
}

void loop() {
  static uint32_t frames = 0, lastReport = 0;
  Sync s;
  syncAt(playPos(), s);
  DisplayCtl d;
  renderFrame(s, d);
  oledPush(d);
  frames++;
  uint32_t ms = millis();
  if (ms - lastReport >= 5000) {
    Serial.printf("fps %.1f  bar %d\n", frames * 1000.0f / (ms - lastReport), s.bar);
    frames = 0; lastReport = ms;
  }
}
