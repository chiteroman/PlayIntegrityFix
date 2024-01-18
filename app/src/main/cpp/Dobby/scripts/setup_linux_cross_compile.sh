#!/bin/sh

# if error, exit
set -

sudo apt update
sudo apt-get install -y \
  apt-utils \
  build-essential \
  curl \
  wget \
  unzip \
  gcc-multilib \
  make \
  zsh

mkdir -p ~/opt

cd ~/opt
CMAKE_VERSION=3.20.2
CMAKE_DOWNLOAD_PACKAGE=cmake-$CMAKE_VERSION-linux-x86_64
wget https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/$CMAKE_DOWNLOAD_PACKAGE.tar.gz &&
  tar -zxf $CMAKE_DOWNLOAD_PACKAGE.tar.gz >/dev/null &&
  mv $CMAKE_DOWNLOAD_PACKAGE cmake-$CMAKE_VERSION
CMAKE_HOME=~/opt/cmake-$CMAKE_VERSION

cd ~/opt
LLVM_VERSION=14.0.0
LLVM_DOWNLOAD_PACKAGE=clang+llvm-$LLVM_VERSION-x86_64-linux-gnu-ubuntu-18.04
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-$LLVM_VERSION/$LLVM_DOWNLOAD_PACKAGE.tar.xz &&
  tar -xf $LLVM_DOWNLOAD_PACKAGE.tar.xz >/dev/null &&
  mv $LLVM_DOWNLOAD_PACKAGE llvm-$LLVM_VERSION
LLVM_HOME=~/opt/llvm-$LLVM_VERSION

cd ~/opt
NDK_VERSION=r25b
NDK_DOWNLOAD_PACKAGE=android-ndk-$NDK_VERSION-linux
NDK_DOWNLOAD_UNZIP_PACKAGE=android-ndk-$NDK_VERSION
wget https://dl.google.com/android/repository/$NDK_DOWNLOAD_PACKAGE.zip &&
  unzip -q $NDK_DOWNLOAD_PACKAGE.zip >/dev/null &&
  mv $NDK_DOWNLOAD_UNZIP_PACKAGE ndk-$NDK_VERSION &&
  rm $NDK_DOWNLOAD_PACKAGE.zip
ANDROID_NDK_HOME=~/opt/android-ndk-$NDK_VERSION
