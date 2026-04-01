# HackRF One — Hardware Reference

## Specs

| Parametyr | Stoinost |
|-----------|----------|
| Chestoten obhvat | 1 MHz — 6 GHz |
| Bandwidth | do 20 MHz (20 Msps) |
| Rezhim | half-duplex (TX ili RX, ne ednowremenno) |
| ADC/DAC rezolucia | 8-bit I + 8-bit Q |
| USB | 2.0 High Speed (480 Mbps) |
| Zahranvane | USB (500 mA) |
| Konnektor | SMA antenen izhod |
| TX power | ~10 dBm max (s AMP) |
| Firmware | DFU bootloader + SPI flash |
| Open source | hardware + firmware + host tools |

## Chipove — blok diagrama

```
ANTENNA (SMA)
    │
    ▼
┌───────────────────────────────────────────────────┐
│                  RF FRONTEND                       │
│                                                   │
│  ┌─────────┐    ┌────────────┐    ┌────────────┐  │
│  │ MGA-    │    │  RFFC5072  │    │  MAX2837   │  │
│  │ 81563   │───▶│  Mixer/    │───▶│  Baseband  │  │
│  │ RF Amp  │    │  Synth     │    │  xcvr      │  │
│  │ LNA/PA  │    │  (LO)      │    │  (IF)      │  │
│  └─────────┘    └────────────┘    └─────┬──────┘  │
│   +14 dB         80 MHz-4.2 GHz    RX LNA 0-40dB  │
│   ON/OFF         Local Oscillator  TX VGA 0-47dB   │
│   -a flag                          2.3-2.7 GHz IF  │
└────────────────────────────────────────┬──────────┘
                                         │ analog IQ
                                         ▼
┌───────────────────────────────────────────────────┐
│                  DATA PATH                         │
│                                                   │
│  ┌────────────┐   ┌──────────┐   ┌─────────────┐  │
│  │  MAX5864   │   │  CPLD    │   │  LPC4320    │  │
│  │  ADC/DAC   │──▶│  XC2C64A │──▶│  MCU        │  │
│  │            │   │  (Xilinx)│   │  M4 + M0    │  │
│  │  8-bit     │   │          │   │             │  │
│  │  20 Msps   │   │  SGPIO   │   │  USB 2.0 HS │  │
│  └────────────┘   │  marshal │   └──────┬──────┘  │
│                   └──────────┘          │          │
└─────────────────────────────────────────┼──────────┘
                                          │ USB
                                          ▼
                                       HOST PC
```

## Chipove — detailno

### 1. MGA-81563 — RF usilavtel

| Parametyr | Stoinost |
|-----------|----------|
| Tip | GaAs MMIC usilavtel |
| Gain | +14 dB |
| Noise figure | 2.4 dB |
| Obhvat | 0.1 — 6 GHz |
| Rezhim | LNA (RX) ili PA (TX), hardware switch |
| Kontrol | ON/OFF prez GPIO, firmware flag `-a 0/1` |
| Zabelezhka | Edin i sysht chip za RX i TX |

### 2. RFFC5072 — Mixer + Synthesizer

| Parametyr | Stoinost |
|-----------|----------|
| Tip | Wideband synthesizer/VCO s mixer |
| LO obhvat | 85 MHz — 4200 MHz |
| Funkcia | Prevryshta RF ↔ IF (baseband) |
| PLL | Integrirane, fractional-N |
| Kontrol | SPI ot LPC4320 |
| Zabelezhka | Tova e "tunerat" — opredelq na koq chestota priemame/predavame |

### 3. MAX2837 — Baseband Transceiver

| Parametyr | Stoinost |
|-----------|----------|
| Tip | 2.3-2.7 GHz transceiver (izpolzva se kato IF) |
| RX LNA gain | 0-40 dB (stypka 8 dB), flag `-l` |
| TX VGA gain | 0-47 dB (stypka 1 dB), flag `-x` |
| Bandwidth | 1.75 — 28 MHz (nastroim) |
| Kontrol | SPI ot LPC4320 |
| Zabelezhka | Raboti kato IF usilavtel/filter sled RFFC5072 mixer-a |

### 4. MAX5864 — ADC/DAC

