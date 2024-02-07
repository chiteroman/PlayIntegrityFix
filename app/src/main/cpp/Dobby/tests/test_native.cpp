#include "dobby.h"

#include <unistd.h>
#include <stdio.h>

#define LOG(fmt, ...) printf("[test_native] " fmt, ##__VA_ARGS__)

void test_execve() {
  char *argv[] = {NULL};
  char *envp[] = {NULL};
  
  LOG("test execve");
  
  DobbyInstrument(DobbySymbolResolver(0, "_execve"), [](void *, DobbyRegisterContext *ctx) {
    LOG("execve: %s", (char *)ctx->general.regs.rdi);
    return;
  });

  execve("ls", argv, envp);

  return;
}

int main(int argc, char *argv[]) {
    log_set_level(0);
    
  test_execve();
    
  return 0;
}
