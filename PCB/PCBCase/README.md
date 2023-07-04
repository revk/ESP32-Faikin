* PCB Case


This is designed to take an KICad 6 kicad_pcb file and produce an openscad file that is the case.
The problem is that you need 3D models for your parts for this to work sensibly, so the C code has a list of footprints as basic openscad outlines. This is not like the 3D models in KiCad which are accurate, it is simply boxes and shapes that allow for the 3D box to have a sensibly sized cut out for the part.
In many cases this means the model is just a cuboid. But in some cases it is slightly more important as it has the attached connector included.
Any parts of the design that would breach the case cause cut outs and supports in the case surround.

![275724777_4932056986873909_2086496272107808800_n](https://user-images.githubusercontent.com/996983/158376722-9541f6dd-25f3-4107-ac4b-4513a761b210.jpg)

## models

Modules are individual scad instructions, and can be named as the first match of :-

- The footprint name (without `prefix:`)
- The footprint name (without `prefix:`) where single numeric component is replaced with `0`
- The 3D model name(s) in the footprint

The module is called with `pushed` and `hulled` which are Boolean, and in the case of the `0` version, with `n` which is the number that was replaced with 0.