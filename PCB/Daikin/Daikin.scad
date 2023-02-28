// Generated case design for PCB/Daikin/Daikin.kicad_pcb
// By https://github.com/revk/PCBCase
// Generated 2023-02-14 10:56:44
// title:	Daikin ESP32-PICO-MINI Module
// date:	${DATE}
// rev:	1
// company:	Adrian Kennard, Andrews & Arnold Ltd
//

// Globals
margin=0.500000;
overlap=2.000000;
lip=0.000000;
casebase=3.000000;
casetop=5.000000;
casewall=3.000000;
fit=0.000000;
edge=2.000000;
pcbthickness=0.800000;
nohull=false;
hullcap=1.000000;
hulledge=1.000000;
useredge=false;

module outline(h=pcbthickness,r=0){linear_extrude(height=h)offset(r=r)polygon(points=[[13.200000,0.400000],[13.200000,22.600000],[13.082843,22.882843],[12.800000,23.000000],[0.400000,23.000000],[0.117157,22.882843],[0.000000,22.600000],[0.000000,0.400000],[0.117157,0.117157],[0.400000,0.000000],[12.800000,0.000000],[13.082843,0.117157]],paths=[[0,1,2,3,4,5,6,7,8,9,10,11]]);}

module pcb(h=pcbthickness,r=0){linear_extrude(height=h)offset(r=r)polygon(points=[[13.200000,0.400000],[13.200000,22.600000],[13.082843,22.882843],[12.800000,23.000000],[0.400000,23.000000],[0.117157,22.882843],[0.000000,22.600000],[0.000000,0.400000],[0.117157,0.117157],[0.400000,0.000000],[12.800000,0.000000],[13.082843,0.117157]],paths=[[0,1,2,3,4,5,6,7,8,9,10,11]]);}
spacing=29.200000;
pcbwidth=13.200000;
pcblength=23.000000;
// Populated PCB
module board(pushed=false,hulled=false){
translate([6.600000,17.406750,0.800000])translate([0.000000,2.700000,0.000000])rotate([90.000000,-0.000000,-0.000000])m0(pushed,hulled); // RevK:ESP32-PICO-MINI-02 ESP32-PICO-MINI-02 (back)
translate([6.600000,10.100000,0.800000])translate([5.080000,0.000000,0.000000])rotate([-0.000000,-0.000000,-90.000000])m1(pushed,hulled); // RevK:PinHeader_1x05_P2.54mm_Right PinHeader_1x05_P2.54mm_Horizontal (back)
translate([1.050000,16.450000,0.000000])rotate([0,0,-90.000000])rotate([180,0,0])m2(pushed,hulled); // RevK:C_0805_ C_0805_2012Metric
translate([10.000000,16.900000,0.000000])rotate([0,0,-90.000000])rotate([180,0,0])m3(pushed,hulled); // RevK:R_0603 R_0603_1608Metric
translate([4.150000,12.850000,0.000000])rotate([0,0,90.000000])rotate([180,0,0])m4(pushed,hulled); // RevK:C_0603 C_0603_1608Metric
translate([10.850000,12.600000,0.000000])rotate([0,0,180.000000])rotate([180,0,0])rotate([-0.000000,-0.000000,-180.000000])m5(pushed,hulled); // RevK:SOT-323_SC-70 SOT-323_SC-70
translate([4.170000,16.830000,0.000000])rotate([0,0,180.000000])rotate([180,0,0])m6(pushed,hulled); // RevK:D_1206_ D_1206_3216Metric
translate([8.550000,19.750000,0.000000])rotate([0,0,-90.000000])rotate([180,0,0])m4(pushed,hulled); // RevK:C_0603 C_0603_1608Metric
translate([9.125000,22.100000,0.000000])rotate([180,0,0])m3(pushed,hulled); // RevK:R_0603 R_0603_1608Metric
translate([11.200000,16.900000,0.000000])rotate([0,0,-90.000000])rotate([180,0,0])m3(pushed,hulled); // RevK:R_0603 R_0603_1608Metric
translate([12.400000,16.900000,0.000000])rotate([0,0,-90.000000])rotate([180,0,0])m3(pushed,hulled); // RevK:R_0603 R_0603_1608Metric
translate([2.200000,13.350000,0.000000])rotate([180,0,0])m3(pushed,hulled); // RevK:R_0603 R_0603_1608Metric
translate([7.300000,16.450000,0.000000])rotate([0,0,-90.000000])rotate([180,0,0])m2(pushed,hulled); // RevK:C_0805_ C_0805_2012Metric
translate([8.000000,11.800000,0.000000])rotate([180,0,0])m3(pushed,hulled); // RevK:R_0603 R_0603_1608Metric
translate([11.825000,19.500000,0.000000])rotate([0,0,90.000000])rotate([180,0,0])translate([0.000000,-0.100000,0.400000])rotate([-90.000000,-0.000000,-0.000000])m7(pushed,hulled); // RevK:MHS190RGBCT LED_0603_1608Metric
translate([2.150000,12.100000,0.000000])rotate([180,0,0])m4(pushed,hulled); // RevK:C_0603 C_0603_1608Metric
translate([6.450000,20.000000,0.000000])rotate([0,0,180.000000])rotate([180,0,0])rotate([-0.000000,-0.000000,-90.000000])m8(pushed,hulled); // RevK:SOT-23-Thin-6-Reg SOT-23-6
translate([2.550000,20.000000,0.000000])rotate([180,0,0])rotate([-0.000000,-0.000000,-90.000000])m9(pushed,hulled); // RevK:L_4x4_ TYA4020
translate([11.600000,22.100000,0.000000])rotate([0,0,180.000000])rotate([180,0,0])m3(pushed,hulled); // RevK:R_0603 R_0603_1608Metric
}

