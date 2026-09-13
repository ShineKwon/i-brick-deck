# ESP32-C6-LCD-1.47 샘플의 빌드·업로드·시리얼 모니터를 묶은 개발용 Makefile

CLI  := /Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli
FQBN := esp32:esp32:esp32c6:CDCOnBoot=cdc,FlashSize=4M
PORT ?= /dev/cu.usbmodem214201
SKETCH := ibrick_ui

.PHONY: all build flash monitor ports clean

all: build flash

build:
	"$(CLI)" compile --fqbn "$(FQBN)" $(SKETCH)

flash:
	"$(CLI)" upload --fqbn "$(FQBN)" -p $(PORT) $(SKETCH)

# 모니터는 포트를 잡고 있으면 다음 업로드가 실패한다. 업로드 전에 Ctrl+C 로 끊을 것.
monitor:
	"$(CLI)" monitor -p $(PORT) -c baudrate=115200

ports:
	"$(CLI)" board list

clean:
	rm -rf $(SKETCH)/build
