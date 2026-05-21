#pragma once

#include <jni.h>

namespace menu {

bool RegisterGlesJniNatives(JNIEnv *env, jobject classLoader);

}  // namespace menu
