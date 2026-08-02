# l2d: reusable Love2D-like framework over SDL2 (+SDL2_image, +SDL2_ttf).
find_package(SDL2 REQUIRED)

# Some SDL2_image/SDL2_ttf installs (e.g. CxxDroid, Debian/Ubuntu) provide
# neither a CMake config nor pkg-config, so fall back to locating the
# headers/libraries directly.
find_package(SDL2_image CONFIG QUIET)
find_package(SDL2_ttf CONFIG QUIET)

if(NOT TARGET SDL2_image::SDL2_image)
  find_path(SDL2_IMAGE_INCLUDE_DIR SDL_image.h PATH_SUFFIXES SDL2)
  find_library(SDL2_IMAGE_LIBRARY NAMES SDL2_image)
  if(NOT SDL2_IMAGE_INCLUDE_DIR OR NOT SDL2_IMAGE_LIBRARY)
    message(FATAL_ERROR "SDL2_image not found (looked for SDL_image.h and libSDL2_image)")
  endif()
  add_library(SDL2_image::SDL2_image UNKNOWN IMPORTED)
  set_target_properties(SDL2_image::SDL2_image PROPERTIES
    IMPORTED_LOCATION "${SDL2_IMAGE_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SDL2_IMAGE_INCLUDE_DIR}")
endif()

if(NOT TARGET SDL2_ttf::SDL2_ttf)
  find_path(SDL2_TTF_INCLUDE_DIR SDL_ttf.h PATH_SUFFIXES SDL2)
  find_library(SDL2_TTF_LIBRARY NAMES SDL2_ttf)
  if(NOT SDL2_TTF_INCLUDE_DIR OR NOT SDL2_TTF_LIBRARY)
    message(FATAL_ERROR "SDL2_ttf not found (looked for SDL_ttf.h and libSDL2_ttf)")
  endif()
  add_library(SDL2_ttf::SDL2_ttf UNKNOWN IMPORTED)
  set_target_properties(SDL2_ttf::SDL2_ttf PROPERTIES
    IMPORTED_LOCATION "${SDL2_TTF_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SDL2_TTF_INCLUDE_DIR}")
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