| Parametyr | Stoinost |
|-----------|----------|
| ADC rezolucia | 8-bit |
| DAC rezolucia | 8-bit |
| Max sample rate | 22 Msps |
| Interfeys | Parelelen I/Q izhod |
| Zabelezhka | RX: analog→digital (ADC), TX: digital→analog (DAC) |

### 5. XC2C64A — CPLD (Xilinx CoolRunner-II)

| Parametyr | Stoinost |
|-----------|----------|
| Tip | CPLD, 64 makrokelki |
| Funkcia | Marshal-va danni mejdu MAX5864 i SGPIO |
| SGPIO | 8 slice-a, po 4 bytes na interrupt |
| Kontrol | JTAG programirane ot LPC4320 |
| Zabelezhka | "Lepiloto" mejdu analog i digital chast |

### 6. LPC4320 — MCU (NXP)

| Parametyr | Stoinost |
|-----------|----------|
| Arhitektura | Dual-core: Cortex-M4 + Cortex-M0 |
| Clock | do 204 MHz |
| SRAM | 200 KB (multiple banks) |
| USB | High Speed 2.0 (480 Mbps) |
| SGPIO | 8 slices, parallel I/O za IQ danni |
| Bootloader | DFU (USB Device Firmware Upgrade) |
| Flash | Vynshеn SPI flash (firmware) |

#### M4 core (glaven procesor)
- USB interfeys — priema/izprashta IQ danni ot/kym host
- Konfigurirane na RF chipove prez SPI (RFFC5072, MAX2837)
- Nastroiki: chestota, gain, sample rate, bandwidth
- `usb_bulk_buffer[32KB]` @ 0x20008000 — ring buffer za IQ

#### M0 core (realno-vremenен coprocesоr)
- SGPIO TX/RX loop v asembler (`sgpio_m0.s`)
- Chete/pishe 32 bytes na vseki SGPIO interrupt
- Niama OS, niama interrupts — gol loop
- Komunikacia s M4 prez `m0_state` struct v shared SRAM
- Vizh [CORTEX-M0.md](CORTEX-M0.md) za Thumb-1 ogranicheniq i branch range

## Gain tablica

### RX (priemanе)

| Etap | Chip | Gain | Flag | Stypka |
|------|------|------|------|--------|
| RF Amp | MGA-81563 | 0 ili +14 dB | `-a 0/1` | ON/OFF |
| IF LNA | MAX2837 | 0-40 dB | `-l 0..40` | 8 dB |
| BB VGA | MAX2837 | 0-62 dB | `-g 0..62` | 2 dB |
| **Total** | | **0-116 dB** | | |

### TX (predavanе)

| Etap | Chip | Gain | Flag | Stypka |
|------|------|------|------|--------|
| BB VGA | MAX2837 | 0-47 dB | `-x 0..47` | 1 dB |
| RF Amp | MGA-81563 | 0 ili +14 dB | `-a 0/1` | ON/OFF |
| **Total** | | **0-61 dB** | | |

### Tipichiн TX power na antenata

| VGA (-x) | AMP (-a) | Priblizhitelna moshtnost |
|----------|----------|--------------------------|
| 20 | 0 | ~ -15 dBm |
| 30 | 0 | ~ -5 dBm |
| 40 | 0 | ~ 0 dBm |
| 47 | 0 | ~ +5 dBm |
| 47 | 1 | ~ +10 dBm (~10 mW) |

## SPI Flash

| Parametyr | Stoinost |
|-----------|----------|
| Chip | W25Q80BV (Winbond) |
| Razmer | 1 MB |
| Interfeys | SPI |
| Sydarzhanie | hackrf_usb.bin firmware |
| Flash tool | `hackrf_spiflash -w firmware.bin` |

## Clock

| Iztochnik | Chestota | Izpolzvane |
|-----------|----------|------------|
| Kristal (XTAL) | 10 MHz | Reference za PLL |
| CLKIN (vynshеn) | 10 MHz SMA vhod | Vynshеn reference (opcionalno) |
| CLKOUT | 10 MHz SMA izhod | Reference za drugi ustroistvа |
| Si5351C | programmable | Generira clocks za chipovete |

## Konektori

