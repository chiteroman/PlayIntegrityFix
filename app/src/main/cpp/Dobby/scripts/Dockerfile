FROM ubuntu:focal

ARG DEBIAN_FRONTEND='noninteractive'

RUN apt-key adv --keyserver 'keyserver.ubuntu.com' --recv-key 'C99B11DEB97541F0' &&
  apt-add-repository -y -u 'https://cli.github.com/packages' &&
  apt-add-repository 'deb https://apt.kitware.com/ubuntu/ focal main'

ADD setup_linux_cross_compile.sh /root/setup_linux_cross_compile.sh
RUN sh /root/setup_linux_cross_compile.sh
