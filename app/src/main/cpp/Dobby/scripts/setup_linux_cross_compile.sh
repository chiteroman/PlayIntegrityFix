#!/bin/sh

set -x
set -e

# sudo dpkg --add-architecture armhf
# sudo dpkg --add-architecture i386
# sudo dpkg --add-architecture arm64
# sudo apt-get -y update
# sudo apt-get -y dist-upgrade
# sudo apt-get -y install git build-essential libssl-dev pkg-config unzip gcc-multilib
# sudo apt-get -y install libc6-armhf-cross libc6-dev-armhf-cross gcc-arm-linux-gnueabihf libssl-dev:armhf
# sudo apt-get -y install libc6-i386-cross libc6-dev-i386-cross gcc-i686-linux-gnu libssl-dev:i386
# sudo apt-get -y install libc6-arm64-cross libc6-dev-arm64-cross gcc-aarch64-linux-gnu libssl-dev:arm64

sudo apt-get -y update
sudo apt-get -y install aptitude
sudo apt-get -f -y install \
  apt-utils \
  binutils \
  build-essential \
  curl \
  wget \
  unzip \
  gcc-multilib \
  g++-multilib \
  make \
  zsh

sudo apt-get -f -y install gcc g++ libc6-dev
sudo apt-get -f -y install gcc-i686-linux-gnu g++-i686-linux-gnu
sudo apt-get -f -y install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
sudo apt-get -f -y install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

mkdir -p ~/opt

cd ~/opt
CMAKE_VERSION=3.25.2
CMAKE_DOWNLOAD_PACKAGE=cmake-$CMAKE_VERSION-linux-x86_64
wget https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/$CMAKE_DOWNLOAD_PACKAGE.tar.gz &&
  tar -zxf $CMAKE_DOWNLOAD_PACKAGE.tar.gz >/dev/null &&
  mv $CMAKE_DOWNLOAD_PACKAGE cmake-$CMAKE_VERSION
CMAKE_HOME=~/opt/cmake-$CMAKE_VERSION

cd ~/opt
LLVM_VERSION=15.0.6
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
