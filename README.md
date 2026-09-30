# i-brick deck

키캡형 스트림덱의 UI·인터랙션 샌드박스. **Waveshare ESP32-C6-LCD-1.47** 보드에서
컬러 LCD에 키 두 개를 그리고, 물리 키 입력을 **BLE HID**로 호스트에 보낸다.

> **이 저장소의 위치** — 제품 프로토타입이 아니라 UI 루프와 이벤트 모양을 익히는 샌드박스다.
> 설계가 수렴한 제품 사양은 nRF52840 + e-ink 공유 패널 + 무배터리라 HID 경로, 갱신 모델,
> 입력 기구가 모두 다르다. 자세한 내용은 [`context-notes.md`](context-notes.md).

## 지금 되는 것

| 키 | 짧게 | 길게 |
|---|---|---|
| KEY1 (BOOT / GPIO2) | 크롬 뒤로가기 | — |
| KEY2 (GPIO3) | 음소거 | 호스트 프로필 전환 (macOS ↔ Windows) |

- 320×172 가로 화면에 키 2개, 각 키 배경에 코드로 렌더링한 캐릭터
- BLE HID 키보드 + Consumer Control (리포트 2개)
- 헤더에 BLE 연결 상태와 현재 호스트 프로필 표시
- 색·해상도 확인용 테스트 화면 4장 (`START_IN_TEST`)

### 호스트 프로필이 필요한 이유

HID는 문자가 아니라 **usage code**를 보내고 해석은 호스트가 한다. 그래서 같은 기기에서
두 키가 정반대로 동작한다.

- **뒤로가기** — macOS `Cmd+←`, Windows `Alt+←`. **갈린다**
- **음소거** — Consumer Control usage `0xE2`. **두 OS 동일**

프로필이 필요한 키와 아닌 키가 섞여 있어서, 프로필은 기기 안에 N개로 저장해야 한다.

## 하드웨어

Waveshare ESP32-C6-LCD-1.47 (ST7789 172×320, 실측 플래시 8MB)

| | GPIO |
|---|---|
| LCD | MOSI 6 · SCLK 7 · CS 14 · DC 15 · RST 21 · BL 22 |
| TF 카드 | MISO 5 · CS 4 (LCD와 SPI 버스 공유) |
| RGB LED | 8 (WS2812) |
| 사용자 키 | 2, 3 (INPUT_PULLUP) |
| BOOT | 9 — 스트래핑 핀. 리셋 순간만 피하면 키로 써도 된다 |

## 빌드

Arduino IDE를 열지 않고 번들된 `arduino-cli`를 Makefile에서 직접 부른다.

```bash
make build     # 컴파일
make flash     # esptool --no-stub 로 업로드
make monitor   # 시리얼 로그 (업로드 전 Ctrl+C)
make ports     # 포트 확인
```

### 버전이 세트로 묶여 있다

| | 고정 버전 |
|---|---|
| esp32 코어 | **3.0.2** |
| LovyanGFX | **1.1.16** |

LovyanGFX 1.2.x는 ESP-IDF 5.2+ 매크로(`PERIPH_RCC_ATOMIC`)를 쓰는데 코어 3.0.2는 IDF 5.1이라
컴파일이 깨진다. **Arduino IDE에서 라이브러리 업데이트를 수락하면 빌드가 망가진다.**
올리려면 코어와 라이브러리를 같이 올려야 한다.

### 코드가 `.ino`가 아니라 `.cpp`인 이유

`arduino-cli`는 `.ino`에만 함수 프로토타입을 자동 삽입하는데, 그 생성에
Arduino 패치판 `ctags 5.8-arduino11`이 필요하다. 이 바이너리가 x86 전용이라
Rosetta가 빠진 macOS 26+에서는 실행되지 않는다.

코드를 `.cpp`에 두면 프로토타입 삽입 대상이 아니므로 이 의존성이 사라진다.
`ibrick_ui.ino`는 스케치 폴더 이름을 맞추기 위한 빈 파일이다 —
**여기에 코드를 쓰면 의존성이 되살아난다.** 대신 `.cpp`는 `Arduino.h`를 직접 포함한다.

### C6의 USB 주의점

네이티브 USB Serial/JTAG는 보드레이트 변경과 스텁 로더에서 연결이 끊긴다.
그래서 `arduino-cli upload`가 아니라 **esptool을 `--no-stub`으로 직접** 부른다.
포트 이름도 다시 꽂을 때마다 바뀌어서 Makefile이 자동 탐지한다.

## 키 배경 이미지

`ibrick_ui/bg_key*.h`는 **생성물이다. 손으로 고치지 않는다.**

```bash
python3 tools/make_char.py cat /tmp/cat.bmp /tmp/cat.png      # 렌더 + 미리보기
python3 tools/img2rgb565.py /tmp/cat.bmp ibrick_ui/bg_key2.h 148 128 BGKEY2
make flash
```

외부 이미지를 쓰려면 `make_char.py` 단계를 건너뛰고 `img2rgb565.py`에 PNG/JPEG를 바로 넣는다.
둘 다 macOS 내장 `sips`만 쓰므로 외부 의존성이 없다.

- 키 하나 = **148 × 128 px** (247 PPI라 실물 15.2 × 13.2 mm)
- 라운드 모서리는 변환 단계에서 배경색으로 미리 마스킹한다

## 페어링

호스트 블루투스 목록에서 **"i-brick deck"** 으로 잡힌다.

> 다시 플래시한 뒤 페어링이 안 되면 호스트에서 기존 등록을 **먼저 삭제**할 것.
> 펌웨어를 새로 올리면 본딩 키가 새로 생기는데, 옛 등록이 남아 있으면 조용히 실패한다.

## 용량

| | 플래시 | RAM |
|---|---|---|
| UI만 | 370,572 (11%) | 14,304 |
| UI + BLE HID | 1,269,738 (37%) | 38,772 (11%) |

앱 파티션 3.19MB (`default_8MB`, app0/app1 OTA 2슬롯).

## 문서

- [`checklist.md`](checklist.md) — 진행 상황
- [`context-notes.md`](context-notes.md) — 결정과 이유, 막혔던 것들의 재현 과정
