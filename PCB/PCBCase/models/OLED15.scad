translate([44.5,37,0])rotate([0,0,180])
{
	translate([0,0,6])hull()for(x=[1.5,44.5-1.5])for(y=[1.5,37-1.5])translate([x,y,0])cylinder(r=1.4995,h=1.599,$fn=24);
	for(x=[2.5,44.5-2.5])for(y=[2.5,37-2.5])translate([x,y,0])cylinder(d=4.98,h=6,$fn=6);
	for(x=[2.5,44.5-2.5])for(y=[2.5,37-2.5])translate([x,y,7])cylinder(d=3.99,h=1.5,$fn=24);
	for(x=[2.5,44.5-2.5])for(y=[2.5,37-2.5])translate([x,y,-3])cylinder(d=3.99,h=2,$fn=24);
	translate([5.25,0,6])cube([34,37,3.2]);
	translate([40.73,9.61,7.5])cube([2.54,7*2.54,1.5]); // pins
	if(!hulled)hull()
	{
		translate([8.25,2,9.199]) cube([28,28,20]);
		if(!pushed) translate([4.25,-2,13.5]) cube([36,36,20]);
	}
}
