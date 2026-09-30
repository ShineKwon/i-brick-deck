// ESP32-C6-LCD-1.47 에 2키 스트림덱 UI 를 그리고 물리 키 입력을 처리하는 샘플 스케치

static constexpr char FW_VERSION[] = "0.1";   // VERSION 파일과 같이 올린다

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "bg_key.h"    // BACK — 토끼 후드 소녀   (tools/make_char.py girl)
#include "bg_key2.h"   // MUTE — 고양이 후드 "쉿" (tools/make_char.py cat)

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

// 부팅하자마자 색·해상도 테스트 4장을 보려면 true 로 바꿔 다시 빌드한다.
// 평소에는 바로 UI 로 들어가야 스트림덱으로 쓸 수 있어서 기본이 false 다.
static constexpr bool START_IN_TEST = false;

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

// HID 는 문자가 아니라 usage code 를 보내고 해석은 호스트가 한다.
// 그래서 "크롬 뒤로가기"가 macOS 는 Cmd+<-, Windows 는 Alt+<- 로 갈린다.
// 반면 음소거는 Consumer Control usage 0xE2 라 두 OS 가 똑같다 —
// 프로필이 필요한 키와 아닌 키가 여기서 갈린다.
enum HostOS : uint8_t { HOST_MAC = 0, HOST_WIN = 1 };
static HostOS hostOS = HOST_MAC;

static constexpr uint8_t MOD_LCTRL = 0x01, MOD_LALT = 0x04, MOD_LGUI = 0x08;
static constexpr uint8_t KEY_LEFT  = 0x50;
static constexpr uint16_t CC_MUTE  = 0x00E2;

struct KeyDef {
  const char*     label;
  const char*     subMac;   // 호스트 프로필에 따라 다르게 보여 준다
  const char*     subWin;
  const uint16_t* art;      // 배경 이미지. nullptr 이면 단색 면을 그린다
};
static const KeyDef KEYS[2] = {
  { "BACK",  "Cmd+<-", "Alt+<-", BGKEY_DATA  },
  { "MUTE",  "Mute",   "Mute",   BGKEY2_DATA },
};
static const char* subOf(int i) {
  return hostOS == HOST_MAC ? KEYS[i].subMac : KEYS[i].subWin;
}

// ── BLE HID ─────────────────────────────────────────────────────────────────
// 리포트 두 개를 쓴다. 1번은 일반 키보드, 2번은 Consumer Control(미디어 키).
// 음소거 같은 미디어 키는 키보드 리포트로는 못 보낸다.
static const uint8_t REPORT_MAP[] = {
  0x05, 0x01,        // Usage Page (Generic Desktop)
  0x09, 0x06,        //   Usage (Keyboard)
  0xA1, 0x01,        //   Collection (Application)
  0x85, 0x01,        //     Report ID (1)
  0x05, 0x07,        //     Usage Page (Keyboard)
  0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01,
  0x75, 0x01, 0x95, 0x08, 0x81, 0x02,          // 모디파이어 8bit
  0x95, 0x01, 0x75, 0x08, 0x81, 0x03,          // 예약 1byte
  0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
  0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,  // 키코드 6byte
  0xC0,
  0x05, 0x0C,        // Usage Page (Consumer)
  0x09, 0x01,        //   Usage (Consumer Control)
  0xA1, 0x01,        //   Collection (Application)
  0x85, 0x02,        //     Report ID (2)
  0x15, 0x00, 0x26, 0xFF, 0x03,                // Logical 0 ~ 0x3FF
  0x19, 0x00, 0x2A, 0xFF, 0x03,                // Usage 0 ~ 0x3FF
  0x75, 0x10, 0x95, 0x01, 0x81, 0x00,          // 16bit usage 1개
  0xC0
};

static BLEHIDDevice*     hid    = nullptr;
static BLECharacteristic* kbIn  = nullptr;   // 리포트 1 — 키보드
static BLECharacteristic* ccIn  = nullptr;   // 리포트 2 — 미디어
static volatile bool bleConnected = false;
static volatile bool uiDirty      = false;   // 콜백에서 그리지 않고 loop 에서 다시 그린다

