# PCB makefile (3d Cases)

all: png stl

%.stl:	%.scad
	echo "Making $@"
	/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD $< -o $@
	echo "Made $@"

stl: 	Faikin/Faikin.stl Faikin/FaikinT.stl Faikin/FaikinB.stl

png:	$(patsubst %.kicad_pcb,%.png,$(wildcard */*.kicad_pcb))

Faikin/Faikin.scad: Faikin/Faikin.kicad_pcb PCBCase/case PCBCase/case
	PCBCase/case -o $@ $< --edge=2 --top=6 --bottom=3 --pcb=2

Faikin/FaikinB.scad: Faikin/Faikin.kicad_pcb PCBCase/case PCBCase/case
	PCBCase/case -o $@ $< --edge=2 --top=6 --bottom=3 --pcb=2 -B

Faikin/FaikinT.scad: Faikin/Faikin.kicad_pcb PCBCase/case PCBCase/case
	PCBCase/case -o $@ $< --edge=2 --top=6 --bottom=3 --pcb=2 -T

PCBCase/clean: PCBCase/clean.c
	make -C PCBCase

PCBCase/case: PCBCase/case.c
	make -C PCBCase

%.png:	%.kicad_pcb PCBCase/clean PCBCase/render
	PCBCase/render $<
