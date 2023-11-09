# Build

## CMake build options

```
option(DOBBY_GENERATE_SHARED "Build shared library" ON)

option(DOBBY_DEBUG "Enable debug logging" OFF)

option(NearBranch "Enable near branch trampoline" ON)

option(FullFloatingPointRegisterPack "Save and pack all floating-point registers" OFF)

option(Plugin.SymbolResolver "Enable symbol resolver" ON)

option(Plugin.ImportTableReplace "Enable import table replace " OFF)

option(Plugin.Android.BionicLinkerUtil "Enable android bionic linker util" OFF)

option(DOBBY_BUILD_EXAMPLE "Build example" OFF)

option(DOBBY_BUILD_TEST "Build test" OFF)
```

## Build with `scripts/platform_builder.py`

#### Build for iphoneos

```shell
python3 scripts/platform_builder.py --platform=iphoneos --arch=all
```

#### Build for macos

```
python3 scripts/platform_builder.py --platform=macos --arch=all
```

#### Build for linux

```
# prepare and download cmake/llvm
sh scripts/setup_linux_cross_compile.sh
python3 scripts/platform_builder.py --platform=linux --arch=all --cmake_dir=$HOME/opt/cmake-3.25.2 --llvm_dir=$HOME/opt/llvm-15.0.6
```

#### Build for android

```
# prepare and download cmake/llvm/ndk
sh scripts/setup_linux_cross_compile.sh
python3 scripts/platform_builder.py --platform=android --arch=all --cmake_dir=$HOME/opt/cmake-3.25.2 --llvm_dir=$HOME/opt/llvm-15.0.6 --android_ndk_dir=$HOME/opt/ndk-r25b
```

## Build with CMake

#### Build for host

```shell
cd Dobby && mkdir cmake-build-host && cd cmake-build-host
cmake ..
make -j4
```

## Build with Android  Studio CMake

```
if(NOT TARGET dobby)
set(DOBBY_DIR /Users/jmpews/Workspace/Project.wrk/Dobby)
macro(SET_OPTION option value)
  set(${option} ${value} CACHE INTERNAL "" FORCE)
endmacro()
SET_OPTION(DOBBY_DEBUG OFF)
SET_OPTION(DOBBY_GENERATE_SHARED OFF)
add_subdirectory(${DOBBY_DIR} dobby)
get_property(DOBBY_INCLUDE_DIRECTORIES
  TARGET dobby
  PROPERTY INCLUDE_DIRECTORIES)
include_directories(
  .
  ${DOBBY_INCLUDE_DIRECTORIES}
  $<TARGET_PROPERTY:dobby,INCLUDE_DIRECTORIES>
)
endif()

add_library(native-lib SHARED
  ${DOBBY_DIR}/examples/socket_example.cc
  native-lib.cpp)
```
