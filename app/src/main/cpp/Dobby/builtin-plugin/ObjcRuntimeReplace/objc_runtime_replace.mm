#include "ObjcRuntimeReplace/objc_runtime_repalce.h"
#include "dobby_internal.h"

#include <stdio.h>
#include <objc/runtime.h>

/* clang -rewrite-objc main.m */

IMP DobbyObjcReplace(Class class_, SEL sel_, IMP fake_impl) {
  Method method_ = class_getInstanceMethod(class_, sel_);
  if (!method_)
    method_ = class_getClassMethod(class_, sel_);

  if (!method_) {
    DLOG(0, "Not found class: %s, selector: %s method\n", class_getName(class_), sel_getName(sel_));
    return NULL;
  }

  return method_setImplementation(method_, (IMP)fake_impl);
}

void DobbyObjcReplaceEx(const char *class_name, const char *selector_name, void *fake_impl, void **out_orig_impl) {
  Class class_ = objc_getClass(class_name);
  SEL sel_ = sel_registerName(selector_name);

  Method method_ = class_getInstanceMethod(class_, sel_);
  if (!method_)
    method_ = class_getClassMethod(class_, sel_);

  if (!method_) {
    DLOG(0, "Not found class: %s, selector: %s method\n", class_name, selector_name);
    return;
  }

  void *orig_impl = NULL;
  orig_impl = (void *)method_setImplementation(method_, (IMP)fake_impl);
  if (out_orig_impl) {
    *out_orig_impl = orig_impl;
  }
  return;
}

void *DobbyObjcResolveMethodImp(const char *class_name, const char *selector_name) {
  Class class_ = objc_getClass(class_name);
  SEL sel_ = sel_registerName(selector_name);

  Method method_ = class_getInstanceMethod(class_, sel_);
  if (!method_)
    method_ = class_getClassMethod(class_, sel_);

  if (!method_) {
    DLOG(0, "Not found class: %s, selector: %s method\n", class_name, selector_name);
    return NULL;
  }
  return (void *)method_getImplementation(method_);
}
