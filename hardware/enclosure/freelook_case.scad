// FreeLook v1 enclosure
// In-line two-piece slide box for the ESP32-C3 Super Mini + MPU6500.
// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Nitin Kumar
//
// Units: millimetres. Designed to 3D print WITHOUT supports.
//
// Parts (set `part` below, or render "plate" for all on one bed):
//   tray   - open-top box, prints cavity-up as drawn.
//   lid    - sliding cover; PRINT IT FLIPPED (top face on the bed). The
//            "plate" layout already flips it for you.
//   clip   - strap clip; PRINT IT ON ITS BACK so the strap slot opens upward.
//            The "plate" layout already rotates it for you.
//   coupon - a short rail offcut to dial in the slide fit before printing big.
//
// Layout (looking down): back wall with USB-C is at X=0 (faces back of head),
// IMU sits flat at the front. The lid slides in from the front and snaps.
//
// First print: render "coupon", print it, check the lid rail slides with a
// light friction fit. Tune `slide_clr` until it is right, then print the rest.

/* [Render] */
part = "assembly"; // [assembly, tray, lid, clip, coupon, plate]

/* [Components - measure and update] */
// ESP32-C3 Super Mini PCB (X = length, Y = width)
mcu_l = 22.5;
mcu_w = 18.0;
mcu_top = 2.5;     // tallest top-side component above the PCB
// MPU6500 module PCB
imu_l = 20.0;
imu_w = 16.0;
imu_top = 2.5;
pcb_t = 1.2;       // PCB thickness (both)
// USB-C connector on the Super Mini
usb_w = 9.2;       // shell width (Y) plus a little
usb_h = 3.4;       // shell height (Z)

/* [Layout] */
gap = 10.0;        // X gap between the two boards for the folded 5-pin cable

/* [Box and fit] */
wall = 2.0;        // side / back wall thickness
floor_t = 2.0;     // floor thickness
lid_t = 2.0;       // lid plate thickness
clr = 0.4;         // clearance between a board edge and the wall (per side)
stand = 1.0;       // standoff the boards rest on (solder clearance underneath)
slide_clr = 0.25;  // lid slide clearance (TUNE with the coupon)
rail_d = 2.2;      // how far the lid edge reaches into the side wall
lip_h = 1.6;       // height of the lip that holds the lid down
snap = 0.8;        // snap-detent diameter

/* [LED] */
led_d = 3.2;       // hole for a 3 mm LED
led_from_back = 9; // LED X position, measured from the back (USB) wall

/* [Strap clip] */
strap_w = 25.0;    // goggles strap width
strap_t = 3.0;     // goggles strap thickness
clip_grip = 0.6;   // strap-slot pinch (interference); larger grips tighter
clip_len = 18.0;   // how far the clip extends along the strap
rail_w = 8.0;      // dovetail rail width (clip <-> tray)

$fn = 48;
eps = 0.01;

// ---- Derived dimensions ----------------------------------------------------
in_w = max(mcu_w, imu_w) + 2 * clr;            // interior width  (Y)
in_l = mcu_l + gap + imu_l + 2 * clr;          // interior length (X), open front
ch   = stand + pcb_t + max(mcu_top, imu_top, usb_h) + 0.6;  // clear height to lid underside

ow = in_w + 2 * wall;          // outer width
ol = wall + in_l;              // outer length (back wall only; front is open)
wall_top = ch + lid_t + slide_clr + lip_h;     // top of the side walls
oh = floor_t + wall_top;       // outer height

// Z of the USB connector bottom (sits on the PCB top, board on a standoff).
usb_z0 = stand + pcb_t;

// ---- Helpers ---------------------------------------------------------------

// A peaked (teardrop) rectangular hole through the back wall, so its top
// prints as a 45-degree peak with no flat overhang.
module usb_cutout() {
    cw = usb_w;
    translate([-eps, -cw / 2, usb_z0]) {
        cube([wall + 2 * eps, cw, usb_h]);
        // 45-degree roof: a diamond prism whose lower half overlaps the rect.
        translate([wall / 2 + eps, 0, usb_h])
            rotate([45, 0, 0])
                cube([wall + 2 * eps, cw / sqrt(2), cw / sqrt(2)], center = true);
    }
}

