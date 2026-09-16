#include "ImGuiRenderer.h"

#include <android/native_window_jni.h>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <GLES3/gl3.h>

#include "IconsFontAwesome7.h"
#include "casncadia_mono.h"
#include "common/Log.h"
#include "fa_solid_900.h"
#include "imgui.h"
#include "imgui_impl_android.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include "JniString.h"
#include "MenuRenderer.h"

namespace menu {
namespace {

class MutexLock {
 public:
  explicit MutexLock(pthread_mutex_t *mutex) : mutex_(mutex) {
    pthread_mutex_lock(mutex_);
  }

  ~MutexLock() { pthread_mutex_unlock(mutex_); }

 private:
  pthread_mutex_t *mutex_;
};

std::string TruncateToInputCapacity(ImGuiInputTextState *state, const std::string &text) {
  if (state == nullptr || (state->Flags & ImGuiInputTextFlags_CallbackResize) != 0) {
    return text;
  }

  const int capacity = state->BufCapacity > 0 ? state->BufCapacity - 1 : 0;
  if (static_cast<int>(text.size()) <= capacity) {
    return text;
  }

  const char *begin = text.data();
  const char *end = begin + text.size();
  const char *safeEnd = ImTextFindValidUtf8CodepointEnd(begin, end, begin + capacity);
  return std::string(begin, safeEnd);
}

ImGuiWindow *FindTouchWindow(ImGuiContext *context, float x, float y) {
  if (context == nullptr) {
    return nullptr;
  }

  const ImVec2 position(x, y);
  for (int index = context->Windows.Size - 1; index >= 0; --index) {
    ImGuiWindow *window = context->Windows[index];
    if (window == nullptr || (!window->Active && !window->WasActive) ||
        window->Hidden || window->Collapsed ||
        (window->Flags & ImGuiWindowFlags_NoMouseInputs) != 0) {
      continue;
    }
    if (window->OuterRectClipped.Contains(position)) {
      return window;
    }
  }
  return nullptr;
}

ImGuiWindow *FindTouchWindowById(ImGuiContext *context, std::uint32_t id) {
  if (context == nullptr || id == 0) {
    return nullptr;
  }

  for (int index = 0; index < context->Windows.Size; ++index) {
    ImGuiWindow *window = context->Windows[index];
    if (window != nullptr && window->ID == id) {
      return window;
    }
  }
  return nullptr;
}

}  // namespace

ImGuiRenderer &ImGuiRenderer::Get() {
  static ImGuiRenderer renderer;
  return renderer;
}

void ImGuiRenderer::Initialize(JNIEnv *env, jobject surface, float density,
                               bool enableGestureScroll) {
  MutexLock lock(&imguiMutex_);

  if (initialized_) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplAndroid_Shutdown();
    ReleaseSurface();
  } else {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    initialized_ = true;
  }

  gestureScrollEnabled_ = enableGestureScroll;
  ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = enableGestureScroll;
  // Initialization may also run when Android recreates the rendering surface.
  // Discard any unfinished gesture before applying the new configuration.
  touchActive_ = false;
  touchCanScroll_ = false;
  touchScrolling_ = false;
  suppressTouchClick_ = false;
  touchAxisLocked_ = false;
  pendingTouchScrollX_ = 0.0f;
  pendingTouchScrollY_ = 0.0f;
  touchWindowId_ = 0;
  mouseDown_ = false;
  ImGui::GetIO().AddMouseButtonEvent(0, false);
  ImGui::GetIO().MouseDown[0] = false;

  window_ = ANativeWindow_fromSurface(env, surface);
  ImGui_ImplAndroid_Init(window_);
  ImGui_ImplOpenGL3_Init("#version 300 es");

  LoadFonts();
  ApplyMenuStyle(density);
}