| Konnektor | Tip | Funkcia |
|-----------|-----|---------|
| ANT | SMA female | Antenna (TX/RX) |
| CLKIN | SMA female | Vynshеn 10 MHz reference (vhod) |
| CLKOUT | SMA female | 10 MHz reference (izhod) |
| USB | micro-B | Data + zahranvane |
| P20 | 2x5 header | GPIO, CPLD JTAG, I2C |
| P28 | 2x5 header | Oshte GPIO, SGPIO debug |

## DFU Mode

Za vlizane v DFU mode:
1. Natisni i dryzhi buton **RESET**
2. Natisni i dryzhi buton **DFU**
3. Pusnesh **RESET** (prodylzhi da dryzhish DFU)
4. Pusnesh **DFU** sled 1 sekunda

Proverka: `dfu-util -l` triabva da pokazhe `[1fc9:000c]`

### !!! VNIMANIE — pravilni .bin failove !!!

- `hackrf_usb_dfu.bin` — za DFU download (`dfu-util`) !!! TOZI !!!
- `hackrf_usb.bin` — za SPI flash (`hackrf_spiflash -w`)
- `hackrf_usb_ram.bin` — NE e za DFU! Niama DFU header!

```
dfu-util --device 1fc9:000c --alt 0 --download hackrf_usb_dfu.bin
```

NE polzvai `hackrf_usb_ram.bin` s `dfu-util` — niama da boot-ne!!!

### USB IDs
- DFU mode: `1fc9:000c` — NXP LPC4330 DFU bootloader
- Normal mode: `1d50:6089` — HackRF One (Great Scott Gadgets)

### DFU troubleshooting (sw2)
Na sw2 (Haswell i5-4570, xHCI) `dfu-util 0.11` dava `LIBUSB_ERROR_IO` pri download.
Device se vizhda pravilno (High Speed 480Mbps, dfuIDLE) no transfer-at failva na 0 bytes.
- PortaPack PRECHI na DFU !!! Mahni go predi DFU flash !!!
- `hackrf_spiflash -w hackrf_usb.bin` — raboti v normalen rezhim (bez DFU), ne triabva mahane na PortaPack
- Probvai razlichni USB portove (USB 2.0 portove rabotyat po-dobre ot USB 3.0/xHCI)

### HackRF One hardware reviziq
Nashiq HackRF e ot 13 feb 2014 — edna ot purvite revizii (pre-r6).
Firmware triabva da se kompilira ot v2024.02.1 tag — git HEAD (2026) ima
nesyvmestimi clock/PLL promeni za po-novi revizii (r9, r10).

### lsusb v DFU mode
```
Bus 003 Device 023: ID 1fc9:000c NXP Semiconductors LPC4330FET180
Negotiated speed: High Speed (480Mbps)
bMaxPacketSize0: 64
iManufacturer: NXP
iProduct: LPC
iSerial: ABCD
```

## PortaPack (add-on)

PortaPack se zakachva na P20/P28 header-ite i dobavq:
- LCD ILI9341 — 320x240 color, 16-bit parallel 8080-I interfeys
- Audio codec WM8731 — I2S in/out
- microSD slot
- 5-way buton + rotary encoder
- Spodelq LPC43xx MCU-to na HackRF (ne e otdelen procesor)

Firmware: Mayhem (portapack-mayhem) — nai-popularniq fork s mnogo apps.

### PortaPack i DFU mode — VNIMANIE!

PortaPack prechi na DFU flash-vane! P20/P28 header-ite koito polzva
spodelqt GPIO pinove s LPC4320 boot mode detection i USB signali.
Pri DFU boot PortaPack dryzhi tezi pinove v greshno systoqnie →
`LIBUSB_ERROR_IO` pri dfu-util download.

Reshenia:
- **Razkachi PortaPack** predi DFU flash (fizicheski mahni ot header-ite)
- **Polzvai `hackrf_spiflash`** vmesto DFU — raboti v normalen rezhim,
  bez da se maha PortaPack:
  `hackrf_spiflash -w firmware.bin`
- `hackrf_spiflash` pishe direktno v SPI flash prez USB vendor request,
  zaobilq DFU napylno

### PortaPack LCD pin mapping

