cmake_minimum_required(VERSION 3.16)
project(MasterOfDarknessRecomp C)
set(CMAKE_C_STANDARD 99)

set(MOD_CORE_SOURCES
  src/core/sms.c
  src/core/z80.c
  src/game/mod_wide.c
  src/game/mod_enhance.c)

find_package(SDL2 REQUIRED CONFIG)

add_executable(masterofdarkness src/app/main.c src/app/music.c third_party/miniz/miniz.c ${MOD_CORE_SOURCES})
target_include_directories(masterofdarkness PRIVATE third_party/miniz third_party/stb)
if(TARGET SDL2::SDL2main)
  target_link_libraries(masterofdarkness PRIVATE SDL2::SDL2main)
endif()
if(TARGET SDL2::SDL2-static)
  target_link_libraries(masterofdarkness PRIVATE SDL2::SDL2-static m)
else()
  target_link_libraries(masterofdarkness PRIVATE SDL2::SDL2 m)
endif()

add_executable(headless tools/headless.c ${MOD_CORE_SOURCES})
target_link_libraries(headless PRIVATE m)

# Recompiled build for analysis/verification. The generated code is a translation of the
# game's program, so it is never committed or shipped: generate it from your own ROM with
#   python3 tools/recomp/recomp.py mod.sms coverage.bin generated
file(GLOB MOD_GENERATED ${CMAKE_SOURCE_DIR}/generated/recomp_*.c)
if(MOD_GENERATED)
  add_executable(headless_recomp tools/headless.c ${MOD_CORE_SOURCES} src/recomp/recomp_rt.c ${MOD_GENERATED})
  target_compile_definitions(headless_recomp PRIVATE MOD_HAVE_RECOMP)
  target_link_libraries(headless_recomp PRIVATE m)
endif()
