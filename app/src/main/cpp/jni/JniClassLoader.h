#pragma once

#include <jni.h>

namespace menu {

jclass LoadClass(JNIEnv *env, jobject classLoader, const char *className);
void ClearPendingException(JNIEnv *env);
void DescribeAndClearPendingException(JNIEnv *env);

}  // namespace menu
