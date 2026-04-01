# SDR HOWTO — SCteam

## SDR Hardware Comparison

### Bit depth vs Bandwidth trade-off (USB 2.0)

USB 2.0 realen throughput: ~35 MB/s (ot 480 Mbps theoretical)

| SDR         | ADC/DAC | IQ bytes/sample | Max MSPS   | BW        |
|-------------|---------|-----------------|------------|-----------|
| HackRF One  | 8-bit   | 2 bytes (I+Q)   | ~17.5 MSPS | 20 MHz    |
| PlutoSDR    | 12-bit  | 4 bytes (I+Q)   | ~8.75 MSPS | 20 MHz*   |
| RTL-SDR     | 8-bit   | 2 bytes (I+Q)   | ~2.4 MSPS  | 2.4 MHz   |

* PlutoSDR bandwidth do 56 MHz s hack, no USB 2.0 ogranicava realnite samples.

Formuli:
- 8-bit IQ: 35 MB/s / 2 bytes = 17.5 MSPS
- 12-bit IQ (packed v 16-bit): 35 MB/s / 4 bytes = 8.75 MSPS

### PlutoSDR (ADALM-PLUTO)

Revizii:
- Rev.B — 1 TX/RX, AD9363, Zynq 7010, niama vynshno clock SMA
- Rev.C — 2x2 MIMO, AD9363, Zynq 7020, USB-C, SMA clock vhod

Specifiacii:
- ADC/DAC: 12-bit (v AD9363)
- Stock chestota: 325 MHz – 3.8 GHz
- Hack-nat: 70 MHz – 6 GHz (AD9363 = binnat AD9361, syshtiq silicii)
- Bandwidth stock: 20 MHz, hack: 56 MHz
- TX power: ~7 dBm (5 mW) max — tryabva vhnshen PA za realna rabota
- FPGA: Xilinx Zynq 7010 (Rev.B) / 7020 (Rev.C)
- USB 2.0 — GLAVNO OGRANICENIE!

AD9363 vs AD9361:
- Syshtiq chip, AD9363 e firmware-locked versiq
- Hack otkluchva ogranichenieto prez SSH:
  ssh root@192.168.2.1 (parola: analog)
  fw_setenv compatible ad9361
  fw_setenv ad9361_ext_band_enable 1
  reboot

Vhnshen clock (Rev.C):
- SMA vhod za 10 MHz ili 40 MHz reference
- GPS disciplined 10 MHz raboti s 40 MHz PLL konfiguraciq

srsRAN (4G LTE):
- Raboti s PlutoSDR prez SoapyPlutoSDR
- Prakticheski limit: 5-6 MHz LTE bandwidth (USB 2.0)
- 10/20 MHz LTE — nenadezden
- Tryabva: libiio, libad9361-iio, SoapyPlutoSDR, srsRAN

GNU Radio integraciq:
- gr-iio — nativno ot Analog Devices (preporycan)
- SoapySDR + SoapyPlutoSDR — po-universalen, tryabva za srsRAN
- I dvata rabotyat na Gentoo

Kupuvane:
- Mouser: https://www.mouser.com/c/?q=ADALM-PLUTO
- Digikey: https://www.digikey.com/en/products/result?keywords=ADALM-PLUTO
- Analog Devices: https://www.analog.com/en/resources/evaluation-hardware-and-software/evaluation-boards-kits/adalm-pluto.html
- IZBYAGVAI AliExpress klonove — falshivi AD9363 chipove

Rekomandaciq: Rev.C za MIMO + SMA clock vhod

### USRP B200 (Ettus/NI)

- 12-bit, USB 3.0, 56 MHz BW, 70 MHz – 6 GHz
- Reshava USB bottleneck-a: 12-bit x 56 MSPS
- Cena: ~$1400 — za seriozna rabota

## Gentoo emerge za SDR stack

package.use:
  net-wireless/gnuradio grc qt5 soapy zeromq analog audio channels digital dtv fec filter network modtool sdl trellis utils vocoder wavelet
  net-wireless/gr-osmosdr hackrf rtlsdr soapy iqbalance
  net-wireless/soapysdr hackrf rtlsdr

emerge:
  emerge -av net-wireless/gnuradio net-wireless/gr-osmosdr net-wireless/soapysdr

## ZeroMQ v GNU Radio

ZeroMQ pozvolyava IQ stream mejdu otdelni programi:
- GNU Radio → Python dekoder
- GNU Radio → drug kompyutyr po mrezha
- GNU Radio TX → GNU Radio RX (loopback test)

Blokove: ZMQ PUB Sink, ZMQ SUB Source, ZMQ PUSH/PULL
