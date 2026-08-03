# Android APK build

Packages cpp-l2d as an installable Android app (SDL activity + JNI glue),
distinct from CxxDroid's direct-execute mode and from running under
Termux:X11 — see the mode-detection comments in `../l2d/src/engine.cpp`.

## First-time setup

    ./setup-sdl.sh          # downloads SDL2 C source + Java glue (not tracked in git)
    echo "sdk.dir=/path/to/android-sdk" > local.properties

## Build

    ./gradlew assembleDebug
    # APK: app/build/outputs/apk/debug/app-debug.apk

## Notes

- The native library must be named `main` (`SDLActivity.getLibraries()`
  loads `{"SDL2", "main"}`); see `app/jni/CMakeLists.txt`.
- Builds compile with `-DL2D_ANDROID_APK`, which enables the immersive
  fullscreen call in `Engine::Engine` and is otherwise inert — desktop,
  CxxDroid, and Termux:X11 builds never define it.
- `app/build.gradle` stages `app/assets/` and `l2d/resources/` into the APK
  asset root at build time so `Engine::basePath()`-relative paths resolve
  the same way as on desktop.
