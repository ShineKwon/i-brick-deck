# 컨텍스트 노트 — i-brick deck

## 이 보드의 위치 (2026-09-13)
Waveshare **ESP32-C6-LCD-1.47** 은 **UI/인터랙션 학습용 샌드박스**다.
앞서 설계가 수렴한 제품 사양은 **nRF52840 + e-ink 1장 공유 + 무배터리**라 다음이 전부 다르다.

- **HID 경로** — ESP32-C6 에는 USB Serial/JTAG 컨트롤러만 있고 USB-OTG 가 없다.
  임의의 USB HID 클래스를 못 만든다. (USB-OTG 는 ESP32-S2/S3 에만 있음)
  따라서 C6 로 스트림덱을 하려면 **BLE HID 뿐**이다. nRF52840 은 USB FS 가 있어 둘 다 된다.
- **리프레시 모델** — 컬러 LCD 는 즉시 갱신, e-ink 는 전체 2s / 부분 0.3s.
  레이어 전환 연출이 완전히 다르다.
- **입력 기구** — 실제 제품은 측면 탭 + 택트, 이 보드는 GPIO 직결.

즉 **UI 루프와 이벤트 모양을 익히는 용도**이지 제품 프로토타입이 아니다.

## 확정된 보드 사실
| 항목 | 값 |
|---|---|
| 포트 | `/dev/cu.usbmodem214201` |
| FQBN | `esp32:esp32:esp32c6:CDCOnBoot=cdc,FlashSize=4M` |
| LCD | ST7789, 172x320, SPI2 |
| LCD 핀 | MOSI 6 · SCLK 7 · CS 14 · DC 15 · RST 21 · BL 22 |
| TF 카드 | MISO 5 · MOSI 6 · SCLK 7 · CS 4 (LCD 와 SPI 버스 공유) |
| RGB LED | GPIO8 (WS2812) |
| 플래시 | 4MB |
| 사용자 키 | GPIO2, GPIO3 (INPUT_PULLUP) |

**피해야 할 핀** — 4·5·6·7(SPI/SD), 8(RGB), 12·13(USB D±), 14·15·21·22(LCD),
그리고 **9(BOOT 스트래핑)**. 9번은 리셋 시 눌려 있으면 다운로드 모드로 들어가서
디버깅 중에 엉뚱하게 헷갈린다. 키로 쓰지 않는다.

## 결정과 이유
- **Arduino CLI (ESP-IDF 아님)** — esp32 코어 3.0.2 가 이미 깔려 있어 추가 설치가 0이다.
  ESP-IDF 는 새로 받으면 수 GB + 수십 분. 샘플 단계에서 지불할 이유가 없다.
  IDE 를 따로 안 켜도 되게 번들 CLI 를 직접 부른다.
- **LovyanGFX (LVGL 아님)** — 버튼 2개에 LVGL 은 과하고 `lv_conf.h` 설정이 번거롭다.
  LovyanGFX 는 ST7789 커스텀 핀 설정이 쉽고 C6 를 지원한다 (1.2.28).
- **`CDCOnBoot=cdc`** — 안 켜면 네이티브 USB 포트로 `Serial` 출력이 안 나온다.
- **RGB LED 를 생존신호로** — LCD 설정이 틀려 검은 화면일 때 펌웨어가 도는지 구분된다.

## 미해결
- **패널 오프셋** — `OFFSET_X = 34, OFFSET_Y = 0` 은 (240-172)/2 산수에서 나온 추정.
  부팅 후 4초간 보정 패턴을 띄우게 해 뒀으니 네 변이 온전한지 눈으로 확인하고 확정할 것.
  `setRotation` 을 바꾸면 오프셋도 같이 바뀐다.
- **`invert = true`** — IPS 라 넣었는데 색이 반전돼 보이면 이 값을 뒤집는다.

## 제품 쪽에서 넘어온 미해결 (재도출 방지)
- **액티브 영역 27.0 / 27.6mm** — GDEY0154D67 스펙표가 자기모순이다.
  액티브 27x27 / 피치 0.14 / PPI 183.66 이 서로 안 맞는다. PPI 역산 27.66 만이 대각 1.54" 와 맞음.
  Good Display 에 기구 도면을 요청해 확정해야 하고, 키캡 폭이 전부 여기서 나온다.

