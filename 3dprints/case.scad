$fn=50;

difference() {
    cube([30,50,10]);
    translate([3,3,-1]) cube([24,44,8]);
    translate([15,25,6.9]) cylinder(d=5, h=12);
}