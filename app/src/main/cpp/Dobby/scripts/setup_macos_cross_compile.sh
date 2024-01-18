#!/bin/sh

# if error, exit
set -

mkdir -p ~/opt

cd ~/opt
CMAKE_VERSION=3.20.2
CMAKE_DOWNLOAD_PACKAGE=cmake-$CMAKE_VERSION-macos-universal
wget https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/$CMAKE_DOWNLOAD_PACKAGE.tar.gz &&
  tar -zxf $CMAKE_DOWNLOAD_PACKAGE.tar.gz >/dev/null &&
  mv $CMAKE_DOWNLOAD_PACKAGE cmake-$CMAKE_VERSION
CMAKE_HOME=~/opt/cmake-$CMAKE_VERSION

cd ~/opt
LLVM_VERSION=14.0.0
LLVM_DOWNLOAD_PACKAGE=clang+llvm-$LLVM_VERSION-x86_64-apple-darwin
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/$LLVM_DOWNLOAD_PACKAGE.tar.xz &&
  tar -xf $LLVM_DOWNLOAD_PACKAGE.tar.xz >/dev/null &&
  mv $LLVM_DOWNLOAD_PACKAGE llvm-$LLVM_VERSION
LLVM_HOME=~/opt/llvm-$LLVM_VERSION
