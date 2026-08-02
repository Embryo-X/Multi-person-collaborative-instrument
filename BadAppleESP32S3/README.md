# ESP32-S3 88-key piano score mode

This PlatformIO project loads 88 piano samples from `all_piano/钢琴音色MP3`, starts score mode automatically, and plays them through a MAX98357A-compatible I2S amplifier.

## Wiring used by the firmware

| Amplifier | ESP32-S3 |
|---|---:|
| LRC | GPIO20 |
| BCLK | GPIO21 |
| DIN | GPIO47 |
| GAIN | GPIO48 (input/high-impedance) |
| SD | GPIO45 (driven HIGH after boot) |
| GND | GND |
| VCC | 5V |

Connect the speaker only between the amplifier's `SPK+` and `SPK-` outputs. Neither speaker terminal goes to GND.

## Buttons and RGB LEDs

- Button 1 shorts GPIO4 to GPIO5.
- Button 2 shorts GPIO6 to GPIO7.
- Button 3 shorts GPIO15 to GPIO16.
- RGB LED 1 B/G/R: GPIO17/GPIO18/GPIO8.
- RGB LED 2 B/G/R: GPIO3/GPIO46/GPIO9.
- RGB LED 3 B/G/R: GPIO10/GPIO11/GPIO12.

The 88 notes are assigned to buttons 1, 2, and 3 in round-robin order. Blue prompts the expected button, green confirms and plays the note, and red reports a wrong button. A prompt auto-plays after 1.5 seconds, so the complete scale also runs unattended. The sequence restarts after all 88 notes.

## Build and upload

Install PlatformIO, then run from this directory:

```powershell
pio run
pio run --target upload
pio run --target uploadfs
pio device monitor
```

The pre-build script copies and renames all 88 source samples to `data/piano/01.mp3` through `data/piano/88.mp3`. Uploading the firmware does not upload the samples; the separate `uploadfs` step is required.

The custom partition table assumes an ESP32-S3 with at least 8 MB of flash. It provides a 1 MiB application partition and about 6.94 MiB for SPIFFS.

Software output gain is set to `0.5`.

GPIO20 is also the ESP32-S3 native USB D+ signal. If this board uses native USB rather than a USB-to-UART bridge, audio activity may disrupt USB serial. In that case, move LRC to another free output-capable GPIO and update `Pins::LRC` in `src/main.cpp`.