void ImGuiRenderer::UpdateSize(int width, int height) {
  MutexLock lock(&imguiMutex_);
  if (!HasContext()) {
    return;
  }

  screenWidth_ = width;
  screenHeight_ = height;
  glViewport(0, 0, width, height);
  ImGui::GetIO().DisplaySize =
      ImVec2(static_cast<float>(width), static_cast<float>(height));
}

void ImGuiRenderer::RenderFrame() {
  MutexLock lock(&imguiMutex_);
  if (!HasContext()) {
    return;
  }

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplAndroid_NewFrame();
  ImGui::NewFrame();
  ApplyPendingKeyboardAction();
  SuppressTouchClick();

  if (menuVisible_) {
    RenderMenuWindow();
  }
  ApplyTouchScroll();
  FinishPendingKeyboardAction();

  if (!touchActive_) {
    suppressTouchClick_ = false;
    touchCanScroll_ = false;
    touchAxisLocked_ = false;
    touchWindowId_ = 0;
  }

  UpdateWindowRectCache();
  UpdateKeyboardState();

  ImGui::Render();
  glViewport(0, 0, screenWidth_, screenHeight_);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiRenderer::Shutdown() {
  MutexLock lock(&imguiMutex_);
  if (!initialized_) {
    return;
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplAndroid_Shutdown();
  ImGui::DestroyContext();
  ReleaseSurface();

  initialized_ = false;
  screenWidth_ = 0;
  screenHeight_ = 0;
  mouseDown_ = false;
  touchActive_ = false;
  touchCanScroll_ = false;
  touchScrolling_ = false;
  suppressTouchClick_ = false;
  touchAxisLocked_ = false;
  touchScrollAxis_ = 1;
  touchStartX_ = 0.0f;
  touchStartY_ = 0.0f;
  touchLastX_ = 0.0f;
  touchLastY_ = 0.0f;
  pendingTouchScrollX_ = 0.0f;
  pendingTouchScrollY_ = 0.0f;
  touchWindowId_ = 0;
  nativeTouchLogCount_ = 0;
  ClearWindowRectCache();
  ClearKeyboardState();
}

void ImGuiRenderer::HandleTouch(bool down, float x, float y) {
  MutexLock lock(&imguiMutex_);
  if (!HasContext()) {
    if (nativeTouchLogCount_ < 8) {
      LogWarn("drop touch before imgui context down=%d x=%.1f y=%.1f",
              down ? 1 : 0, x, y);
      ++nativeTouchLogCount_;
    }
    return;
  }

  ApplyTouch(down, x, y);
  if (nativeTouchLogCount_ < 24) {
    LogInfo("apply touch down=%d x=%.1f y=%.1f", down ? 1 : 0, x, y);
    ++nativeTouchLogCount_;
  }
}

jobjectArray ImGuiRenderer::GetWindowRects(JNIEnv *env) {
  WindowRectSnapshot snapshots[kMaxTouchWindows] = {};
  {
    MutexLock lock(&imguiMutex_);
    std::memcpy(snapshots, windowRectCache_, sizeof(snapshots));
  }

  jclass stringClass = env->FindClass("java/lang/String");
  if (stringClass == nullptr) {
    return nullptr;
  }

  jobjectArray results = env->NewObjectArray(kMaxTouchWindows, stringClass, nullptr);
  env->DeleteLocalRef(stringClass);
  if (results == nullptr) {
    return nullptr;
  }

  char buffer[128];
  for (int index = 0; index < kMaxTouchWindows; ++index) {
    const WindowRectSnapshot &snapshot = snapshots[index];
    if (!snapshot.active) {
      jstring emptyRect = env->NewStringUTF("1000|0|0|0|0");
      env->SetObjectArrayElement(results, index, emptyRect);
      env->DeleteLocalRef(emptyRect);
      continue;
    }

    std::snprintf(buffer, sizeof(buffer), "%d|%.4f|%.4f|%.4f|%.4f",
                  snapshot.id, snapshot.x, snapshot.y, snapshot.w, snapshot.h);
    jstring rect = env->NewStringUTF(buffer);
    env->SetObjectArrayElement(results, index, rect);
    env->DeleteLocalRef(rect);
  }

  return results;
}

jobjectArray ImGuiRenderer::GetKeyboardState(JNIEnv *env) {
  int mode = 0;
  int activeId = 0;
  std::string text;
  {
    MutexLock lock(&imguiMutex_);
    mode = keyboardMode_;
    activeId = keyboardActiveId_;
    text = keyboardText_;
  }

  jclass stringClass = env->FindClass("java/lang/String");
  if (stringClass == nullptr) {
    return nullptr;
  }

  jobjectArray results = env->NewObjectArray(3, stringClass, nullptr);
  env->DeleteLocalRef(stringClass);
  if (results == nullptr) {
    return nullptr;
  }

  char numberBuffer[32];
  std::snprintf(numberBuffer, sizeof(numberBuffer), "%d", mode);
  jstring modeString = env->NewStringUTF(numberBuffer);
  env->SetObjectArrayElement(results, 0, modeString);
  env->DeleteLocalRef(modeString);

  std::snprintf(numberBuffer, sizeof(numberBuffer), "%d", activeId);
  jstring idString = env->NewStringUTF(numberBuffer);
  env->SetObjectArrayElement(results, 1, idString);
  env->DeleteLocalRef(idString);

  jstring textString = Utf8ToJString(env, text);
  env->SetObjectArrayElement(results, 2, textString);
  env->DeleteLocalRef(textString);
  return results;
}

void ImGuiRenderer::CommitKeyboardText(const std::string &text) {
  MutexLock lock(&imguiMutex_);
  if (!HasContext()) {
    return;
  }

  hasPendingKeyboardCommit_ = true;
  hasPendingKeyboardCancel_ = false;
  pendingKeyboardActiveId_ = keyboardActiveId_;
  pendingKeyboardText_ = text;
  ClearKeyboardState();
}

void ImGuiRenderer::CancelKeyboardText() {
  MutexLock lock(&imguiMutex_);
  hasPendingKeyboardCancel_ = true;
  hasPendingKeyboardCommit_ = false;
  pendingKeyboardActiveId_ = keyboardActiveId_;
  pendingKeyboardText_.clear();
  ClearKeyboardState();
}

bool ImGuiRenderer::IsMenuVisible() const {
  return menuVisible_;
}

bool ImGuiRenderer::HasContext() const {
  return initialized_ && ImGui::GetCurrentContext() != nullptr;
}

void ImGuiRenderer::ApplyMenuStyle(float density) {
  ImGuiIO &io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.FontGlobalScale = density > 0.0f ? density * 0.55f : 1.0f;

  ImGui::StyleColorsDark();
  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowRounding = 8.0f;
  style.ChildRounding = 8.0f;
  style.FrameRounding = 7.0f;
  style.PopupRounding = 8.0f;
  style.ScrollbarRounding = 8.0f;
  style.GrabRounding = 8.0f;
  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 1.0f;
  style.ItemSpacing = ImVec2(10.0f, 8.0f);
  style.FramePadding = ImVec2(12.0f, 8.0f);
  style.WindowPadding = ImVec2(14.0f, 14.0f);
  style.TouchExtraPadding = ImVec2(6.0f, 6.0f);
  style.ScrollbarSize = 18.0f;
  style.GrabMinSize = 22.0f;
}

void ImGuiRenderer::ApplyTouch(bool down, float x, float y) {
  ImGuiIO &io = ImGui::GetIO();
  io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
  io.AddMousePosEvent(x, y);

  if (down && !touchActive_) {
    touchActive_ = true;
    touchCanScroll_ = false;
    touchScrolling_ = false;
    suppressTouchClick_ = false;
    touchAxisLocked_ = false;
    touchScrollAxis_ = 1;
    touchStartX_ = x;
    touchStartY_ = y;
    touchLastX_ = x;
    touchLastY_ = y;
    pendingTouchScrollX_ = 0.0f;
    pendingTouchScrollY_ = 0.0f;

    ImGuiContext *context = ImGui::GetCurrentContext();
    ImGuiWindow *window = FindTouchWindow(context, x, y);
    if (window != nullptr) {
      touchWindowId_ = static_cast<std::uint32_t>(window->ID);
      // Keep the title bar and scrollbar as normal ImGui interactions.
      touchCanScroll_ = gestureScrollEnabled_ && window->InnerRect.Contains(ImVec2(x, y));
    } else {
      touchWindowId_ = 0;
    }
  } else if (touchActive_) {
    // Include the final UP position: fast swipes may have no intermediate MOVE.
    const float deltaX = x - touchLastX_;
    const float deltaY = y - touchLastY_;
    const float fromStartX = x - touchStartX_;
    const float fromStartY = y - touchStartY_;

    if (!touchScrolling_ && touchCanScroll_) {
      const float threshold = ImMax(8.0f, io.MouseDragThreshold);
      if (fromStartX * fromStartX + fromStartY * fromStartY >=
          threshold * threshold) {
        touchScrolling_ = true;
        touchAxisLocked_ = true;
        touchScrollAxis_ = std::fabs(fromStartY) >= std::fabs(fromStartX) ? 1 : 0;
        suppressTouchClick_ = true;
      }
    }

    if (touchScrolling_) {
      // Direct manipulation: moving the finger down moves the content down,
      // which means the ImGui scroll offset moves up.
      if (touchScrollAxis_ == 1) {
        pendingTouchScrollY_ -= deltaY;
      } else {
        pendingTouchScrollX_ -= deltaX;
      }
    }

    touchLastX_ = x;
    touchLastY_ = y;
    if (!down && touchCanScroll_ && !touchScrolling_) {
      // Content presses are deferred until release so InputText never sees the
      // initial DOWN of a scroll gesture. ImGui's input queue delivers this tap
      // as a press followed by a release on successive frames.
      io.AddMouseButtonEvent(0, true);
      io.AddMouseButtonEvent(0, false);
    }
  }

  if (!down) {
    if (touchScrolling_) {
      suppressTouchClick_ = true;
    }
    touchActive_ = false;
    touchScrolling_ = false;
    // Keep the selected axis until the pending final movement is applied by
    // the render thread. The UP event can arrive before the next frame.
    touchLastX_ = x;
    touchLastY_ = y;
  }

  // Title bars, scrollbars, and gesture-disabled input keep normal press/drag behavior.
  const bool forwardDown = down && !touchCanScroll_;
  if (mouseDown_ != forwardDown) {
    io.AddMouseButtonEvent(0, forwardDown);
    mouseDown_ = forwardDown;
  }
}

void ImGuiRenderer::SuppressTouchClick() {
  if (!gestureScrollEnabled_ || (!touchScrolling_ && !suppressTouchClick_)) {
    return;
  }

  ImGuiIO &io = ImGui::GetIO();
  ImGui::ClearActiveID();
  io.MouseDown[0] = false;
  io.MouseClicked[0] = false;
  io.MouseDoubleClicked[0] = false;
  io.MouseClickedCount[0] = 0;
  io.MouseReleased[0] = false;
  io.MouseDownDuration[0] = -1.0f;
  io.MouseDownDurationPrev[0] = -1.0f;
}

void ImGuiRenderer::ApplyTouchScroll() {
  if (!gestureScrollEnabled_ ||
      (pendingTouchScrollX_ == 0.0f && pendingTouchScrollY_ == 0.0f)) {
    return;
  }

  ImGuiContext *context = ImGui::GetCurrentContext();
  ImGuiWindow *window = FindTouchWindowById(context, touchWindowId_);
  if (window == nullptr && context != nullptr) {
    window = context->HoveredWindow;
  }
  if (window == nullptr) {
    pendingTouchScrollX_ = 0.0f;
    pendingTouchScrollY_ = 0.0f;
    return;
  }

  const int axis = touchAxisLocked_ ? touchScrollAxis_ : 1;
  while (window->ParentWindow != nullptr && window->ScrollMax[axis] <= 0.0f) {
    window = window->ParentWindow;
  }

  if ((window->Flags & ImGuiWindowFlags_NoMouseInputs) != 0 ||
      (window->Flags & ImGuiWindowFlags_NoScrollWithMouse) != 0 ||
      window->ScrollMax[axis] <= 0.0f) {
    pendingTouchScrollX_ = 0.0f;
    pendingTouchScrollY_ = 0.0f;
    return;
  }

  if (axis == 1) {
    ImGui::SetScrollY(window,
                      ImClamp(window->Scroll.y + pendingTouchScrollY_,
                              0.0f, window->ScrollMax.y));
    pendingTouchScrollY_ = 0.0f;
  } else {
    ImGui::SetScrollX(window,
                      ImClamp(window->Scroll.x + pendingTouchScrollX_,
                              0.0f, window->ScrollMax.x));
    pendingTouchScrollX_ = 0.0f;
  }
}

void ImGuiRenderer::LoadFonts() {
  ImGuiIO &io = ImGui::GetIO();

  ImFontConfig fontConfig;
  fontConfig.OversampleH = 2;
  fontConfig.OversampleV = 1;
  fontConfig.PixelSnapH = true;

  io.Fonts->AddFontFromMemoryCompressedBase85TTF(
      CascadiaMono_compressed_data_base85, 25.0f, &fontConfig,
      io.Fonts->GetGlyphRangesDefault());

  static const ImWchar iconRanges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
  ImFontConfig iconConfig;
  iconConfig.MergeMode = true;
  iconConfig.PixelSnapH = true;
  iconConfig.GlyphMinAdvanceX = 18.0f;

  io.Fonts->AddFontFromMemoryTTF(
      const_cast<unsigned char *>(fa_solid_900), sizeof(fa_solid_900), 18.0f,
      &iconConfig, iconRanges);
}

void ImGuiRenderer::UpdateWindowRectCache() {
  ClearWindowRectCache();
  if (!HasContext()) {
    return;
  }

  ImGuiContext *context = ImGui::GetCurrentContext();
  if (context == nullptr) {
    return;
  }

  int outIndex = 0;
  for (int index = 0; index < context->Windows.Size && outIndex < kMaxTouchWindows;
       ++index) {
    ImGuiWindow *window = context->Windows[index];
    if (window == nullptr || !window->WasActive || window->RootWindow != window) {
      continue;
    }

    const ImVec2 pos = window->Pos;
    const ImVec2 size = window->Size;
    const float maxX = (screenWidth_ > 0)
                           ? ImMax(0.0f, static_cast<float>(screenWidth_) - size.x)
                           : pos.x;
    const float maxY = (screenHeight_ > 0)
                           ? ImMax(0.0f, static_cast<float>(screenHeight_) - size.y)
                           : pos.y;

    WindowRectSnapshot &snapshot = windowRectCache_[outIndex++];
    snapshot.active = true;
    snapshot.id = static_cast<int>(window->ID);
    snapshot.x = ImClamp(pos.x, 0.0f, maxX);
    snapshot.y = ImClamp(pos.y, 0.0f, maxY);
    snapshot.w = size.x;
    snapshot.h = size.y;
  }
}

void ImGuiRenderer::ClearWindowRectCache() {
  std::memset(windowRectCache_, 0, sizeof(windowRectCache_));
}

void ImGuiRenderer::UpdateKeyboardState() {
  ClearKeyboardState();
  if (!HasContext()) {
    return;
  }

  ImGuiContext *context = ImGui::GetCurrentContext();
  if (context == nullptr) {
    return;
  }

  ImGuiInputTextState *state = &context->InputTextState;
  if (context->ActiveId == 0 || state->ID == 0 || state->ID != context->ActiveId ||
      state->TextA.Data == nullptr ||
      (state->Flags & ImGuiInputTextFlags_ReadOnly) != 0) {
    return;
  }

  keyboardMode_ = (state->Flags & ImGuiInputTextFlags_Multiline) != 0 ? 2 : 1;
  keyboardActiveId_ = static_cast<int>(state->ID);
  keyboardText_.assign(state->TextA.Data, state->TextLen);
}

void ImGuiRenderer::ApplyPendingKeyboardAction() {
  if (!hasPendingKeyboardCommit_ && !hasPendingKeyboardCancel_) {
    return;
  }

  ImGuiContext *context = ImGui::GetCurrentContext();
  if (context == nullptr || context->ActiveId == 0 ||
      (pendingKeyboardActiveId_ != 0 &&
       static_cast<int>(context->ActiveId) != pendingKeyboardActiveId_)) {
    hasPendingKeyboardCommit_ = false;
    hasPendingKeyboardCancel_ = false;
    clearActiveAfterKeyboardCommit_ = false;
    pendingKeyboardActiveId_ = 0;
    pendingKeyboardText_.clear();
    return;
  }

  if (hasPendingKeyboardCancel_) {
    ImGui::ClearActiveID();
    hasPendingKeyboardCommit_ = false;
    hasPendingKeyboardCancel_ = false;
    clearActiveAfterKeyboardCommit_ = false;
    pendingKeyboardActiveId_ = 0;
    pendingKeyboardText_.clear();
    return;
  }

  if (hasPendingKeyboardCommit_) {
    ReplaceActiveInputText(pendingKeyboardText_);
    clearActiveAfterKeyboardCommit_ = true;
  }

  hasPendingKeyboardCommit_ = false;
  hasPendingKeyboardCancel_ = false;
  pendingKeyboardActiveId_ = 0;
  pendingKeyboardText_.clear();
}

void ImGuiRenderer::FinishPendingKeyboardAction() {
  if (!clearActiveAfterKeyboardCommit_) {
    return;
  }

  if (HasContext()) {
    ImGui::ClearActiveID();
  }
  clearActiveAfterKeyboardCommit_ = false;
}

void ImGuiRenderer::ReplaceActiveInputText(const std::string &text) {
  ImGuiContext *context = ImGui::GetCurrentContext();
  if (context == nullptr || context->ActiveId == 0) {
    return;
  }

  ImGuiInputTextState *state = &context->InputTextState;
  if (state->ID == 0 || state->ID != context->ActiveId ||
      (state->Flags & ImGuiInputTextFlags_ReadOnly) != 0) {
    return;
  }

  const std::string safeText = TruncateToInputCapacity(state, text);
  const int requiredSize = static_cast<int>(safeText.size()) + 1;
  const bool resizable = (state->Flags & ImGuiInputTextFlags_CallbackResize) != 0;
  const int targetSize = resizable ? requiredSize : ImMax(requiredSize, state->BufCapacity);
  state->TextA.resize(targetSize);
  if (!safeText.empty()) {
    std::memcpy(state->TextA.Data, safeText.data(), safeText.size());
  }
  state->TextA[safeText.size()] = '\0';
  state->TextLen = static_cast<int>(safeText.size());
  state->TextSrc = state->TextA.Data;
  state->Edited = true;
  state->CursorFollow = true;
  state->CursorAnimReset();
  state->SetSelection(state->TextLen, state->TextLen);

  context->ActiveIdHasBeenEditedThisFrame = true;
  context->ActiveIdHasBeenEditedBefore = true;
}

void ImGuiRenderer::ClearKeyboardState() {
  keyboardMode_ = 0;
  keyboardActiveId_ = 0;
  keyboardText_.clear();
}

void ImGuiRenderer::ReleaseSurface() {
  if (window_ != nullptr) {
    ANativeWindow_release(window_);
    window_ = nullptr;
  }
}

}  // namespace menu
