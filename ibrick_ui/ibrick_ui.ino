// ESP32-C6-LCD-1.47 에 2키 스트림덱 UI 를 그리고 물리 키 입력을 처리하는 샘플 스케치

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ── 보드 핀맵 (Waveshare ESP32-C6-LCD-1.47) ─────────────────────────────────
static constexpr int PIN_LCD_MOSI = 6;
static constexpr int PIN_LCD_SCLK = 7;   // TF 카드와 SPI 버스 공유
static constexpr int PIN_LCD_CS   = 14;
static constexpr int PIN_LCD_DC   = 15;
static constexpr int PIN_LCD_RST  = 21;
static constexpr int PIN_LCD_BL   = 22;
static constexpr int PIN_RGB_LED  = 8;   // WS2812

// 172x320 패널은 ST7789 메모리(240x320) 가운데에 물려 있어 열 오프셋이 필요하다.
// 부팅 직후 4초간 보정 패턴을 띄우니 네 변이 다 보이는지 확인하고 이 값을 맞춘다.
static constexpr int OFFSET_X = 34;
static constexpr int OFFSET_Y = 0;

// 0/2 = 세로 172x320, 1/3 = 가로 320x172.
// 위아래(또는 좌우)가 뒤집혀 보이면 1 <-> 3 을 바꾼다.
static constexpr int ROTATION = 1;

// 사용자 키 2개. GPIO2/3 은 스트래핑 핀이 아니고 LCD·SD·USB 와도 겹치지 않는다.
// 스위치를 달기 전에는 점퍼선으로 GND 에 잠깐 대면 눌린 것으로 잡힌다.
static constexpr int PIN_KEY1 = 2;
static constexpr int PIN_KEY2 = 3;

// 보드의 BOOT 버튼. 스트래핑 핀이라 "리셋되는 순간" 눌려 있으면 다운로드 모드로 들어가지만,
// 부팅이 끝난 뒤에는 평범한 입력이라 버튼으로 읽어도 된다. 업로드 중에만 누르지 않으면 된다.
// 스위치를 안 달고도 바로 눌러 볼 수 있게 KEY1 에 붙여 둔다.
static constexpr int PIN_BOOT = 9;
static constexpr int BOOT_ALIAS = 0;   // 0 = KEY1 에 붙임, 1 = KEY2, -1 = 안 씀

// ── 디스플레이 ──────────────────────────────────────────────────────────────
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI      _bus;
  lgfx::Light_PWM    _light;

 public:
  LGFX() {
    {
      auto c = _bus.config();
      c.spi_host   = SPI2_HOST;
      c.spi_mode   = 0;
      c.freq_write = 40000000;
      c.freq_read  = 16000000;
      c.spi_3wire  = true;
      c.use_lock   = true;
      c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk = PIN_LCD_SCLK;
      c.pin_mosi = PIN_LCD_MOSI;
      c.pin_miso = -1;
      c.pin_dc   = PIN_LCD_DC;
      _bus.config(c);
      _panel.setBus(&_bus);
    }
    {
      auto c = _panel.config();
      c.pin_cs   = PIN_LCD_CS;
      c.pin_rst  = PIN_LCD_RST;
      c.pin_busy = -1;
      c.panel_width  = 172;
      c.panel_height = 320;
      // ST7789 의 프레임 메모리는 240x320 이고 172 폭 패널이 그 가운데에 물려 있다.
      // LovyanGFX 는 회전 시 이 값으로 colstart/rowstart 를 다시 계산하므로
      // (기본값 240x240 그대로 두면) 가로 회전에서 오프셋이 어긋난다.
      c.memory_width  = 240;
      c.memory_height = 320;
      c.offset_x = OFFSET_X;
      c.offset_y = OFFSET_Y;
      c.offset_rotation = 0;
      c.readable   = false;
      c.invert     = true;   // IPS 라 반전 필요
      c.rgb_order  = false;
      c.dlen_16bit = false;
      c.bus_shared = true;   // TF 카드와 버스 공유
      _panel.config(c);
    }
    {
      auto c = _light.config();
      c.pin_bl = PIN_LCD_BL;
      c.invert = false;
      c.freq   = 12000;
      c.pwm_channel = 0;
      _light.config(c);
      _panel.setLight(&_light);
    }
    setPanel(&_panel);
  }
};

