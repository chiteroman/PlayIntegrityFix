#include <stdlib.h> /* getenv */
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <fstream>

#include <set>

#include <unordered_map>

#include <sys/types.h>
#include <sys/socket.h>

std::unordered_map<int, const char *> posix_socket_file_descriptors;

int (*orig_bind)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int fake_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
}

static const char *get_traced_socket(int fd, bool removed) {
  std::unordered_map<int, const char *>::iterator it;
  it = posix_socket_file_descriptors.find(fd);
  if (it != posix_socket_file_descriptors.end()) {
    if (removed)
      posix_socket_file_descriptors.erase(it);
    return it->second;
  }
  return NULL;
}

int (*orig_connect)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int fake_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
  const char *traced_socket = get_traced_socket(sockfd, false);
  if (traced_socket) {
    LOG(1, "[-] connect: %s\n", traced_socket);
  }
  return orig_connect(sockfd, addr, addrlen);
}

ssize_t (*orig_send)(int sockfd, const void *buf, size_t len, int flags);
ssize_t fake_send(int sockfd, const void *buf, size_t len, int flags) {
  const char *traced_socket = get_traced_socket(sockfd, false);
  if (traced_socket) {
    LOG(1, "[-] send: %s, buf: %p, len: %zu\n", traced_socket, buf, len);
  }
  return orig_send(sockfd, buf, len, flags);
}

ssize_t (*orig_recv)(int sockfd, void *buf, size_t len, int flags);
ssize_t fake_recv(int sockfd, void *buf, size_t len, int flags) {
  const char *traced_socket = get_traced_socket(sockfd, false);
  if (traced_socket) {
    LOG(1, "[-] recv: %s, buf: %p, len: %zu\n", traced_socket, buf, len);
  }
  return orig_recv(sockfd, buf, len, flags);
}