// Generated case design for Faikin2/Faikin.kicad_pcb
// By https://github.com/revk/PCBCase
// Generated 2023-08-10 08:54:14
// title:	PCB-FAIKIN
// rev:	1
// company:	Adrian Kennard, Andrews & Arnold Ltd
//

// Globals
margin=0.500000;
overlap=2.000000;
lip=0.000000;
casebase=2.600000;
casetop=4.000000;
casewall=3.000000;
fit=0.000000;
edge=2.000000;
pcbthickness=0.800000;
nohull=false;
hullcap=1.000000;
hulledge=1.000000;
useredge=false;

module outline(h=pcbthickness,r=0){linear_extrude(height=h)offset(r=r)polygon(points=[[0.000000,0.000000],[0.000000,15.400000],[30.700000,15.400000],[30.700000,0.000000]],paths=[[0,1,2,3]]);}

module pcb(h=pcbthickness,r=0){linear_extrude(height=h)offset(r=r)polygon(points=[[0.000000,0.000000],[0.000000,15.400000],[30.700000,15.400000],[30.700000,0.000000]],paths=[[0,1,2,3]]);}
spacing=46.700000;
pcbwidth=30.700000;
pcblength=15.400000;
// Populated PCB
module board(pushed=false,hulled=false){
translate([22.100000,6.200000,0.800000])rotate([0,0,180.000000])m2(pushed,hulled); // RevK:R_0402 R_0402_1005Metric (back)
translate([21.100000,3.885000,0.800000])rotate([0,0,90.000000])m5(pushed,hulled); // RevK:C_0603_ C_0603_1608Metric (back)
translate([18.250000,12.800000,0.800000])rotate([0,0,90.000000])translate([0.000000,-0.400000,0.000000])m8(pushed,hulled); // RevK:LED-RGB-1.6x1.6 LED_0603_1608Metric (back)
translate([18.250000,12.800000,0.800000])rotate([0,0,90.000000])translate([0.000000,0.400000,0.000000])m8(pushed,hulled); // RevK:LED-RGB-1.6x1.6 LED_0603_1608Metric (back)
translate([16.500000,11.900000,0.800000])rotate([0,0,180.000000])m2(pushed,hulled); // RevK:R_0402 R_0402_1005Metric (back)
translate([16.500000,14.700000,0.800000])rotate([0,0,180.000000])m12(pushed,hulled); // RevK:C_0402 C_0402_1005Metric (back)
translate([22.100000,10.200000,0.800000])rotate([0,0,180.000000])m12(pushed,hulled); // RevK:C_0402 C_0402_1005Metric (back)
translate([16.100000,1.785000,0.800000])rotate([0,0,-90.000000])m2(pushed,hulled); // RevK:R_0402 R_0402_1005Metric (back)
translate([20.600000,12.650000,0.800000])rotate([0,0,-90.000000])m15(pushed,hulled); // RevK:SOT-363_SC-70-6 SOT-363_SC-70-6 (back)
translate([16.500000,12.800000,0.800000])rotate([0,0,180.000000])m2(pushed,hulled); // RevK:R_0402 R_0402_1005Metric (back)
translate([18.600000,2.585000,0.800000])rotate([0,0,180.000000])m18(pushed,hulled); // RevK:SOT-23-6-MD8942 SOT-23-6 (back)
translate([18.900000,14.700000,0.800000])rotate([0,0,180.000000])m2(pushed,hulled); // RevK:R_0402 R_0402_1005Metric (back)
translate([21.100000,1.785000,0.800000])rotate([0,0,-90.000000])m2(pushed,hulled); // RevK:R_0402 R_0402_1005Metric (back)
translate([17.100000,9.700000,0.800000])rotate([0,0,180.000000])m12(pushed,hulled); // RevK:C_0402 C_0402_1005Metric (back)
translate([22.100000,9.200000,0.800000])m2(pushed,hulled); // RevK:R_0402 R_0402_1005Metric (back)
translate([22.100000,7.200000,0.800000])m2(pushed,hulled); // RevK:R_0402 R_0402_1005Metric (back)
translate([7.700000,7.700000,0.800000])rotate([0,0,90.000000])m19(pushed,hulled); // RevK:ESP32-S3-MINI-1 ESP32-S3-MINI-1 (back)
translate([18.600000,4.585000,0.800000])m5(pushed,hulled); // RevK:C_0603_ C_0603_1608Metric (back)
translate([24.200000,12.700000,0.800000])rotate([0,0,-90.000000])m21(pushed,hulled,5); // RevK:JST_EH_S5B-EH_1x05_P2.50mm_Horizontal JST_EH_S5B-EH_1x05_P2.50mm_Horizontal (back)
translate([18.950000,9.700000,0.800000])m12(pushed,hulled); // RevK:C_0402 C_0402_1005Metric (back)
translate([18.600000,7.185000,0.800000])rotate([-0.000000,-0.000000,-90.000000])m24(pushed,hulled); // RevK:L_4x4_ TYA4020 (back)
translate([16.500000,13.700000,0.800000])rotate([0,0,180.000000])m2(pushed,hulled); // RevK:R_0402 R_0402_1005Metric (back)
translate([16.100000,3.885000,0.800000])rotate([0,0,-90.000000])m5(pushed,hulled); // RevK:C_0603_ C_0603_1608Metric (back)
translate([18.600000,0.685000,0.800000])m2(pushed,hulled); // RevK:R_0402 R_0402_1005Metric (back)
translate([22.100000,8.200000,0.800000])rotate([0,0,180.000000])m12(pushed,hulled); // RevK:C_0402 C_0402_1005Metric (back)
}

