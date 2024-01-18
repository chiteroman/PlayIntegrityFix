#include "dobby.h"

#include "logging/logging.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <iostream>
#include <map>
#include <vector>

std::map<void *, const char *> *func_map;

// clang-format off
const char *func_array[] = {
//   "__loader_dlopen",

  "dlsym",
  "dlclose",

  "open",
  "write",
  "read",
  "close",

  "socket",
  "connect",
  "bind",
  "listen",
  "accept",
  "send",
  "recv",

  // "pthread_create"
};

const char *func_short_array[] = {
  "accept",
};
// clang-format on

#define pac_strip(symbol)
#if defined(__APPLE__) && __arm64e__
#if __has_feature(ptrauth_calls)
#define pac_strip(symbol)
//#define pac_strip(symbol) *(void **)&symbol = (void *)ptrauth_sign_unauthenticated((void *)symbol, ptrauth_key_asia, 0)
#endif
#endif

#define install_hook(name, fn_ret_t, fn_args_t...)                                                                     \
  fn_ret_t (*orig_##name)(fn_args_t);                                                                                  \
  fn_ret_t fake_##name(fn_args_t);                                                                                     \
  /* __attribute__((constructor)) */ static void install_hook_##name() {                                               \
    void *sym_addr = DobbySymbolResolver(NULL, #name);                                                                 \
    DobbyHook(sym_addr, (dobby_dummy_func_t)fake_##name, (dobby_dummy_func_t *)&orig_##name);                          \
    pac_strip(orig_##name);                                                                                            \
    printf("install hook %s:%p:%p\n", #name, sym_addr, orig_##name);                                                   \
  }                                                                                                                    \
  fn_ret_t fake_##name(fn_args_t)

install_hook(pthread_create, int, pthread_t *thread, const pthread_attr_t *attrs, void *(*start_routine)(void *),
             void *arg, unsigned int create_flags) {
  LOG(1, "pthread_create: %p", start_routine);
  return orig_pthread_create(thread, attrs, start_routine, arg, create_flags);
}

void common_handler(void *address, DobbyRegisterContext *ctx) {
  auto iter = func_map->find(address);
  if (iter != func_map->end()) {
    LOG(1, "func %s:%p invoke", iter->second, iter->first);
  }
}

uint64_t socket_demo_server(void *ctx);

uint64_t socket_demo_client(void *ctx);

#if 1

__attribute__((constructor)) static void ctor() {
  void *func = NULL;
  log_set_level(0);

  func_map = new std::map<void *, const char *>();
  for (int i = 0; i < sizeof(func_array) / sizeof(char *); ++i) {
    func = DobbySymbolResolver(NULL, func_array[i]);
    if (func == NULL) {
      LOG(1, "func %s not resolve", func_array[i]);
      continue;
    }
    func_map->insert(std::pair<void *, const char *>(func, func_array[i]));
  }

  for (auto iter = func_map->begin(), e = func_map->end(); iter != e; iter++) {
    bool is_short = false;
    for (int i = 0; i < sizeof(func_short_array) / sizeof(char *); ++i) {
      if (strcmp(func_short_array[i], iter->second) == 0) {
        is_short = true;
        break;
      }
    }
    if (is_short) {
      dobby_enable_near_branch_trampoline();
      DobbyInstrument(iter->first, common_handler);
      dobby_disable_near_branch_trampoline();
    } else {
      DobbyInstrument(iter->first, common_handler);
    }
  }

#if defined(__APPLE__)
  // DobbyImportTableReplace(NULL, "_pthread_create", (void *)fake_pthread_create, (void **)&orig_pthread_create);
#endif

  install_hook_pthread_create();

  pthread_t socket_server;
  pthread_create(&socket_server, NULL, (void *(*)(void *))socket_demo_server, NULL);

  usleep(10000);
  pthread_t socket_client;
  pthread_create(&socket_client, NULL, (void *(*)(void *))socket_demo_client, NULL);

  // pthread_join(socket_client, 0);
  // pthread_join(socket_server, 0);
}

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 49494

uint64_t socket_demo_server(void *ctx) {
  int server_fd, new_socket;
  struct sockaddr_in address;
  int opt = 1;
  int addrlen = sizeof(address);
  char buffer[1024] = {0};
  char *hello = "Hello from server";

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    ERROR_LOG("socket failed: %s", strerror(errno));
    return -1;
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    ERROR_LOG("setsockopt: %s", strerror(errno));
    return -1;
  }

  address.sin_family = AF_INET;
  address.sin_port = htons(PORT);
  address.sin_addr.s_addr = INADDR_ANY;

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    ERROR_LOG("bind failed: %s", strerror(errno));
    return -1;
  }
  if (listen(server_fd, 3) < 0) {
    ERROR_LOG("listen failed: %s", strerror(errno));
    return -1;
  }
  if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
    ERROR_LOG("accept failed: %s", strerror(errno));
    return -1;
  }

  int ret = recv(new_socket, buffer, 1024, 0);
  LOG(1, "[server] %s", buffer);

  send(new_socket, hello, strlen(hello), 0);
  LOG(1, "[server] Hello message sent");
  return 0;
}

uint64_t socket_demo_client(void *ctx) {
  int sock = 0;
  struct sockaddr_in serv_addr;
  char *hello = "Hello from client";
  char buffer[1024] = {0};
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    ERROR_LOG("socket failed");
    return -1;
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);

  // Convert IPv4 and IPv6 addresses from text to binary form
  if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
    ERROR_LOG("inet_pton failed");
    return -1;
  }

  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    ERROR_LOG("connect failed");
    return -1;
  }

  send(sock, hello, strlen(hello), 0);
  LOG(1, "[client] Hello message sent");

  int ret = recv(sock, buffer, 1024, 0);
  LOG(1, "[client] %s", buffer);
  return 0;
}

#endif
