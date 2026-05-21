#include "GlesJniBridge.h"

#include "JniClassLoader.h"
#include "JniString.h"
#include "ImGuiRenderer.h"

namespace menu {
namespace {

constexpr const char *kGlesClassName = "com.sahilm9098.arkmodmenu.GLES3JNIView";

void NativeInitImgui(JNIEnv *env, jclass clazz, jobject surface, jfloat density) {
  (void)clazz;
  ImGuiRenderer::Get().Initialize(env, surface, density);
}

void NativeUpdateSize(JNIEnv *env, jclass clazz, jint width, jint height) {
  (void)env;
  (void)clazz;
  ImGuiRenderer::Get().UpdateSize(width, height);
}

void NativeTick(JNIEnv *env, jclass clazz) {
  (void)env;
  (void)clazz;
  ImGuiRenderer::Get().RenderFrame();
}

void NativeImguiShutdown(JNIEnv *env, jclass clazz) {
  (void)env;
  (void)clazz;
  ImGuiRenderer::Get().Shutdown();
}

void NativeMotionEventClick(
    JNIEnv *env, jclass clazz, jboolean down, jfloat posX, jfloat posY) {
  (void)env;
  (void)clazz;
  ImGuiRenderer::Get().HandleTouch(down == JNI_TRUE, posX, posY);
}

jobjectArray NativeGetWindowRect(JNIEnv *env, jclass clazz) {
  (void)clazz;
  return ImGuiRenderer::Get().GetWindowRects(env);
}

jobjectArray NativeGetKeyboardState(JNIEnv *env, jclass clazz) {
  (void)clazz;
  return ImGuiRenderer::Get().GetKeyboardState(env);
}

void NativeCommitKeyboardText(JNIEnv *env, jclass clazz, jstring text) {
  (void)clazz;
  ImGuiRenderer::Get().CommitKeyboardText(JStringToUtf8(env, text));
}

void NativeCancelKeyboardText(JNIEnv *env, jclass clazz) {
  (void)env;
  (void)clazz;
  ImGuiRenderer::Get().CancelKeyboardText();
}

jboolean NativeIsMenuVisible(JNIEnv *env, jclass clazz) {
  (void)env;
  (void)clazz;
  return ImGuiRenderer::Get().IsMenuVisible() ? JNI_TRUE : JNI_FALSE;
}

JNINativeMethod gGlesMethods[] = {
    {"nativeIsMenuVisible", "()Z", reinterpret_cast<void *>(NativeIsMenuVisible)},
    {"initImgui", "(Landroid/view/Surface;F)V",
     reinterpret_cast<void *>(NativeInitImgui)},
    {"updateSize", "(II)V", reinterpret_cast<void *>(NativeUpdateSize)},
    {"Tick", "()V", reinterpret_cast<void *>(NativeTick)},
    {"imguiShutdown", "()V", reinterpret_cast<void *>(NativeImguiShutdown)},
    {"motionEventClick", "(ZFF)V", reinterpret_cast<void *>(NativeMotionEventClick)},
    {"getWindowRect", "()[Ljava/lang/String;",
     reinterpret_cast<void *>(NativeGetWindowRect)},
    {"getKeyboardState", "()[Ljava/lang/String;",
     reinterpret_cast<void *>(NativeGetKeyboardState)},
    {"commitKeyboardText", "(Ljava/lang/String;)V",
     reinterpret_cast<void *>(NativeCommitKeyboardText)},
    {"cancelKeyboardText", "()V", reinterpret_cast<void *>(NativeCancelKeyboardText)},
};

}  // namespace

bool RegisterGlesJniNatives(JNIEnv *env, jobject classLoader) {
  jclass glesClass = LoadClass(env, classLoader, kGlesClassName);
  if (glesClass == nullptr) {
    return false;
  }

  const jint result = env->RegisterNatives(
      glesClass, gGlesMethods,
      static_cast<jint>(sizeof(gGlesMethods) / sizeof(gGlesMethods[0])));
  env->DeleteLocalRef(glesClass);
  return result == JNI_OK;
}

}  // namespace menu
