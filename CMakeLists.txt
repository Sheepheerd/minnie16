cmake_minimum_required(VERSION 3.20)
project(minnie16 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(cpu16 STATIC
  src/cpu.c
  src/bus.c
  src/decoder.c
  src/simulator.c
  src/debug.c
  src/mmio.c
)
target_include_directories(cpu16 PUBLIC src)

find_package(SDL2 CONFIG)
if(SDL2_FOUND)
  target_sources(cpu16 PRIVATE src/display_sdl.c)
  target_link_libraries(cpu16 PUBLIC SDL2::SDL2)
else()
  message(WARNING "SDL2 not found, building without the graphics window")
  target_sources(cpu16 PRIVATE src/display_null.c)
endif()

add_executable(minnie src/main.c)
target_link_libraries(minnie PRIVATE cpu16)

enable_testing()
# add_subdirectory(tests)

