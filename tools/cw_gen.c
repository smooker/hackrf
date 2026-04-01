/*
 * cw_gen.c — Interactive HackRF CW signal generator
 *
 * Keyboard-controlled continuous wave transmitter.
 * Uses custom HackRF firmware with TRANSCEIVER_MODE_CW = 6.
 *
 * Controls:
 *   Up/Down    — +/- 10 MHz
 *   Right/Left — +/- 1 MHz
 *   +/-        — +/- 100 kHz
 *   a          — toggle RF amp (MGA-81563)
 *   g/G        — gain +1/-1 dB
 *   q/Ctrl+C   — quit
 *
 * Build: gcc -O2 -o cw_gen cw_gen.c $(pkg-config --cflags --libs libusb-1.0)
 *
 * Copyright 2026 SCteam (smooker/LZ1CCM)
 * License: GPL-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <termios.h>
#include <libusb.h>

#define HACKRF_VID 0x1d50
#define HACKRF_PID 0x6089

/* Vendor requests */
#define HACKRF_SET_TRANSCEIVER_MODE  1
#define HACKRF_SAMPLE_RATE_SET       6
#define HACKRF_BASEBAND_FILTER_SET   7
#define HACKRF_SET_FREQ             16
#define HACKRF_AMP_ENABLE           17
#define HACKRF_SET_TXVGA_GAIN       21

/* Transceiver modes */
#define MODE_OFF      0
#define MODE_CW       6

/* Frequency limits */
#define FREQ_MIN      1000000ULL      /*   1 MHz */
#define FREQ_MAX      6000000000ULL   /*   6 GHz */

static libusb_device_handle *dev = NULL;
static volatile int running = 1;
static struct termios orig_termios;

static void sighandler(int sig)
{
	(void)sig;
	running = 0;
}

