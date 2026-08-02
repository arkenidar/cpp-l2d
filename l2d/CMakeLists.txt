# l2d: reusable Love2D-like framework over SDL2 (+SDL2_image, +SDL2_ttf).
find_package(SDL2 REQUIRED)

# Some SDL2_image/SDL2_ttf packages (e.g. CxxDroid, Debian/Ubuntu) only ship
# pkg-config files, not CMake config files, so fall back to pkg-config when
# the CONFIG lookup fails.
find_package(SDL2_image CONFIG QUIET)
find_package(SDL2_ttf CONFIG QUIET)

if(NOT TARGET SDL2_image::SDL2_image OR NOT TARGET SDL2_ttf::SDL2_ttf)
  find_package(PkgConfig REQUIRED)
endif()

if(NOT TARGET SDL2_image::SDL2_image)
  pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)
  add_library(SDL2_image::SDL2_image ALIAS PkgConfig::SDL2_IMAGE)
endif()

if(NOT TARGET SDL2_ttf::SDL2_ttf)
  pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)
  add_library(SDL2_ttf::SDL2_ttf ALIAS PkgConfig::SDL2_TTF)
endif()

add_library(l2d STATIC
  src/graphics.cpp
  src/engine.cpp
)

target_include_directories(l2d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(l2d PUBLIC SDL2::SDL2 SDL2_image::SDL2_image SDL2_ttf::SDL2_ttf)

# The library's default font (Vera Sans 13px, same as LÖVE embeds) must
# sit in resources/ next to any executable that links l2d.
set(L2D_RESOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources PARENT_SCOPE)
