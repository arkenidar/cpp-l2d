package org.arkenidar.cppl2d;

import org.libsdl.app.SDLActivity;

/**
 * Launcher activity. All the work happens in SDL's Java glue (SDLActivity,
 * copied from the SDL2 tarball by android/setup-sdl.sh) and in the native
 * library built from the l2d/app CMake targets; this subclass only gives
 * the app's own package a launcher entry.
 */
public class CppL2dActivity extends SDLActivity {
}
