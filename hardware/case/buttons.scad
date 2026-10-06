include <./roundedcube.scad>

height = 5.3;

s = 8.5;

px = 11.43;
py = 11.4;

cols = 16;
rows = 8;

inlayslack = 1;
rubberslack = .6;

inlayheight = 2.6 - inlayslack;

w = 0.42*3;
h = 0.12*3;

cross = true;

only_blockers = false;

only_inlay = false;

if(!only_blockers && !only_inlay) {
intersection() {
difference() {
union() {
for(x=[0:cols-1]) for(y=[0:rows-1])
translate([x * px, y * py, height / 2]) roundedcube([s, s, height], center = true, radius = 1.5, apply_to = "zmax");

//translate([- s, -s, 0])cube([7 * px + s * 2, 7 * py + s * 2, inlayheight ]);
if (cross) {
for(k=[1-cols/2:rows-1+cols/2])
translate([cols/2 * px, k * py, h/2]) rotate([0, 0, 45]) cube([2*(cols+rows)*px, w, h], center = true);

for(k=[-cols/2:rows-2+cols/2])
translate([cols/2 * px, k * py, h/2]) rotate([0, 0, -45]) cube([2*(cols+rows)*px, w, h], center = true);
} else {
for(x=[0:cols-1]) for(y=[0:rows-1]) {
translate([x * px, y * py - s / 4, h/2]) cube([12, w, h], center=true);
translate([x * px, y * py + s / 4, h/2]) cube([12, w, h], center=true);
translate([x * px - s / 4, y * py, h/2]) cube([w, 12, h], center=true);
translate([x * px + s / 4, y * py, h/2]) cube([w, 12, h], center=true);
}
}

}
translate([3.5 * px, 3.5 * py, 0]) cylinder(h = 20, d = 5.4, center = true);
translate([(cols - 4.5) * px, 3.5 * py, 0]) cylinder(h = 20, d = 5.4, center = true);
translate([-px/2-1, -py/2-1, 0]) cylinder(h = 20, d = 5.8, center = true);
translate([(cols - 0.5) * px+1, (rows - 0.5) * py+1, 0]) cylinder(h = 20, d = 5.8, center = true);
translate([-px/2-1, (rows - 0.5) * py+1, 0]) cylinder(h = 20, d = 5.8, center = true);
translate([(cols - 0.5) * px+1, -py/2-1, 0]) cylinder(h = 20, d = 5.8, center = true);

}
translate([-s/2-1.5, -s/2-1.5, -1]) roundedcube([cols*px-2.9+3, rows*py-2.9+3, 10], radius=1.5, center=false, apply_to = "zmax");


}

for(x=[0:cols-1]) for(y=[0:rows-1])
translate([x * px + s/2, y * py, inlayheight / 2]) cube([3.25, s/3, inlayheight], center = true);

for(y=[0:rows-1])
translate([-1 * px + s - 1, y * py, inlayheight / 2]) cube([2.75, s/4, inlayheight], center = true);
}
if (only_blockers && !only_inlay) {
color("blue")
for(x=[0:cols-1]) for(y=[0:rows-1])
translate([x * px - s/2+.15, y * py, (height - 2) / 2]) cube([.4, s-3, height - 2], center = true);
}

if (!only_blockers && only_inlay) {


color("blue") translate([0, 0, inlayheight]) difference() {
 translate([(cols-1)/2*px, (rows-1)/2*py, (inlayslack - rubberslack) / 2]) cube([(cols-1)*px, (rows-1)*py, inlayslack - rubberslack], center = true);
 
 for(x=[0:cols-1]) for(y=[0:rows-1])
translate([x * px, y * py, height / 2]) roundedcube([s+rubberslack, s + rubberslack, height], center = true, radius = 1.5, apply_to = "zmax");

}
}