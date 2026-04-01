# HackRF — SCteam Fork

Custom HackRF firmware s CW mode i PortaPack LCD status display.

Fork na [greatscottgadgets/hackrf](https://github.com/greatscottgadgets/hackrf), baziran na v2024.02.1 release.
HackRF One ot 2014 (pre-r6 reviziq).

## Dokumentacia

| Dokument | Opisanie |
|----------|----------|
| [HACKRF_ONE.md](HACKRF_ONE.md) | Hardware reference — chipove, RF path, gain tablica, pin mapping, firmware arhitektura |
| [CORTEX-M0.md](CORTEX-M0.md) | Cortex-M0 Thumb-1 instruction set, branch range ogranicheniq, M0 registrova konvencia |
| [README_CW.md](README_CW.md) | CW mode implementacia, build instrukcii, host tools |

## Kakvo e promeneno

- `firmware/common/hackrf_core.h` — dobaven `TRANSCEIVER_MODE_CW = 6`
- `firmware/hackrf_usb/usb_api_transceiver.c` — CW mode: zapylva ring buffer s I=127 Q=0, nastroyva m4_count trick za postoqnen carrier bez USB pachki

M0 asembler (`sgpio_m0.s`) e **nedokosnat** — CW mode izpolzva syshtestvuvashtiq `TX_RUN` rezhim.

## Build

**VNIMANIE: Kompilirai SAMO s GCC ARM 9/10 (2019-2020)!**

**TOVA E POVRATARASHT SE PROBLEM!** Veche dva pyti sme go dyrlili —
pyrviq pyt predi godini, vtoriq pyt na 2026-04-01. I dva pyti sme
zabravili. CHETI TOVA PREDI DA KOMPILIRASH.

LPC4320 firmware-yt sydyrzha mnogo inline ASM (sgpio_m0.s, startup).
Po-novi GCC ARM versii (11+) generiraat nevvaliden binary — DFU upload
minava uspeshno, no firmware-yt NE boot-va. Izglezha kato USB/DFU
problem, no realnata prichina e toolchain-a.

Provereno: GCC ARM 9-2019/10-2020 — RABOTI.
GCC ARM 11/12/13/14 — NE RABOTI (tihno schupva ASM-a).

Backup toolchain: smooker ima tar.gz s raboteshti versii — pitai predi
da emergevash nov cross-dev!

```bash
# Proveri versiqta PYRVO:
arm-none-eabi-gcc --version
# Triabva: 10.x (2020)

cd firmware/hackrf_usb
mkdir -p build && cd build
cmake .. -DCMAKE_OBJCOPY=/usr/bin/arm-none-eabi-objcopy -DCMAKE_POLICY_VERSION_MINIMUM=3.5
make -j4
```

## Flash

```bash
# DFU mode (vremenno v RAM)
dfu-util --device 1fc9:000c --alt 0 --download build/hackrf_usb_dfu.bin

# SPI flash (permanentno)
hackrf_spiflash -w build/hackrf_usb.bin
```

## Host tools

```bash
# CW carrier (libhackrf)
gcc -O2 -o cw_tx cw_tx.c -I/usr/include/libhackrf -lhackrf
./cw_tx 10000000    # 10 MHz postoqnen carrier

# Burst (ednokraten)
gcc -O2 -o burst_tx burst_tx.c -I/usr/include/libhackrf -lhackrf
./burst_tx 1000000  # 1 MHz burst ~6.5 ms
```

## SCteam

Proekt na **smooker** (LZ1CCM) i **Claude** (AI asistent).
Pernik, 2026.

73 de LZ1CCM!
