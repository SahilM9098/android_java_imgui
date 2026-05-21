#pragma once

#include <android/native_window.h>
#include <cstdint>
#include <jni.h>
#include <pthread.h>
#include <string>

namespace menu {

constexpr int kMaxTouchWindows = 20;

class ImGuiRenderer final {
 public:
  static ImGuiRenderer &Get();

  void Initialize(JNIEnv *env, jobject surface, float density);
  void UpdateSize(int width, int height);
  void RenderFrame();
  void Shutdown();
  void HandleTouch(bool down, float x, float y);
  jobjectArray GetWindowRects(JNIEnv *env);
  jobjectArray GetKeyboardState(JNIEnv *env);
  void CommitKeyboardText(const std::string &text);
  void CancelKeyboardText();
  bool IsMenuVisible() const;

 private:
  struct WindowRectSnapshot {
    bool active;
    int id;
    float x;
    float y;
    float w;
    float h;
  };

  ImGuiRenderer() = default;
  ~ImGuiRenderer() = default;
  ImGuiRenderer(const ImGuiRenderer &) = delete;
  ImGuiRenderer &operator=(const ImGuiRenderer &) = delete;

  bool HasContext() const;
  void ApplyMenuStyle(float density);
  void ApplyTouch(bool down, float x, float y);
  void LoadFonts();
  void UpdateWindowRectCache();
  void ClearWindowRectCache();
  void UpdateKeyboardState();
  void ApplyPendingKeyboardAction();
  void FinishPendingKeyboardAction();
  void ReplaceActiveInputText(const std::string &text);
  void ClearKeyboardState();
  void ReleaseSurface();

  bool initialized_ = false;
  bool menuVisible_ = true;
  ANativeWindow *window_ = nullptr;
  int32_t screenWidth_ = 0;
  int32_t screenHeight_ = 0;
  bool mouseDown_ = false;
  int nativeTouchLogCount_ = 0;
  int keyboardMode_ = 0;
  int keyboardActiveId_ = 0;
  std::string keyboardText_;
  bool hasPendingKeyboardCommit_ = false;
  bool hasPendingKeyboardCancel_ = false;
  bool clearActiveAfterKeyboardCommit_ = false;
  int pendingKeyboardActiveId_ = 0;
  std::string pendingKeyboardText_;
  WindowRectSnapshot windowRectCache_[kMaxTouchWindows] = {};
  pthread_mutex_t imguiMutex_ = PTHREAD_MUTEX_INITIALIZER;
};

}  // namespace menu
