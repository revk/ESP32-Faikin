// Generated case design for Faikin/Faikin.kicad_pcb
// By https://github.com/revk/PCBCase
// Generated 2023-10-15 10:42:36
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
pcbthickness=1.000000;
nohull=false;
hullcap=1.000000;
hulledge=1.000000;
useredge=false;

module outline(h=pcbthickness,r=0){linear_extrude(height=h)offset(r=r)polygon(points=[[0.000000,0.750000],[0.000000,15.250000],[0.036708,15.481763],[0.143237,15.690839],[0.309161,15.856763],[0.518237,15.963292],[0.750000,16.000000],[29.450000,16.000000],[29.641342,15.961939],[29.803553,15.853553],[29.911939,15.691342],[29.950000,15.500000],[29.950000,0.500000],[29.911939,0.308658],[29.803553,0.146447],[29.641342,0.038061],[29.450000,0.000000],[0.750000,0.000000],[0.518237,0.036708],[0.309161,0.143237],[0.143237,0.309161],[0.036708,0.518237]],paths=[[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21]]);}

module pcb(h=pcbthickness,r=0){linear_extrude(height=h)offset(r=r)polygon(points=[[0.000000,0.750000],[0.000000,15.250000],[0.036708,15.481763],[0.143237,15.690839],[0.309161,15.856763],[0.518237,15.963292],[0.750000,16.000000],[29.450000,16.000000],[29.641342,15.961939],[29.803553,15.853553],[29.911939,15.691342],[29.950000,15.500000],[29.950000,0.500000],[29.911939,0.308658],[29.803553,0.146447],[29.641342,0.038061],[29.450000,0.000000],[0.750000,0.000000],[0.518237,0.036708],[0.309161,0.143237],[0.143237,0.309161],[0.036708,0.518237]],paths=[[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21]]);}
spacing=45.950000;
pcbwidth=29.950000;
pcblength=16.000000;
// Populated PCB
module board(pushed=false,hulled=false){
translate([21.700000,13.400000,1.000000])rotate([0,0,90.000000])m2(pushed,hulled); // RevK:R_0402 R_0402_1005Metric (back)
translate([21.125000,4.185000,1.000000])rotate([0,0,90.000000])m5(pushed,hulled); // RevK:C_0603_ C_0603_1608Metric (back)
translate([17.325000,11.200000,1.000000])rotate([0,0,-135.000000])m7(pushed,hulled); // RevK:SMD1010 SMD1010 (back)
translate([16.525000,15.000000,1.000000])rotate([0,0,180.000000])m10(pushed,hulled); // RevK:C_0402 C_0402_1005Metric (back)
translate([16.825000,1.000000,1.000000])rotate([0,0,180.000000])m2(pushed,hulled); // RevK:R_0402 R_0402_1005Metric (back)
translate([19.525000,13.787500,1.000000])rotate([0,0,180.000000])m13(pushed,hulled); // RevK:SOT-363_SC-70-6 SOT-363_SC-70-6 (back)
translate([18.625000,2.885000,1.000000])rotate([0,0,180.000000])m16(pushed,hulled); // RevK:SOT-23-6-MD8942 SOT-23-6 (back)
translate([20.075000,12.050000,1.000000])m2(pushed,hulled); // RevK:R_0402 R_0402_1005Metric (back)
translate([20.425000,1.000000,1.000000])rotate([0,0,180.000000])m2(pushed,hulled); // RevK:R_0402 R_0402_1005Metric (back)
translate([20.075000,11.150000,1.000000])m10(pushed,hulled); // RevK:C_0402 C_0402_1005Metric (back)
translate([7.725000,8.000000,1.000000])rotate([0,0,90.000000])m17(pushed,hulled); // RevK:ESP32-S3-MINI-1 ESP32-S3-MINI-1 (back)
translate([18.625000,4.885000,1.000000])m5(pushed,hulled); // RevK:C_0603_ C_0603_1608Metric (back)
translate([23.450000,13.000000,1.000000])rotate([0,0,-90.000000])m19(pushed,hulled,5); // RevK:JST_EH_S5B-EH_1x05_P2.50mm_Horizontal JST_EH_S5B-EH_1x05_P2.50mm_Horizontal (back)
translate([20.325000,10.150000,1.000000])m5(pushed,hulled); // RevK:C_0603_ C_0603_1608Metric (back)
translate([18.625000,7.485000,1.000000])rotate([-0.000000,-0.000000,-90.000000])m23(pushed,hulled); // RevK:L_4x4_ TYA4020 (back)
translate([16.125000,4.185000,1.000000])rotate([0,0,-90.000000])m5(pushed,hulled); // RevK:C_0603_ C_0603_1608Metric (back)
translate([18.625000,0.985000,1.000000])m2(pushed,hulled); // RevK:R_0402 R_0402_1005Metric (back)
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

module m7(pushed=false,hulled=false)
{ // RevK:SMD1010 SMD1010
b(0,0,0,1,1,.8);
if(!hulled&&pushed)b(0,0,0,1,1,20);
}

module m10(pushed=false,hulled=false)
{ // RevK:C_0402 C_0402_1005Metric
b(0,0,0,1.5,0.65,0.2); // Pad size
b(0,0,0,1.0,0.5,1); // Chip
}

module m13(pushed=false,hulled=false)
{ // RevK:SOT-363_SC-70-6 SOT-363_SC-70-6
b(0,0,0,1.15,2.0,1.1);
b(0,0,0,2.1,2.0,0.6);
}

module m16(pushed=false,hulled=false)
{ // RevK:SOT-23-6-MD8942 SOT-23-6
b(0,0,0,3.05,3.05,0.5);
b(0,0,0,1.45,3.05,1.1);
}

module m17(pushed=false,hulled=false)
{ // RevK:ESP32-S3-MINI-1 ESP32-S3-MINI-1
translate([-15.4/2,-15.45/2,0])
{
	if(!hulled)cube([15.4,20.5,0.8]);
	translate([0.7,0.5,0])cube([14,13.55,2.4]);
}
}

module m19(pushed=false,hulled=false,n=0)
{ // RevK:JST_EH_S5B-EH_1x05_P2.50mm_Horizontal JST_EH_S5B-EH_1x05_P2.50mm_Horizontal
b(2.5*(n/2)-1.25,5+3.6,0,2.5*n+2.5,6+10,4);
b(2.5*(n/2)-1.25,0,0,2.5*n+2.5,3.2,1.5);
if(!hulled)for(a=[0:1:n-1])translate([2.5*a,0,-3])cylinder(d1=0.5,d2=2.5,h=3,$fn=12);
}

module m23(pushed=false,hulled=false)
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
