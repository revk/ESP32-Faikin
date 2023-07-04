#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := BlueCoinT
SUFFIX := $(shell components/ESP32-RevK/buildsuffix)

all:	
	@echo Make: $(PROJECT_NAME)$(SUFFIX).bin
	@idf.py build
	@cp --remove-destination build/$(PROJECT_NAME).bin $(PROJECT_NAME)$(SUFFIX).bin
	@echo Done: $(PROJECT_NAME)$(SUFFIX).bin

issue:  set
	cp --remove-destination BlueCoinT*.bin release

set:    wroom solo pico

pico:
	components/ESP32-RevK/setbuildsuffix -S1-PICO-SSD1681
	@make

wroom:
	components/ESP32-RevK/setbuildsuffix -S1-SSD1681
	@make

solo:	# This is Shellys and no display
	components/ESP32-RevK/setbuildsuffix -S1-SOLO
	@make

flash:
	idf.py flash

monitor:
	idf.py monitor

clean:
	idf.py clean

menuconfig:
	idf.py menuconfig

pull:
	git pull
	git submodule update --recursive

update:
	git submodule update --init --recursive --remote
	git commit -a -m "Library update"
