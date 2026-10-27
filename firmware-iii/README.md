# butns 16x8: iii firmware

Alternative firmware for the butns 16x8 board, built with the pico-sdk. It runs [monome iii](https://codeberg.org/tehn/iii) Lua scripts and adds colour functions for the RGB LEDs. A second mode speaks the monome serial protocol, like the Arduino firmware in `../firmware`.

## Modes

| Mode | USB id | Use with |
|------|--------|----------|
| iii (default) | `CAFE:1101`, product "iii grid" | [diii](https://monome.org/diii) or the `diii` command line tool, USB MIDI |
| monome | `2E8A:000A`, product "monome" | serialosc, norns, monome apps |

- **Switch mode:** hold the top-left key while plugging in. The mode is stored in flash. The top row flashes blue for iii, green for monome.
- **Rotate by 180 degrees:** hold the bottom-right key while plugging in.
- **Maximum brightness:** hold both top corner keys for one second, then press any key: its column sets the maximum (column 1 = 1/16, column 16 = 100 %). The setting ends 3 seconds after the last key press and is stored. Works in both modes.

## Lua API

Coordinates start at 1, levels run from 0 to 15. Changes become visible with `grid_refresh()`.

| Function | |
|---|---|
| `event_grid(x, y, z)` | defined by the script, called on press (z = 1) and release (z = 0) |
| `grid_led(x, y, z [, rel])` | set a level, `rel` adds `z` to the current level |
| `grid_led_get(x, y)` | current level |
| `grid_led_all(z [, rel])` | all keys |
| `grid_refresh()` | show the changes |
| `grid_size_x()`, `grid_size_y()` | 16, 8 |
| `grid_intensity(z)` | overall brightness, 0 to 15 |
| `grid_color(r, g, b)` | tint for `grid_led` and `grid_led_all`, default white |
| `grid_led_rgb(x, y, r, g, b)` | colour of one key (0 to 255 per channel), ignores the tint |
| `grid_color_intensity(z)` | brightness of `grid_led_rgb` keys, 0 to 15 |

The colour functions follow the NeoTrellis iii firmware and the [diii NeoTrellis emulator](https://jonwaterschoot.github.io/diii-neotrellis-emulator/), so scripts written there run here. Scripts that should also run on a monochrome monome grid check `if grid_led_rgb then ... end`. Colour and brightness reset to the defaults whenever iii starts a script.

All other functions (metros, MIDI, file system, REPL) come from iii unchanged.

## LED current

The firmware estimates the current of every frame and scales the whole frame down above `MAX_LED_CURRENT_MA` (400 mA, `src/config.h`). One SK6812-EC20 draws up to 12 mA per colour channel plus about 1 mA idle.

## Building

Dependencies are git submodules in `lib/`:

| Path | Source | Version |
|------|--------|---------|
| `lib/iii` | https://codeberg.org/tehn/iii | v1.1.2 |
| `lib/lua` | https://github.com/lua/lua | v5.5.1 |
| `lib/littlefs` | https://github.com/littlefs-project/littlefs | v2.9.3 |

```
git submodule update --init firmware-iii/lib/iii firmware-iii/lib/lua firmware-iii/lib/littlefs
export PICO_SDK_PATH=/path/to/pico-sdk        # pico-sdk 2.2.0
cmake -S firmware-iii -B build-iii -G Ninja
cmake --build build-iii
```

The result is `build-iii/butns-iii.uf2`. The Raspberry Pi Pico extension for VS Code works as well: open `firmware-iii` as project.

Flash: hold BOOTSEL on the Pico while plugging in, then copy the `.uf2` to the drive that appears.

## Licence

iii is GPLv3, so this firmware is distributed under the GPLv3 as a whole. The rest of the repository keeps its MIT licence. The WS2812 PIO program follows the pico-examples (BSD-3-Clause), the USB descriptors follow the TinyUSB examples (MIT).
