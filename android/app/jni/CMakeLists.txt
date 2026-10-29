cmake_minimum_required(VERSION 3.6)
project(MOD_ANDROID C)

# SDL2 source is cloned into jni/SDL by the build workflow
add_subdirectory(SDL)

set(MOD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
add_library(main SHARED
  ${MOD_ROOT}/src/app/main.c
  ${MOD_ROOT}/src/core/sms.c
  ${MOD_ROOT}/src/core/z80.c
  ${MOD_ROOT}/src/game/mod_wide.c
  ${MOD_ROOT}/src/game/mod_enhance.c
  ${MOD_ROOT}/src/app/music.c
  ${MOD_ROOT}/third_party/miniz/miniz.c)
target_include_directories(main PRIVATE ${MOD_ROOT}/third_party/miniz ${MOD_ROOT}/third_party/stb)
target_compile_options(main PRIVATE -O2)
target_link_libraries(main SDL2 m)
