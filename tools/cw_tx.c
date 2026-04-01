/*
 * cw_tx.c — HackRF CW (continuous wave) transmitter
 *
 * Activates CW mode on custom HackRF firmware (TRANSCEIVER_MODE_CW = 6).
 * The firmware fills a 32KB ring buffer with constant IQ data (I=127, Q=0)
 * and the M0 core loops it forever through SGPIO → CPLD → DAC → antenna.
 *
 * Usage: cw_tx <freq_hz> [txvga_gain] [amp_enable]
 *   freq_hz    - TX frequency in Hz (e.g. 144500000 for 144.5 MHz)
 *   txvga_gain - IF gain 0-47 dB (default: 20)
 *   amp_enable - RF amp 0/1 (default: 0)
 *
 * Build: gcc -O2 -o cw_tx cw_tx.c $(pkg-config --cflags --libs libusb-1.0)
 *
 * Copyright 2026 SCteam (smooker/LZ1CCM)
 * License: GPL-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <libusb.h>

#define HACKRF_VID 0x1d50
#define HACKRF_PID 0x6089

/* Vendor requests (from hackrf.c) */
#define HACKRF_SET_TRANSCEIVER_MODE  1
#define HACKRF_SAMPLE_RATE_SET       6
#define HACKRF_BASEBAND_FILTER_SET   7
#define HACKRF_SET_FREQ             16
#define HACKRF_AMP_ENABLE           17
#define HACKRF_SET_LNA_GAIN         19
#define HACKRF_SET_VGA_GAIN         20
#define HACKRF_SET_TXVGA_GAIN       21

/* Transceiver modes */
#define MODE_OFF      0
#define MODE_RX       1
#define MODE_TX       2
#define MODE_CW       6

static libusb_device_handle *dev = NULL;
static volatile int running = 1;

static void sighandler(int sig)
{
	(void)sig;
	running = 0;
}

static int vendor_req(uint8_t request, uint16_t value, uint16_t index,
                      uint8_t *data, uint16_t length)
{
	return libusb_control_transfer(dev,
		LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
		LIBUSB_RECIPIENT_DEVICE,
		request, value, index, data, length, 0);
}

static int vendor_req_in(uint8_t request, uint16_t value, uint16_t index,
                         uint8_t *data, uint16_t length)
{
	return libusb_control_transfer(dev,
		LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
		LIBUSB_RECIPIENT_DEVICE,
		request, value, index, data, length, 1000);
}

static int set_freq(uint64_t freq_hz)
{
	/* HACKRF_SET_FREQ uses a 8-byte structure:
	 * uint32_t freq_mhz (MHz part)
	 * uint32_t freq_hz  (Hz remainder)
	 */
	uint8_t buf[8];
	uint32_t freq_mhz = (uint32_t)(freq_hz / 1000000ULL);
	uint32_t freq_remainder = (uint32_t)(freq_hz % 1000000ULL);
	buf[0] = freq_mhz & 0xff;
	buf[1] = (freq_mhz >> 8) & 0xff;
	buf[2] = (freq_mhz >> 16) & 0xff;
	buf[3] = (freq_mhz >> 24) & 0xff;
	buf[4] = freq_remainder & 0xff;
	buf[5] = (freq_remainder >> 8) & 0xff;
	buf[6] = (freq_remainder >> 16) & 0xff;
	buf[7] = (freq_remainder >> 24) & 0xff;

	return libusb_control_transfer(dev,
		LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
		LIBUSB_RECIPIENT_DEVICE,
		HACKRF_SET_FREQ, 0, 0, buf, 8, 0);
}


