cmake_minimum_required(VERSION 3.20)
project(gitviz VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies ──────────────────────────────────────────────────────────────
include(FetchContent)

# FTXUI
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
  GIT_TAG        v5.0.0
)
FetchContent_MakeAvailable(ftxui)

# libgit2 (system install expected: sudo apt install libgit2-dev)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGIT2 REQUIRED libgit2)

# ── Sources ───────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/repo.cpp
    src/graph.cpp
    src/tui.cpp
    src/diff.cpp
    src/blame.cpp
)

add_executable(gitviz ${SOURCES})

target_include_directories(gitviz PRIVATE
    src/
    ${LIBGIT2_INCLUDE_DIRS}
)

target_link_libraries(gitviz PRIVATE
    ${LIBGIT2_LIBRARIES}
    ftxui::screen
    ftxui::dom
    ftxui::component
)

target_compile_options(gitviz PRIVATE -Wall -Wextra -O2)