LCD-to e na 16-bit parallel 8080-I bus (ne SPI!):
- Data bus: GPIO3_8..15 (P7_0..P7_7) — 8-bit
- WRx: GPIO1_10 (P2_9) — write strobe
- RDx: GPIO5_4 (P2_4) — read strobe
- ADDR: GPIO5_1 (P2_1) — 0=command, 1=data
- DIR: GPIO1_13 (P2_13) — data direction

16-bit data se prashta na 2 fazi: high byte (ADDR=0) posle low byte (ADDR=1).

HackRF standartniаt firmware veche ima LCD poddryzhka (`ui_portapack.c`) —
detect-va PortaPack pri boot i risuvа tablichen status na ekrana avtomatichno.

## Firmware arhitektura

### Source repo
- GitHub: `greatscottgadgets/hackrf`
- Glaven branch: `main` (niama development branch)
- Posleden release: `v2026.01.3`
- HEAD e +79 commita pred release-a (TX interpolation, FPGA fixes)

### Dual-core model (LPC4320)

```
M4 core (204 MHz)                    M0 core (204 MHz)
┌─────────────────────┐              ┌──────────────────┐
│ USB bulk transfers   │              │ sgpio_m0.s       │
│ RF chip SPI config   │   shared     │ (gol asembler)   │
│ UI (LCD) updates     │◄──SRAM──────▶│                  │
│ Vendor requests      │              │ TX: buf→SGPIO    │
│                      │  m0_state    │ RX: SGPIO→buf    │
│ usb_bulk_buffer[32K] │  {m0_count,  │ 32 bytes/ISR     │
│ @ 0x20008000         │   m4_count}  │ wrap & 0x7FFF    │
└─────────────────────┘              └──────────────────┘
```

### M0 rezhimi (sgpio_m0.s)

| Mode | Stoinost | Opisanie |
|------|----------|----------|
| IDLE | 0 | Nichego ne pravi |
| WAIT | 1 | Chaka komanda |
| RX | 2 | SGPIO → buffer (priemanе) |
| TX_START | 3 | Inicializacia na TX |
| TX_RUN | 4 | Buffer → SGPIO (predavanе) |

### TX data path

```
Host USB → usb_bulk_buffer[32KB] → M0 (32 bytes/ISR) → SGPIO → CPLD → MAX5864 DAC → antenna
                                    │
                                    ├── m4_count - m0_count >= 32 → predavai
                                    └── m4_count - m0_count < 32  → tx_zeros (pachki!)
```

### USB Vendor Requests (hackrf_usb.c)

| Request | Funkcia |
|---------|---------|
| SET_TRANSCEIVER_MODE | TX/RX/OFF |
| SET_FREQ | Chestota (RFFC5072 + MAX2837) |
| SET_SAMPLE_RATE | Sample rate |
| SET_BASEBAND_FILTER_BW | MAX2837 bandwidth |
| SET_LNA_GAIN | MAX2837 RX LNA (0-40 dB) |
| SET_VGA_GAIN | MAX2837 RX VGA (0-62 dB) |
| SET_TXVGA_GAIN | MAX2837 TX VGA (0-47 dB) |
| SET_AMP_ENABLE | MGA-81563 ON/OFF |
| SET_ANTENNA_ENABLE | Antenna bias tee |

### UI abstraction (hackrf_ui.h)

HackRF firmware ima `hackrf_ui_t` vtable s callback-ove za vsqka promqna.
Pri boot firmware-at detect-va PortaPack i registrira `ui_portapack` implementaciq.
Vseki `hackrf_ui()->set_*()` call update-va LCD-to avtomatichno.

Implementacii:
- `ui_portapack.c` — PortaPack LCD (ILI9341, tablichen vid, 8x16 font)
- `ui_rad1o.c` — rad1o badge (drug hardware)
- Ako niama UI — `hackrf_ui()` vryshta NULL, callback-ovete se ignorirаt

### PortaPack UI v HackRF firmware (ui_portapack.c)

Tova **NE e Mayhem** — tova e standartniаt HackRF firmware!
Risuvа tablichen vid na LCD s tekushtite nastroiki:

```
┌──────────────────────────────┐
│ FREQ: 433.920.000 MHz        │
│                              │
│ [AMP] ──▶ [MIX] ──▶ [BB]    │  blok diagrama s bitmaps
│                              │
│ AMP:  ON  +14dB              │
│ LNA:  24 dB                  │
│ VGA:  32 dB                  │
│ BW:   10.00 MHz              │
│ SR:   2000000                │
│ CLK:  internal               │
│ MODE: TX                     │
└──────────────────────────────┘
```