static LGFX lcd;

// ── 레이아웃 (320 x 172 가로) ───────────────────────────────────────────────
// 세로 172 안에서 헤더를 44px 로 잡으면 화면의 26% 를 먹으므로 28px 로 줄였다.
static constexpr int SCR_W = 320, SCR_H = 172;
static constexpr int PAD = 8, HEADER_H = 28;
static constexpr int KEY_W = (SCR_W - PAD * 3) / 2;           // 148
static constexpr int KEY_H = SCR_H - HEADER_H - PAD * 2;      // 128
static constexpr int KEY_Y = HEADER_H + PAD;                  // 36
static constexpr int KEY_X[2] = { PAD, PAD * 2 + KEY_W };     // 8, 164

static constexpr uint16_t C_BG     = 0x18E3;  // 짙은 회색
static constexpr uint16_t C_FACE   = 0x2986;
static constexpr uint16_t C_EDGE   = 0x4A69;
static constexpr uint16_t C_TEXT   = 0xE71C;
static constexpr uint16_t C_DIM    = 0x8410;
static constexpr uint16_t C_LIVE   = 0x0651;  // 청록 강조
static constexpr uint16_t C_PRESS  = 0x04B2;
// 키 배경 캐릭터. 라벨을 가리지 않게 눌리지 않았을 때는 차분한 회색으로 깔린다.
static constexpr uint16_t C_ART       = 0x4208;
static constexpr uint16_t C_ART_HI    = 0x738E;
static constexpr uint16_t C_ART_ON    = 0x1470;
static constexpr uint16_t C_ART_ON_HI = 0x3E57;
static constexpr uint16_t C_BLUSH     = 0xBBCF;

struct KeyDef {
  const char* label;
  const char* sub;
  bool        art;    // 배경 캐릭터를 그릴지. 그리면 라벨이 아래로 내려간다
};
static const KeyDef KEYS[2] = {
  { "COPY",  "Cmd+C", true  },
  { "MUTE",  "F10",   false },
};

// ── 키 입력 (디바운스 + 짧게/길게) ──────────────────────────────────────────
static constexpr uint32_t DEBOUNCE_MS   = 25;
static constexpr uint32_t LONGPRESS_MS  = 600;

struct KeyState {
  uint8_t  pin;
  int8_t   altPin    = -1;     // 같은 키를 누르는 두 번째 입력 (BOOT). -1 = 없음
  bool     raw       = false;  // true = 눌림 (풀업이라 LOW 가 눌림)
  bool     stable    = false;
  uint32_t tChange   = 0;
  uint32_t tPress    = 0;
  bool     longFired = false;
};
static KeyState keys[2];

// ── 그리기 ──────────────────────────────────────────────────────────────────
static void drawHeader() {
  lcd.fillRect(0, 0, SCR_W, HEADER_H, C_BG);
  lcd.drawFastHLine(0, HEADER_H - 1, SCR_W, C_EDGE);
  lcd.setFont(&fonts::Font2);
  lcd.setTextDatum(middle_left);
  lcd.setTextColor(C_LIVE, C_BG);
  lcd.drawString("i-brick deck", PAD, HEADER_H / 2);
  lcd.setFont(&fonts::Font0);
  lcd.setTextDatum(middle_right);
  lcd.setTextColor(C_DIM, C_BG);
  lcd.drawString("2-key sample", SCR_W - PAD, HEADER_H / 2);
}

