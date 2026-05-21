#include "JniClassLoader.h"

namespace menu {

jclass LoadClass(JNIEnv *env, jobject classLoader, const char *className) {
  if (env == nullptr || classLoader == nullptr || className == nullptr) {
    return nullptr;
  }

  jclass classLoaderClass = env->FindClass("java/lang/ClassLoader");
  if (classLoaderClass == nullptr) {
    return nullptr;
  }

  jmethodID loadClass = env->GetMethodID(
      classLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
  if (loadClass == nullptr) {
    env->DeleteLocalRef(classLoaderClass);
    return nullptr;
  }

  jstring name = env->NewStringUTF(className);
  if (name == nullptr) {
    env->DeleteLocalRef(classLoaderClass);
    return nullptr;
  }

  auto loadedClass =
      static_cast<jclass>(env->CallObjectMethod(classLoader, loadClass, name));
  env->DeleteLocalRef(name);
  env->DeleteLocalRef(classLoaderClass);
  return loadedClass;
}

void ClearPendingException(JNIEnv *env) {
  if (env != nullptr && env->ExceptionCheck()) {
    env->ExceptionClear();
  }
}

void DescribeAndClearPendingException(JNIEnv *env) {
  if (env != nullptr && env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
  }
}

}  // namespace menu
