#include "JavaVmResolver.h"

#include <dlfcn.h>

namespace menu {
namespace {

using JNI_GetCreatedJavaVMs_t = jint (*)(JavaVM **, jsize, jsize *);
using AndroidRuntimeGetJavaVM_t = JavaVM *(*)();

JavaVM *TryResolveVmFromHandle(void *handle) {
  if (handle == nullptr) {
    return nullptr;
  }

  auto getVms =
      reinterpret_cast<JNI_GetCreatedJavaVMs_t>(dlsym(handle, "JNI_GetCreatedJavaVMs"));
  if (getVms != nullptr) {
    JavaVM *vm = nullptr;
    jsize count = 0;
    if (getVms(&vm, 1, &count) == JNI_OK && count > 0 && vm != nullptr) {
      return vm;
    }
  }

  auto getVm = reinterpret_cast<AndroidRuntimeGetJavaVM_t>(
      dlsym(handle, "AndroidRuntimeGetJavaVM"));
  if (getVm != nullptr) {
    return getVm();
  }

  return nullptr;
}

}  // namespace

JavaVM *ResolveJavaVm() {
  if (JavaVM *vm = TryResolveVmFromHandle(RTLD_DEFAULT)) {
    return vm;
  }

  const char *libraries[] = {
      "libandroid_runtime.so",
      "/system/lib64/libandroid_runtime.so",
      "libnativehelper.so",
      "/apex/com.android.art/lib64/libnativehelper.so",
      "libart.so",
      "/apex/com.android.art/lib64/libart.so",
  };

  for (const char *library : libraries) {
    void *handle = dlopen(library, RTLD_NOW | RTLD_GLOBAL);
    if (JavaVM *vm = TryResolveVmFromHandle(handle)) {
      return vm;
    }
  }

  return nullptr;
}

}  // namespace menu
