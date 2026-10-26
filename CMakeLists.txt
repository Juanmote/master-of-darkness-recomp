cmake_minimum_required(VERSION 3.16)
project(MasterOfDarknessRecomp C)
set(CMAKE_C_STANDARD 99)

set(MOD_CORE_SOURCES
  src/core/sms.c
  src/core/z80.c
  src/game/mod_wide.c)

find_package(SDL2 REQUIRED CONFIG)

add_executable(masterofdarkness src/app/main.c ${MOD_CORE_SOURCES})
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
