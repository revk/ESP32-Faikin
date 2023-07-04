b(0,0,0,10.1,10.1,0.8);
b(0,0,0.3,8.5,8.5,6);
if(!pushed&&!hulled)translate([0,0,2.5])for(a=[[0,0,0],[90,0,0],[-90,0,0],[0,90,0]])rotate(a)for(x=[-2,2])for(y=[-2,2])translate([x,y,0])cylinder(d=2,h=10,$fn=8);
