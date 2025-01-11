// Generated case design for Faikin/Faikin.kicad_pcb
// By https://github.com/revk/PCBCase
// Generated 2025-01-11 14:07:09
// title:	PCB-FAIKIN
// rev:	1
// company:	Adrian Kennard, Andrews & Arnold Ltd
//

// Globals
margin=0.200000;
lip=2.000000;
casebottom=2.600000;
casetop=4.600000;
casewall=3.000000;
fit=0.000000;
edge=2.000000;
pcbthickness=1.200000;
nohull=false;
hullcap=1.000000;
hulledge=1.000000;
useredge=false;

module outline(h=pcbthickness,r=0){linear_extrude(height=h)offset(r=r)polygon(points=[[31.500000,0.500000],[31.500000,15.500000],[31.353553,15.853553],[31.000000,16.000000],[0.750000,16.000000],[0.375000,15.899519],[0.100481,15.625000],[0.000000,15.250000],[0.000000,0.750000],[0.100481,0.375000],[0.375000,0.100481],[0.750000,0.000000],[31.000000,0.000000],[31.353553,0.146447]],paths=[[0,1,2,3,4,5,6,7,8,9,10,11,12,13]]);}

module pcb(h=pcbthickness,r=0){linear_extrude(height=h)offset(r=r)polygon(points=[[31.500000,0.500000],[31.500000,15.500000],[31.353553,15.853553],[31.000000,16.000000],[0.750000,16.000000],[0.375000,15.899519],[0.100481,15.625000],[0.000000,15.250000],[0.000000,0.750000],[0.100481,0.375000],[0.375000,0.100481],[0.750000,0.000000],[31.000000,0.000000],[31.353553,0.146447]],paths=[[0,1,2,3,4,5,6,7,8,9,10,11,12,13]]);}
spacing=47.500000;
pcbwidth=31.500000;
pcblength=16.000000;
// Parts to go on PCB (top)
module parts_top(part=false,hole=false,block=false){
translate([21.600000,13.000000,1.200000])rotate([0,0,90.000000])m0(part,hole,block,casetop); // RevK:R_0402 R_0402_1005Metric (back)
translate([21.230000,4.185000,1.200000])rotate([0,0,90.000000])m1(part,hole,block,casetop); // RevK:C_0603_ C_0603_1608Metric (back)
translate([16.700000,11.275000,1.200000])rotate([0,0,180.000000])m2(part,hole,block,casetop); // RevK:SMD1010+ SMD1010 (back)
translate([21.600000,14.850000,1.200000])rotate([0,0,-90.000000])m3(part,hole,block,casetop); // D3 (back)
translate([16.500000,14.400000,1.200000])m4(part,hole,block,casetop); // RevK:C_0402 C_0402_1005Metric (back)
translate([16.930000,1.000000,1.200000])rotate([0,0,180.000000])m0(part,hole,block,casetop); // RevK:R_0402 R_0402_1005Metric (back)
translate([20.700000,14.850000,1.200000])rotate([0,0,90.000000])m3(part,hole,block,casetop); // D3 (back)
translate([18.600000,13.400000,1.200000])rotate([0,0,180.000000])m5(part,hole,block,casetop); // Q1 (back)
translate([18.730000,2.885000,1.200000])rotate([0,0,180.000000])m6(part,hole,block,casetop); // RevK:SOT-23-6-MD8942 SOT-23-6 (back)
translate([17.200000,15.300000,1.200000])rotate([0,0,180.000000])m0(part,hole,block,casetop); // RevK:R_0402 R_0402_1005Metric (back)
translate([20.700000,13.000000,1.200000])rotate([0,0,90.000000])m3(part,hole,block,casetop); // D3 (back)
translate([20.530000,1.000000,1.200000])rotate([0,0,180.000000])m0(part,hole,block,casetop); // RevK:R_0402 R_0402_1005Metric (back)
translate([21.100000,10.500000,1.200000])rotate([0,0,90.000000])m4(part,hole,block,casetop); // RevK:C_0402 C_0402_1005Metric (back)
translate([7.830000,8.000000,1.200000])rotate([0,0,90.000000])m7(part,hole,block,casetop); // U4 (back)
translate([18.730000,4.885000,1.200000])m1(part,hole,block,casetop); // RevK:C_0603_ C_0603_1608Metric (back)
translate([23.555000,13.000000,1.200000])rotate([0,0,-90.000000])m8(part,hole,block,casetop,05); // J1 (back)
translate([20.100000,10.800000,1.200000])rotate([0,0,90.000000])m1(part,hole,block,casetop); // RevK:C_0603_ C_0603_1608Metric (back)
translate([18.730000,7.485000,1.200000])rotate([-0.000000,-0.000000,-90.000000])m9(part,hole,block,casetop); // RevK:L_4x4_ TYA4020 (back)
translate([19.400000,15.300000,1.200000])m0(part,hole,block,casetop); // RevK:R_0402 R_0402_1005Metric (back)
translate([16.230000,4.185000,1.200000])rotate([0,0,-90.000000])m1(part,hole,block,casetop); // RevK:C_0603_ C_0603_1608Metric (back)
translate([18.730000,0.985000,1.200000])m0(part,hole,block,casetop); // RevK:R_0402 R_0402_1005Metric (back)
}

