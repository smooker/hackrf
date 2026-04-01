# HackRF CW Firmware — SCteam

Custom HackRF firmware s CW (continuous wave) loop mode za postoqnen signal bez USB pachki.

## Problema

Standartniаt HackRF firmware streaming-va IQ danni prez USB v realno vreme.
Pri TX tova syzdava ~0.1ms gaps mejdu USB bulk transfers — signalat izglezha
kato pachki vmesto neprекъsnat sinus. Za signal generator/chestotomer test — nepriemlivo.

## Reshenie

Nov M0 rezhim MODE_CW=5 v sgpio_m0.s:
- M4 zapylva 32KB buffer s konstantni IQ danni (I=127, Q=0)
- M0 chete ot nachalo na bufera na vseki SGPIO interrupt
- NIAMA proverka na margin/m4_count — niama gaps
- 108 cikyla ot 163 bydzhet — komfortno

## Arhitektura

```
LPC4320 (HackRF MCU)
├── M4 core — USB host interface, nastroiki
│   └── usb_bulk_buffer[32KB] @ 0x20008000
│       zapylva se s IQ danni ot USB (normalen rezhim)
│       ili s konstantni stoinosti (CW mode)
│
├── M0 core — SGPIO loop (sgpio_m0.s)
│   ├── MODE_RX (2):  SGPIO → buffer
│   ├── MODE_TX (3,4): buffer → SGPIO (s margin check)
│   ├── MODE_CW (5):  buffer start → SGPIO (bez margin check!)
│   └── 32 bytes na vseki SGPIO interrupt, 163 cikyla bydzhet
│
├── SGPIO → CPLD → MAX5864 DAC → antenna
│
└── CW Mode:
    M0 MODE_CW pishe konstanta 0x007F007F vyv vsichki 8 SGPIO slice-a
    Niama m4_count tracking, niama shortfall, niama gaps
```

## Failove

| Fail | Opisanie |
|------|----------|
| `firmware/hackrf_usb/usb_api_transceiver.c` | TX mode handler, CW mode funkcia |
| `firmware/hackrf_usb/sgpio_m0.s` | M0 TX loop (asembler) |
| `firmware/common/m0_state.h` | M0/M4 shared state struct |
| `firmware/hackrf_usb/usb_bulk_buffer.h` | Ring buffer definicii |
| `firmware/common/sgpio.c` | SGPIO konfiguracia |

## Build

```bash
cd firmware/hackrf_usb
mkdir -p build && cd build
cmake .. -DCMAKE_OBJCOPY=/usr/bin/arm-none-eabi-objcopy
make -j4
```

Izhodни failove:
- `hackrf_usb.bin` — za SPI flash (`hackrf_spiflash -w`)
- `hackrf_usb_dfu.bin` — za DFU mode (`dfu-util --download`)
- `hackrf_usb_ram.bin` — za RAM zarezhdane (test bez flash)

## Flash

### DFU mode (vremenno, v RAM)
```bash
# HackRF v DFU mode (RESET + DFU buton)
dfu-util --device 1fc9:000c --alt 0 --download hackrf_usb_dfu.bin
```

### SPI flash (permanentno)
```bash
hackrf_spiflash -w hackrf_usb.bin
```

## Host tool — CW TX

```c
// cw_tx.c — izpolzva libhackrf za postoqnen carrier
// gcc -O2 -o cw_tx cw_tx.c -I/usr/include/libhackrf -lhackrf
// ./cw_tx 10000000    (10 MHz)
// ./cw_tx 150000000   (150 MHz)
```

## Host tool — Burst TX

```c
// burst_tx.c — ednokraten burst ~4-7ms
// gcc -O2 -o burst_tx burst_tx.c -I/usr/include/libhackrf -lhackrf
// ./burst_tx 1000000   (1 MHz burst)
```

## PortaPack

HackRF + PortaPack (LCD + butoni) — standalone rezhim.
Mayhem firmware e flashnat, no se planira custom app za:
- CW signal generator s UI
- Chestotomer test na HP 5328A

## CLKIN — vhnshen clock vhod (10 MHz reference)

HackRF One ima SMA konektor za vhnshen 10 MHz reference clock.

Iziskvaniia za CLKIN signala:
- Chestota: 10 MHz (firmware proverava 9-11 MHz diapazon)
- Nivo: 3.3V CMOS ili LVCMOS (NE 50 ohm sinusoida!)
- Si5351C CLKIN pin — otiva direktno kym PLL
- Na r9: firmware meri s Timer2 za 50ms — validno ako 9 MHz < f < 11 MHz
- Na pre-r9: Si5351C registyr 0 bit LOS (Loss of Signal)

Avtomatichno prevklyuchvane:
- Firmware detektira CLKIN pri startup i pri activate_best_clock_source()
- Ako CLKIN e validen → PLL_SOURCE_CLKIN (vmesto XTAL)
- PortaPack ikonata "EXT" v dolniq dqsen ygyl pokazva vhnshen clock

Komandi:
  hackrf_clock -i              # procheti CLKIN status
  hackrf_clock -a              # procheti vsichki clock nastroiki

Prilozhenie:
- GPS disciplined 10 MHz (GPSDO) → CLKIN → tochnost < 0.01 ppm
- OCXO 10 MHz reference
- HP 5328A 10 MHz reference izhod

## CLKOUT — clock izhod (10 MHz)

Si5351C generira 10 MHz na CLKOUT SMA konektora.

Komandi:
  hackrf_clock -o 1            # vklyuchi CLKOUT (10 MHz)
  hackrf_clock -o 0            # izklyuchi CLKOUT
  hackrf_clock -r 3            # procheti CLKOUT nastroiki

Tehnichesko:
- Izhod: Si5351C CLK2 (pre-r9) ili CLK3 (r9)
- Multisynth delitel: 800 MHz VCO / 80 = 10 MHz
- Nivo: 3.3V CMOS
- Na r9: dopylnitelen GPIO enable (H1R9_CLKOUT_EN)

Prilozhenie:
- Reference za drug SDR (daisy chain)
- Kalibraciq na chestotomer
- Sinhronizaciq na nyakolko HackRF-a

VNIMANIE: CLKOUT e 3.3V CMOS! Ne vyrzvai direktno kym 50 ohm vhod —
tryabva seriino syprotivlenie ili atenuator.

## Belezhki

- HackRF range: 1 MHz — 6 GHz
- TX power: ~10 dBm max (VGA=47 + AMP=ON), no AMP e invertiran na pre-r6!
- DAC: MAX5864, 8-bit, do 20 Msps
- USB bulk transfer: 16384 bytes = ~4ms pri 2 Msps
- PortaPack model: H1/H2 (da se utochni)
- AMP (MGA-81563): na pre-r6 board bypass dava PO-VISOKO nivo ot amplifier!
