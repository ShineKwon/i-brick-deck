# ESP32-C6-LCD-1.47 샘플의 빌드·업로드·시리얼 모니터를 묶은 개발용 Makefile

CLI  := /Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli
# 플래시는 실측 8MB (esptool flash_id: Device 4017). 4M 으로 잡으면 절반을 못 쓴다.
FQBN := esp32:esp32:esp32c6:CDCOnBoot=cdc,FlashSize=8M,PartitionScheme=default_8MB

# esptool 을 직접 부른다. arduino-cli upload 는 --no-stub 을 못 넘기는데,
# C6 의 네이티브 USB Serial/JTAG 는 스텁을 올리면 연결이 끊겨
# "Unable to verify flash chip connection" 으로 실패한다.
ESPTOOL := $(HOME)/Library/Arduino15/packages/esp32/tools/esptool_py/4.6/esptool
BUILD   := build
PORT ?= /dev/cu.usbmodem214201
SKETCH := ibrick_ui

.PHONY: all build flash monitor ports clean

all: build flash

build:
	"$(CLI)" compile --fqbn "$(FQBN)" --output-dir $(BUILD) $(SKETCH)

# merged.bin 은 8MB 전체를 써서 40초가 걸린다. 필요한 네 조각만 올리면 ~300KB 다.
BOOT_APP0 := $(HOME)/Library/Arduino15/packages/esp32/hardware/esp32/3.0.2/tools/partitions/boot_app0.bin

flash:
	"$(ESPTOOL)" --chip esp32c6 --port $(PORT) --no-stub --baud 115200 write_flash \
		0x0     $(BUILD)/$(SKETCH).ino.bootloader.bin \
		0x8000  $(BUILD)/$(SKETCH).ino.partitions.bin \
		0xe000  $(BOOT_APP0) \
		0x10000 $(BUILD)/$(SKETCH).ino.bin

# 모니터는 포트를 잡고 있으면 다음 업로드가 실패한다. 업로드 전에 Ctrl+C 로 끊을 것.
monitor:
	"$(CLI)" monitor -p $(PORT) -c baudrate=115200

ports:
	"$(CLI)" board list

clean:
	rm -rf $(BUILD)
