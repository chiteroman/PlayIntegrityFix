import os
import pipes
import re
import shutil
import subprocess
import sys
import logging

import argparse

platforms = {
  "macos": ["x86_64", "arm64", "arm64e"],
  "iphoneos": ["arm64", "arm64e"],
  "linux": ["x86", "x86_64", "arm", "arm64"],
  "android": ["x86", "x86_64", "armeabi-v7a", "arm64-v8a"]
}


class PlatformBuilder(object):
  cmake_args = list()
  cmake_build_type = "Release"
  cmake_build_verbose = False
  cmake_build_dir = ""

  library_build_type = "static"

  project_dir = ""
  output_dir = ""

  shared_output_name = ""
  static_output_name = ""

  platform = ""
  arch = ""

  def __init__(self, project_dir, library_build_type, platform, arch):
    self.project_dir = project_dir
    self.library_build_type = library_build_type
    self.platform = platform
    self.arch = arch

    self.cmake_build_dir = f"{self.project_dir}/build/cmake-build-{platform}-{arch}"
    self.output_dir = f"{self.project_dir}/build/{platform}/{arch}"

    self.cmake = "cmake" if PlatformBuilder.cmake_dir is None else f"{PlatformBuilder.cmake_dir}/bin/cmake"
    self.clang = "clang" if PlatformBuilder.llvm_dir is None else f"{PlatformBuilder.llvm_dir}/bin/clang"
    self.clangxx = "clang++" if PlatformBuilder.llvm_dir is None else f"{PlatformBuilder.llvm_dir}/bin/clang++"

    self.setup_common_args()

  def cmake_generate_build_system(self):
    cmake_cmd_options = ["-S {}".format(self.project_dir), "-B {}".format(self.cmake_build_dir)]
    cmd = [f"{self.cmake}"] + cmake_cmd_options + self.cmake_args
    # subprocess.run(cmd, check=True)
    cmd_line = " ".join(cmd)
    print(cmd_line)
    os.system(cmd_line)

  def setup_common_args(self):
    self.cmake_args += [f"-DCMAKE_C_COMPILER={self.clang}", f"-DCMAKE_CXX_COMPILER={self.clangxx}"]

    self.cmake_args += ["-DCMAKE_BUILD_TYPE={}".format(self.cmake_build_type)]

    if self.library_build_type == "shared":
      pass
      # self.cmake_args += ["-DDOBBY_GENERATE_SHARED=ON"]
    elif self.library_build_type == "static":
      pass
      # self.cmake_args += ["-DDOBBY_GENERATE_SHARED=OFF"]

  def build(self):
    subprocess.run(["mkdir", "-p", "{}".format(self.output_dir)], check=True)
    self.cmake_generate_build_system()

    subprocess.run("cmake --build . --clean-first --target dobby --target dobby_static -- -j8", cwd=self.cmake_build_dir, shell=True, check=True)

    os.system(f"mkdir -p {self.output_dir}")
    os.system(f"cp {self.cmake_build_dir}/{self.shared_output_name} {self.output_dir}")
    os.system(f"cp {self.cmake_build_dir}/{self.static_output_name} {self.output_dir}")


class WindowsPlatformBuilder(PlatformBuilder):

  def __init__(self, project_dir, library_build_type, platform, arch):
    super().__init__(project_dir, library_build_type, platform, arch)

    if self.library_build_type == "shared":
      self.output_name = "libdobby.dll"
    else:
      self.output_name = "libdobby.lib"

    triples = {
      "x86": "i686-pc-windows-msvc",
      "x64": "x86_64-pc-windows-msvc",
      # "arm": "arm-pc-windows-msvc",
      "arm64": "arm64-pc-windows-msvc",
    }

    # self.cmake_args += ["--target {}".format(triples[arch])]
    self.cmake_args += [
      "-DCMAKE_SYSTEM_PROCESSOR={}".format(arch),
    ]


class LinuxPlatformBuilder(PlatformBuilder):

  def __init__(self, project_dir, library_build_type, arch):
    super().__init__(project_dir, library_build_type, "linux", arch)

    self.shared_output_name = "libdobby.so"
    self.static_output_name = "libdobby.a"

    targets = {
      "x86": "i686-linux-gnu",
      "x86_64": "x86_64-linux-gnu",
      "arm": "arm-linux-gnueabi",
      "aarch64": "aarch64-linux-gnu",
    }

    # self.cmake_args += ["--target={}".format(targets[arch])]
    self.cmake_args += [
      "-DCMAKE_SYSTEM_NAME=Linux",
      "-DCMAKE_SYSTEM_PROCESSOR={}".format(arch),
    ]


