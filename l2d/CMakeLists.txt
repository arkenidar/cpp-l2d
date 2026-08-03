# l2d: reusable Love2D-like framework over SDL2 (CPU-only software
# rendering, no GL/GPU required). Image decoding via stb_image and font
# rasterization via stb_truetype replace SDL2_image and SDL2_ttf, so the
# only external dependency is SDL2 itself.
#
# Uses the plain SDL2_INCLUDE_DIRS/SDL2_LIBRARIES variables rather than
# the SDL2::SDL2 imported target: some SDL2 installs (e.g. CxxDroid's
# bundled FindSDL2.cmake) define the variables correctly but produce a
# broken imported target (e.g. stuffing the whole link line into
# IMPORTED_LOCATION), so this is the form that works everywhere.
#
# Skipped when the Android APK build (android/app/jni/CMakeLists.txt) has
# already brought in SDL2 as a target via add_subdirectory(SDL): there is
# no system SDL2 package to find under the NDK toolchain, and linking
# against the SDL2 target directly works fine there.
if(NOT TARGET SDL2)
  find_package(SDL2 REQUIRED)
endif()

add_library(l2d STATIC
  src/graphics.cpp
  src/engine.cpp
  src/stb_impl.cpp
)

target_include_directories(l2d
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stb
)
if(TARGET SDL2)
  target_link_libraries(l2d PUBLIC SDL2)
else()
  target_include_directories(l2d PUBLIC ${SDL2_INCLUDE_DIRS})
  target_link_libraries(l2d PUBLIC ${SDL2_LIBRARIES})
endif()

# The library's default font (Vera Sans 13px, same as LÖVE embeds) must
# sit in resources/ next to any executable that links l2d.
set(L2D_RESOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources PARENT_SCOPE)