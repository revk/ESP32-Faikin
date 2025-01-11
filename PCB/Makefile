# PCB makefile (3d Cases)

all: png stl

%.stl:	%.scad
	echo "Making $@"
	/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD $< -o $@
	echo "Made $@"

stl: 	Faikin/Faikin.stl

png:	Faikin/top.png Faikin/bottom.png

Faikin/Faikin.scad: Faikin/Faikin.kicad_pcb PCBCase/case Makefile PCBCase/case Makefile
	PCBCase/case -o $@ $< --edge=2 --top=4.6 --bottom=2.6 --pcb=2

PCBCase/case: PCBCase/case.c
	make -C PCBCase

Faikin/top.png: Faikin/Faikin.kicad_pcb
	/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli pcb render -o $@ $< --perspective --rotate ' -40,0,30' --zoom=0.85 --floor --pan ' -0.9,2.3,0' --width=2300 --height=1900 --background=opaque

Faikin/bottom.png: Faikin/Faikin.kicad_pcb
	/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli pcb render -o $@ $< --perspective --rotate ' -40,180,-30' --zoom=0.85 --floor --pan ' -0.9,2.3,0' --width=2300 --height=1900 --background=opaque