module b(cx,cy,z,w,l,h){translate([cx-w/2,cy-l/2,z])cube([w,l,h]);}
module m2(pushed=false,hulled=false)
{ // RevK:R_0402 R_0402_1005Metric
b(0,0,0,1.5,0.65,0.2); // Pad size
b(0,0,0,1.0,0.5,0.5); // Chip
}

module m5(pushed=false,hulled=false)
{ // RevK:C_0603_ C_0603_1608Metric
b(0,0,0,1.6,0.95,0.2); // Pad size
b(0,0,0,1.6,0.8,1); // Chip
}

module m8(pushed=false,hulled=false)
{ // RevK:LED-RGB-1.6x1.6 LED_0603_1608Metric
b(0,0,0,1.6,0.8,0.25);
b(0,0,0,1.2,0.8,0.55);
b(0,0,0,0.8,0.8,0.95);
if(!hulled&&pushed)b(0,0,0,1,1,20);
}

module m12(pushed=false,hulled=false)
{ // RevK:C_0402 C_0402_1005Metric
b(0,0,0,1.5,0.65,0.2); // Pad size
b(0,0,0,1.0,0.5,1); // Chip
}

module m15(pushed=false,hulled=false)
{ // RevK:SOT-363_SC-70-6 SOT-363_SC-70-6
b(0,0,0,1.15,2.0,1.1);
b(0,0,0,2.1,2.0,0.6);
}

module m18(pushed=false,hulled=false)
{ // RevK:SOT-23-6-MD8942 SOT-23-6
b(0,0,0,3.05,3.05,0.5);
b(0,0,0,1.45,3.05,1.1);
}

module m19(pushed=false,hulled=false)
{ // RevK:ESP32-S3-MINI-1 ESP32-S3-MINI-1
translate([-15.4/2,-16.6/2+2.7,0])
{
	if(!hulled)cube([15.4,20.5,0.8]);
	translate([0.7,0.5,0])cube([14,13.55,2.4]);
}
}

module m21(pushed=false,hulled=false,n=0)
{ // RevK:JST_EH_S5B-EH_1x05_P2.50mm_Horizontal JST_EH_S5B-EH_1x05_P2.50mm_Horizontal
b(2.5*(n/2)-1.25,5+3.6,0,2.5*n+2.5,6+10,4);
b(2.5*(n/2)-1.25,0,0,2.5*n+2.5,3.2,1.5);
if(!hulled)for(a=[0:1:n-1])translate([2.5*a,0,-3])cylinder(d1=0.5,d2=2.5,h=3,$fn=12);
}

module m24(pushed=false,hulled=false)
{ // RevK:L_4x4_ TYA4020
b(0,0,0,4,4,3);
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