// The lid-capture channel cut into one side wall (right side, mirror for left).
// Carves the slot the lid edge slides in, and chamfers the lip underside at
// 45 degrees so it prints without support.
module side_channel_cut() {
    z0 = ch;                       // ledge top: lid edge rests here
    z1 = ch + lid_t + slide_clr;   // lip underside
    y0 = in_w / 2 - eps;           // inner wall face
    // the slot
    translate([wall, y0, z0])
        cube([in_l + eps, rail_d + eps, z1 - z0]);
    // 45-degree chamfer under the lip (rises going outward into the wall)
    translate([wall, y0, z1])
        rotate([45, 0, 0])
            cube([in_l + eps, rail_d * 1.6, rail_d * 1.6]);
    // snap dimple near the front, into the channel ceiling
    translate([ol - snap * 2, in_w / 2 + rail_d / 2, z1])
        sphere(d = snap);
}

// Dovetail rail on the underside of the tray that the strap clip slides onto.
// Wider at the bottom (printed first, on the bed) so it needs no support.
module clip_rail() {
    z_top = -floor_t;              // tray underside
    h = 2.4;
    translate([ol / 2, 0, z_top - h / 2])
        hull() {
            translate([0, 0, h / 2 - eps]) cube([rail_w * 0.7, 0.1, eps], center = true);  // narrow top
            translate([0, 0, -h / 2]) cube([rail_w, clip_len, eps], center = true);        // wide base
        }
}

// ---- Tray ------------------------------------------------------------------
module tray() {
    difference() {
        union() {
            // outer shell
            translate([0, -ow / 2, -floor_t]) cube([ol, ow, oh]);
            clip_rail();
        }
        // interior cavity (open top, open front)
        translate([wall, -in_w / 2, 0])
            cube([in_l + eps, in_w, ch + lid_t + slide_clr + eps]);
        // make the front fully open above the floor
        translate([wall, -in_w / 2, 0])
            cube([in_l + eps, in_w, wall_top + eps]);
        // lid channels both sides
        side_channel_cut();
        mirror([0, 1, 0]) side_channel_cut();
        // USB-C cutout in the back wall
        translate([0, 0, 0]) usb_cutout();
    }
    // standoff ledges the boards rest on (perimeter, leaves solder clearance)
    board_ledges();
    // low divider between the two boards (with a cable notch)
    divider();
}

// Perimeter ledges so the PCBs sit `stand` above the floor.
module board_ledges() {
    lw = 1.4;  // ledge width
    // along both long sides
    for (s = [-1, 1])
        translate([wall, s * (in_w / 2 - lw / 2) - lw / 2 + lw / 2, 0])
            translate([0, (s < 0 ? 0 : -lw), 0])
                cube([in_l, lw, stand]);
    // back ledge under the MCU
    translate([wall, -in_w / 2, 0]) cube([1.4, in_w, stand]);
}

// Divider rib between MCU and IMU pockets, with a central notch for the cable.
module divider() {
    dx = wall + clr + mcu_l + gap / 2;
    difference() {
        translate([dx - 1, -in_w / 2, 0]) cube([2, in_w, stand + pcb_t + 0.5]);
        translate([dx - 1 - eps, -in_w / 4, stand]) cube([2 + 2 * eps, in_w / 2, ch]);
    }
}

// ---- Lid -------------------------------------------------------------------
// Drawn in its in-use orientation (top up). Print it flipped (see "plate").
module lid() {
    plate_w = in_w + 2 * (rail_d - slide_clr);
    difference() {
        union() {
            // plate, edges reach into the side channels
            translate([wall, -plate_w / 2, ch + slide_clr / 2])
                cube([in_l, plate_w, lid_t]);
            // front end wall that closes the open front
            translate([ol - wall, -in_w / 2, 0])
                cube([wall, in_w, ch + lid_t]);
            // press ribs hold the boards down
            press_ribs();
            // snap bumps on the tongues near the front
            for (s = [-1, 1])
                translate([ol - snap * 2, s * (in_w / 2 + rail_d / 2), ch + slide_clr / 2 + lid_t])
                    sphere(d = snap);
        }
        // LED hole through the plate
        translate([wall + led_from_back, 0, ch])
            cylinder(d = led_d, h = lid_t + slide_clr + 1);
    }
}

