cmake_minimum_required(VERSION 3.29)
project(untitled LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

include(cmake/CPM.cmake)

# Add GLFW dependency
CPMAddPackage(
    NAME GLFW
    GITHUB_REPOSITORY glfw/glfw
    GIT_TAG 3.3.9
    OPTIONS
        "GLFW_BUILD_TESTS OFF"
        "GLFW_BUILD_EXAMPLES OFF"
        "GLFW_BUILD_DOCS OFF"
)

# Add ImGui dependency
CPMAddPackage(
    NAME IMGUI
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG v1.91.6
)

# Find OpenGL and ZLIB
find_package(OpenGL REQUIRED)
find_package(ZLIB REQUIRED)

# Create ImGui library
add_library(imgui STATIC
    ${IMGUI_SOURCE_DIR}/imgui.cpp
    ${IMGUI_SOURCE_DIR}/imgui_demo.cpp
    ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
    ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
    ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
    ${IMGUI_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${IMGUI_SOURCE_DIR}
    ${IMGUI_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC
    glfw
    OpenGL::GL
)

add_executable(${CMAKE_PROJECT_NAME} gui.cpp)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR})
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE STB_IMAGE_IMPLEMENTATION)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    imgui
    glfw
    OpenGL::GL
    ZLIB::ZLIB
)

# Set C++ standard for all targets
set_target_properties(${CMAKE_PROJECT_NAME} imgui PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
