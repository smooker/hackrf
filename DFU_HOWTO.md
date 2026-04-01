# HackRF DFU — NXP LPC4320 Boot Process

## Какво е DFU

Device Firmware Upgrade — USB протокол за зареждане на firmware в RAM
без SPI flash. Използва се за:
- възстановяване на brick-нат HackRF
- тестване на firmware без да пишеш flash
- развитие на firmware (бърз цикъл)

## LPC4320 Boot ROM

NXP LPC4320 има вграден ROM bootloader който поддържа DFU.
Активира се с DFU бутона на HackRF:
1. Задръж DFU бутона
2. Натисни RESET (или включи USB)
3. Пусни DFU след като 3V3 LED светне

Устройството се появява като USB `1fc9:000c` (NXP DFU).

## Формат на .dfu файла

```
┌─────────────────────────────────┐
│ NXP Boot Header (16 bytes)      │ ← dfu.py генерира
├─────────────────────────────────┤
│ Firmware Binary (_dfu.bin)      │ ← компилиран с -DDFU_MODE
│ (M4 code + embedded M0 binary) │    и RAM linker script
├─────────────────────────────────┤
│ DFU Suffix (16 bytes)           │ ← dfu-suffix добавя
└─────────────────────────────────┘
```

### NXP Boot Header (16 bytes)

ROM bootloader-ът на LPC4320 очаква header преди image-а:

```
Offset  Size  Стойност         Описание
0x00    1     0xDA             Magic byte 1
0x01    1     0xFF             Magic byte 2
0x02    2     filesize/512+1   Брой 512-byte блокове (LE16)
0x04    12    0xFF * 12        Reserved (padding)
```

Генерира се от `firmware/dfu.py`:
```python
x = struct.pack('<H', os.path.getsize(name + '_dfu.bin') // 512 + 1)
header = [0xDA, 0xFF, x[0], x[1]] + [0xFF] * 12
```

Пример за hackrf_usb_dfu.bin (43044 bytes):
- 43044 / 512 = 84.07 → int(84) + 1 = 85 = 0x0055
- Header: `DA FF 55 00 FF FF FF FF FF FF FF FF FF FF FF FF`

### DFU Suffix (16 bytes, USB DFU 1.1 spec)

```
Offset  Size  Стойност    Описание
0x00    2     0xFFFF      bcdDevice (any)
0x01    2     0x000C      idProduct (NXP DFU)
0x02    2     0x1FC9      idVendor (NXP)
0x06    2     0x0100      bcdDFU (DFU 1.0)
0x08    3     "UFD"       ucDfuSignature (reversed "DFU")
0x0B    1     0x10        bLength (16)
0x0C    4     CRC32       dwCRC (на целия файл вкл. suffix без CRC)
```

Добавя се от `dfu-suffix`:
```bash
dfu-suffix --vid=0x1fc9 --pid=0x000c --did=0x0 -a file.dfu
```

## Три варианта firmware binaries

Build системата генерира 3 варианта за всеки проект:

| Variant | Compile flags | Linker script | Предназначение |
|---------|--------------|---------------|----------------|
| `hackrf_usb.bin` | CFLAGS_M4 | rom_to_ram | SPI flash (permanентно) |
| `hackrf_usb_ram.bin` | CFLAGS_M4_RAM | RAM only | JTAG/SWD зареждане |
| `hackrf_usb_dfu.bin` | CFLAGS_M4_RAM + `-DDFU_MODE` | RAM only | DFU зареждане |

Разлика `_dfu` vs `_ram`: само `-DDFU_MODE` флага, който
може да промени USB descriptor-и или boot поведение.

## Build pipeline (от CMakeLists)

```
hackrf_usb_dfu.bin          (make, автоматично)
  │
  ├─ cp → _tmp.dfu
  ├─ dfu-suffix --vid=0x1fc9 --pid=0x000c -a _tmp.dfu
  ├─ python3 dfu.py hackrf_usb  → _header.bin
  ├─ cat _header.bin _tmp.dfu → hackrf_usb.dfu
  └─ cleanup _tmp.dfu _header.bin
```