class SrvCB : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    bleConnected = true;  uiDirty = true;
  }
  void onDisconnect(BLEServer* srv) override {
    bleConnected = false; uiDirty = true;
    srv->startAdvertising();                 // 끊기면 다시 광고
  }
};

static void bleBegin() {
  BLEDevice::init("i-brick deck");
  BLEServer* srv = BLEDevice::createServer();
  srv->setCallbacks(new SrvCB());

  hid  = new BLEHIDDevice(srv);
  kbIn = hid->inputReport(1);
  ccIn = hid->inputReport(2);
  hid->manufacturer()->setValue("i-brick");
  hid->pnp(0x02, 0xE502, 0xA111, 0x0210);
  hid->hidInfo(0x00, 0x01);
  hid->reportMap((uint8_t*)REPORT_MAP, sizeof(REPORT_MAP));
  hid->startServices();
  hid->setBatteryLevel(100);

  // HID over GATT 는 암호화를 요구한다. 입력 수단이 없으니 Just Works 본딩.
  BLESecurity* sec = new BLESecurity();
  sec->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
  sec->setCapability(ESP_IO_CAP_NONE);
  sec->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  BLEAdvertising* adv = srv->getAdvertising();
  adv->setAppearance(HID_KEYBOARD);
  adv->addServiceUUID(hid->hidService()->getUUID());
  adv->start();
}

// 아래 두 함수는 delay 를 쓰므로 loop 문맥에서만 부른다. BLE 콜백에서 부르면 안 된다.
static void sendKey(uint8_t mod, uint8_t key) {
  if (!bleConnected) { Serial.println("  (BLE 미연결 — 전송 안 함)"); return; }
  uint8_t down[8] = { mod, 0, key, 0, 0, 0, 0, 0 };
  uint8_t up[8]   = { 0 };
  kbIn->setValue(down, 8); kbIn->notify();
  delay(12);
  kbIn->setValue(up, 8);   kbIn->notify();
}

static void sendConsumer(uint16_t usage) {
  if (!bleConnected) { Serial.println("  (BLE 미연결 — 전송 안 함)"); return; }
  uint8_t down[2] = { (uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8) };
  uint8_t up[2]   = { 0, 0 };
  ccIn->setValue(down, 2); ccIn->notify();
  delay(12);
  ccIn->setValue(up, 2);   ccIn->notify();
}

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
  lcd.setTextColor(C_DIM, C_BG);
  lcd.drawString("v" FW_VERSION, PAD + 80, HEADER_H / 2);
  lcd.setFont(&fonts::Font0);
  lcd.setTextDatum(middle_right);
  lcd.setTextColor(C_DIM, C_BG);
  lcd.drawString(hostOS == HOST_MAC ? "MAC" : "WIN", SCR_W - PAD, HEADER_H / 2);
  lcd.drawString("BLE", SCR_W - PAD - 30, HEADER_H / 2);
  lcd.fillCircle(SCR_W - PAD - 38, HEADER_H / 2, 3,
                 bleConnected ? C_LIVE : C_DIM);
}

