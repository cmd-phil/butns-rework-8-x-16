# Overview

grid 128 (16x8) variant of [jhbruhn/butns](https://github.com/jhbruhn/butns), a small grid clone heavily based on previous work by [hugelton/btns](https://github.com/hugelton/Btns).
The overview image shows the original 8x8 version.

![overview image](docs/overview.jpeg)

Uses a Rasperry Pi Pico Microcontroller, NeoPixel RGB LEDs (SK6812), cheap buttons and a 3d-printed case and button-overlay.
The tilt-sensor of the 8x8 version is not available: all usable GPIOs are needed for the 8x16 matrix.

## 16x8 specifics

| Item | Value |
|------|-------|
| PCB size | 190.44 mm x 99 mm, 2 layers |
| Rows `SW_ROW_1..8` | GPIO0..7 |
| Columns `SW_COL_1..7` | GPIO8..14 |
| Columns `SW_COL_8..15` | GPIO16, 17, 18, 19, 20, 21, 22, 26 |
| Column `SW_COL_16` | GPIO15 |
| LED data | GPIO28 via 74LVC1G17, one chain of 128 LEDs, row by row, 16 per row, starting top left |
| Free GPIO | GPIO27 |
| Mounting holes | 4 corners, 2 inner (M2.5) |

The firmware limits the estimated LED current to `MAX_LED_CURRENT_MA` (400 mA, see `firmware/src/firmware.cpp`).
One SK6812-EC20 draws up to 12 mA per colour channel plus about 1 mA idle, so 128 LEDs at full level exceed what a USB port and the 0.2 mm supply traces can deliver.
Hold the top-left key while plugging in for the default orientation, the bottom-right key for 180 degrees.

# Building

It is highly recommended to order the PCB assembled from JLCPCB. That assembly will include everything other than the parts in the BOM.

## BOM

| Part                            | Quantity | Source                                                                                                                                                                                                                                                                                                                        | Required                      | 
|---------------------------------|----------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------|
| PCB with components             | 1        | JLCPCB, ordered with Gerber and Assembly files from latest release. Gerber: `butns-JLCPCB.zip`, BOM: `butns_bom_jlc.csv`, CPL: `butns_cpl_jlc.csv`.                                                                                                                                                                                                                                                            | yes                           | 
| Raspberry Pi Pico               | 1        | Original Raspberry Pi Pico, or [USB-C Variant](https://de.aliexpress.com/item/1005007393927221.html)                                                                                                                                                                                                                          | yes                           |   |
| M2.5x6 Button Head Screws       | 6        | [M2.5x6](https://www.aliexpress.com/item/32810852732.html)                                                                                                                                                                                                                                                                    | yes                           | 
| Case                        | 1        | 3d-printed (FDM). `case.stl` contains the top and bottom half. In the image, it is printed in black PLA, 0.2mm layerheight.                                                                                                                                                                                                   | yes                           | 
| Button Matrix                     | 1        | 3d-printed (FDM). Print `buttons.stl` with white PLA and 0.12mm layerheight and no infill, 2 wall lines, concentric top infill. These should guide the LEDs light well. Optional: Add `buttons-black-blockers.stl` while importing as a multi-material print and print those in black to block light from neighboring buttons | yes (black blockers optional) | 
| Button TPU Case Inlay         | 1        | 3d-printed (FDM). Print with TPU, perhaps in black. Will help against rattling of the buttons in the case.                                                                                                                                                                                                                    | no                            | 

## Assembly

Flash microcontroller with `firmware.uf2`. Solder MCU to PCB. Screw everything into the case. Done!

Hint: The cases Top half has one specific orientation where it fits nicely: The top-screw-posts of the top-half with the larger cutoaways go to the side opposite of the USB Connector.