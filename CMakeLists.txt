# CMake project for fxSDK (fx-9860GIII monochrome .g1a add-in).
cmake_minimum_required(VERSION 3.15)
project(DinoRun VERSION 1.0 LANGUAGES C ASM)
include(GenerateG1A)
find_package(Gint 2.9 REQUIRED)
add_executable(dinorun src/main.c)
target_compile_options(dinorun PRIVATE -Os -Wall -Wextra)
target_link_libraries(dinorun Gint::Gint)
if("${FXSDK_PLATFORM_LONG}" STREQUAL "fx9860G")
  generate_g1a(TARGET dinorun OUTPUT "DINORUN.g1a"
    NAME "DINO RUN" ICON assets-fx/icon.png)
endif()
