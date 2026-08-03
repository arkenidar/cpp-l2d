#!/bin/sh
# Build (if stale), install (if the APK changed), and launch the Android APK
# on a connected device/emulator, then stream its logcat output.
#
# "Fresh"/"upgradable" here just means: let Gradle decide. `assembleDebug`
# is already incremental (it no-ops down to individual .o files and Java
# classes via up-to-date checks), and `adb install -r` only replaces what's
# on the device if the APK's signature/content actually differs. So this
# script doesn't try to duplicate that staleness logic itself - it always
# asks Gradle to build, then always asks adb to install, and lets both
# tools skip the work if there's nothing to do. Re-run freely.
#
# Usage:
#   ./run-on-device.sh              # build, install, launch, tail logcat
#   ./run-on-device.sh --no-log      # build, install, launch, then exit
#   ./run-on-device.sh --release     # same, but the release build variant
#
# Env overrides (see the "Tunables" section below for what each one does):
#   ADB=/path/to/adb ./run-on-device.sh
set -eu
cd "$(dirname "$0")"

# --- Tunables -----------------------------------------------------------
# The app's applicationId + launcher activity (see app/build.gradle and
# app/src/main/AndroidManifest.xml). Change these if you rename the app.
PACKAGE="org.arkenidar.cppl2d"
ACTIVITY="$PACKAGE/.CppL2dActivity"

# adb usually isn't on PATH outside Android Studio; point ADB at it
# explicitly if `which adb` comes up empty. Override via env: ADB=... below.
ADB="${ADB:-adb}"

BUILD_VARIANT=Debug   # matches --release below
GRADLE_TASK=assembleDebug
APK_PATH="app/build/outputs/apk/debug/app-debug.apk"
TAIL_LOG=1

# --- Parse args -----------------------------------------------------------
for arg in "$@"; do
  case "$arg" in
    --no-log) TAIL_LOG=0 ;;
    --release)
      BUILD_VARIANT=Release
      GRADLE_TASK=assembleRelease
      APK_PATH="app/build/outputs/apk/release/app-release.apk"
      ;;
    *)
      echo "Unknown option: $arg" >&2
      echo "Usage: $0 [--no-log] [--release]" >&2
      exit 1
      ;;
  esac
done

# --- Sanity checks --------------------------------------------------------
if ! command -v "$ADB" >/dev/null 2>&1; then
  echo "error: '$ADB' not found." >&2
  echo "Point ADB at your SDK, e.g.:" >&2
  echo "  ADB=\$HOME/apps/android-sdk/platform-tools/adb $0" >&2
  exit 1
fi

if [ ! -d app/jni/SDL ]; then
  echo "error: app/jni/SDL is missing. Run ./setup-sdl.sh first." >&2
  exit 1
fi

# Exactly one device/emulator must be attached and authorized, otherwise
# every adb command below (install/start/logcat) would be ambiguous about
# which target to use.
device_count=$("$ADB" devices | tail -n +2 | grep -c '[[:space:]]device$' || true)
if [ "$device_count" -eq 0 ]; then
  echo "error: no authorized device/emulator found. Checklist:" >&2
  echo "  - phone plugged in via USB (or emulator running)" >&2
  echo "  - USB debugging enabled in Developer Options" >&2
  echo "  - 'Allow USB debugging?' accepted on the device screen" >&2
  "$ADB" devices -l >&2
  exit 1
elif [ "$device_count" -gt 1 ]; then
  echo "error: multiple devices attached; set ANDROID_SERIAL to pick one:" >&2
  "$ADB" devices -l >&2
  exit 1
fi

# --- Build ----------------------------------------------------------------
# Gradle's own up-to-date checks make this a no-op when nothing changed
# (C++ via CMake/ninja, Java/Kotlin via javac's incremental compiler).
echo "==> Building ($GRADLE_TASK)..."
./gradlew "$GRADLE_TASK"

# --- Install ---------------------------------------------------------------
# -r (reinstall, keep data) rather than uninstall+install: preserves the
# app's own storage across runs, and adb/pm already diff the APK content
# under the hood, so this is a no-op if the build didn't actually change.
echo "==> Installing $APK_PATH..."
"$ADB" install -r "$APK_PATH"

# --- Launch ----------------------------------------------------------------
# force-stop first: otherwise `am start` on an already-running activity
# just brings it to front instead of restarting it, which would mask a
# crash-on-launch (the exact bug this project hit before - see git log).
echo "==> Launching $ACTIVITY..."
"$ADB" shell am force-stop "$PACKAGE"
"$ADB" logcat -c   # drop old log lines so the tail below starts clean
"$ADB" shell am start -n "$ACTIVITY"

# --- Logcat ----------------------------------------------------------------
# Filtered to this app's own PID, which covers both SDLActivity's Java-side
# lifecycle logs (tagged "SDL") and anything the native game code logs via
# SDL_Log/__android_log_print - they all run in this same process.
# Ctrl-C to stop; the app keeps running on the device.
if [ "$TAIL_LOG" -eq 1 ]; then
  echo "==> Streaming logcat for $PACKAGE (Ctrl-C to stop)..."
  pid=""
  # The process may take a moment to spawn after `am start`.
  for _ in $(seq 1 20); do
    pid=$("$ADB" shell pidof "$PACKAGE" 2>/dev/null || true)
    [ -n "$pid" ] && break
    sleep 0.2
  done
  if [ -z "$pid" ]; then
    echo "warning: couldn't find the app's PID; showing unfiltered logcat." >&2
    "$ADB" logcat
  else
    "$ADB" logcat --pid="$pid"
  fi
fi
