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
  - `common/` contains shared logging helpers
- Overlay touch forwarding for active ImGui windows
- Android soft keyboard support:
  - single-line ImGui inputs open a compact top editor
  - multiline ImGui inputs open a half-screen editor panel
  - OK commits text back to the active ImGui input
  - Cancel/back clears focus without committing
- Full Java-to-payload build pipeline and native-only rebuild task

## Project Layout

```text
app/src/main/java/com/sahilm9098/arkmodmenu/
  GLES3JNIView.java        Native render surface bridge
  ImeInputController.java  Android soft-keyboard editor UI
  Loader.java              Runtime loader entry point
  MenuOverlay.java         Overlay window and touch handling

app/src/main/cpp/
  common/                  Logging helpers
  jni/                     JNI registration and string conversion
  payload/                 Java VM resolution and DEX bootstrap
  render/                  ImGui lifecycle, frame rendering, menu UI
  imgui/                   Dear ImGui sources and Android/OpenGL backends
  Dobby/                   Hooking dependency headers/libs
  Font/                    Embedded font assets
  dex_payload.h            Generated embedded DEX payload header
```

## Requirements

- Android Studio or Android Gradle Plugin command-line setup
- Android SDK with compile SDK 36 installed
- Android NDK installed
- CMake 3.22.1
- Java 11 compatibility for project sources
- `arm64-v8a` target device or emulator

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

Use the native-only task when only C++ files changed and `dex_payload.h` is already current:

```bash
./gradlew :app:buildNativeOnly
```

For a quick compile check:

```bash
./gradlew :app:assembleDebug
```

The final payload library is written to:

```text
output/libmenu.so
```

## Development Notes

- Keep `local.properties`, Gradle outputs, CMake outputs, APKs, and `output/` out of Git.
- `dex_payload.h` is generated, but it is kept in source control so a fresh checkout has a valid header for the first native compile pass.
- If Java overlay code changes, run `buildFullPipeline`; if only ImGui/native code changes, `buildNativeOnly` is enough.
- The ImGui menu body currently lives in `app/src/main/cpp/render/MenuRenderer.cpp`.