module b(cx,cy,z,w,l,h){translate([cx-w/2,cy-l/2,z])cube([w,l,h]);}
module m0(pushed=false,hulled=false)
{ // RevK:ESP32-PICO-MINI-02 ESP32-PICO-MINI-02
rotate([-90,0,0])
translate([-13.2/2,-16.6/2,0])
{
	if(!hulled)cube([13.2,16.6,0.8]);
	cube([13.2,11.2,2.4]);
}
}

module m1(pushed=false,hulled=false)
{ // RevK:PinHeader_1x05_P2.54mm_Right PinHeader_1x05_P2.54mm_Horizontal
n=5;
if(!hulled)for(x=[0:1:n-1])translate([-0.25,-x*2.54-0.25,-2.5])cube([0.5,0.5,3]); // Un-cropped pins
for(x=[0:1:n-1])translate([0,-x*2.54,-0.81])cylinder(d=2,h=1); // Cropped pins / solder
for(x=[0:1:n-1])translate([0,-x*2.54,-1.8])cylinder(d1=1,d2=2,h=1); // Cropped pins / solder
translate([-1.27,-(n-0.5)*2.54,0]) // Header plastic
cube([3,n*2.54,2.54]);
translate([1.27,-(n-0.5)*2.54-0.5,0]) // Plug
cube([10,n*2.54+1,2.54+1]);
}

module m2(pushed=false,hulled=false)
{ // RevK:C_0805_ C_0805_2012Metric
b(0,0,0,2,1.45,0.2); // Pad size
b(0,0,0,2,1.2,1); // Chip
}

module m3(pushed=false,hulled=false)
{ // RevK:R_0603 R_0603_1608Metric
b(0,0,0,2.8,0.95,0.5); // Pad size
}

module m4(pushed=false,hulled=false)
{ // RevK:C_0603 C_0603_1608Metric
b(0,0,0,1.6,0.95,0.2); // Pad size
b(0,0,0,1.6,0.8,1); // Chip
}

