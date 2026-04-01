# USB EHCI/xHCI Port Routing — HOWTO

## Проблем

HackRF One DFU mode не работи с xHCI USB контролери.
`dfu-util` upload-ва данни, но LPC4320 bootloader-ът не получава
финалния zero-length DFU_DNLOAD пакет → firmware не стартира.

## Intel 8/9 Series USB Port Routing

Intel PCH (8 Series / C220, 9 Series) имат:
- **xHCI** controller (00:14.0) — USB 3.0 + USB 2.0
- **EHCI #1** controller (00:1d.0) — USB 2.0 companion
- **EHCI #2** controller (00:1a.0) — USB 2.0 companion

Физическите USB портове могат да се рутират между xHCI и EHCI
чрез PCI configuration registers.

### ПРАВИЛНИЯТ регистър: XUSB2PR (offset 0xD0)

**ВНИМАНИЕ: Регистърът е `0xD0` (XUSB2PR), НЕ `0xDC` (USB2PRM)!**

`0xDC` (USB2PRM) е routing mask но НЕ контролира реалното switch-ване.
`0xD0` (XUSB2PR) е действителният port routing register.

| Register | Offset | Описание |
|----------|--------|----------|
| **XUSB2PR** | **0xD0** | **USB 2.0 Port Routing — ТОЗИ контролира routing!** |
| USB3PRM  | 0xD8   | USB 3.0 Port Routing Mask |
| USB2PRM  | 0xDC   | USB 2.0 Port Routing Mask (НЕ switch-ва!) |

Bit N = 1 → порт N е рутиран към xHCI
Bit N = 0 → порт N е рутиран към EHCI

### Верифицирано на sw2 (2026-04-01)

```
Грешен регистър 0xDC: setpci записва но портовете НЕ се местят
Правилен регистър 0xD0: 0x00000FFF → 12 USB2 порта, всички на xHCI
                         0x00000000 → всички на EHCI — РАБОТИ!
```

EHCI routing резултат (usb_tree.pl):
```
Bus 1 (EHCI #2  PCI=00:1a.0  480M)
  ├── 1-1: Hub (4 ports)
  │   ├── 1-1.1: ShadderGKW-11 (клавиатура)
  │   ├── 1-1.2: USB Optical Mouse [Logitech]
  │   ├── port 3: (empty)
  │   ├── port 4: (empty)

Bus 2 (EHCI #1  PCI=00:1d.0  480M)
  ├── 2-1: Hub (6 ports)
  │   ├── port 1: (empty)
  │   ├── port 2: (empty)
  │   ├── port 3: (empty)
  │   ├── 2-1.4: LPC [NXP] (1fc9:000c) ← HackRF DFU на EHCI!
  │   ├── port 5: (empty)
  │   ├── port 6: (empty)
```

### xHCI Unbind — НЕ РАБОТИ за Intel 8 Series!

```bash
# НЕ РАБОТИ:
echo "0000:00:14.0" > /sys/bus/pci/drivers/xhci_hcd/unbind

# EHCI companion hub-овете са "switchable" — рутират към xHCI.
# Без xHCI, портовете остават без controller.
# Трябва setpci XUSB2PR (0xD0) вместо unbind.
```

## Процедура за HackRF DFU flash (sw2)

**ВАЖНО: Работете през SSH от друга машина! USB клавиатура/мишка ще се преместят на EHCI.**

```bash
# 1. Запази текущата маска:
OLD_MASK=$(setpci -s 00:14.0 0xd0.l)
echo "Old XUSB2PR: 0x$OLD_MASK"
# Очаквано: 0x00000fff (12 порта на xHCI)

# 2. Рутирай всички USB2 портове към EHCI:
setpci -s 00:14.0 0xd0.l=00000000

# 3. Включи HackRF в DFU mode (натисни DFU бутон, включи USB)

# 4. Провери — HackRF трябва да е на Bus 1 или 2 (EHCI):
perl usb_tree.pl
# Очаквано: Bus 2, port 2-1.4: LPC [NXP] (1fc9:000c)

# 5. DFU Flash:
dfu-util --device 1fc9:000c --alt 0 -D /chroot/claude/home/claude-agent/work/hackrf/upstream/firmware/hackrf_usb/build/hackrf_usb.dfu

# 6. Изчакай 5 сек, провери дали boot-на:
lsusb | grep 1d50
# Очаквано: 1d50:6089 = HackRF One

# 7. Permanent SPI flash:
hackrf_spiflash -w /chroot/claude/home/claude-agent/work/hackrf/upstream/firmware/hackrf_usb/build/hackrf_usb.bin

# 8. Върни USB routing обратно към xHCI:
setpci -s 00:14.0 0xd0.l=$OLD_MASK

# 9. Power cycle HackRF (извади и включи USB)

# 10. Финален тест:
hackrf_info
```

## Машини

| Машина | xHCI PCI | EHCI PCI | XUSB2PR default | Физически EHCI |
|--------|----------|----------|-----------------|----------------|
| st     | 00:14.0  | НЯМА     | —               | НЯМА — само xHCI! |
| sw2    | 00:14.0  | 00:1a.0, 00:1d.0 | 0x00000FFF (12 порта) | Да, чрез setpci 0xD0 |
| sw1    | 00:14.0  | 00:1a.0, 00:1d.0 | TBD | TBD |

## Диагностика

```bash
# USB controller-и:
lspci | grep -i usb

# USB дърво (пълно, с празни портове):
perl usb_tree.pl

# Порт скенер (интерактивен):
perl usb_port_scan.pl

# Port routing (ПРАВИЛНИЯТ регистър):
setpci -s 00:14.0 0xd0.l    # XUSB2PR — реален routing
setpci -s 00:14.0 0xdc.l    # USB2PRM — mask (НЕ switch-ва!)
```

## Хронология на грешки

1. Пробвахме `0xDC` (USB2PRM) — записва стойност но портовете не се местят
2. Пробвахме xHCI unbind — EHCI companion hubs останаха без routing, нищо не работи
3. Намерихме `0xD0` (XUSB2PR) — правилният регистър, портовете се местят веднага
4. HackRF DFU (1fc9:000c) се появи на Bus 2 EHCI — ГОТОВ за flash
