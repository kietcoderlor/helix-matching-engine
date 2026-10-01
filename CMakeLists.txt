cmake_minimum_required(VERSION 3.20)

project(helix
  VERSION 0.1.0
  DESCRIPTION "Helix simulated limit-order exchange"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HELIX_ENABLE_SANITIZERS
  "Enable ASan/UBSan (Unix debug / clang-cl). Off by default."
  OFF)

if(MSVC)
  add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(HELIX_ENABLE_SANITIZERS AND NOT MSVC)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

enable_testing()

add_subdirectory(engine)
