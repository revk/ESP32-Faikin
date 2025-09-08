// Generated case design for Faikin/Faikin.kicad_pcb
// By https://github.com/revk/PCBCase
// Generated 2025-09-08 09:46:22
// title:	PCB-FAIKIN
// rev:	1
// company:	Adrian Kennard, Andrews & Arnold Ltd
//

// Globals
margin=0.250000;
lip=3.000000;
lipa=0;
lipt=1;
casebottom=3.000000;
casetop=7.000000;
casewall=3.000000;
fit=0.000000;
snap=0.150000;
edge=2.000000;
pcbthickness=1.200000;
function pcbthickness()=1.200000;
nohull=false;
hullcap=1.000000;
hulledge=1.000000;
useredge=false;
datex=0.000000;
datey=0.000000;
datet=0.500000;
dateh=3.000000;
datea=0;
date="2025-08-27";
datef="OCRB";
spacing=51.000000;
pcbwidth=35.000000;
function pcbwidth()=35.000000;
pcblength=16.000000;
function pcblength()=16.000000;
originx=139.500000;
originy=65.000000;

module outline(h=pcbthickness,r=0){linear_extrude(height=h)offset(r=r)polygon(points=[[17.500000,-3.000000],[17.500000,3.000000],[12.500000,8.000000],[-16.750000,8.000000],[-17.125000,7.899519],[-17.399519,7.625000],[-17.500000,7.250000],[-17.500000,-7.250000],[-17.399519,-7.625000],[-17.125000,-7.899519],[-16.750000,-8.000000],[12.500000,-8.000000]],paths=[[0,1,2,3,4,5,6,7,8,9,10,11]]);}

