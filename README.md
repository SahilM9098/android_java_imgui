# Android ImGui Runtime Payload

Android ImGui Runtime Payload is an Android native library project that packages a Java overlay loader into a native payload. The Java side is converted into `dex_payload.h`, compiled into `libmenu.so`, and loaded at runtime to initialize the overlay UI and render Dear ImGui with OpenGL ES.

This project is intended for authorized testing, research, and internal tooling on apps and devices you own or have permission to inspect.

## Features

- Runtime Java payload bootstrapping through `InMemoryDexClassLoader`
- Dear ImGui rendering on Android with OpenGL ES 3
- Modular native code split by responsibility:
  - `payload/` resolves the Java VM and starts the embedded DEX loader
  - `jni/` registers Java native methods and handles JNI conversion helpers
  - `render/` owns ImGui initialization, rendering, touch handling, and keyboard handoff
  - `Utilities/common/` contains shared logging helpers
- Overlay touch forwarding for active ImGui windows
- Optional touch-drag scrolling with axis locking; content taps activate on release
- ShadowHook initialization with startup status logging
- Android soft keyboard support:
  - single-line ImGui inputs open a compact top editor
  - multiline ImGui inputs open a half-screen editor panel
  - OK commits text back to the active ImGui input
  - Cancel dismisses the editor without committing; Android handles Back through the keyboard and dialog
  - dialog-based editor supports native selection handles and contextual text actions
- Full Java-to-payload build pipeline and native-only rebuild task

## Project Layout

```text
app/src/main/java/com/sahilm9098/arkmodmenu/
  GLES3JNIView.java        Native render surface bridge
  ImeInputController.java  Android soft-keyboard editor UI
  Loader.java              Runtime loader entry point
  MenuOverlay.java         Overlay window and touch handling

app/src/main/cpp/
  Utilities/
    common/                Logging helpers
    Font/                  Embedded font assets
    shadowhook/            Hooking dependency headers/libs
  jni/                     JNI registration and string conversion
  payload/                 Java VM resolution and DEX bootstrap
  render/                  ImGui lifecycle, frame rendering, menu UI
  imgui/                   Dear ImGui sources and Android/OpenGL backends
  dex_payload.h            Generated embedded DEX payload header
```

## Requirements

- Android Studio or Android Gradle Plugin command-line setup
- Gradle 9.7.1 (provided by the Gradle wrapper)
- Android Gradle Plugin 9.0.1
- Android SDK with compile/target SDK 36 installed
- Android NDK r29 (`29.0.14206865`)
- CMake 3.22.1
- JDK 25 to run Gradle and compile the project; Java source and bytecode target remain Java 17
- Android 8.0 (API 26) or newer
- `arm64-v8a` target device or emulator

Create an untracked `local.properties` file in the project root with your SDK location:

```properties
sdk.dir=/absolute/path/to/Android/Sdk
```

Alternatively, set `ANDROID_HOME` to your SDK directory. To make ADB available in your terminal:

```bash
export ANDROID_HOME="/absolute/path/to/Android/Sdk"
export PATH="$ANDROID_HOME/platform-tools:$PATH"
```

Put these exports in your shell startup file to make them available in new terminals.

## Build Commands

Use the full pipeline whenever Java files change:

```bash
./gradlew :app:buildFullPipeline
```

This performs:

1. Release APK build
2. `classes.dex` extraction
3. `app/src/main/cpp/dex_payload.h` regeneration
4. Second native release compile using the refreshed payload
5. `libmenu.so` copy into `output/`

Run the full pipeline for the first build and after Java, resource, or dependency changes affecting the embedded payload. To force the tasks to rerun:

```bash
./gradlew :app:buildFullPipeline --rerun-tasks --console=plain
```

Use the native-only task when only C++ files changed and `dex_payload.h` is already current:

```bash
./gradlew :app:buildNativeOnly
```

Although `buildNativeOnly` depends on `assembleRelease` and may run Java tasks, it does not regenerate `dex_payload.h`. Running `assembleRelease` or `assembleDebug` alone also does not refresh the embedded Java payload.

For a quick compile check:

```bash
./gradlew :app:assembleDebug
```

The final payload library is written to:

```text
output/libmenu.so
```

The release APK used for DEX extraction is written to `app/build/outputs/apk/release/app-release-unsigned.apk`.

## Gesture Scrolling

Configure the option in [GlesJniBridge.cpp](app/src/main/cpp/jni/GlesJniBridge.cpp), inside `NativeInitImgui`:

```cpp
constexpr bool enableGestureScroll = true;
ImGuiRenderer::Get().Initialize(env, surface, density, enableGestureScroll);
```

- `true` (current default): drag content to scroll; drag the window title bar to move it. Content taps are delivered on release, so starting a scroll over an input does not briefly activate it.
- `false`: disable gesture scrolling and use normal ImGui interaction, including dragging windows from empty content.

Scrollbars remain usable in both modes. Rebuild with `./gradlew buildNativeOnly` after changing the option.

## Text Selection

Long-press text in the Android editor for selection handles and actions such as Select all, Cut, Copy, Paste, and Share. Available actions depend on the selection, clipboard, and device. The dialog-based implementation is in [ImeInputController.java](app/src/main/java/com/sahilm9098/arkmodmenu/ImeInputController.java).

## Deployment and Logs

Copy the rebuilt library to the path used by your application's loader. Replace `your.target.package` with its package name; the destination directory must exist and be writable:

```bash
adb devices
adb push output/libmenu.so /sdcard/Android/media/your.target.package/files/libmenu.so
```

Copying the library does not load it. Restart or reload through your existing loading workflow to use the updated native code and embedded Java classes.

When loaded, the library initializes ShadowHook and starts the Java payload thread. Inspect native and overlay logs with:

```bash
adb logcat -s ImguiRenderer MenuOverlay ImeInputController
```

## Development Notes

- Keep `local.properties`, Gradle outputs, CMake outputs, APKs, and `output/` out of Git.
- `dex_payload.h` is generated, but it is kept in source control so a fresh checkout has a valid header for the first native compile pass.
- If Java overlay code changes, run `buildFullPipeline`; if only ImGui/native code changes, `buildNativeOnly` is enough.
- The ImGui menu body currently lives in `app/src/main/cpp/render/MenuRenderer.cpp`.
- The menu currently includes test controls, selectable rows, and text fields for testing scrolling and input.
- Include the regenerated `dex_payload.h` when committing Java payload changes.
