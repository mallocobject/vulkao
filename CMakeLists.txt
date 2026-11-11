cmake_minimum_required(VERSION 3.21)
project(vulkao LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
    add_compile_options(/utf-8 /W3 /Zc:__cplusplus)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(shaders)

file(GLOB_RECURSE VULKAO_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(vulkao ${VULKAO_SOURCES})

target_include_directories(vulkao PRIVATE D:/sdk/VulkanSDK/1.4.363.1/Include)
target_link_directories(vulkao PRIVATE D:/sdk/VulkanSDK/1.4.363.1/Lib)
target_link_libraries(vulkao PRIVATE SDL3 vulkan-1)

vulkao_add_shaders(vulkao
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/shader.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/shader.frag"
)