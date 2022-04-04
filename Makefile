#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := Daikin
SUFFIX := $(shell components/ESP32-RevK/buildsuffix)
MODELS := Daikin

all:
	@echo Make: $(PROJECT_NAME)$(SUFFIX).bin
	@idf.py build
	@cp build/$(PROJECT_NAME).bin $(PROJECT_NAME)$(SUFFIX).bin
	@echo Done: $(PROJECT_NAME)$(SUFFIX).bin

tools:	faikin

set:    wroom solo pico

pico:
	components/ESP32-RevK/setbuildsuffix -S1-PICO
	@make

wroom:
	components/ESP32-RevK/setbuildsuffix -S1
	@make

solo:
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

# Program the FTDI
ftdi: ftdizap/ftdizap
	./ftdizap/ftdizap --serial="RevK" --description="Daikin" --cbus2-mode=17 --self-powered=1

PCBCase/case: PCBCase/case.c
	make -C PCBCase

ifeq ($(shell uname),Darwin)
INCLUDES=-I/usr/local/include/
LIBS=-L/usr/local/Cellar/popt/1.18/lib/
else
LIBS=
INCLUDES=
endif

faikin: faikin.c
	gcc -O -o $@ $< -lpopt ${INCLUDES} ${LIBS}

scad:	$(patsubst %,KiCad/%.scad,$(MODELS))
stl:	$(patsubst %,KiCad/%.stl,$(MODELS))

%.stl: %.scad
	echo "Making $@"
	/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD $< -o $@
	echo "Made $@"

KiCad/Daikin.scad: KiCad/Daikin.kicad_pcb PCBCase/case Makefile
	PCBCase/case -o $@ $< --edge=2 --base=2.5