// i-brick 마스코트. 제품 이름대로 머리 위에 브릭 스터드가 두 개 있다.
// 비트맵이 아니라 도형으로 그려서 색만 바꾸면 눌림 상태를 같이 표현할 수 있다.
static void drawMascot(int cx, int cy, bool pressed, uint16_t bg) {
  const uint16_t body = pressed ? C_ART_ON    : C_ART;
  const uint16_t hi   = pressed ? C_ART_ON_HI : C_ART_HI;

  lcd.fillRoundRect(cx - 20, cy - 40,  15,  8, 3, body);   // 브릭 스터드
  lcd.fillRoundRect(cx +  5, cy - 40,  15,  8, 3, body);
  lcd.fillRoundRect(cx - 44, cy - 18,  10, 26, 5, body);   // 팔
  lcd.fillRoundRect(cx + 34, cy - 18,  10, 26, 5, body);
  lcd.fillRoundRect(cx - 32, cy - 33,  64, 54, 12, body);  // 머리·몸통
  lcd.fillRoundRect(cx - 24, cy + 19,  18, 10, 5, body);   // 발
  lcd.fillRoundRect(cx +  6, cy + 19,  18, 10, 5, body);

  lcd.fillCircle(cx - 13, cy - 14, 8, hi);                 // 눈
  lcd.fillCircle(cx + 13, cy - 14, 8, hi);
  lcd.fillCircle(cx - 12, cy - 13, 4, bg);                 // 눈동자는 바탕색으로 뚫는다
  lcd.fillCircle(cx + 14, cy - 13, 4, bg);
  lcd.fillEllipse(cx - 25, cy + 1, 5, 3, C_BLUSH);         // 볼
  lcd.fillEllipse(cx + 25, cy + 1, 5, 3, C_BLUSH);
  lcd.fillEllipseArc(cx, cy + 1, 7, 10, 5, 8, 25, 155, hi); // 입

  lcd.drawFastHLine(cx - 8, cy - 40, 16, body);            // 스터드 사이 연결
}

// 키 하나를 그린다. 나중에 도형 대신 아이콘 비트맵을 넣는 자리가 여기다.
static void drawKey(int i, bool pressed) {
  const int x = KEY_X[i];
  const int y = KEY_Y;
  const uint16_t face = pressed ? C_PRESS : C_FACE;
  const uint16_t edge = pressed ? C_LIVE  : C_EDGE;

  lcd.fillRoundRect(x, y, KEY_W, KEY_H, 10, face);
  lcd.drawRoundRect(x, y, KEY_W, KEY_H, 10, edge);
  if (pressed) lcd.drawRoundRect(x + 1, y + 1, KEY_W - 2, KEY_H - 2, 9, edge);

  int labelY = y + KEY_H / 2 - 14;
  int subY   = y + KEY_H / 2 + 18;
  if (KEYS[i].art) {
    drawMascot(x + KEY_W / 2, y + 48, pressed, face);
    labelY = y + KEY_H - 36;   // 캐릭터가 위를 쓰므로 라벨은 아래 띠로
    subY   = y + KEY_H - 15;
  }

  lcd.setTextDatum(middle_center);
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(pressed ? C_LIVE : C_TEXT);   // 배경 투명 — 캐릭터를 지우지 않는다
  lcd.drawString(KEYS[i].label, x + KEY_W / 2, labelY);

  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(C_DIM);
  lcd.drawString(KEYS[i].sub, x + KEY_W / 2, subY);

  lcd.setTextDatum(top_left);
  lcd.setFont(&fonts::Font0);
  lcd.setTextColor(C_DIM);
  lcd.drawString(keys[i].altPin >= 0 ? (i == 0 ? "GPIO2 + BOOT" : "GPIO3 + BOOT")
                                     : (i == 0 ? "GPIO2" : "GPIO3"),
                 x + 8, y + 8);
}

// 오프셋 보정 패턴. 네 변의 흰 테두리와 네 모서리 사각형이 모두 온전히 보여야 한다.
static void drawCalibration() {
  lcd.fillScreen(TFT_BLACK);
  lcd.drawRect(0, 0, SCR_W, SCR_H, TFT_WHITE);
  lcd.drawLine(0, 0, SCR_W - 1, SCR_H - 1, TFT_RED);
  lcd.drawLine(SCR_W - 1, 0, 0, SCR_H - 1, TFT_BLUE);
  lcd.fillRect(0, 0, 10, 10, TFT_RED);                       // 좌상
  lcd.fillRect(SCR_W - 10, 0, 10, 10, TFT_GREEN);            // 우상
  lcd.fillRect(0, SCR_H - 10, 10, 10, TFT_BLUE);             // 좌하
  lcd.fillRect(SCR_W - 10, SCR_H - 10, 10, 10, TFT_YELLOW);  // 우하

  lcd.setTextDatum(middle_center);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.setFont(&fonts::Font2);
  lcd.drawString("CALIBRATION", SCR_W / 2, SCR_H / 2 - 30);
  char buf[32];
  snprintf(buf, sizeof(buf), "%dx%d", (int)lcd.width(), (int)lcd.height());
  lcd.drawString(buf, SCR_W / 2, SCR_H / 2);
  snprintf(buf, sizeof(buf), "rot %d  off %d,%d", ROTATION, OFFSET_X, OFFSET_Y);
  lcd.drawString(buf, SCR_W / 2, SCR_H / 2 + 22);
}

