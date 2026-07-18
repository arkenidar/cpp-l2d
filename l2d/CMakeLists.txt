# l2d: reusable Love2D-like framework over SDL2 (+SDL2_image, +SDL2_ttf).
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED CONFIG)
find_package(SDL2_ttf REQUIRED CONFIG)

add_library(l2d STATIC
  src/graphics.cpp
  src/engine.cpp
)

target_include_directories(l2d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(l2d PUBLIC SDL2::SDL2 SDL2_image::SDL2_image SDL2_ttf::SDL2_ttf)

# The library's default font (Vera Sans 13px, same as LÖVE embeds) must
# sit in resources/ next to any executable that links l2d.
set(L2D_RESOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources PARENT_SCOPE)