parts_top=5;
// Parts to go on PCB (bottom)
module parts_bottom(part=false,hole=false,block=false){
}

parts_bottom=0;
module b(cx,cy,z,w,l,h){translate([cx-w/2,cy-l/2,z])cube([w,l,h]);}
module m0(part=false,hole=false,block=false,height)
{ // RevK:R_0402 R_0402_1005Metric
// 0402 Resistor
if(part)
{
	b(0,0,0,1.5,0.65,0.2); // Pad size
	b(0,0,0,1.0,0.5,0.5); // Chip
}
}

module m1(part=false,hole=false,block=false,height)
{ // RevK:C_0603_ C_0603_1608Metric
// 0603 Capacitor
if(part)
{
	b(0,0,0,1.6,0.8,1); // Chip
	b(0,0,0,1.6,0.95,0.2); // Pad size
}
}

module m2(part=false,hole=false,block=false,height)
{ // RevK:SMD1010+ SMD1010
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
                translate([0,0,height])cylinder(d=2,h=1,$fn=16);
        }
}
if(block)
{
        hull()
        {
                b(0,0,0,2.4,2.4,1);
                translate([0,0,height])cylinder(d=4,h=1,$fn=16);
        }
}
}

module m3(part=false,hole=false,block=false,height)
{ // D3
// DFN1006-2L
if(part)
{
	b(0,0,0,1.0,0.6,0.45); // Chip
}
}

module m4(part=false,hole=false,block=false,height)
{ // RevK:C_0402 C_0402_1005Metric
// 0402 Capacitor
if(part)
{
	b(0,0,0,1.0,0.5,1); // Chip
	b(0,0,0,1.5,0.65,0.2); // Pad size
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
{ // RevK:SOT-23-6-MD8942 SOT-23-6
// SOT-23-6
if(part)
{
	b(0,0,0,1.726,3.026,1.2); // Part
	b(0,0,0,3.6,2.5,0.5); // Pins
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
	}
	if(hole)
	{
		cube([15.4,20.5,0.8]);
	}
}
}

module m8(part=false,hole=false,block=false,height,N=0)
{ // J1
if(part)
{
	b(2.5*(N/2)-1.25,3.6,0,2.5*N+2.5,6,4);
	b(2.5*(N/2)-1.25,0,0,2.5*N+2.5,3.2,1.5);
	for(a=[0:1:N-1])translate([2.5*a,0,-3])cylinder(d1=0.5,d2=2.5,h=3,$fn=12);
}
if(hole)
{
	b(2.5*(N/2)-1.25,5+3.6,0,2.5*N+2.5,6+10,4);
}
}

module m9(part=false,hole=false,block=false,height)
{ // RevK:L_4x4_ TYA4020
// 4x4 Inductor
if(part)
{
	b(0,0,0,4,4,3);
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

module top_half(step=false)
{
	difference()
	{
		translate([-casebottom-100,-casewall-100,pcbthickness-lip/2+0.01]) cube([pcbwidth+casewall*2+200,pcblength+casewall*2+200,height]);
		if(step)translate([0,0,pcbthickness-lip/2-0.01])pcb_hulled(lip,casewall/2+fit);
	}
}

module bottom_half(step=false)
{
	translate([-casebottom-100,-casewall-100,pcbthickness+lip/2-height-0.01]) cube([pcbwidth+casewall*2+200,pcblength+casewall*2+200,height]);
	if(step)translate([0,0,pcbthickness-lip/2])pcb_hulled(lip,casewall/2-fit);
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
	intersection()
	{
		parts_top(hole=true);
		case_wall();
	}
}

module bottom_side_hole()
{
	intersection()
	{
		parts_bottom(hole=true);
		case_wall();
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

module top_cut()
{
	difference()
	{
		top_half(true);
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
		 top_cut();
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
			top_half();
		}
		if(parts_top)minkowski()
		{
			if(nohull)parts_top(part=true);
			else hull()parts_top(part=true);
			translate([0,0,margin-height])cylinder(r=margin,h=height,$fn=8);
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

module top()
{
	translate([casewall,casewall+pcblength,pcbthickness+casetop])rotate([180,0,0])difference()
	{
		union()
		{
			top_body();
			top_edge();
		}
		parts_space();
		translate([0,0,pcbthickness-height])pcb(height,r=margin);
	}
}

module bottom_body()
{
	difference()
	{
		intersection()
		{
			solid_case();
			translate([0,0,-height])pcb_hulled(height);
			bottom_half();
		}
		if(parts_bottom)minkowski()
		{
			if(nohull)parts_bottom(part=true);
			else hull()parts_bottom(part=true);
			translate([0,0,-margin])cylinder(r=margin,h=height,$fn=8);
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

module bottom()
{
	translate([casewall,casewall,casebottom])difference()
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
bottom(); translate([spacing,0,0])top();