// 키 하나를 그린다. 나중에 도형 대신 아이콘 비트맵을 넣는 자리가 여기다.
static void drawKey(int i, bool pressed) {
  const int x = KEY_X[i];
  const int y = KEY_Y;
  const uint16_t edge = pressed ? C_LIVE : C_EDGE;
  int labelY = y + KEY_H / 2 - 14;
  int subY   = y + KEY_H / 2 + 18;

  if (KEYS[i].art) {
    // 이미지는 키 크기와 같고 모서리가 배경색으로 미리 마스킹돼 있다.
    // rgb565_t 로 넘기면 바이트 순서를 추측할 필요가 없다.
    lcd.pushImage(x, y, BGKEY_W, BGKEY_H, (const lgfx::rgb565_t*)KEYS[i].art);
    labelY = y + KEY_H - 36;   // 그림이 위를 쓰므로 라벨은 아래 띠로
    subY   = y + KEY_H - 15;
  } else {
    lcd.fillRoundRect(x, y, KEY_W, KEY_H, 10, pressed ? C_PRESS : C_FACE);
  }

  // 테두리는 그림 위에 — 눌림 표시는 이 이중 테두리와 라벨 색이 맡는다
  lcd.drawRoundRect(x, y, KEY_W, KEY_H, 10, edge);
  if (pressed) lcd.drawRoundRect(x + 1, y + 1, KEY_W - 2, KEY_H - 2, 9, edge);

  lcd.setTextDatum(middle_center);
  lcd.setFont(&fonts::Font4);
  lcd.setTextColor(pressed ? C_LIVE : C_TEXT);   // 배경 투명 — 그림을 지우지 않는다
  lcd.drawString(KEYS[i].label, x + KEY_W / 2, labelY);

  lcd.setFont(&fonts::Font2);
  lcd.setTextColor(KEYS[i].art ? C_TEXT : C_DIM);
  lcd.drawString(subOf(i), x + KEY_W / 2, subY);

  lcd.setTextDatum(top_left);
  lcd.setFont(&fonts::Font0);
  lcd.setTextColor(C_DIM);
  lcd.drawString(keys[i].altPin >= 0 ? (i == 0 ? "GPIO2 + BOOT" : "GPIO3 + BOOT")
                                     : (i == 0 ? "GPIO2" : "GPIO3"),
                 x + 8, y + 8);
}

// ── 색·해상도 확인용 테스트 화면 ────────────────────────────────────────────
// 화면 캡션은 영문이다. LovyanGFX 의 한글 폰트(efontKR)는 소스만 17MB 라
// 플래시에 올릴 수 없다. 한글 설명은 시리얼로 내보낸다.
static constexpr int TEST_PAGES = 4;
static constexpr int CAP_H = 16;                  // 아래 캡션 띠
static constexpr int AREA_H = SCR_H - CAP_H;      // 156
static int  testPage = 0;
static bool testMode = true;

static void testCaption(const char* s) {
  lcd.fillRect(0, AREA_H, SCR_W, CAP_H, TFT_BLACK);
  lcd.setFont(&fonts::Font0);
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextDatum(middle_left);
  lcd.drawString(s, 4, AREA_H + CAP_H / 2);
  char b[8];
  snprintf(b, sizeof(b), "%d/%d", testPage + 1, TEST_PAGES);
  lcd.setTextDatum(middle_right);
  lcd.drawString(b, SCR_W - 4, AREA_H + CAP_H / 2);
}

// 1 — 컬러바 8개와 16단 그레이. 바 순서가 틀리면 rgb_order 가 뒤집힌 것이다.
static void testColorBars() {
  static const uint8_t BAR[8][3] = {
    {255,255,255},{255,255,0},{0,255,255},{0,255,0},
    {255,0,255},{255,0,0},{0,0,255},{0,0,0}
  };
  const int bw = SCR_W / 8, top = 90;
  for (int i = 0; i < 8; ++i)
    lcd.fillRect(i * bw, 0, bw, top, lcd.color565(BAR[i][0], BAR[i][1], BAR[i][2]));
  const int gw = SCR_W / 16;
  for (int i = 0; i < 16; ++i) {
    const uint8_t v = i * 17;
    lcd.fillRect(i * gw, top, gw, AREA_H - top, lcd.color565(v, v, v));
  }
  testCaption("BARS W Y C G M R B K / 16 GRAYS");
}

// 2 — 채널별 그라데이션. RGB565 는 R5 G6 B5 라 초록만 계단이 두 배로 촘촘해야 한다.
static void testRamps() {
  for (int x = 0; x < SCR_W; ++x) {
    const uint8_t v = (uint32_t)x * 255 / (SCR_W - 1);
    const int h = AREA_H / 3;
    lcd.drawFastVLine(x, 0,       h,            lcd.color565(v, 0, 0));
    lcd.drawFastVLine(x, h,       h,            lcd.color565(0, v, 0));
    lcd.drawFastVLine(x, h * 2,   AREA_H - h*2, lcd.color565(0, 0, v));
  }
  testCaption("RAMPS R/G/B - GREEN=64 steps, R,B=32");
}

