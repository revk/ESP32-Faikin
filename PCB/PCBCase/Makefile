all:	case tracklen PCB/Test/Test.scad

case:	case.c models models/final.scad
	gcc -I/usr/local/include -L/usr/local/lib -O -o $@ $< -lpopt -lm -g

tracklen:	tracklen.c
	gcc -I/usr/local/include -L/usr/local/lib -O -o $@ $< -lpopt -lm -g

PCB/Test/Test.scad:	PCB/Test/Test.kicad_pcb case Makefile
	./case -o $@ $< -M models --debug
