cmake_minimum_required(VERSION 3.20)
project(gitviz VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── FTXUI (auto-downloaded, no install needed) ────────────────────────────────
include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
  GIT_TAG        v5.0.0
)
FetchContent_MakeAvailable(ftxui)

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

target_include_directories(gitviz PRIVATE src/)

target_link_libraries(gitviz PRIVATE
    ftxui::screen
    ftxui::dom
    ftxui::component
)

target_compile_options(gitviz PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /O2>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -O2>
)