// 3 — 1픽셀 패턴 세 구역. 셋 다 균일한 회색으로 보여야 픽셀이 안 빠진 것이다.
static void testResolution() {
  const int z = SCR_W / 3;
  lcd.startWrite();
  for (int y = 0; y < AREA_H; ++y) {
    for (int x = 0; x < z; ++x)                       // 체커보드
      lcd.writePixel(x, y, ((x ^ y) & 1) ? TFT_WHITE : TFT_BLACK);
    for (int x = z; x < z * 2; ++x)                   // 세로줄
      lcd.writePixel(x, y, (x & 1) ? TFT_WHITE : TFT_BLACK);
    for (int x = z * 2; x < SCR_W; ++x)               // 가로줄
      lcd.writePixel(x, y, (y & 1) ? TFT_WHITE : TFT_BLACK);
  }
  lcd.endWrite();
  testCaption("1PX CHECKER | V-LINES | H-LINES");
}

// 4 — 가장자리와 오프셋. 네 변과 네 모서리가 온전히 보여야 한다.
static void testEdges() {
  lcd.fillScreen(TFT_BLACK);
  lcd.drawRect(0, 0, SCR_W, SCR_H, TFT_WHITE);
  lcd.drawLine(0, 0, SCR_W - 1, SCR_H - 1, TFT_RED);
  lcd.drawLine(SCR_W - 1, 0, 0, SCR_H - 1, TFT_BLUE);
  lcd.fillRect(0, 0, 10, 10, TFT_RED);
  lcd.fillRect(SCR_W - 10, 0, 10, 10, TFT_GREEN);
  lcd.fillRect(0, SCR_H - 10, 10, 10, TFT_BLUE);
  lcd.fillRect(SCR_W - 10, SCR_H - 10, 10, 10, TFT_YELLOW);

  lcd.setFont(&fonts::Font2);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.drawString("EDGE & OFFSET", SCR_W / 2, SCR_H / 2 - 22);
  char buf[40];
  snprintf(buf, sizeof(buf), "%dx%d rot%d off %d,%d",
           (int)lcd.width(), (int)lcd.height(), ROTATION, OFFSET_X, OFFSET_Y);
  lcd.drawString(buf, SCR_W / 2, SCR_H / 2);
  lcd.setFont(&fonts::Font0);
  lcd.drawString("4/4  press key -> UI", SCR_W / 2, SCR_H / 2 + 22);
}

static const char* const TEST_HINT[TEST_PAGES] = {
  "1/4 컬러바 — 왼쪽부터 흰·노랑·하늘·초록·자홍·빨강·파랑·검정 순이어야 한다.\n"
  "    노랑과 하늘이 바뀌었으면 R/B 채널이 뒤집힌 것(rgb_order).\n"
  "    아래 16단 그레이는 양끝 두 칸이 뭉개지지 않아야 한다.",
  "2/4 그라데이션 — RGB565 는 R5 G6 B5 다.\n"
  "    빨강·파랑은 32단(약 10px 폭), 초록은 64단(약 5px 폭)으로 보여야 정상.\n"
  "    계단이 보이는 건 패널 탓이 아니라 16비트 색의 한계다.",
  "3/4 해상도 — 왼쪽 체커보드, 가운데 세로줄, 오른쪽 가로줄 전부 1픽셀이다.\n"
  "    떨어져서 보면 세 구역 모두 균일한 회색이어야 한다.\n"
  "    줄무늬·얼룩·색번짐이 보이면 픽셀이 빠지거나 스케일링이 끼어든 것.",
  "4/4 가장자리 — 흰 테두리 네 변과 네 모서리 사각형이 온전해야 한다.\n"
  "    좌우 끝에 노이즈 띠가 보이면 OFFSET_X 가 틀린 것이다."
};

