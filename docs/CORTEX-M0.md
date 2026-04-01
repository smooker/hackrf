# Cortex-M0 — Belezhki za HackRF SGPIO coprocesora

## Kakvo e SGPIO?

SGPIO = Serial General Purpose Input/Output. Specifichen periferen modul na LPC43xx
(niama go v STM32 ili drugi ARM-ove). Raboti kato SPI na steroidi — 8 paralеlni
shift registra po 32 bita, clock driven, bez CPU namesa.

V HackRF se polzva za vysokoskorosten transfer na IQ danni mejdu MCU i CPLD/DAC:

```
M0 pishe 32 bytes       SGPIO 8x32-bit shift       CPLD         MAX5864
┌────────────┐ na ISR   ┌───────────────────┐ 8-bit ┌──────┐    ┌─────┐
│ ring buffer├─────────▶│ shadow registri   ├──────▶│XC2C64├───▶│ DAC │──▶ antenna
│ 32KB       │          │ → shift out       │  bus  │      │    │     │
└────────────┘          └───────────────────┘       └──────┘    └─────┘
                         ↑ clock ot Si5351C
```

Kak raboti:
- 8 "slice-a" (kanala), vseki po 32 bita = 32 bytes na interrupt
- Na vseki clock takt SGPIO shift-va danni kym CPLD
- Kogato 32 bita sa izprateni — SGPIO vdiga "exchange" interrupt
- M0 obrabotvа interrupt-a: zapylva shadow registrite s novite 32 bytes
- SGPIO prehvyrlyа shadow → active na sledvashtiq exchange

Sravnenie s drugi interfeysi:
- GPIO — rychno toggle na pin, baven, CPU intensive
- SPI — 1 data liniq + clock, do ~50 MHz
- SGPIO — 8 paralеlni shift kanala, clock driven, do 204 MHz, avtonomen

## Zashto M0?

HackRF LPC4320 ima dva procesora:
- **M4** (204 MHz) — glaven, USB, SPI, UI
- **M0** (204 MHz) — realtime coprocesоr za SGPIO ↔ buffer

M0 e izbran za SGPIO zashtoto:
- Niama interrupts (gol polling loop) → deterministic timing
- Ne se zabavq ot USB/SPI/UI operacii na M4
- Raboti na syshtiq clock (204 MHz) — dostyatchno byrz

## Instruction Set: Thumb-1 (ARMv6-M)

Cortex-M0 poddarzhа **samo Thumb-1** (16-bit instrukcii). Tova e nai-ogranicheniаt ARM instruction set.

### Branch range ogranicheniq

| Instrukcia | Tip | Range | Bytes |
|---|---|---|---|
| `beq`, `bne`, `bgt`... | Conditional | **±256 bytes** | 8-bit signed × 2 |
| `b` | Unconditional | ±2048 bytes | 11-bit signed × 2 |
| `bl` | Branch+Link | ±16 MB | 32-bit (2×16-bit) |
| `bx` | Branch Exchange | neogranichen | registyr |

**VAZHNO:** Conditional branch (`beq`, `bne`, `bgt`) ima samo **±256 bytes** range!
Tova oznachava che dve rutini triabva da sa na mаksimum ~128 instrukции edna ot druga
za da mogat da se vikat s conditional branch.

### Sravnenie s Cortex-M4

| | Cortex-M0 | Cortex-M4 |
|---|---|---|
| Instruction set | Thumb-1 (ARMv6-M) | Thumb-2 (ARMv7E-M) |
| Conditional branch | ±256 bytes | ±1 MB (`beq.w`) |
| DSP instrukcii | niama | da (SIMD, MAC) |
| FPU | niama | opcionalеn (SP FPU) |
| IT block | niama | da (If-Then) |
| Hardware divide | niama | da (UDIV, SDIV) |

### Prakticheski posledici za HackRF firmware

1. **Ordering e kritichen** — vsichki rutini v `sgpio_m0.s` sa podredeni tyrstelivo
   za da sa na ±256 bytes edna ot druga. Komentirano e v "Ordering constraints".

2. **Ne mozhesh da dobavqsh kod lesno** — dori 20 reda dobaveni v sredatasа
   mogat da "iztikyat" rutini izvyn branch range i da poluchiш
   `Error: branch out of range`.

3. **Trampoline pattern** — ako triabva da se skochshi dalech s conditional branch:
   ```asm
   beq nearby_trampoline    // conditional, ±256 bytes
   ...
   nearby_trampoline:
   b far_away_target        // unconditional, ±2048 bytes
   ```

4. **Registrovo adresirаne** — za oshte po-dalech:
   ```asm
   ldr r0, =far_target      // zarezhdash adres v registyr
   bx r0                    // branch prez registyr (neogranichen)
   ```
   No tova e bavno (2 takta + literal pool).

## HackRF M0 registrova konvencia

V `sgpio_m0.s` registrite sa s fiksirano prednaznachenie:

| Registyr | Ime | Prednaznachenie |
|----------|-----|-----------------|
| r4 | `state` | Pointer kym `m0_state` struct |
| r5 | `count` | `m0_count` — broi obraboteni bytes |
| r6 | `sgpio_data` | SGPIO shadow registers base (0x40101100) |
| r7 | `buf_mask` | 0x7FFF — ring buffer mask |
| r8 | `buf_base` | USB bulk buffer base (0x20008000) |
| r9 | `hi_zero` | 0 (izpolzva se chesto) |
| r10 | `buf_size_minus_32` | 0x7FE0 (za RX proverka) |
| r11 | `shortfall_length` | Tekushta dyzhina na shortfall |
| r0-r3 | scratch | Vremenni, clobber-vat se |

## M0 Timing

Vseki SGPIO interrupt = 32 bytes IQ data.
Pri 2 Msps: 32 bytes / 4 bytes_per_sample = 8 samples → interrupt na vseki 4 µs.

M0 triabva da obraboти 32 bytes za < 4 µs pri 204 MHz = ~816 takta.
TX loop-at e ~120 takta — mnogo zapas.

## Uroci naucheni

1. **Ne pipai M0 asemblera ako ne e nuzhno** — branch range ogranicheniqta
   na Thumb-1 pravqt vsqka promqna riskova.

2. **m4_count trick** — vmesto nov M0 mode, mozhesh da "izlyzheш"
   M0 che ima bezkraino data: `m4_count = 0xFFFFFFFF`. M0 loop-va
   ring buffer-a prez `& 0x7FFF` bez da vliza v `tx_zeros`. Raboti ~18 min.

3. **Ordering constraints** — ako triabva da dobavish kod, procheti
   komentara v `sgpio_m0.s` za podredbata i proveri vsichki conditional
   branches che ostavan v range.

## Spravka

- ARM Cortex-M0 Technical Reference Manual (ARM DDI 0432)
- ARMv6-M Architecture Reference Manual
- `firmware/hackrf_usb/sgpio_m0.s` — 750+ reda komenТiran asembler
