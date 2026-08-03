# Native side of the Android APK build. SDL2 source is placed in SDL/ by
# android/setup-sdl.sh (not tracked in git). The game library must be named
# "main": SDLActivity.getLibraries() loads {"SDL2", "main"}.
#
# Reuses l2d/CMakeLists.txt as-is (add_subdirectory), but compiles the
# app/src/*.cpp sources directly into a SHARED "main" library instead of
# going through app/CMakeLists.txt, which hardcodes add_executable() for
# the desktop/CxxDroid build.
cmake_minimum_required(VERSION 3.20)
project(cpp-l2d-android CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(SDL)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../l2d l2d)

add_library(main SHARED
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../app/src/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../app/src/demo.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../app/src/router.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../app/src/viewport.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../app/src/scene.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../app/src/shapes.cpp
)

# L2D_ANDROID_APK enables the APK-only paths in engine.cpp/graphics.cpp
# (immersive fullscreen, AssetManager-backed asset reads). See the comment
# in engine.cpp for why this is a dedicated macro and not __ANDROID__.
# Applied to l2d too: engine.cpp/graphics.cpp live in that static library,
# not in "main", so the define must reach both targets.
target_compile_definitions(main PRIVATE L2D_ANDROID_APK)
target_compile_definitions(l2d PRIVATE L2D_ANDROID_APK)
target_link_libraries(main l2d SDL2 m)
