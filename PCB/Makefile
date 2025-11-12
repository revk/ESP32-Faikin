# PCB makefile (3d Cases)

all: png stl */README.md

%.stl:	%.scad
	echo "Making $@"
	/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD $< -o $@
	echo "Made $@"

stl: 	Faikout/Faikout[TCB].stl 

png:	$(patsubst %.kicad_pcb,%.png,$(wildcard */*.kicad_pcb))

Faikout/FaikoutT.scad: Faikout/Faikout.kicad_pcb PCBCase/case PCBCase/case
	PCBCase/case -o $@ $< --edge=2 --top=7 --bottom=3 --pcb=2 --split-file --lip-type=1 --logo-angle=-90 --logo-x=-5

PCBCase/clean: PCBCase/clean.c
	make -C PCBCase

PCBCase/case: PCBCase/case.c
	make -C PCBCase

%.png:	%.kicad_pcb PCBCase/clean PCBCase/render
	PCBCase/render $<

%/README.md: %/*.kicad_pcb %/*.stl PCBCase/readme
	PCBCase/readme $@
