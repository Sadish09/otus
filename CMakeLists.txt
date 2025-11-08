cmake_minimum_required(VERSION 3.16)
project(otus VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
        ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui.git
        GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

add_executable(otus
        src/main.cpp
        src/CpuSampler.cpp
        src/MemSampler.cpp
        src/DiskSampler.cpp
        src/GpuSampler.cpp
        src/ProcSampler.cpp
)

target_include_directories(otus PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(otus PRIVATE ftxui::screen ftxui::dom ftxui::component dl)
