# PCB makefile (3d Cases)

all: png stl

%.stl:	%.scad
	echo "Making $@"
	/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD $< -o $@
	echo "Made $@"

stl: 	Faikin/Faikin.stl

png:	$(patsubst %.kicad_pcb,%.png,$(wildcard */*.kicad_pcb))

Faikin/Faikin.scad: Faikin/Faikin.kicad_pcb PCBCase/case Makefile PCBCase/case Makefile
	PCBCase/case -o $@ $< --edge=2 --top=4.6 --bottom=2.6 --pcb=2

PCBCase/clean: PCBCase/clean.c
	make -C PCBCase

PCBCase/case: PCBCase/case.c
	make -C PCBCase

%.png:	%.kicad_pcb PCBCase/clean PCBCase/render Makefile
	PCBCase/render $<