static void drawUI() {
  lcd.fillScreen(C_BG);
  drawHeader();
  drawKey(0, false);
  drawKey(1, false);
}

// ── 이벤트 처리 ─────────────────────────────────────────────────────────────
static void onPress(int i)     { Serial.printf("[KEY%d] press\n", i + 1);
                                 neopixelWrite(PIN_RGB_LED, i == 0 ? 0 : 12, i == 0 ? 12 : 0, 12); }
static void onShort(int i)     { Serial.printf("[KEY%d] short  -> %s (%s)\n", i + 1, KEYS[i].label, KEYS[i].sub); }
static void onLong(int i)      { Serial.printf("[KEY%d] LONG   -> 레이어 전환 자리\n", i + 1); }
static void onRelease(int i)   { Serial.printf("[KEY%d] release\n", i + 1);
                                 neopixelWrite(PIN_RGB_LED, 0, 0, 0); }

static void pollKeys() {
  const uint32_t now = millis();
  for (int i = 0; i < 2; ++i) {
    KeyState& k = keys[i];
    const bool raw = (digitalRead(k.pin) == LOW)
                  || (k.altPin >= 0 && digitalRead(k.altPin) == LOW);
    if (raw != k.raw) { k.raw = raw; k.tChange = now; }

    if (raw != k.stable && (now - k.tChange) >= DEBOUNCE_MS) {
      k.stable = raw;
      if (raw) {
        k.tPress = now; k.longFired = false;
        drawKey(i, true); onPress(i);
      } else {
        drawKey(i, false);
        if (!k.longFired) onShort(i);
        onRelease(i);
      }
    }
    if (k.stable && !k.longFired && (now - k.tPress) >= LONGPRESS_MS) {
      k.longFired = true; onLong(i);
    }
  }
}

// ── 진입점 ──────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // LCD 가 안 켜져도 펌웨어가 도는지 알 수 있게 먼저 LED 를 흰색으로
  neopixelWrite(PIN_RGB_LED, 16, 16, 16);

  pinMode(PIN_KEY1, INPUT_PULLUP);
  pinMode(PIN_KEY2, INPUT_PULLUP);
  keys[0].pin = PIN_KEY1;
  keys[1].pin = PIN_KEY2;
  if (BOOT_ALIAS >= 0) {
    pinMode(PIN_BOOT, INPUT_PULLUP);
    keys[BOOT_ALIAS].altPin = PIN_BOOT;
  }

  lcd.init();
  lcd.setRotation(ROTATION);
  lcd.setBrightness(200);   // 백라이트를 명시적으로 켠다

  Serial.println();
  Serial.println("=== i-brick deck / 2-key sample ===");
  Serial.printf("LCD %dx%d  rotation=%d  offset(%d,%d)\n",
                (int)lcd.width(), (int)lcd.height(), ROTATION, OFFSET_X, OFFSET_Y);
  Serial.printf("KEY1=GPIO%d  KEY2=GPIO%d  (INPUT_PULLUP, GND 에 닿으면 눌림)\n", PIN_KEY1, PIN_KEY2);
  if (BOOT_ALIAS >= 0)
    Serial.printf("BOOT(GPIO%d) -> KEY%d  (업로드 중에는 누르지 말 것)\n", PIN_BOOT, BOOT_ALIAS + 1);
  Serial.println("보정 패턴 4초 -> UI");

  drawCalibration();
  delay(4000);
  drawUI();

  neopixelWrite(PIN_RGB_LED, 0, 0, 0);
}

void loop() {
  pollKeys();
  delay(2);
}