static void term_raw(void)
{
	struct termios raw;
	tcgetattr(STDIN_FILENO, &orig_termios);
	raw = orig_termios;
	raw.c_lflag &= ~(ECHO | ICANON);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;  /* 100ms timeout */
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void term_restore(void)
{
	tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static int vendor_req(uint8_t request, uint16_t value, uint16_t index,
                      uint8_t *data, uint16_t length)
{
	return libusb_control_transfer(dev,
		LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
		LIBUSB_RECIPIENT_DEVICE,
		request, value, index, data, length, 0);
}

static int set_freq(uint64_t freq_hz)
{
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

static int set_txvga(uint32_t gain)
{
	uint8_t retval = 0;
	int r = libusb_control_transfer(dev,
		LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR |
		LIBUSB_RECIPIENT_DEVICE,
		HACKRF_SET_TXVGA_GAIN, 0, gain, &retval, 1, 0);
	return (r == 1 && retval) ? 0 : -1;
}

static void show_status(uint64_t freq, uint32_t gain, int amp)
{
	printf("\r\033[K  %llu.%03llu %03llu MHz  |  VGA %2u dB  |  AMP %s  ",
		(unsigned long long)(freq / 1000000ULL),
		(unsigned long long)((freq % 1000000ULL) / 1000),
		(unsigned long long)(freq % 1000),
		gain,
		amp ? "ON " : "OFF");
	fflush(stdout);
}

int main(int argc, char *argv[])
{
	uint64_t freq = (argc > 1) ? strtoull(argv[1], NULL, 10) : 100000000ULL;
	uint32_t txvga = (argc > 2) ? atoi(argv[2]) : 20;
	int amp = (argc > 3) ? atoi(argv[3]) : 0;  /* default OFF — amp is inverted on pre-r6 boards */

	if (txvga > 47) txvga = 47;
	if (freq < FREQ_MIN) freq = FREQ_MIN;
	if (freq > FREQ_MAX) freq = FREQ_MAX;

	signal(SIGINT, sighandler);
	signal(SIGTERM, sighandler);

	int r = libusb_init(NULL);
	if (r < 0) {
		fprintf(stderr, "libusb_init: %s\n", libusb_strerror(r));
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

	/* Set sample rate: 2 MHz */
	{
		uint8_t buf[8];
		uint32_t sr = 2000000, div = 1;
		buf[0]=sr&0xff; buf[1]=(sr>>8)&0xff;
		buf[2]=(sr>>16)&0xff; buf[3]=(sr>>24)&0xff;
		buf[4]=div&0xff; buf[5]=(div>>8)&0xff;
		buf[6]=(div>>16)&0xff; buf[7]=(div>>24)&0xff;
		libusb_control_transfer(dev,
			LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_VENDOR |
			LIBUSB_RECIPIENT_DEVICE,
			HACKRF_SAMPLE_RATE_SET, 0, 0, buf, 8, 0);
	}

	/* Set baseband filter: 1750000 Hz */
	{
		uint32_t bw = 1750000;
		vendor_req(HACKRF_BASEBAND_FILTER_SET, bw & 0xffff, bw >> 16, NULL, 0);
	}

	/* Set frequency */
	set_freq(freq);

	/* Activate CW mode first — transceiver_startup() resets switchctrl */
	r = vendor_req(HACKRF_SET_TRANSCEIVER_MODE, MODE_CW, 0, NULL, 0);
	if (r < 0) {
		fprintf(stderr, "set CW mode: %s\n", libusb_strerror(r));
		goto cleanup;
	}

	/* Set gain and amp AFTER CW mode (startup resets switches) */
	set_txvga(txvga);
	vendor_req(HACKRF_AMP_ENABLE, amp ? 1 : 0, 0, NULL, 0);

	printf("HackRF CW Generator — Interactive\n");
	printf("  Up/Down: +/-10 MHz  Left/Right: +/-1 MHz  +/-: +/-100 kHz\n");
	printf("  a: amp toggle  g/G: gain +/-1  q: quit\n\n");

	term_raw();
	show_status(freq, txvga, amp);

	while (running) {
		char c;
		int n = read(STDIN_FILENO, &c, 1);
		if (n <= 0) continue;

		/* Arrow keys: ESC [ A/B/C/D */
		if (c == 0x1b) {
			char seq[2];
			if (read(STDIN_FILENO, &seq[0], 1) <= 0) continue;
			if (seq[0] != '[') continue;
			if (read(STDIN_FILENO, &seq[1], 1) <= 0) continue;

			switch (seq[1]) {
			case 'A': /* Up: +10 MHz */
				freq += 10000000ULL;
				break;
			case 'B': /* Down: -10 MHz */
				if (freq > 10000000ULL + FREQ_MIN)
					freq -= 10000000ULL;
				else
					freq = FREQ_MIN;
				break;
			case 'C': /* Right: +1 MHz */
				freq += 1000000ULL;
				break;
			case 'D': /* Left: -1 MHz */
				if (freq > 1000000ULL + FREQ_MIN)
					freq -= 1000000ULL;
				else
					freq = FREQ_MIN;
				break;
			}
		} else {
			switch (c) {
			case '+': case '=':
				freq += 100000ULL;
				break;
			case '-': case '_':
				if (freq > 100000ULL + FREQ_MIN)
					freq -= 100000ULL;
				else
					freq = FREQ_MIN;
				break;
			case 'a': case 'A':
				amp = !amp;
				vendor_req(HACKRF_AMP_ENABLE, amp ? 1 : 0, 0, NULL, 0);
				show_status(freq, txvga, amp);
				continue;
			case 'g':
				if (txvga < 47) txvga++;
				set_txvga(txvga);
				show_status(freq, txvga, amp);
				continue;
			case 'G':
				if (txvga > 0) txvga--;
				set_txvga(txvga);
				show_status(freq, txvga, amp);
				continue;
			case 'q': case 'Q':
				running = 0;
				continue;
			default:
				continue;
			}
		}

		/* Clamp frequency */
		if (freq > FREQ_MAX) freq = FREQ_MAX;
		if (freq < FREQ_MIN) freq = FREQ_MIN;

		set_freq(freq);
		show_status(freq, txvga, amp);
	}

	term_restore();
	printf("\n\nStopping...\n");

	vendor_req(HACKRF_SET_TRANSCEIVER_MODE, MODE_OFF, 0, NULL, 0);

cleanup:
	libusb_release_interface(dev, 0);
	libusb_close(dev);
	libusb_exit(NULL);

	printf("Done.\n");
	return 0;
}
