#pragma once

#include <jni.h>
#include <string>

namespace menu {

std::string JStringToUtf8(JNIEnv *env, jstring value);
jstring Utf8ToJString(JNIEnv *env, const std::string &value);

}  // namespace menu