module press_ribs() {
    // a rib over each board, protruding down to just touch the board top
    mcu_top_z = stand + pcb_t + mcu_top;
    imu_top_z = stand + pcb_t + imu_top;
    // MCU rib
    translate([wall + clr + mcu_l * 0.3, -3, mcu_top_z])
        cube([mcu_l * 0.4, 6, max(0.1, (ch + slide_clr / 2) - mcu_top_z)]);
    // IMU rib
    translate([ol - wall - clr - imu_l * 0.7, -3, imu_top_z])
        cube([imu_l * 0.4, 6, max(0.1, (ch + slide_clr / 2) - imu_top_z)]);
}

// ---- Strap clip ------------------------------------------------------------
// Drawn in use orientation (mounted under the tray). Print on its back so the
// strap slot opens upward (the "plate" layout rotates it for you).
module clip() {
    base_t = 2.0;
    jaw_t  = 1.6;
    slot   = strap_t - clip_grip;   // pinch the strap
    width  = strap_w + 2;
    difference() {
        union() {
            // base plate (under the tray, with the dovetail socket)
            translate([-clip_len / 2, -width / 2, 0]) cube([clip_len, width, base_t]);
            // back wall of the jaw (closed end, at +X)
            translate([clip_len / 2 - jaw_t, -width / 2, -slot - jaw_t])
                cube([jaw_t, width, slot + jaw_t + base_t]);
            // bottom jaw lip (the spring) under the strap
            translate([-clip_len / 2, -width / 2, -slot - jaw_t])
                cube([clip_len, width, jaw_t]);
            // small retaining bump to bite the strap
            translate([0, 0, -slot])
                rotate([0, 90, 0]) cylinder(d = 1.2, h = width, center = true);
        }
        // dovetail socket that slides onto the tray rail
        translate([0, 0, base_t]) scale([1, 1, 1]) clip_socket();
    }
}

module clip_socket() {
    h = 2.4;
    translate([0, 0, -h])
        hull() {
            translate([0, 0, h - eps]) cube([rail_w * 0.7 + 0.4, clip_len + 1, eps], center = true);
            translate([0, 0, 0]) cube([rail_w + 0.4, clip_len + 1, eps], center = true);
        }
}

// ---- Slide-fit test coupon -------------------------------------------------
module coupon() {
    seg = 18;
    intersection() {
        union() { tray(); }
        translate([0, -ow / 2 - 1, -floor_t - 5]) cube([wall + seg, ow + 2, oh + 10]);
    }
    // a matching short lid piece, next to it, already flipped for printing
    translate([0, ow + 6, ch + lid_t + slide_clr / 2])
        rotate([180, 0, 0])
            intersection() {
                lid();
                translate([0, -ow, -5]) cube([wall + seg, 2 * ow, oh + 10]);
            }
}

// ---- Assembly preview ------------------------------------------------------
module assembly() {
    color("MediumSlateBlue") tray();
    color("Gainsboro", 0.6) lid();
    color("DimGray") translate([0, 0, -floor_t - 2.4]) clip();
}

// ---- Print plate (all parts flat, no supports) -----------------------------
module plate() {
    tray();
    // lid flipped top-down, moved aside
    translate([0, ow + 8, ch + lid_t + slide_clr / 2]) rotate([180, 0, 0]) lid();
    // clip rotated so the strap slot opens up
    translate([ol + 18, 0, strap_t]) rotate([-90, 0, 0]) clip();
}

// ---- Dispatch --------------------------------------------------------------
if (part == "assembly") assembly();
else if (part == "tray") tray();
else if (part == "lid") lid();
else if (part == "clip") clip();
else if (part == "coupon") coupon();
else if (part == "plate") plate();
