rotate([90,0,0])
rotate([0,0,90])
translate([-11.5,-17.5,-5.5])
{
	translate([0,0,4])cube([23,35,1.6]); // Board
	translate([0.1,1.5,0])cube([22.8,33.4,7]); // Parts
	translate([0,24.92,-4])cube([2.54,10.08,11]); // Header
	// Hole for air
	if(!pushed)for(x=[-5,5])translate([14.5+x,-134.9,3.5])rotate([-90,0,0])cylinder(h=234.9,d=2,$fn=24);
}

