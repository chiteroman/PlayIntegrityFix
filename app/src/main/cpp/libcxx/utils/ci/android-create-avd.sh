#!/usr/bin/env bash
#===----------------------------------------------------------------------===##
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
#===----------------------------------------------------------------------===##

set -e

# Disable the permissions on this dev node.
sudo chmod a+rwx /dev/kvm

$ANDROID_HOME/cmdline-tools/latest/bin/avdmanager \
    --verbose create avd \
    --name libcxx_avd \
    --package "system-images;android-33;google_apis;x86_64" \
    --sdcard 4000M --device pixel_5

$ANDROID_HOME/emulator/emulator @libcxx_avd -no-window &

$ANDROID_HOME/platform-tools/adb wait-for-device
