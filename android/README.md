# Android APK build

Packages cpp-l2d as an installable Android app (SDL activity + JNI glue),
distinct from CxxDroid's direct-execute mode and from running under
Termux:X11 — see the mode-detection comments in `../l2d/src/engine.cpp`.

Modeled directly on arkenidar/sdl-cb-maze-game's android/ project, which is
confirmed working on a physical device.

## First-time setup

    ./setup-sdl.sh          # downloads SDL2 C source + Java glue (not tracked in git)
    echo "sdk.dir=/path/to/android-sdk" > local.properties

## Build

    ./gradlew assembleDebug
    # APK: app/build/outputs/apk/debug/app-debug.apk

## Install / reinstall

If a device reports "update" instead of "install" on a supposedly fresh
device, an APK with this applicationId (org.arkenidar.cppl2d) is already
present — uninstall it first if you want a clean install:

    adb uninstall org.arkenidar.cppl2d
    adb install app/build/outputs/apk/debug/app-debug.apk

If the app installs but crashes immediately on launch, capture the crash
with logcat while it starts:

    adb logcat -c
    adb shell am start -n org.arkenidar.cppl2d/.CppL2dActivity
    adb logcat | grep -i -E "cppl2d|AndroidRuntime|SDL|FATAL"

## Notes

- The native library must be named `main` (`SDLActivity.getLibraries()`
  loads `{"SDL2", "main"}`); see `app/jni/CMakeLists.txt`.
- applicationId/namespace is `org.arkenidar.cppl2d` — distinct from
  arkenidar/sdl-cb-maze-game's `org.arkenidar.mazegame` and arkenidar/capp's
  `com.capp.app`, so it should never collide with either on the same device.
- Builds compile with `-DL2D_ANDROID_APK`, which enables the immersive
  fullscreen call in `Engine::Engine` and is otherwise inert — desktop,
  CxxDroid, and Termux:X11 builds never define it.
- `app/build.gradle` stages `app/assets/` and `l2d/resources/` into the APK
  asset root at build time so `Engine::basePath()`-relative paths resolve
  the same way as on desktop.