module pcb(h=pcbthickness,r=0){linear_extrude(height=h)offset(r=r)polygon(points=[[17.500000,-3.000000],[17.500000,3.000000],[12.500000,8.000000],[-16.750000,8.000000],[-17.125000,7.899519],[-17.399519,7.625000],[-17.500000,7.250000],[-17.500000,-7.250000],[-17.399519,-7.625000],[-17.125000,-7.899519],[-16.750000,-8.000000],[12.500000,-8.000000]],paths=[[0,1,2,3,4,5,6,7,8,9,10,11]]);}
module C19(){translate([-0.250000,0.400000,1.200000])rotate([0,0,90.000000])children();}
module part_C19(part=true,hole=false,block=false)
{
translate([-0.250000,0.400000,1.200000])rotate([0,0,90.000000])m0(part,hole,block,casetop); // RevK:C_0201 C_0201_0603Metric (back)
};
module R6(){translate([-0.800000,0.400000,1.200000])rotate([0,0,-90.000000])children();}
module part_R6(part=true,hole=false,block=false)
{
translate([-0.800000,0.400000,1.200000])rotate([0,0,-90.000000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module R1(){translate([3.300000,5.400000,1.200000])rotate([0,0,90.000000])children();}
module part_R1(part=true,hole=false,block=false)
{
translate([3.300000,5.400000,1.200000])rotate([0,0,90.000000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module C5(){translate([4.100000,0.900000,1.200000])rotate([0,0,180.000000])children();}
module part_C5(part=true,hole=false,block=false)
{
translate([4.100000,0.900000,1.200000])rotate([0,0,180.000000])m0(part,hole,block,casetop); // RevK:C_0201 C_0201_0603Metric (back)
};
module V4(){translate([-4.500000,-18.000000,1.200000])children();}
module part_V4(part=true,hole=false,block=false)
{
};
module D1(){translate([0.300000,2.800000,1.200000])rotate([0,0,180.000000])children();}
module part_D1(part=true,hole=false,block=false)
{
translate([0.300000,2.800000,1.200000])rotate([0,0,180.000000])m2(part,hole,block,casetop); // D1 (back)
};
module V5(){translate([-27.500000,0.000000,1.200000])rotate([0,0,90.000000])children();}
module part_V5(part=true,hole=false,block=false)
{
};
module V3(){translate([-4.500000,18.000000,1.200000])children();}
module part_V3(part=true,hole=false,block=false)
{
};
module C16(){translate([-1.250000,-3.500000,1.200000])rotate([0,0,-90.000000])children();}
module part_C16(part=true,hole=false,block=false)
{
translate([-1.250000,-3.500000,1.200000])rotate([0,0,-90.000000])m3(part,hole,block,casetop); // RevK:C_0402 C_0402_1005Metric (back)
};
module D3(){translate([3.400000,6.700000,1.200000])rotate([0,0,-90.000000])children();}
module part_D3(part=true,hole=false,block=false)
{
translate([3.400000,6.700000,1.200000])rotate([0,0,-90.000000])m4(part,hole,block,casetop); // D3 (back)
};
module R5(){translate([0.300000,0.400000,1.200000])rotate([0,0,90.000000])children();}
module part_R5(part=true,hole=false,block=false)
{
translate([0.300000,0.400000,1.200000])rotate([0,0,90.000000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module C7(){translate([2.580000,0.100000,1.200000])children();}
module part_C7(part=true,hole=false,block=false)
{
translate([2.580000,0.100000,1.200000])m0(part,hole,block,casetop); // RevK:C_0201 C_0201_0603Metric (back)
};
module C6(){translate([-0.429742,6.600000,1.200000])children();}
module part_C6(part=true,hole=false,block=false)
{
translate([-0.429742,6.600000,1.200000])m0(part,hole,block,casetop); // RevK:C_0201 C_0201_0603Metric (back)
};
module D4(){translate([2.500000,6.700000,1.200000])rotate([0,0,90.000000])children();}
module part_D4(part=true,hole=false,block=false)
{
translate([2.500000,6.700000,1.200000])rotate([0,0,90.000000])m4(part,hole,block,casetop); // D3 (back)
};
module Q1(){translate([0.100000,5.000000,1.200000])rotate([0,0,180.000000])children();}
module part_Q1(part=true,hole=false,block=false)
{
translate([0.100000,5.000000,1.200000])rotate([0,0,180.000000])m5(part,hole,block,casetop); // Q1 (back)
};
module C2(){translate([-1.250000,-7.100000,1.200000])rotate([0,0,90.000000])children();}
module part_C2(part=true,hole=false,block=false)
{
translate([-1.250000,-7.100000,1.200000])rotate([0,0,90.000000])m0(part,hole,block,casetop); // RevK:C_0201 C_0201_0603Metric (back)
};
module R3(){translate([-0.429742,7.200000,1.200000])rotate([0,0,180.000000])children();}
module part_R3(part=true,hole=false,block=false)
{
translate([-0.429742,7.200000,1.200000])rotate([0,0,180.000000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module D2(){translate([2.500000,5.000000,1.200000])rotate([0,0,90.000000])children();}
module part_D2(part=true,hole=false,block=false)
{
translate([2.500000,5.000000,1.200000])rotate([0,0,90.000000])m4(part,hole,block,casetop); // D3 (back)
};
module FID2(){translate([-9.750000,10.000000,1.200000])children();}
module part_FID2(part=true,hole=false,block=false)
{
};
module U1(){translate([0.200000,-1.387500,1.200000])rotate([0,0,90.000000])children();}
module part_U1(part=true,hole=false,block=false)
{
translate([0.200000,-1.387500,1.200000])rotate([0,0,90.000000])m6(part,hole,block,casetop); // U1 (back)
};
module C3(){translate([4.800000,-3.700000,1.200000])rotate([0,0,-90.000000])children();}
module part_C3(part=true,hole=false,block=false)
{
translate([4.800000,-3.700000,1.200000])rotate([0,0,-90.000000])m3(part,hole,block,casetop); // RevK:C_0402 C_0402_1005Metric (back)
};
module TP1(){translate([2.000000,2.800000,1.200000])children();}
module part_TP1(part=true,hole=false,block=false)
{
};
module U4(){translate([-9.750000,0.000000,1.200000])rotate([0,0,90.000000])children();}
module part_U4(part=true,hole=false,block=false)
{
translate([-9.750000,0.000000,1.200000])rotate([0,0,90.000000])m7(part,hole,block,casetop); // U4 (back)
};
module C1(){translate([-1.350000,0.400000,1.200000])rotate([0,0,90.000000])children();}
module part_C1(part=true,hole=false,block=false)
{
translate([-1.350000,0.400000,1.200000])rotate([0,0,90.000000])m0(part,hole,block,casetop); // RevK:C_0201 C_0201_0603Metric (back)
};
module V2(){translate([-4.500000,-8.000000,1.200000])rotate([0,0,180.000000])children();}
module part_V2(part=true,hole=false,block=false)
{
};
module C15(){translate([4.100000,2.800000,1.200000])rotate([0,0,90.000000])children();}
module part_C15(part=true,hole=false,block=false)
{
translate([4.100000,2.800000,1.200000])rotate([0,0,90.000000])m8(part,hole,block,casetop); // RevK:C_0805 C_0805_2012Metric (back)
};
module J1(){translate([6.055000,5.000000,1.200000])rotate([0,0,-90.000000])children();}
module part_J1(part=true,hole=false,block=false)
{
translate([6.055000,5.000000,1.200000])rotate([0,0,-90.000000])m9(part,hole,block,casetop,05); // J1 (back)
};
module FID3(){translate([-9.750000,-10.000000,1.200000])children();}
module part_FID3(part=true,hole=false,block=false)
{
};
module R8(){translate([-1.100000,-0.600000,1.200000])rotate([0,0,180.000000])children();}
module part_R8(part=true,hole=false,block=false)
{
translate([-1.100000,-0.600000,1.200000])rotate([0,0,180.000000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module C17(){translate([4.100000,-1.000000,1.200000])rotate([0,0,90.000000])children();}
module part_C17(part=true,hole=false,block=false)
{
translate([4.100000,-1.000000,1.200000])rotate([0,0,90.000000])m8(part,hole,block,casetop); // RevK:C_0805 C_0805_2012Metric (back)
};
module L1(){translate([1.700000,-5.200000,1.200000])rotate([0,0,-90.000000])children();}
module part_L1(part=true,hole=false,block=false)
{
translate([1.700000,-5.200000,1.200000])rotate([0,0,-90.000000])m10(part,hole,block,casetop); // L1 (back)
};
module R2(){translate([1.600000,6.900000,1.200000])rotate([0,0,90.000000])children();}
module part_R2(part=true,hole=false,block=false)
{
translate([1.600000,6.900000,1.200000])rotate([0,0,90.000000])m1(part,hole,block,casetop); // RevK:R_0201 R_0201_0603Metric (back)
};
module C18(){translate([0.850000,0.400000,1.200000])rotate([0,0,90.000000])children();}
module part_C18(part=true,hole=false,block=false)
{
translate([0.850000,0.400000,1.200000])rotate([0,0,90.000000])m0(part,hole,block,casetop); // RevK:C_0201 C_0201_0603Metric (back)
};
module C4(){translate([4.700000,-5.650000,1.200000])rotate([0,0,-90.000000])children();}
module part_C4(part=true,hole=false,block=false)
{
translate([4.700000,-5.650000,1.200000])rotate([0,0,-90.000000])m0(part,hole,block,casetop); // RevK:C_0201 C_0201_0603Metric (back)
};
module PCB1(){translate([-4.500000,0.000000,1.200000])children();}
module part_PCB1(part=true,hole=false,block=false)
{
};
// Parts to go on PCB (top)
module parts_top(part=false,hole=false,block=false){
part_C19(part,hole,block);
part_R6(part,hole,block);
part_R1(part,hole,block);
part_C5(part,hole,block);
part_V4(part,hole,block);
part_D1(part,hole,block);
part_V5(part,hole,block);
part_V3(part,hole,block);
part_C16(part,hole,block);
part_D3(part,hole,block);
part_R5(part,hole,block);
part_C7(part,hole,block);
part_C6(part,hole,block);
part_D4(part,hole,block);
part_Q1(part,hole,block);
part_C2(part,hole,block);
part_R3(part,hole,block);
part_D2(part,hole,block);
part_FID2(part,hole,block);
part_U1(part,hole,block);
part_C3(part,hole,block);
part_TP1(part,hole,block);
part_U4(part,hole,block);
part_C1(part,hole,block);
part_V2(part,hole,block);
part_C15(part,hole,block);
part_J1(part,hole,block);
part_FID3(part,hole,block);
part_R8(part,hole,block);
part_C17(part,hole,block);
part_L1(part,hole,block);
part_R2(part,hole,block);
part_C18(part,hole,block);
part_C4(part,hole,block);
part_PCB1(part,hole,block);
}

parts_top=8;
module TP10(){translate([-4.250000,4.700000,0.000000])rotate([180,0,0])children();}
module part_TP10(part=true,hole=false,block=false)
{
};
module TP8(){translate([-8.750000,4.750000,0.000000])rotate([180,0,0])children();}
module part_TP8(part=true,hole=false,block=false)
{
};
module J2(){translate([14.500000,0.000000,0.000000])rotate([0,0,90.000000])rotate([180,0,0])children();}
module part_J2(part=true,hole=false,block=false)
{
};
module TP2(){translate([-6.500000,-4.750000,0.000000])rotate([180,0,0])children();}
module part_TP2(part=true,hole=false,block=false)
{
};
module TP3(){translate([-8.750000,-4.750000,0.000000])rotate([180,0,0])children();}
module part_TP3(part=true,hole=false,block=false)
{
};
module TP4(){translate([-11.000000,-4.750000,0.000000])rotate([180,0,0])children();}
module part_TP4(part=true,hole=false,block=false)
{
};
module TP6(){translate([-13.250000,4.750000,0.000000])rotate([180,0,0])children();}
module part_TP6(part=true,hole=false,block=false)
{
};
module TP7(){translate([-4.250000,-4.750000,0.000000])rotate([180,0,0])children();}
module part_TP7(part=true,hole=false,block=false)
{
};
module V1(){translate([-4.500000,8.000000,0.000000])rotate([0,0,180.000000])rotate([180,0,0])children();}
module part_V1(part=true,hole=false,block=false)
{
};
module TP5(){translate([-11.000000,4.750000,0.000000])rotate([180,0,0])children();}
module part_TP5(part=true,hole=false,block=false)
{
};
module V6(){translate([17.500000,0.000000,0.000000])rotate([0,0,90.000000])rotate([180,0,0])children();}
module part_V6(part=true,hole=false,block=false)
{
};
module TP9(){translate([-6.500000,4.700000,0.000000])rotate([180,0,0])children();}
module part_TP9(part=true,hole=false,block=false)
{
};
module J3(){translate([-9.670000,0.000000,0.000000])rotate([0,0,-90.000000])rotate([180,0,0])children();}
module part_J3(part=true,hole=false,block=false)
{
};
// Parts to go on PCB (bottom)
module parts_bottom(part=false,hole=false,block=false){
part_TP10(part,hole,block);
part_TP8(part,hole,block);
part_J2(part,hole,block);
part_TP2(part,hole,block);
part_TP3(part,hole,block);
part_TP4(part,hole,block);
part_TP6(part,hole,block);
part_TP7(part,hole,block);
part_V1(part,hole,block);
part_TP5(part,hole,block);
part_V6(part,hole,block);
part_TP9(part,hole,block);
part_J3(part,hole,block);
}

parts_bottom=0;
module b(cx,cy,z,w,l,h){translate([cx-w/2,cy-l/2,z])cube([w,l,h]);}
module m0(part=false,hole=false,block=false,height)
{ // RevK:C_0201 C_0201_0603Metric
// 0402 Capacitor
if(part)
{
        b(0,0,0,1.1,0.4,0.2); // Pad size
        b(0,0,0,0.6,0.3,0.3); // Chip
}
}

module m1(part=false,hole=false,block=false,height)
{ // RevK:R_0201 R_0201_0603Metric
// 0402 Resistor
if(part)
{
	b(0,0,0,1.1,0.4,0.2); // Pad size
	b(0,0,0,0.6,0.3,0.3); // Chip
}
}

module m2(part=false,hole=false,block=false,height)
{ // D1
// 1x1mm LED
if(part)
{
        b(0,0,0,1.2,1.2,.8);
}
if(hole)
{
        hull()
        {
                b(0,0,.8,1.2,1.2,1);
                translate([0,0,height])cylinder(d=2.001,h=1,$fn=16);
        }
}
if(block)
{
        hull()
        {
                b(0,0,.8,2.8,2.8,1);
                translate([0,0,height])cylinder(d=4,h=1,$fn=16);
        }
}
}

module m3(part=false,hole=false,block=false,height)
{ // RevK:C_0402 C_0402_1005Metric
// 0402 Capacitor
if(part)
{
	b(0,0,0,1.0,0.5,1); // Chip
	b(0,0,0,1.5,0.65,0.2); // Pad size
}
}

module m4(part=false,hole=false,block=false,height)
{ // D3
// DFN1006-2L
if(part)
{
	b(0,0,0,1.0,0.6,0.45); // Chip
}
}

module m5(part=false,hole=false,block=false,height)
{ // Q1
if(part)
{
	b(0,0,0,1.15,2.0,1.1);
	b(0,0,0,2.1,2.0,0.6);
}
}

module m6(part=false,hole=false,block=false,height)
{ // U1
// SOT-563
if(part)
{
	b(0,0,0,1.3,1.7,1); // Part
	b(0,0,0,1.35,2.1,0.2); // Pads
}
}

module m7(part=false,hole=false,block=false,height)
{ // U4
// ESP32-S3-MINI-1
translate([-15.4/2,-15.45/2,0])
{
	if(part)
	{
		cube([15.4,20.5,0.8]);
		translate([0.7,0.5,0])cube([14,13.55,2.4]);
		cube([15.4,20.5,0.8]);
	}
}
}

module m8(part=false,hole=false,block=false,height)
{ // RevK:C_0805 C_0805_2012Metric
// 0805 Capacitor
if(part)
{
	b(0,0,0,2,1.2,1); // Chip
	b(0,0,0,2,1.45,0.2); // Pad size
}
}

module m9(part=false,hole=false,block=false,height,N=0)
{ // J1
if(part)
{
	b(2.5*(N/2)-1.25,3.6,0,2.5*N+2.5,6,4);
	b(2.5*(N/2)-1.25,0,0,2.5*N+2.5,3.2,1.5);
	for(a=[0:1:N-1])translate([2.5*a,0,-3.2])hull()
	{ // messy but F5 was not showing at all
		cylinder(d=0.64,h=0.1,$fn=12);
		translate([0,0,3.2-pcbthickness])cylinder(d=2,h=pcbthickness,$fn=12);
	}
}
if(hole)
{
	b(2.5*(N/2)-1.25,5+3.6,-0.01,2.5*N+2.5,6+10,4);
}
}

module m10(part=false,hole=false,block=false,height)
{ // L1
// 5x5x4 Inductor
if(part)
{
	b(0,0,0,5,5,4.3);
}
}

// Generate PCB casework

height=casebottom+pcbthickness+casetop;
$fn=48;

module pyramid()
{ // A pyramid
 polyhedron(points=[[0,0,0],[-height,-height,height],[-height,height,height],[height,height,height],[height,-height,height]],faces=[[0,1,2],[0,2,3],[0,3,4],[0,4,1],[4,3,2,1]]);
}


module pcb_hulled(h=pcbthickness,r=0)
{ // PCB shape for case
	if(useredge)outline(h,r);
	else hull()outline(h,r);
}

module solid_case(d=0)
{ // The case wall
	hull()
        {
                translate([0,0,-casebottom])pcb_hulled(height,casewall-edge);
                translate([0,0,edge-casebottom])pcb_hulled(height-edge*2,casewall);
        }
}

module preview()
{
	pcb();
	color("#0f0")parts_top(part=true);
	color("#0f0")parts_bottom(part=true);
	color("#f00")parts_top(hole=true);
	color("#f00")parts_bottom(hole=true);
	color("#00f8")parts_top(block=true);
	color("#00f8")parts_bottom(block=true);
}

module top_half(fit=0)
{
	difference()
	{
		translate([-casebottom-100,-casewall-100,pcbthickness+0.01]) cube([pcbwidth+casewall*2+200,pcblength+casewall*2+200,height]);
		translate([0,0,pcbthickness])
        	{
			snape=lip/5;
			snaph=(lip-snape*2)/3;
			if(lipt==1)rotate(lipa)hull()
			{
				translate([0,-pcblength,lip/2])cube([0.001,pcblength*2,0.001]);
				translate([-lip/2,-pcblength,0])cube([lip,pcblength*2,0.001]);
			} else if(lipt==2)for(a=[0,90,180,270])rotate(a+lipa)hull()
			{
				translate([0,-pcblength-pcbwidth,lip/2])cube([0.001,pcblength*2+pcbwidth*2,0.001]);
				translate([-lip/2,-pcblength-pcbwidth,0])cube([lip,pcblength*2+pcbwidth*2,0.001]);
			}
            		difference()
            		{
                		pcb_hulled(lip,casewall);
				if(snap)
                        	{
					hull()
					{
						pcb_hulled(0.1,casewall/2-snap/2+fit);
						translate([0,0,snape])pcb_hulled(snaph,casewall/2+snap/2+fit);
						translate([0,0,lip-snape-snaph])pcb_hulled(0.1,casewall/2-snap/2+fit);
					}
					translate([0,0,lip-snape-snaph])pcb_hulled(snaph,casewall/2-snap/2+fit);
					hull()
					{
						translate([0,0,lip-snape])pcb_hulled(0.1,casewall/2-snap/2+fit);
						translate([0,0,lip])pcb_hulled(0.1,casewall/2+snap/2+fit);
					}
                        	}
				else pcb_hulled(lip,casewall/2+fit);
				if(lipt==0)translate([-pcbwidth,-pcblength,0])cube([pcbwidth*2,pcblength*2,lip]);
				else if(lipt==1) rotate(lipa)translate([0,-pcblength,0])hull()
				{
					translate([lip/2,0,0])cube([pcbwidth,pcblength*2,lip]);
					translate([-lip/2,0,lip])cube([pcbwidth,pcblength*2,lip]);
				}
				else if(lipt==2)for(a=[0,180])rotate(a+lipa)hull()
                		{
                            		translate([lip/2,lip/2,0])cube([pcbwidth+pcblength,pcbwidth+pcblength,lip]);
                            		translate([-lip/2,-lip/2,lip])cube([pcbwidth+pcblength,pcbwidth+pcblength,lip]);
                		}
            		}
            		difference()
            		{
				if(snap)
                        	{
					hull()
					{
						translate([0,0,-0.1])pcb_hulled(0.1,casewall/2+snap/2-fit);
						translate([0,0,snape-0.1])pcb_hulled(0.1,casewall/2-snap/2-fit);
					}
					translate([0,0,snape])pcb_hulled(snaph,casewall/2-snap/2-fit);
					hull()
					{
						translate([0,0,snape+snaph])pcb_hulled(0.1,casewall/2-snap/2-fit);
						translate([0,0,lip-snape-snaph])pcb_hulled(snaph,casewall/2+snap/2-fit);
						translate([0,0,lip-0.1])pcb_hulled(0.1,casewall/2-snap/2-fit);
					}
                        	}
				else pcb_hulled(lip,casewall/2-fit);
				if(lipt==1)rotate(lipa+180)translate([0,-pcblength,0])hull()
				{
					translate([lip/2,0,0])cube([pcbwidth,pcblength*2,lip+0.1]);
					translate([-lip/2,0,lip])cube([pcbwidth,pcblength*2,lip+0.1]);
				}
				else if(lipt==2)for(a=[90,270])rotate(a+lipa)hull()
                		{
                            		translate([lip/2,lip/2,0])cube([pcbwidth+pcblength,pcbwidth+pcblength,lip]);
                            		translate([-lip/2,-lip/2,lip])cube([pcbwidth+pcblength,pcbwidth+pcblength,lip]);
                		}
			}
            	}
		minkowski()
                {
                	union()
                	{
                		parts_top(part=true);
                		parts_bottom(part=true);
                	}
                	translate([-0.01,-0.01,-height])cube([0.02,0.02,height]);
                }
        }
	minkowski()
        {
        	union()
                {
                	parts_top(part=true);
                	parts_bottom(part=true);
                }
                translate([-0.01,-0.01,0])cube([0.02,0.02,height]);
        }
}

module case_wall()
{
	difference()
	{
		solid_case();
		translate([0,0,-height])pcb_hulled(height*2);
	}
}

module top_side_hole()
{
	difference()
	{
		intersection()
		{
			parts_top(hole=true);
			case_wall();
		}
		translate([0,0,-casebottom])pcb_hulled(height,casewall-edge);
	}
}

module bottom_side_hole()
{
	difference()
	{
		intersection()
		{
			parts_bottom(hole=true);
			case_wall();
		}
		translate([0,0,edge-casebottom])pcb_hulled(height-edge*2,casewall);
	}
}

module parts_space()
{
	minkowski()
	{
		union()
		{
			parts_top(part=true,hole=true);
			parts_bottom(part=true,hole=true);
		}
		sphere(r=margin,$fn=6);
	}
}

module top_cut(fit=0)
{
	difference()
	{
		top_half(fit);
		if(parts_top)difference()
		{
			minkowski()
			{ // Penetrating side holes
				top_side_hole();
				rotate([180,0,0])
				pyramid();
			}
			minkowski()
			{
				top_side_hole();
				rotate([0,0,45])cylinder(r=margin,h=height,$fn=4);
			}
		}
	}
	if(parts_bottom)difference()
	{
		minkowski()
		{ // Penetrating side holes
			bottom_side_hole();
			pyramid();
		}
			minkowski()
			{
				bottom_side_hole();
				rotate([0,0,45])translate([0,0,-height])cylinder(r=margin,h=height,$fn=4);
			}
	}
}

module bottom_cut()
{
	difference()
	{
		 translate([-casebottom-50,-casewall-50,-height]) cube([pcbwidth+casewall*2+100,pcblength+casewall*2+100,height*2]);
		 top_cut(-fit);
	}
}

module top_body()
{
	difference()
	{
		intersection()
		{
			solid_case();
			pcb_hulled(height);
		}
		if(parts_top)minkowski()
		{
			if(nohull)parts_top(part=true);
			else hull(){parts_top(part=true);pcb_hulled();}
			translate([0,0,margin-height])cylinder(r=margin*2,h=height,$fn=8);
		}
	}
	intersection()
	{
		solid_case();
		parts_top(block=true);
	}
}

module top_edge()
{
	intersection()
	{
		case_wall();
		top_cut();
	}
}

module top_pos()
{ // Position for plotting bottom
	translate([0,0,pcbthickness+casetop])rotate([180,0,0])children();
}

module pcb_pos()
{	// Position PCB relative to base 
		translate([0,0,pcbthickness-height])children();
}

module top()
{
	top_pos()difference()
	{
		union()
		{
			top_body();
			top_edge();
		}
		parts_space();
		pcb_pos()pcb(height,r=margin);
	}
}

module bottom_body()
{ // Position for plotting top
	difference()
	{
		intersection()
		{
			solid_case();
			translate([0,0,-height])pcb_hulled(height+pcbthickness);
		}
		if(parts_bottom)minkowski()
		{
			if(nohull)parts_bottom(part=true);
			else hull()parts_bottom(part=true);
			translate([0,0,-margin])cylinder(r=margin*2,h=height,$fn=8);
		}
	}
	intersection()
	{
		solid_case();
		parts_bottom(block=true);
	}
}

module bottom_edge()
{
	intersection()
	{
		case_wall();
		bottom_cut();
	}
}

module bottom_pos()
{
	translate([0,0,casebottom])children();
}

module bottom()
{
	bottom_pos()difference()
	{
		union()
		{
        		bottom_body();
        		bottom_edge();
		}
		parts_space();
		pcb(height,r=margin);
	}
}

module datecode()
{
	minkowski()
	{
		translate([datex,datey,-1])rotate(datea)scale([-1,1])linear_extrude(1)text(date,size=dateh,halign="center",valign="center",font=datef);
		cylinder(d1=datet,d2=0,h=datet,$fn=6);
	}
}
top();
