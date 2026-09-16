cmake_minimum_required(VERSION 3.20)
project(minnie16 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(cpu16 STATIC
  src/cpu.c
  src/alu.c
)
target_include_directories(cpu16 PUBLIC src)

add_executable(minnie src/main.c)
target_link_libraries(minnie PRIVATE cpu16)

enable_testing()
# add_subdirectory(tests)
