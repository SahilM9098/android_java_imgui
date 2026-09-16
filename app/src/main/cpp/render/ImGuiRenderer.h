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

  // Gesture scrolling reserves content drags for scrolling and title-bar drags for moving.
  void Initialize(JNIEnv *env, jobject surface, float density, bool enableGestureScroll);
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
  void ApplyTouchScroll();
  void SuppressTouchClick();
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
  bool gestureScrollEnabled_ = false;
  ANativeWindow *window_ = nullptr;
  int32_t screenWidth_ = 0;
  int32_t screenHeight_ = 0;
  bool mouseDown_ = false;
  bool touchActive_ = false;
  bool touchCanScroll_ = false;
  bool touchScrolling_ = false;
  bool suppressTouchClick_ = false;
  bool touchAxisLocked_ = false;
  int touchScrollAxis_ = 1;
  float touchStartX_ = 0.0f;
  float touchStartY_ = 0.0f;
  float touchLastX_ = 0.0f;
  float touchLastY_ = 0.0f;
  float pendingTouchScrollX_ = 0.0f;
  float pendingTouchScrollY_ = 0.0f;
  std::uint32_t touchWindowId_ = 0;
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
