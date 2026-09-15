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
- [x] 색·해상도 테스트 4장 (부팅 시 진입, 키로 넘김)
- [x] `drawKey(i, pressed)` — 나중에 아이콘 비트맵이 들어갈 자리
- [x] 키 2개 **가로 배치** (148x128, 화면 320x172)
- [x] 디바운스 25ms + 롱프레스 600ms 이벤트
- [x] RGB LED 를 독립 생존신호로 사용
- [x] 컴파일 통과 — 291KB / 22%, RAM 14KB
- [x] 업로드 성공 — 부팅 배너 `LCD 320x172 rotation=1 offset(34,0)`
- [x] 플래시 8MB 로 정정 (4M 로 잡고 있었음), 업로드 40초 → 10초
- [ ] **화면 보정 확인 — 네 변 테두리와 네 모서리 사각형이 온전히 보이는가** ← 눈으로 봐야 함
- [x] BOOT(GPIO9) 를 KEY1 에 붙임 — 스위치 없이 바로 눌러 볼 수 있음
- [x] 부팅 정상 확인 (리부트 루프·패닉 없음)
- [x] COPY 키 배경 — 토끼 후드 소녀 148x128 RGB565
- [x] MUTE 키 배경 — 고양이 후드 "쉿" 148x128 RGB565
- [ ] 실제로 눌러서 확인 (BOOT 버튼 또는 GPIO2/3 ↔ GND)

## BLE HID
- [x] BLE HID 키보드 + Consumer Control (리포트 2개)
- [x] KEY1(BOOT) = 크롬 뒤로가기 — macOS Cmd+←, Windows Alt+←
- [x] KEY2 = 음소거 (Consumer usage 0xE2, 두 OS 동일)
- [x] KEY2 롱프레스 = 호스트 프로필 전환, 헤더와 키 라벨에 반영
- [x] 헤더에 BLE 연결 상태 점 + MAC/WIN 표시
- [x] 연결 끊기면 자동 재광고
- [x] 콜백에서 그리지 않고 uiDirty 플래그로 loop 에서 다시 그림
- [x] 빌드 1,269,738B (37%) / RAM 38,772B (11%)
- [x] 업로드 후 광고 시작 확인, 패닉 없음
- [ ] **호스트에서 페어링 확인**
- [ ] **크롬에서 실제로 뒤로가기 되는지 확인**

## 다음 (이 샘플 범위 밖)
- [ ] 아이콘 비트맵 렌더링
- [ ] 레이어 전환 (롱프레스 핸들러는 자리만 잡아 둠)
- [ ] BLE HID — C6 는 USB HID 가 안 되므로 이 경로뿐
