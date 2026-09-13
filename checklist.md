# 체크리스트 — ESP32-C6-LCD-1.47 2키 샘플

## 개발 환경
- [x] 보드 연결 확인 (Espressif USB JTAG/serial, VID 0x303A PID 0x1001)
- [x] 포트 확인 — `/dev/cu.usbmodem214201`
- [x] Arduino esp32 코어 확인 — 3.0.2 (이미 설치됨, 추가 다운로드 불필요)
- [x] 번들 arduino-cli 사용 — Arduino IDE.app 내부 0.35.3
- [x] LovyanGFX **1.1.16** 설치 (1.2.28 은 코어 3.0.2 와 안 맞음 — 노트 참조)
- [x] Makefile (build / flash / monitor / ports)

## 2키 UI 샘플
- [x] ST7789 LGFX 설정 (172x320, SPI2, 버스 공유)
- [x] 백라이트 GPIO22 명시적 점등
- [x] 오프셋 보정 패턴 (부팅 후 4초)
- [x] `drawKey(i, pressed)` — 나중에 아이콘 비트맵이 들어갈 자리
- [x] 키 2개 세로 배치 (156x126)
- [x] 디바운스 25ms + 롱프레스 600ms 이벤트
- [x] RGB LED 를 독립 생존신호로 사용
- [x] 컴파일 통과 — 291KB / 22%, RAM 14KB
- [x] 업로드 성공 — 부팅 배너에서 `LCD 172x320 offset(34,0)` 확인
- [ ] **화면 보정 확인 — 네 변 테두리와 네 모서리 사각형이 온전히 보이는가** ← 눈으로 봐야 함
- [ ] 키 입력 확인 (GPIO2/GPIO3 를 GND 에 대보기)

## 다음 (이 샘플 범위 밖)
- [ ] 아이콘 비트맵 렌더링
- [ ] 레이어 전환 (롱프레스 핸들러는 자리만 잡아 둠)
- [ ] BLE HID — C6 는 USB HID 가 안 되므로 이 경로뿐
