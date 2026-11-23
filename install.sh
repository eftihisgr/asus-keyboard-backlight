#!/bin/sh
# Author:      Eftychios Papasifakis
# File:        install.sh
# Project:     ASUS Keyboard Backlight (asus-keyboard-backlight)
# SPDX-License-Identifier: MIT
#
# Description:
#   Builds and installs the CLI and the GUI and the desktop entry and the
#   udev rule then reloads udev so it works without a reboot
#
#   Run from the project folder: ./install.sh

# Stop at the first command that fails
set -e

# Keep the window open until Enter is pressed on success or failure
# so the output can be read
trap 'printf "\nPress Enter to close..."; read -r _' EXIT

# Configure and compile into the build folder
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Copy the binaries and the desktop entry and the udev rule into the system
sudo cmake --install build

# Load the new udev rule and run it now for the keyboard LED device
# so the permissions change without a reboot
sudo udevadm control --reload-rules
sudo udevadm trigger --action=add --subsystem-match=leds --sysname-match='asus::kbd_backlight'

echo "Installed. Run 'asus-kbd-backlight-gui' or 'asus-kbd-backlight status'."
