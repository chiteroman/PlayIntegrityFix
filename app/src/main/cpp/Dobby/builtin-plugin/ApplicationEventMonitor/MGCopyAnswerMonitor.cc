#include "./dobby_monitor.h"

#include <dlfcn.h>
#include <CoreFoundation/CoreFoundation.h>

#define LOG_TAG "MGCopyAnswer"

static uintptr_t getCallFirstArg(DobbyRegisterContext *ctx) {
  uintptr_t result;
#if defined(_M_X64) || defined(__x86_64__)
#if defined(_WIN32)
  result = ctx->general.regs.rcx;
#else
  result = ctx->general.regs.rdi;
#endif
#elif defined(__arm64__) || defined(__aarch64__)
  result = ctx->general.regs.x0;
#elif defined(__arm__)
  result = ctx->general.regs.r0;
#else
#error "Not Support Architecture."
#endif
  return result;
}

void common_handler(DobbyRegisterContext *ctx, const InterceptEntry *info) {
  CFStringRef key_ = 0;
  key_ = (CFStringRef)getCallFirstArg(ctx);

  char str_key[256] = {0};
  CFStringGetCString(key_, str_key, 256, kCFStringEncodingUTF8);
  LOG("[#] MGCopyAnswer:: %s\n", str_key);
}

#if 0
__attribute__((constructor)) static void ctor() {
  void *lib               = dlopen("/usr/lib/libMobileGestalt.dylib", RTLD_NOW);
  void *MGCopyAnswer_addr = DobbySymbolResolver("libMobileGestalt.dylib", "MGCopyAnswer");
  
  sleep(1);

  dobby_enable_near_branch_trampoline();
  DobbyInstrument((void *)MGCopyAnswer_addr, common_handler);
  dobby_disable_near_branch_trampoline();
}
#endif
