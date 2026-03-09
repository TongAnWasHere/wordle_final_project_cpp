cmake_minimum_required(VERSION 3.20)

project(Wordle LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Fetch dependencies ────────────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)

FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG v5.0.0
)

FetchContent_MakeAvailable(nlohmann_json ftxui)

# ── Executable ───────────────────────────────────────────────────────────────
add_executable(wordle
  src/main.cpp
)

# target_include_directories(wordle PRIVATE include)

target_link_libraries(wordle PRIVATE
  nlohmann_json::nlohmann_json
  ftxui::screen
  ftxui::dom
  ftxui::component
)