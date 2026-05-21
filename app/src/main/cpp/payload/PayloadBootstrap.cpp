#include "PayloadBootstrap.h"

#include <jni.h>
#include <pthread.h>

#include "common/Log.h"
#include "dex_payload.h"
#include "GlesJniBridge.h"
#include "JniClassLoader.h"
#include "JavaVmResolver.h"

namespace menu {
namespace {

constexpr const char *kLoaderClassName = "com.sahilm9098.arkmodmenu.Loader";

class ScopedThreadAttachment {
 public:
  explicit ScopedThreadAttachment(JavaVM *vm) : vm_(vm) {
    if (vm_ == nullptr) {
      return;
    }

    if (vm_->GetEnv(reinterpret_cast<void **>(&env_), JNI_VERSION_1_6) == JNI_OK) {
      return;
    }

    if (vm_->AttachCurrentThread(&env_, nullptr) == JNI_OK) {
      attached_ = true;
    } else {
      env_ = nullptr;
    }
  }

  ~ScopedThreadAttachment() {
    if (attached_ && vm_ != nullptr) {
      vm_->DetachCurrentThread();
    }
  }

  JNIEnv *env() const { return env_; }

 private:
  JavaVM *vm_ = nullptr;
  JNIEnv *env_ = nullptr;
  bool attached_ = false;
};

jobject CreatePayloadClassLoader(JNIEnv *env) {
  jobject byteBuffer = env->NewDirectByteBuffer(
      const_cast<unsigned char *>(payload_dex), static_cast<jlong>(payload_dex_size));
  if (byteBuffer == nullptr) {
    return nullptr;
  }

  jclass classLoaderClass = env->FindClass("java/lang/ClassLoader");
  if (classLoaderClass == nullptr) {
    env->DeleteLocalRef(byteBuffer);
    return nullptr;
  }

  jmethodID getSystemClassLoader = env->GetStaticMethodID(
      classLoaderClass, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
  if (getSystemClassLoader == nullptr) {
    env->DeleteLocalRef(byteBuffer);
    env->DeleteLocalRef(classLoaderClass);
    return nullptr;
  }

  jobject systemClassLoader =
      env->CallStaticObjectMethod(classLoaderClass, getSystemClassLoader);
  if (systemClassLoader == nullptr || env->ExceptionCheck()) {
    env->DeleteLocalRef(byteBuffer);
    env->DeleteLocalRef(classLoaderClass);
    return nullptr;
  }

  jclass inMemoryDexClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
  if (inMemoryDexClass == nullptr) {
    env->DeleteLocalRef(systemClassLoader);
    env->DeleteLocalRef(byteBuffer);
    env->DeleteLocalRef(classLoaderClass);
    return nullptr;
  }

  jmethodID constructor = env->GetMethodID(
      inMemoryDexClass, "<init>", "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
  if (constructor == nullptr) {
    env->DeleteLocalRef(inMemoryDexClass);
    env->DeleteLocalRef(systemClassLoader);
    env->DeleteLocalRef(byteBuffer);
    env->DeleteLocalRef(classLoaderClass);
    return nullptr;
  }

  jobject dexClassLoader =
      env->NewObject(inMemoryDexClass, constructor, byteBuffer, systemClassLoader);

  env->DeleteLocalRef(inMemoryDexClass);
  env->DeleteLocalRef(systemClassLoader);
  env->DeleteLocalRef(byteBuffer);
  env->DeleteLocalRef(classLoaderClass);
  return dexClassLoader;
}

bool InvokeLoaderMain(JNIEnv *env, jobject classLoader) {
  jclass loaderClass = LoadClass(env, classLoader, kLoaderClassName);
  if (loaderClass == nullptr) {
    LogError("Failed to resolve Loader class");
    ClearPendingException(env);
    return false;
  }

  jmethodID mainMethod = env->GetStaticMethodID(loaderClass, "main", "()V");
  if (mainMethod == nullptr) {
    LogError("Failed to resolve Loader.main");
    ClearPendingException(env);
    env->DeleteLocalRef(loaderClass);
    return false;
  }

  env->CallStaticVoidMethod(loaderClass, mainMethod);
  if (env->ExceptionCheck()) {
    DescribeAndClearPendingException(env);
    env->DeleteLocalRef(loaderClass);
    return false;
  }

  env->DeleteLocalRef(loaderClass);
  return true;
}

void *InjectThread(void *) {
  JavaVM *vm = ResolveJavaVm();
  if (vm == nullptr) {
    LogError("Failed to resolve JavaVM");
    return nullptr;
  }

  ScopedThreadAttachment attachment(vm);
  JNIEnv *env = attachment.env();
  if (env == nullptr) {
    LogError("Failed to attach JNI thread");
    return nullptr;
  }

  jobject dexClassLoader = CreatePayloadClassLoader(env);
  if (dexClassLoader == nullptr || env->ExceptionCheck()) {
    LogError("Failed to create payload class loader");
    ClearPendingException(env);
    return nullptr;
  }

  if (!RegisterGlesJniNatives(env, dexClassLoader)) {
    LogError("Failed to register GLES3JNIView natives");
    ClearPendingException(env);
    env->DeleteLocalRef(dexClassLoader);
    return nullptr;
  }

  InvokeLoaderMain(env, dexClassLoader);
  env->DeleteLocalRef(dexClassLoader);
  return nullptr;
}

}  // namespace

void StartPayloadThread() {
  pthread_t thread;
  if (pthread_create(&thread, nullptr, InjectThread, nullptr) == 0) {
    pthread_detach(thread);
    return;
  }

  LogError("Failed to create payload thread");
}

}  // namespace menu