static void drawTestPage(int p) {
  switch (p) {
    case 0: testColorBars();  break;
    case 1: testRamps();      break;
    case 2: testResolution(); break;
    default: testEdges();     break;
  }
  Serial.println(TEST_HINT[p]);
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
static void onShort(int i) {
  Serial.printf("[KEY%d] short  -> %s (%s)\n", i + 1, KEYS[i].label, subOf(i));
  if (i == 0) {
    // 크롬 뒤로가기 — 여기가 호스트에 따라 갈리는 지점이다
    sendKey(hostOS == HOST_MAC ? MOD_LGUI : MOD_LALT, KEY_LEFT);
  } else {
    sendConsumer(CC_MUTE);   // 미디어 키는 두 OS 가 동일하다
  }
}

static void onLong(int i) {
  if (i == 1) {
    hostOS = (hostOS == HOST_MAC) ? HOST_WIN : HOST_MAC;
    Serial.printf("[KEY2] LONG   -> 호스트 프로필 %s\n",
                  hostOS == HOST_MAC ? "macOS (Cmd+<-)" : "Windows (Alt+<-)");
    drawUI();
  } else {
    Serial.println("[KEY1] LONG   -> (예약)");
  }
}
static void onRelease(int i)   { Serial.printf("[KEY%d] release\n", i + 1);
                                 neopixelWrite(PIN_RGB_LED, 0, 0, 0); }

static void advanceTest() {
  if (++testPage >= TEST_PAGES) {
    testMode = false;
    drawUI();
    Serial.println("테스트 끝 -> 일반 UI. 다시 보려면 RESET.");
  } else {
    drawTestPage(testPage);
  }
}

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
        if (testMode) { advanceTest(); continue; }
        drawKey(i, true); onPress(i);
      } else {
        if (testMode) continue;
        drawKey(i, false);
        if (!k.longFired) onShort(i);
        onRelease(i);
      }
    }
    if (!testMode && k.stable && !k.longFired && (now - k.tPress) >= LONGPRESS_MS) {
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
  Serial.printf("=== i-brick deck v%s / 2-key sample ===\n", FW_VERSION);
  Serial.printf("LCD %dx%d  rotation=%d  offset(%d,%d)\n",
                (int)lcd.width(), (int)lcd.height(), ROTATION, OFFSET_X, OFFSET_Y);
  Serial.printf("KEY1=GPIO%d  KEY2=GPIO%d  (INPUT_PULLUP, GND 에 닿으면 눌림)\n", PIN_KEY1, PIN_KEY2);
  if (BOOT_ALIAS >= 0)
    Serial.printf("BOOT(GPIO%d) -> KEY%d  (업로드 중에는 누르지 말 것)\n", PIN_BOOT, BOOT_ALIAS + 1);
  bleBegin();
  Serial.println("BLE 광고 시작 — 호스트의 블루투스 목록에서 \"i-brick deck\" 로 페어링.");
  Serial.printf("호스트 프로필 %s. KEY2 를 길게 누르면 macOS <-> Windows 전환.\n",
                hostOS == HOST_MAC ? "macOS" : "Windows");
  Serial.println("색·해상도 테스트를 보려면 START_IN_TEST 를 true 로 바꿔 다시 빌드한다.");
  Serial.println();

  neopixelWrite(PIN_RGB_LED, 0, 0, 0);
  if (START_IN_TEST) {
    drawTestPage(0);
  } else {
    testMode = false;
    drawUI();
  }
}

void loop() {
  if (uiDirty) {                 // BLE 콜백에서 그리면 SPI 가 깨진다. 여기서 다시 그린다.
    uiDirty = false;
    if (!testMode) drawHeader();
    Serial.println(bleConnected ? "[BLE] 연결됨" : "[BLE] 끊김 — 다시 광고");
  }
  pollKeys();
  delay(2);
}
