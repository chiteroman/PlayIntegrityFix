#pragma once

#include <stdio.h>
#include <objc/runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

IMP DobbyObjcReplace(Class _class, SEL _selector, IMP replacement);

void DobbyObjcReplaceEx(const char *class_name, const char *selector_name, void *fake_impl, void **orig_impl);

void *DobbyObjcResolveMethodImp(const char *class_name, const char *selector_name);

#ifdef __cplusplus
}
#endif
