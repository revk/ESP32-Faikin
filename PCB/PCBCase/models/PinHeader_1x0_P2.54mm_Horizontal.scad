if(!hulled)for(x=[0:1:n-1])translate([-0.25,-x*2.54-0.25,-2.5])cube([0.5,0.5,3]); // Un-cropped pins
for(x=[0:1:n-1])translate([0,-x*2.54,-0.81])cylinder(d=2,h=1); // Cropped pins / solder
for(x=[0:1:n-1])translate([0,-x*2.54,-1.8])cylinder(d1=1,d2=2,h=1); // Cropped pins / solder
translate([-1.27,-(n-0.5)*2.54,0]) // Header plastic
cube([3,n*2.54,2.54]);
translate([1.27,-(n-0.5)*2.54-0.5,0]) // Plug
cube([10,n*2.54+1,2.54+1]);
