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

# libgit2 — works with both vcpkg (Windows) and pkg-config (Linux/macOS)
if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    # Windows / vcpkg path
    find_package(libgit2 CONFIG REQUIRED)
    set(LIBGIT2_TARGET libgit2::libgit2)
else()
    # Linux / macOS path
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBGIT2 REQUIRED libgit2)
    set(LIBGIT2_TARGET ${LIBGIT2_LIBRARIES})
endif()

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
    $<$<NOT:$<BOOL:${CMAKE_TOOLCHAIN_FILE}>>:${LIBGIT2_INCLUDE_DIRS}>
)

target_link_libraries(gitviz PRIVATE
    ${LIBGIT2_TARGET}
    ftxui::screen
    ftxui::dom
    ftxui::component
)

target_compile_options(gitviz PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /O2>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -O2>
)
