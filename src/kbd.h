/*
 * Author:      Eftychios Papasifakis
 * File:        src/kbd.h
 * Project:     ASUS Keyboard Backlight (asus-keyboard-backlight)
 * SPDX-License-Identifier: MIT
 *
 * Description:
 *   Public API for controlling the RGB keyboard backlight of ASUS TUF
 *   laptops
 *
 *   It is a thin wrapper around two sysfs attributes that the asus-wmi
 *   kernel driver exposes in /sys/class/leds/asus::kbd_backlight
 *
 *     brightness     level from 0 to max_brightness
 *     kbd_rgb_mode   write only and takes: cmd mode red green blue speed
 *
 *   Every function that returns int returns 0 on success or a negative
 *   errno value on failure just like a kernel function would
 */
#ifndef KBD_H
#define KBD_H

#include <stdbool.h>
#include <stdint.h>

/* Let the C++ GUI call these C functions without name mangling */
#ifdef __cplusplus
extern "C" {
#endif

/* Where the asus-wmi driver puts the keyboard LED device */
#define KBD_LED_DIR "/sys/class/leds/asus::kbd_backlight"

/* Lighting effects using the mode ids the firmware expects */
enum kbd_mode {
	KBD_MODE_STATIC = 0,      /* one solid color */
	KBD_MODE_BREATHING = 1,   /* color fades in and out */
	KBD_MODE_COLOR_CYCLE = 2, /* cycles through all colors and ignores the given color */
};

/* Effect speed which has no visible effect in static mode */
enum kbd_speed {
	KBD_SPEED_SLOW = 0,
	KBD_SPEED_MEDIUM = 1,
	KBD_SPEED_FAST = 2,
};

/* Return true if this machine has the ASUS RGB keyboard interface */
bool kbd_is_supported(void);

/* Read the current brightness level */
int kbd_get_brightness(int *level);

/* Read the highest brightness level the driver accepts which is 3 on TUF laptops */
int kbd_get_max_brightness(int *level);

/* Set the brightness level where 0 turns the light off */
int kbd_set_brightness(int level);

/*
 * Set the color and effect
 * The firmware saves the setting so it stays after a reboot
 * Returns -EINVAL if mode or speed is out of range
 */
int kbd_set_color(uint8_t red, uint8_t green, uint8_t blue,
		  enum kbd_mode mode, enum kbd_speed speed);

#ifdef __cplusplus
}
#endif

#endif /* KBD_H */
