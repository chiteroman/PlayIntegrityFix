#!/usr/bin/env python3
#===----------------------------------------------------------------------===##
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
#===----------------------------------------------------------------------===##

"""adb_run.py is a utility for running a libc++ test program via adb.
"""

import argparse
import os
import re
import shlex
import subprocess
import sys


# Sync a host file /path/to/file to ${REMOTE_BASE_DIR}/path/to/file.
REMOTE_BASE_DIR = "/data/local/tmp/adb_run"


def sync_test_dir(local_dir: str, remote_dir: str) -> None:
    """Sync the libc++ test directory on the host to the remote device."""

    # Optimization: The typical libc++ test directory has only a single
    # *.tmp.exe file in it. In that case, skip the `mkdir` command, which is
    # normally necessary because we don't know if the target directory already
    # exists on the device.
    local_files = os.listdir(local_dir)
    if len(local_files) == 1:
        local_file = os.path.join(local_dir, local_files[0])
        remote_file = os.path.join(remote_dir, local_files[0])
        if not os.path.islink(local_file) and os.path.isfile(local_file):
            subprocess.check_call(["adb", "push", "--sync", local_file,
                                   remote_file], stdout=subprocess.DEVNULL)
            return

    subprocess.check_call(["adb", "shell", "mkdir", "-p", remote_dir])
    subprocess.check_call(
        ["adb", "push", "--sync", local_dir, os.path.dirname(remote_dir)],
        stdout=subprocess.DEVNULL)


def build_env_arg(env_args: list[str], prepend_path_args: list[str]) -> str:
    components = []
    for arg in env_args:
        k, v = arg.split("=", 1)
        components.append(f"export {k}={shlex.quote(v)}; ")
    for k, v in prepend_path_args:
        components.append(f"export {k}={shlex.quote(v)}${{{k}:+:${k}}}; ")
    return "".join(components)


def main() -> int:
    """Main function (pylint wants this docstring)."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--execdir", type=str, required=True)
    parser.add_argument("--env", type=str, required=False, action="append",
                        default=[])
    parser.add_argument("--prepend-path-env", type=str, nargs=2, required=False,
                        action="append", default=[])
    parser.add_argument("--verbose", "-v", default=False, action="store_true")
    parser.add_argument("command", nargs=argparse.ONE_OR_MORE)
    args = parser.parse_args()

    local_dir = args.execdir
    assert local_dir.startswith("/")
    assert not local_dir.endswith("/")
    remote_dir = REMOTE_BASE_DIR + local_dir
    sync_test_dir(local_dir, remote_dir)

    adb_shell_command = (
        # Set the environment early so that PATH can be overridden. Overriding
        # PATH is useful for:
        #  - Replacing older shell utilities with toybox (e.g. on old devices).
        #  - Adding a `bash` command that delegates to `sh` (mksh).
        f"{build_env_arg(args.env, args.prepend_path_env)}"

        # If we're running as root, switch to the shell user. The libc++
        # filesystem tests require running without root permissions. Some x86
        # emulator devices (before Android N) do not have a working `adb unroot`
        # and always run as root. Non-debug builds typically lack `su` and only
        # run as the shell user.
        #
        # Some libc++ tests create temporary files in the working directory,
        # which might be owned by root. Before switching to shell, make the
        # cwd writable (and readable+executable) to every user.
        #
        # N.B.:
        #  - Avoid "id -u" because it wasn't supported until Android M.
        #  - The `env` and `which` commands were also added in Android M.
        #  - Starting in Android M, su from root->shell resets PATH, so we need
        #    to modify it again in the new environment.
        f"run_without_root() {{ "
        f"  chmod a+rwx .; "
        f"  case \"$(id)\" in "
        f"    *\"uid=0(root)\"*) "
        f"    if command -v env >/dev/null; then "
        f"      su shell $(command -v env) PATH=\"$PATH\" \"$@\"; "
        f"    else "
        f"      su shell \"$@\"; "
        f"    fi;; "
        f"    *) \"$@\";; "
        f"  esac; "
        f"}}; "
    )

    # Older versions of Bionic limit the length of argv[0] to 127 bytes
    # (SOINFO_NAME_LEN-1), and the path to libc++ tests tend to exceed this
    # limit. Changing the working directory works around this limit. The limit
    # is increased to 4095 (PATH_MAX-1) in Android M (API 23).
    command_line = [arg.replace(local_dir + "/", "./") for arg in args.command]

    # Prior to the adb feature "shell_v2" (added in Android N), `adb shell`
    # always created a pty:
    #  - This merged stdout and stderr together.
    #  - The pty converts LF to CRLF.
    #  - The exit code of the shell command wasn't propagated.
    # Work around all three limitations, unless "shell_v2" is present.
    proc = subprocess.run(["adb", "features"], check=True,
                          stdout=subprocess.PIPE, encoding="utf-8")
    adb_features = set(proc.stdout.strip().split())
    has_shell_v2 = "shell_v2" in adb_features
    if has_shell_v2:
        adb_shell_command += (
            f"cd {remote_dir} && run_without_root {shlex.join(command_line)}"
        )
    else:
        adb_shell_command += (
            f"{{"
            f"  stdout=$("
            f"    cd {remote_dir} && run_without_root {shlex.join(command_line)}; "
            f"    echo -n __libcxx_adb_exit__=$?"
            f"  ); "
            f"}} 2>&1; "
            f"echo -n __libcxx_adb_stdout__\"$stdout\""
        )

    adb_command_line = ["adb", "shell", adb_shell_command]
    if args.verbose:
        sys.stderr.write("running: " + shlex.join(adb_command_line) + "\n")

    if has_shell_v2:
        proc = subprocess.run(adb_command_line, shell=False, check=False,
                              encoding="utf-8")
        return proc.returncode
    else:
        proc = subprocess.run(adb_command_line, shell=False, check=False,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              encoding="utf-8")
        # The old `adb shell` mode used a pty, which converted LF to CRLF.
        # Convert it back.
        output = proc.stdout.replace("\r\n", "\n")

        if proc.returncode:
            sys.stderr.write(f"error: adb failed:\n"
                             f"  command: {shlex.join(adb_command_line)}\n"
                             f"  output: {output}\n")
            return proc.returncode

        match = re.match(r"(.*)__libcxx_adb_stdout__(.*)__libcxx_adb_exit__=(\d+)$",
                     output, re.DOTALL)
        if not match:
            sys.stderr.write(f"error: could not parse adb output:\n"
                             f"  command: {shlex.join(adb_command_line)}\n"
                             f"  output: {output}\n")
            return 1

        sys.stderr.write(match.group(1))
        sys.stdout.write(match.group(2))
        return int(match.group(3))


if __name__ == '__main__':
    sys.exit(main())