## 막힌 곳과 해결 (2026-09-13)

**LovyanGFX 1.2.28 이 esp32 코어 3.0.2 에서 컴파일 실패.**

```
Bus_SPI.cpp:244: error: 'PERIPH_RCC_ATOMIC' was not declared in this scope
```

`PERIPH_RCC_ATOMIC` 은 ESP-IDF 5.2+ 매크로인데 코어 3.0.2 는 IDF 5.1 이라 없다.
LovyanGFX 는 1.2.x 부터 이 매크로를 쓴다.

**선택지 두 개**
1. esp32 코어를 3.3.11 로 올린다 (인덱스 갱신하니 나옴). 툴체인 1GB+ 재다운로드.
2. LovyanGFX 를 1.1.16 으로 내린다. 이 버전은 ESP32C6 심볼은 있고 문제 매크로는 안 쓴다.

**2번 선택.** 샘플 단계에서 1GB 를 지불할 이유가 없고, 1.1.16 으로 컴파일·업로드가 통과했다.
나중에 코어를 올리게 되면 LovyanGFX 도 같이 최신으로 올려야 한다 — **두 버전은 세트로 움직인다.**

| 항목 | 고정 버전 |
|---|---|
| esp32 코어 | 3.0.2 (최신은 3.3.11) |
| LovyanGFX | 1.1.16 (최신은 1.2.28) |

**빌드 결과** — 플래시 291,052B / 1,310,720B (22%), 전역 RAM 14,296B (4%).
172x320 풀스크린 스프라이트는 110KB 라 여유는 있지만, 지금은 키 상태가 바뀔 때만
해당 키 사각형만 다시 그리는 방식이라 스프라이트를 안 쓴다.

## 가로 전환에서 나온 것 (2026-09-13)

### 1. `memory_width` / `memory_height` 를 반드시 지정해야 한다

`Panel_Device.hpp` 기본값이 **240 x 240** 인데 ST7789 프레임 메모리는 **240 x 320** 이다.
`Panel_LCD::setRotation` 이 회전할 때 이 값으로 `colstart`/`rowstart` 를 다시 계산한다.

```
_colstart = (rot & 2) ? mw - (pw + ox) : ox;
_rowstart = ((1 << rot) & 0b10010110) ? mh - (ph + oy) : oy;
```

회전 0 은 두 분기 모두 `ox`/`oy` 를 그대로 쓰므로 **기본값이 틀려도 드러나지 않는다.**
회전 2·3 에서 `mh - (ph + oy)` 가 `240 - 320 = -80` 이 되어 깨진다.
`memory_width = 240, memory_height = 320` 을 명시해 네 방향 모두 맞췄다.

### 2. 플래시는 4MB 가 아니라 8MB

`esptool flash_id` → `Device: 4017`, `Detected flash size: 8MB`.
`FlashSize=8M,PartitionScheme=default_8MB` 로 바꾸니 앱 파티션이 1.28MB → 3.19MB 가 됐다
(스케치 291KB 가 22% → 8%). 나중에 아이콘 비트맵을 넣을 공간이 여기서 나온다.

### 3. 업로드가 깨지는 진짜 이유 — 스텁과 보드레이트

두 번 연속 실패했고 메시지가 단계적으로 바뀌었다.

```
Changing baud rate to 921600  ->  A fatal error occurred: No serial data received.
Stub running...              ->  Unable to verify flash chip connection (No serial data received.)
```

C6 의 네이티브 USB Serial/JTAG 에서는 (a) 보드레이트 변경이 의미가 없고 연결을 끊으며,
(b) 스텁 로더를 올리면 통신이 죽는다. **`--no-stub` 이 핵심이고, arduino-cli upload 는
이 옵션을 못 넘긴다.** 그래서 `make flash` 는 esptool 을 직접 부른다.

첫 업로드가 921600 + 스텁으로 성공한 건 운이었다. 재현되지 않는다.

### 4. merged.bin 대신 네 조각으로

`merged.bin` 은 8MB 전체를 패딩해서 쓰기 때문에 40.7 초가 걸렸다.
bootloader(0x0) / partitions(0x8000) / boot_app0(0xe000) / app(0x10000) 네 개만 올리면
312KB, **약 10초**다.
