/*
 * Author:      Eftychios Papasifakis
 * File:        src/cli.c
 * Project:     ASUS Keyboard Backlight (asus-keyboard-backlight)
 * SPDX-License-Identifier: MIT
 *
 * Description:
 *   asus-kbd-backlight is the command line tool for the keyboard backlight
 *   It parses the arguments and calls the API in kbd.h
 *
 * Usage:
 *   asus-kbd-backlight color RRGGBB [static|breathing|cycle] [slow|medium|fast]
 *   asus-kbd-backlight brightness LEVEL
 *   asus-kbd-backlight status
 */
#include "kbd.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Names accepted on the command line where the index is the enum value in kbd.h */
static const char *const mode_names[] = { "static", "breathing", "cycle" };
static const char *const speed_names[] = { "slow", "medium", "fast" };

static void usage(const char *prog)
{
	fprintf(stderr,
		"usage: %s color RRGGBB [static|breathing|cycle] [slow|medium|fast]\n"
		"       %s brightness LEVEL\n"
		"       %s status\n",
		prog, prog, prog);
}

/* Return the index of name in names or -1 if it is not there */
static int lookup(const char *name, const char *const *names, int count)
{
	for (int i = 0; i < count; i++)
		if (strcmp(name, names[i]) == 0)
			return i;
	return -1;
}

/* Turn a hex color like ff8800 or #ff8800 into red green and blue bytes */
static int parse_hex_color(const char *text, uint8_t *r, uint8_t *g, uint8_t *b)
{
	unsigned long rgb;
	char *end;

	if (text[0] == '#')
		text++;
	if (strlen(text) != 6)
		return -EINVAL;

	/* end points at the first character that is not a hex digit */
	rgb = strtoul(text, &end, 16);
	if (*end != '\0')
		return -EINVAL;

	/* The value is 0xRRGGBB so shift each byte down into place */
	*r = (rgb >> 16) & 0xff;
	*g = (rgb >> 8) & 0xff;
	*b = rgb & 0xff;
	return 0;
}

/* color RRGGBB [mode] [speed] where mode and speed are optional */
static int cmd_color(int argc, char **argv)
{
	int mode = KBD_MODE_STATIC, speed = KBD_SPEED_MEDIUM;
	uint8_t r, g, b;

	if (argc < 1 || parse_hex_color(argv[0], &r, &g, &b)) {
		fprintf(stderr, "invalid color, expected RRGGBB (e.g. ff0000)\n");
		return -EINVAL;
	}
	if (argc > 1) {
		mode = lookup(argv[1], mode_names, 3);
		if (mode < 0) {
			fprintf(stderr, "unknown mode: %s\n", argv[1]);
			return -EINVAL;
		}
	}
	if (argc > 2) {
		speed = lookup(argv[2], speed_names, 3);
		if (speed < 0) {
			fprintf(stderr, "unknown speed: %s\n", argv[2]);
			return -EINVAL;
		}
	}
	return kbd_set_color(r, g, b, mode, speed);
}

/* brightness LEVEL where kbd_set_brightness checks the range */
static int cmd_brightness(int argc, char **argv)
{
	char *end;
	long level;

	if (argc < 1)
		return -EINVAL;
	level = strtol(argv[0], &end, 10);
	if (*end != '\0')
		return -EINVAL;
	return kbd_set_brightness((int)level);
}

/*
 * Print the brightness
 * The color cannot be shown because kbd_rgb_mode is write only
 */
static int cmd_status(void)
{
	int level, max, ret;

	ret = kbd_get_brightness(&level);
	if (!ret)
		ret = kbd_get_max_brightness(&max);
	if (!ret)
		printf("brightness: %d/%d\n", level, max);
	return ret;
}

int main(int argc, char **argv)
{
	int ret;

	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}
	if (!kbd_is_supported()) {
		fprintf(stderr, "no ASUS RGB keyboard found at %s\n", KBD_LED_DIR);
		return 1;
	}

	/* argv[1] is the command and everything after it goes to its handler */
	if (strcmp(argv[1], "color") == 0) {
		ret = cmd_color(argc - 2, argv + 2);
	} else if (strcmp(argv[1], "brightness") == 0) {
		ret = cmd_brightness(argc - 2, argv + 2);
	} else if (strcmp(argv[1], "status") == 0) {
		ret = cmd_status();
	} else {
		usage(argv[0]);
		return 1;
	}

	/* EACCES almost always means the udev rule is not installed */
	if (ret == -EACCES)
		fprintf(stderr, "permission denied: install the udev rule (see README)\n");
	else if (ret)
		fprintf(stderr, "error: %s\n", strerror(-ret));
	return ret ? 1 : 0;
}
