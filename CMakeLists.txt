cmake_minimum_required(VERSION 3.16)
project(terminadventure VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v5.0.0
  PATCH_COMMAND git checkout -- . && git apply ${CMAKE_CURRENT_SOURCE_DIR}/patches/ftxui-c0-delivery.patch
)
FetchContent_MakeAvailable(ftxui)

# Create executable
add_executable(terminadventure
    src/main.cpp
    src/treeview/treeview.cpp
    src/dm_tools/dm_tool.cpp
)

# FTXUI is terminal-only, no extra platform libs needed

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(terminadventure PRIVATE -Wall -Wextra -O2)
endif()

# Set output directory
set_target_properties(terminadventure PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
target_link_libraries(terminadventure PRIVATE ftxui::screen)
target_link_libraries(terminadventure PRIVATE ftxui::dom)
target_link_libraries(terminadventure PRIVATE ftxui::component)
