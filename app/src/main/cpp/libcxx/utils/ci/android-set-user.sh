#!/usr/bin/env bash
#===----------------------------------------------------------------------===##
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
#===----------------------------------------------------------------------===##

set -e

uname=$1
uid=$2
gname=$3
gid=$4

# Create a user/group pair matching the outer uid/gid. This can fail if the
# username/uid/groupname/gid already exist. In that case, we'll use the
# existing uid/gid, which is usually OK.
sudo groupadd --gid ${gid} ${gname} || true
sudo useradd --create-home ${uname} --no-user-group -u ${uid} -g ${gid} || true

# sudo will reset the PATH, so add interesting things back in from the docker
# image.
extra_path=\
${ANDROID_HOME}/cmdline-tools/latest/bin:\
${ANDROID_HOME}/platform-tools

shift 4
exec sudo -u "#${uid}" sh -c 'cd; PATH='${extra_path}':${PATH} "$0" "$@"' "$@"