module m5(pushed=false,hulled=false)
{ // RevK:SOT-323_SC-70 SOT-323_SC-70
b(0,0,0,1.26,2.2,1.2);
b(0,0,0,2.2,2.2,0.6);
}

module m6(pushed=false,hulled=false)
{ // RevK:D_1206_ D_1206_3216Metric
b(0,0,0,4.4,1.75,1.2); // Pad size
}

module m7(pushed=false,hulled=false)
{ // RevK:MHS190RGBCT LED_0603_1608Metric
b(0,0,0,1.6,0.8,0.25);
b(0,0,0,1.2,0.8,0.55);
b(0,0,0,0.8,0.8,0.95);
if(!hulled&&pushed)b(0,0,0,1,1,20);
}

module m8(pushed=false,hulled=false)
{ // RevK:SOT-23-Thin-6-Reg SOT-23-6
b(0,0,0,3.05,3.05,0.5);
b(0,0,0,1.45,3.05,1.1);
}

module m9(pushed=false,hulled=false)
{ // RevK:L_4x4_ TYA4020
b(0,0,0,4,4,2.1);
}

height=casebase+pcbthickness+casetop;
$fn=48;

module boardh(pushed=false)
{ // Board with hulled parts
	union()
	{
		if(!nohull)intersection()
		{
			translate([0,0,hullcap-casebase])outline(casebase+pcbthickness+casetop-hullcap*2,-hulledge);
			hull()board(pushed,true);
		}
		board(pushed,false);
		pcb();
	}
}

module boardf()
{ // This is the board, but stretched up to make a push out in from the front
	render()
	{
		intersection()
		{
			translate([-casewall-1,-casewall-1,-casebase-1]) cube([pcbwidth+casewall*2+2,pcblength+casewall*2+2,height+2]);
			union()
			{
				minkowski()
				{
					boardh(true);
					cylinder(h=height+100,d=margin,$fn=8);
				}
				board(false,false);
			}
		}
	}
}

module boardb()
{ // This is the board, but stretched down to make a push out in from the back
	render()
	{
		intersection()
		{
			translate([-casewall-1,-casewall-1,-casebase-1]) cube([pcbwidth+casewall*2+2,pcblength+casewall*2+2,height+2]);
			union()
			{
				minkowski()
				{
					boardh(true);
					translate([0,0,-height-100])
					cylinder(h=height+100,d=margin,$fn=8);
				}
				board(false,false);
			}
		}
	}
}

module boardm()
{
	render()
	{
 		minkowski()
 		{
			translate([0,0,-margin/2])cylinder(d=margin,h=margin,$fn=8);
 			boardh(false);
		}
		//intersection()
    		//{
        		//translate([0,0,-(casebase-hullcap)])pcb(pcbthickness+(casebase-hullcap)+(casetop-hullcap));
        		//translate([0,0,-(casebase-hullcap)])outline(pcbthickness+(casebase-hullcap)+(casetop-hullcap));
			boardh(false);
    		//}
 	}
}

module pcbh(h=pcbthickness,r=0)
{ // PCB shape for case
	if(useredge)outline(h,r);
	else hull()outline(h,r);
}

module pyramid()
{ // A pyramid
 polyhedron(points=[[0,0,0],[-height,-height,height],[-height,height,height],[height,height,height],[height,-height,height]],faces=[[0,1,2],[0,2,3],[0,3,4],[0,4,1],[4,3,2,1]]);
}

module wall(d=0)
{ // The case wall
	translate([0,0,-casebase-d])
	{
		if(useredge)
			intersection()
			{
				pcb(height+d*2,margin/2+d);
				pcbh(height+d*2,margin/2+d);
			}
		else pcbh(height+d*2,margin/2+d);
	}
}

