/*
 * Author:      Eftychios Papasifakis
 * File:        src/kbd.c
 * Project:     ASUS Keyboard Backlight (asus-keyboard-backlight)
 * SPDX-License-Identifier: MIT
 *
 * Description:
 *   Implementation of the keyboard backlight API in kbd.h
 *
 *   sysfs attributes are plain files so all the work is done with the
 *   open read write and close system calls
 *
 *   Errors are returned as negative errno values instead of being printed
 *   so the CLI and the GUI can each show them in their own way
 */
#include "kbd.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Full paths of the attribute files built at compile time from KBD_LED_DIR */
#define BRIGHTNESS_PATH     KBD_LED_DIR "/brightness"
#define MAX_BRIGHTNESS_PATH KBD_LED_DIR "/max_brightness"
#define RGB_MODE_PATH       KBD_LED_DIR "/kbd_rgb_mode"

/*
 * First field written to kbd_rgb_mode
 *   1 applies the setting and saves it in the firmware
 *   0 only applies it until the next reboot
 */
#define KBD_CMD_SAVE 1

/* Write a text value to an attribute file in one write call */
static int write_attr(const char *path, const char *value)
{
	size_t len = strlen(value);
	ssize_t n;
	int fd, ret = 0;

	fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0)
		return -errno;

	/* sysfs passes the whole buffer to the driver store function at once */
	n = write(fd, value, len);
	if (n < 0)
		ret = -errno;
	else if ((size_t)n != len)
		ret = -EIO;

	/* Report a close error only if nothing failed before it */
	if (close(fd) < 0 && !ret)
		ret = -errno;
	return ret;
}

/* Read an attribute file that holds one decimal number */
static int read_int_attr(const char *path, int *out)
{
	char buf[32], *end;
	ssize_t n;
	long value;
	int fd, ret;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -errno;

	/* Leave room for the terminating null byte */
	n = read(fd, buf, sizeof(buf) - 1);
	ret = n < 0 ? -errno : 0;
	close(fd);
	if (ret)
		return ret;

	/* strtol stops at the trailing newline that sysfs adds */
	buf[n] = '\0';
	errno = 0;
	value = strtol(buf, &end, 10);
	if (end == buf || errno)
		return -EINVAL;

	*out = (int)value;
	return 0;
}

/* The RGB attribute only exists on models with an RGB keyboard */
bool kbd_is_supported(void)
{
	return access(RGB_MODE_PATH, F_OK) == 0;
}

int kbd_get_brightness(int *level)
{
	return read_int_attr(BRIGHTNESS_PATH, level);
}

int kbd_get_max_brightness(int *level)
{
	return read_int_attr(MAX_BRIGHTNESS_PATH, level);
}

int kbd_set_brightness(int level)
{
	char value[16];
	int max, ret;

	/* Check the range here so the caller gets a clear EINVAL */
	ret = kbd_get_max_brightness(&max);
	if (ret)
		return ret;
	if (level < 0 || level > max)
		return -EINVAL;

	snprintf(value, sizeof(value), "%d", level);
	return write_attr(BRIGHTNESS_PATH, value);
}

int kbd_set_color(uint8_t red, uint8_t green, uint8_t blue,
		  enum kbd_mode mode, enum kbd_speed speed)
{
	char value[64];

	/* Callers can pass any int as an enum so check the range */
	if (mode < KBD_MODE_STATIC || mode > KBD_MODE_COLOR_CYCLE)
		return -EINVAL;
	if (speed < KBD_SPEED_SLOW || speed > KBD_SPEED_FAST)
		return -EINVAL;

	/* Format expected by the driver: cmd mode red green blue speed */
	snprintf(value, sizeof(value), "%d %d %u %u %u %d",
		 KBD_CMD_SAVE, mode, red, green, blue, speed);
	return write_attr(RGB_MODE_PATH, value);
}
