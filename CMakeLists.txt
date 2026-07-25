cmake_minimum_required(VERSION 3.28)

project(StreetsEnhanced VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL3 CONFIG REQUIRED)

add_executable(StreetsEnhanced
    src/main.cpp
)

target_link_libraries(StreetsEnhanced PRIVATE SDL3::SDL3)

if (MSVC)
    target_compile_options(StreetsEnhanced PRIVATE /W4 /permissive- /EHsc)
endif()
