// Screw 6mm
if(!hulled&&!pushed)
rotate([0,-90,0],$fn=24)
{
	hull()for(x=[-1,1])translate([x,0,-2])cylinder(d=12,h=2);
	hull()for(x=[-1,1])translate([x,0,0])cylinder(d1=12,d2=6,h=3);
	hull()for(x=[-1,1])translate([x,0,0])cylinder(d=6,h=100);
}

