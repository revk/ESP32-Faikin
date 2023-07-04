if(!hulled&&!pushed)
{ // This avoids hull and minkowski as used as a tamper switch so needs to be more exact
	b(0,0,0,6,6,3.2); // Used as a tamper, so exact
	for(x=[-2,2])for(y=[-2,2])translate([x,y,0])cylinder(d=1,h=3.5); // Pips on top
}
b(0,0,0,9,6,1.1); // Legs