Vseki parametyr se update-va avtomatichno prez `hackrf_ui()->set_*()` callback.

### Build firmware

```bash
cd firmware/hackrf_usb
mkdir -p build && cd build
cmake .. -DCMAKE_OBJCOPY=/usr/bin/arm-none-eabi-objcopy
make -j4
```

Izhodni failove:
- `hackrf_usb.bin` — SPI flash (permanenten)
- `hackrf_usb_dfu.bin` — DFU mode (v RAM)
- `hackrf_usb_ram.bin` — RAM zarezhdane (test)

### CW loop mode (planiran patch)

Trick: zapylvame ring buffer-a s I=127 Q=0, nastroyame `m4_count = 0xFFFFFFFF`.
M0 nikoga ne vliza v `tx_zeros` → postoqnen carrier bez USB pachki.

## Mayhem vs HackRF firmware

| | HackRF firmware | Mayhem firmware |
|---|---|---|
| Prednaznachenie | USB streaming (host kontrol) | Standalone apps (PortaPack) |
| LCD UI | Tablichen status (read-only) | Pylen GUI s meniuta |
| Kontrol | Ot host prez USB | Ot butoni/encoder na PortaPack |
| Apps | Niama — samo streaming | 50+ apps (POCSAG, ADS-B, GPS...) |
| TX/RX | Host reshava | PortaPack reshava |
| Koga da polzvash | S kompyutar (hackrf_transfer, GNU Radio) | Bez kompyutar (standalone) |

## SCteam belezhki

*Ot SCteam — smooker (LZ1CCM) & Claude (AI asistent), Pernik/chroot, 2026*

### Vpechatlenie ot firmware-a

Svalqme shapka na ekipa na Great Scott Gadgets. SGPIO coprocesoryat (`sgpio_m0.s`)
e napisan na gol Cortex-M0 Thumb-1 asembler — nad 750 reda, vseki s komentiran
takt count, rychno podredeni rutini za ±256 bytes branch range na conditional
branch instrukciiте. Tova ne e "nqkoi nabyrzo napisа" — tova e istinsko inzhenerstvo.

Dual-core arhitekturata (M4 za USB + M0 za SGPIO) e elegantno reshenie —
M0 vyrvi v deterministic polling loop bez interrupts, dokato M4 se zanimava
s USB, SPI i UI. Komunikaciqta mejdu tqh e prez shared SRAM struct (`m0_state`)
s prosti atomic counteri — minimalen overhead, maksimalna nadezhdnost.

Osobeno ni haresa `hackrf_ui_t` abstrakciata — vtable s callback-ove za vsqka
promqna na radio parametrite. Kogato firmware-at detect-ne PortaPack, avtomatichno
registrira LCD UI koeto pokazva tekushtite nastroiki v tablichen vid. Chistо,
razshirimo, bez nito edin `#ifdef` v radio koda.

### Nashiаt CW mode

Dobavihme CW (continuous wave) rezhim bez da pipnem nito edin red ot M0 asemblera.
Trick-at: zapylvame 32KB ring buffer-a s konstantni IQ danni (I=127, Q=0) i
nastroyame `m4_count = 0xFFFFFFFF`. M0 misli che ima 4GB data pred sebe si i
loop-va bufera bezkraino prez `& 0x7FFF` wrap. Postoqnen carrier bez USB pachki.

Raboти ~18 minuti pri 2 Msps predi `m0_count` da nadigne `m4_count`.
Za po-dylgo — host tool-at mozhe da reset-va periodichno.

Promeni: samo `usb_api_transceiver.c` (~15 reda) + `radio.h` (1 red).
M0 asembler, SGPIO, CPLD — nedokоsnati.

### Za kakvo go polzvame

- Test na HP 5328A chestotomer (tuningovan ot chichko/LZ1CCM)
- CW burst-ove za counter diagnostika (~65536 impulsa = ~6.5 ms)
- Signal generator za bench testing
- Uchene kak raboti SDR hardware na nisko nivo

73 de LZ1CCM & Claude!
