add_executable(cpp-l2d
  src/main.cpp
  src/demo.cpp
  src/router.cpp
  src/viewport.cpp
  src/scene.cpp
  src/shapes.cpp
)

target_link_libraries(cpp-l2d PRIVATE l2d)

# Bundle assets and the l2d default font next to the binary.
add_custom_command(TARGET cpp-l2d POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:cpp-l2d>/assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${L2D_RESOURCES_DIR} $<TARGET_FILE_DIR:cpp-l2d>/resources
)