int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <freq_hz> [txvga_gain] [amp_enable]\n", argv[0]);
		fprintf(stderr, "  freq_hz    - frequency in Hz (1M-6G)\n");
		fprintf(stderr, "  txvga_gain - 0-47 dB (default: 20)\n");
		fprintf(stderr, "  amp_enable - 0 or 1 (default: 0)\n");
		fprintf(stderr, "\nExample: %s 144500000 30 1\n", argv[0]);
		return 1;
	}

	uint64_t freq = strtoull(argv[1], NULL, 10);
	uint32_t txvga = (argc > 2) ? atoi(argv[2]) : 20;
	int amp = (argc > 3) ? atoi(argv[3]) : 0;

	if (txvga > 47) txvga = 47;

	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);

	int r = libusb_init(NULL);
	if (r < 0) {
		fprintf(stderr, "libusb_init failed: %s\n", libusb_strerror(r));
		return 1;
	}

	dev = libusb_open_device_with_vid_pid(NULL, HACKRF_VID, HACKRF_PID);
	if (!dev) {
		fprintf(stderr, "HackRF not found (1d50:6089)\n");
		libusb_exit(NULL);
		return 1;
	}

	libusb_set_auto_detach_kernel_driver(dev, 1);

	r = libusb_claim_interface(dev, 0);
	if (r < 0) {
		fprintf(stderr, "claim_interface: %s\n", libusb_strerror(r));
		libusb_close(dev);
		libusb_exit(NULL);
		return 1;
	}

	printf("HackRF CW TX\n");
	printf("  Freq:     %llu Hz (%.3f MHz)\n",
	       (unsigned long long)freq, freq / 1e6);
	printf("  TX VGA:   %u dB\n", txvga);
	printf("  RF Amp:   %s\n", amp ? "ON (+11dB)" : "OFF");

	/* Set sample rate: 2 MHz (8-byte struct, timeout=0 like libhackrf) */
	{
		uint8_t buf[8];
		uint32_t sr = 2000000, div = 1;
		buf[0]=sr&0xff; buf[1]=(sr>>8)&0xff;
		buf[2]=(sr>>16)&0xff; buf[3]=(sr>>24)&0xff;
		buf[4]=div&0xff; buf[5]=(div>>8)&0xff;
		buf[6]=(div>>16)&0xff; buf[7]=(div>>24)&0xff;
		r = libusb_control_transfer(dev,
			LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
			LIBUSB_RECIPIENT_DEVICE,
			HACKRF_SAMPLE_RATE_SET, 0, 0, buf, 8, 0);
		if (r < 8) fprintf(stderr, "set_sample_rate: %s (r=%d)\n", libusb_strerror(r), r);
		else printf("  Sample rate: 2 MHz\n");
	}

	/* Set baseband filter: 1750000 Hz */
	{
		uint32_t bw = 1750000;
		r = vendor_req(HACKRF_BASEBAND_FILTER_SET, bw & 0xffff, bw >> 16, NULL, 0);
		if (r < 0) fprintf(stderr, "set_bb_filter: %s\n", libusb_strerror(r));
	}

	/* Set frequency */
	r = set_freq(freq);
	if (r < 0) {
		fprintf(stderr, "set_freq: %s\n", libusb_strerror(r));
		goto cleanup;
	}
	printf("  Freq set OK\n");

	/* Activate CW mode first — transceiver_startup() resets switchctrl */
	r = vendor_req(HACKRF_SET_TRANSCEIVER_MODE, MODE_CW, 0, NULL, 0);
	if (r < 0) {
		fprintf(stderr, "set CW mode: %s\n", libusb_strerror(r));
		goto cleanup;
	}

	/* Set TX VGA gain AFTER CW mode (startup resets switches) */
	{
		uint8_t retval = 0;
		r = libusb_control_transfer(dev,
			LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
			LIBUSB_RECIPIENT_DEVICE,
			HACKRF_SET_TXVGA_GAIN, 0, txvga, &retval, 1, 0);
		if (r != 1 || !retval)
			fprintf(stderr, "set_txvga: r=%d retval=%d\n", r, retval);
		else
			printf("  TX VGA gain set: %u dB\n", txvga);
	}

	/* Set RF amp AFTER CW mode */
	r = vendor_req(HACKRF_AMP_ENABLE, amp ? 1 : 0, 0, NULL, 0);
	if (r < 0) fprintf(stderr, "amp_enable: %s\n", libusb_strerror(r));

	printf("  CW TX active! Press Ctrl+C to stop.\n");

	while (running) {
		/* Check if device is still present */
		uint8_t dummy;
		r = libusb_control_transfer(dev,
			LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
			LIBUSB_RECIPIENT_DEVICE,
			14, /* BOARD_ID_READ */
			0, 0, &dummy, 1, 1000);
		if (r < 0) {
			fprintf(stderr, "\nDevice disconnected.\n");
			break;
		}
		sleep(1);
	}

	printf("\nStopping...\n");

	/* Turn off transceiver */
	vendor_req(HACKRF_SET_TRANSCEIVER_MODE, MODE_OFF, 0, NULL, 0);

cleanup:
	libusb_release_interface(dev, 0);
	libusb_close(dev);
	libusb_exit(NULL);

	printf("Done.\n");
	return 0;
}