CMake targets:
```bash
make                      # build-ва .bin файлове (винаги)
make hackrf_usb.dfu       # build-ва .dfu (изисква dfu-suffix)
make hackrf_usb-program   # dfu-util --download hackrf_usb.dfu
make hackrf_usb-flash     # hackrf_spiflash -Rw hackrf_usb.bin
```

## Ръчно генериране на .dfu (без dfu-suffix)

Ако `dfu-suffix` не е инсталиран, може с Python:

```python
#!/usr/bin/env python3
"""Генерира hackrf_usb.dfu от hackrf_usb_dfu.bin"""
import struct, os, binascii

INPUT = "hackrf_usb_dfu.bin"
OUTPUT = "hackrf_usb.dfu"
VID = 0x1FC9
PID = 0x000C

# NXP boot header
size = os.path.getsize(INPUT)
blocks = size // 512 + 1
header = bytes([0xDA, 0xFF]) + struct.pack('<H', blocks) + b'\xFF' * 12

# Read firmware
with open(INPUT, 'rb') as f:
    firmware = f.read()

# DFU suffix (без CRC засега)
suffix_no_crc = struct.pack('<HHHH3sB',
    0xFFFF,      # bcdDevice
    PID,         # idProduct
    VID,         # idVendor
    0x0100,      # bcdDFU
    b'UFD',      # ucDfuSignature
    16           # bLength
)

# CRC32 на всичко (header + firmware + suffix_no_crc)
payload = header + firmware + suffix_no_crc
crc = binascii.crc32(payload) & 0xFFFFFFFF

# Пълен suffix с CRC
suffix = suffix_no_crc + struct.pack('<I', crc)

# Запис
with open(OUTPUT, 'wb') as f:
    f.write(header + firmware + suffix)

print(f"{OUTPUT}: {os.path.getsize(OUTPUT)} bytes "
      f"(header=16, firmware={size}, suffix=16)")
```

## Flash методи

### DFU (в RAM, временно)
```bash
dfu-util --device 1fc9:000c --alt 0 --download hackrf_usb.dfu
```

### SPI flash (перманентно)
```bash
hackrf_spiflash -w hackrf_usb.bin
```
Не изисква DFU mode — работи с нормално свързан HackRF.

### hackrf_spiflash -Rw (flash + reset)
```bash
hackrf_spiflash -Rw hackrf_usb.bin
```
Флашва и рестартира — firmware-ът се зарежда веднага.

## Процедура по оживяване (DFU recovery)

### Стъпка 1: EHCI routing (sw2)
```bash
OLD_MASK=$(setpci -s 00:14.0 0xdc.l)
setpci -s 00:14.0 0xdc.l=00000000
```

### Стъпка 2: HackRF в DFU mode
1. Махни PortaPack (ако има)
2. Задръж DFU бутона
3. Натисни RESET или re-plug USB
4. Пусни DFU след 3V3 LED
5. Провери: `lsusb | grep 1fc9:000c`

### Стъпка 3: Тест с blinky (минимален firmware)
```bash
dfu-util --device 1fc9:000c --alt 0 --download \
  /mnt/hdd/chroot/claude/home/claude/work/hackrf/firmware/blinky/build/blinky_dfu.bin.dfu
```
Ако LED-овете мигат → DFU работи, проблемът е в hackrf_usb.
Ако не мигат → хардуерен проблем или DFU transfer грешка.

### Стъпка 4: Flash hackrf_usb (когато е оправен)
```bash
# DFU (RAM, временно):
dfu-util --device 1fc9:000c --alt 0 --download \
  /mnt/hdd/chroot/claude/home/claude/work/hackrf/firmware/hackrf_usb/build/hackrf_usb.dfu

# SPI flash (перманентно, само ако hackrf_usb boot-ва):
hackrf_spiflash -Rw hackrf_usb.bin
```