class AndroidPlatformBuilder(PlatformBuilder):

  def __init__(self, android_nkd_dir, project_dir, library_build_type, arch):
    super().__init__(project_dir, library_build_type, "android", arch)

    self.shared_output_name = "libdobby.so"
    self.static_output_name = "libdobby.a"

    android_api_level = 21
    if arch == "armeabi-v7a" or arch == "x86":
      android_api_level = 19

    self.cmake_args += [
      "-DCMAKE_SYSTEM_NAME=Android", f"-DCMAKE_ANDROID_NDK={android_nkd_dir}", f"-DCMAKE_ANDROID_ARCH_ABI={arch}",
      f"-DCMAKE_SYSTEM_VERSION={android_api_level}"
    ]


class DarwinPlatformBuilder(PlatformBuilder):

  def __init__(self, project_dir, library_build_type, platform, arch):
    super().__init__(project_dir, library_build_type, platform, arch)

    self.cmake_args += [
      "-DCMAKE_OSX_ARCHITECTURES={}".format(arch),
      "-DCMAKE_SYSTEM_PROCESSOR={}".format(arch),
    ]

    if platform == "macos":
      self.cmake_args += ["-DCMAKE_SYSTEM_NAME=Darwin"]
    elif platform == "iphoneos":
      self.cmake_args += ["-DCMAKE_SYSTEM_NAME=iOS", "-DCMAKE_OSX_DEPLOYMENT_TARGET=9.3"]

    self.shared_output_name = "libdobby.dylib"
    self.static_output_name = "libdobby.a"

  @classmethod
  def lipo_create_fat(cls, project_dir, platform, output_name):
    files = list()
    archs = platforms[platform]
    for arch in archs:
      file = f"{project_dir}/build/{platform}/{arch}/{output_name}"
      files.append(file)

    fat_output_dir = f"{project_dir}/build/{platform}/universal"
    subprocess.run(["mkdir", "-p", "{}".format(fat_output_dir)], check=True)
    cmd = ["lipo", "-create"] + files + ["-output", f"{fat_output_dir}/{output_name}"]
    subprocess.run(cmd, check=True)


if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument("--platform", type=str, required=True)
  parser.add_argument("--arch", type=str, required=True)
  parser.add_argument("--library_build_type", type=str, default="static")
  parser.add_argument("--android_ndk_dir", type=str)
  parser.add_argument("--cmake_dir", type=str)
  parser.add_argument("--llvm_dir", type=str)
  args = parser.parse_args()

  logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")

  platform = args.platform
  arch = args.arch
  library_build_type = args.library_build_type

  PlatformBuilder.cmake_dir = args.cmake_dir
  PlatformBuilder.llvm_dir = args.llvm_dir

  project_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
  logging.info("project dir: {}".format(project_dir))
  if not os.path.exists(f"{project_dir}/CMakeLists.txt"):
    logging.error("Please run this script in Dobby project root directory")
    sys.exit(1)

  if platform not in platforms:
    logging.error("invalid platform {}".format(platform))
    sys.exit(-1)

  if arch != "all" and arch not in platforms[platform]:
    logging.error("invalid arch {} for platform {}".format(arch, platform))
    sys.exit(-1)

  if platform == "android":
    if args.android_ndk_dir is None:
      logging.error("ndk dir is required for android platform")
      sys.exit(-1)

  archs = list()
  if arch == "all":
    archs = platforms[platform]
  else:
    archs.append(arch)
  logging.info("build platform: {}, archs: {}".format(platform, archs))

  builder: PlatformBuilder = None
  for arch_ in archs:
    if platform == "macos":
      builder = DarwinPlatformBuilder(project_dir, library_build_type, platform, arch_)
    elif platform == "iphoneos":
      builder = DarwinPlatformBuilder(project_dir, library_build_type, platform, arch_)
    elif platform == "android":
      builder = AndroidPlatformBuilder(args.android_ndk_dir, project_dir, library_build_type, arch_)
    elif platform == "linux":
      builder = LinuxPlatformBuilder(project_dir, library_build_type, arch_)
    else:
      logging.error("invalid platform {}".format(platform))
      sys.exit(-1)
    logging.info(
      f"build platform: {platform}, arch: {arch_}, cmake_build_dir: {builder.cmake_build_dir}, output_dir: {builder.output_dir}"
    )
    builder.build()

  if platform in ["iphoneos", "macos"] and arch == "all":
    DarwinPlatformBuilder.lipo_create_fat(project_dir, platform, builder.shared_output_name)
    DarwinPlatformBuilder.lipo_create_fat(project_dir, platform, builder.static_output_name)