module cutf()
{ // This cut up from base in the wall
	intersection()
	{
		boardf();
		difference()
		{
			translate([-casewall+0.01,-casewall+0.01,-casebase+0.01])cube([pcbwidth+casewall*2-0.02,pcblength+casewall*2-0.02,casebase+overlap+lip]);
			wall();
			boardb();
		}
	}
}

module cutb()
{ // The cut down from top in the wall
	intersection()
	{
		boardb();
		difference()
		{
			translate([-casewall+0.01,-casewall+0.01,0.01])cube([pcbwidth+casewall*2-0.02,pcblength+casewall*2-0.02,casetop+pcbthickness]);
			wall();
			boardf();
		}
	}
}

module cutpf()
{ // the push up but pyramid
	render()
	intersection()
	{
		minkowski()
		{
			pyramid();
			cutf();
		}
		difference()
		{
			translate([-casewall-0.01,-casewall-0.01,-casebase-0.01])cube([pcbwidth+casewall*2+0.02,pcblength+casewall*2+0.02,casebase+overlap+lip+0.02]);
			wall();
			boardh(true);
		}
		translate([-casewall,-casewall,-casebase])case();
	}
}

module cutpb()
{ // the push down but pyramid
	render()
	intersection()
	{
		minkowski()
		{
			scale([1,1,-1])pyramid();
			cutb();
		}
		difference()
		{
			translate([-casewall-0.01,-casewall-0.01,-0.01])cube([pcbwidth+casewall*2+0.02,pcblength+casewall*2+0.02,casetop+pcbthickness+0.02]);
			wall();
			boardh(true);
		}
		translate([-casewall,-casewall,-casebase])case();
	}
}

module case()
{ // The basic case
	hull()
	{
		translate([casewall,casewall,0])pcbh(height,casewall-edge);
		translate([casewall,casewall,edge])pcbh(height-edge*2,casewall);
	}
}

module cut(d=0)
{ // The cut point in the wall
	translate([casewall,casewall,casebase+lip])pcbh(casetop+pcbthickness-lip+1,casewall/2+d/2+margin/4);
}

module base()
{ // The base
	difference()
	{
		case();
		difference()
		{
			union()
			{
				translate([-1,-1,casebase+overlap+lip])cube([pcbwidth+casewall*2+2,pcblength+casewall*2+2,casetop+1]);
				cut(fit);
			}
		}
		translate([casewall,casewall,casebase])boardf();
		translate([casewall,casewall,casebase])boardm();
		translate([casewall,casewall,casebase])cutpf();
	}
	translate([casewall,casewall,casebase])cutpb();
}

module top()
{
	translate([0,pcblength+casewall*2,height])rotate([180,0,0])
	{
		difference()
		{
			case();
			difference()
			{
				translate([-1,-1,-1])cube([pcbwidth+casewall*2+2,pcblength+casewall*2+2,casebase+overlap+lip-margin+1]);
				cut(-fit);
			}
			translate([casewall,casewall,casebase])boardb();
			translate([casewall,casewall,casebase])boardm();
			translate([casewall,casewall,casebase])cutpb();
		}
		translate([casewall,casewall,casebase])cutpf();
	}
}

module test()
{
	translate([0*spacing,0,0])base();
	translate([1*spacing,0,0])top();
	translate([2*spacing,0,0])pcb();
	translate([3*spacing,0,0])outline();
	translate([4*spacing,0,0])wall();
	translate([5*spacing,0,0])board();
	translate([6*spacing,0,0])board(false,true);
	translate([7*spacing,0,0])board(true);
	translate([8*spacing,0,0])boardh();
	translate([9*spacing,0,0])boardf();
	translate([10*spacing,0,0])boardb();
	translate([11*spacing,0,0])cutpf();
	translate([12*spacing,0,0])cutpb();
	translate([13*spacing,0,0])cutf();
	translate([14*spacing,0,0])cutb();
	translate([15*spacing,0,0])case();
}

module parts()
{
	base();
	translate([spacing,0,0])top();
}
base(); translate([spacing,0,0])top();