### Стъпка 5: Върни USB routing
```bash
setpci -s 00:14.0 0xdc.l=$OLD_MASK
```

### Текущ статус (2026-04-01) — РЕШЕНО ✓
- blinky_dfu.bin.dfu → РАБОТИ (LED мигат) ✓
- hackrf_usb.dfu (GCC 14.2) → DFU boot ✓, SPI boot ✗
- hackrf_usb.dfu (GCC 10.2 + CW patch) → DFU boot ✓, **SPI boot ✓** ✓✓✓
- Board serial: `000000000000000046d067dc29313c47`
- Board: HackRF One pre-r6 (2014), Firmware v2024.02.1 + CW mode

### Fix 1: m0_rom_to_ram (hackrf_usb.c:231)
```c
#ifndef DFU_MODE
    // In DFU mode there is no ROM (SPI flash may be empty/corrupt)
    m0_rom_to_ram();
#endif
```
Без този fix DFU boot НИКОГА не работи ако SPI flash е празен/корумпиран.

### Fix 2: Toolchain
GCC 14.2 генерира binary който не boot-ва от SPI flash.
Използвай GCC 10.2.1 (ARM Embedded Toolchain 10-2020-q4-major).

## Тест: custom build vs official firmware (2026-04-01)

### Резултати
| Firmware | DFU boot | SPI boot | Размер |
|----------|----------|----------|--------|
| Official v2024.02.1 | ✓ | **✓** | 42248 B |
| Наш build (GCC 14.2, без CW) | ✓ | **✗** | 43012 B |
| Наш build (GCC 14.2, с CW) | ✓ | **✗** | 43012 B |
| Наш build (GCC 10.2, с CW) | ✓ | **✓** | 42692 B |
| blinky | ✓ | не е тестван | 15680 B |

### Заключение
Проблемът е в **GCC 14.2 toolchain** (Gentoo arm-none-eabi-gcc 14.2.1).
GCC 10.2.1 (ARM Embedded Toolchain 10-2020-q4-major) генерира работещ binary.
GCC 14.2 генерира +764 bytes по-голям binary с различен code layout
който чупи SPI boot на pre-r6 HackRF One (вероятно SPIFI cache issue).

### Работеща toolchain
```
/home/claude/work/arm/gcc-arm-none-eabi-10-2020-q4-major/bin/arm-none-eabi-gcc
```
Версия: 10.2.1 20201103 (GNU Arm Embedded Toolchain 10-2020-q4-major)

### Build команда (с GCC 10)
```bash
export PATH="/home/claude/work/arm/gcc-arm-none-eabi-10-2020-q4-major/bin:$PATH"
cd firmware/hackrf_usb && rm -rf build && mkdir build && cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5
make -j4
```

### Процедура за flash (пълна, работеща)
1. EHCI routing: `setpci -s 00:14.0 0xdc.l=00000000`
2. DFU mode: DFU бутон + RESET, провери `lsusb | grep 1fc9`
3. DFU boot:
   `dfu-util --device 1fc9:000c --alt 0 --download hackrf_usb.dfu`
4. Провери: `lsusb | grep 1d50`
5. SPI flash (БЕЗ -R!):
   `hackrf_spiflash -w hackrf_usb.bin`
6. Re-plug USB (без DFU бутон)
7. `hackrf_info`
8. Върни routing: `setpci -s 00:14.0 0xdc.l=$OLD_MASK`

## Проблеми на sw2

- xHCI (Intel 00:14.0) губи zero-length финален пакет → DFU upload
  минава но firmware не boot-ва
- Решение: setpci port routing към EHCI (САМО от host, НИКОГА от chroot!)
- hackrf_spiflash работи на xHCI (не ползва DFU)
- xhci_hcd е BUILTIN — не може rmmod

## Бележки

- HackRF One ревизия: 2014 (pre-r6)
- MCU: NXP LPC4320 (dual-core Cortex-M4 + Cortex-M0)
- Firmware version: v2024.02.1 + CW mode patch
- DFU USB ID: 1fc9:000c (NXP Semiconductors)
