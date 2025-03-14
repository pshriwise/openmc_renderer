cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(openmc_renderer C CXX)


# Enable correct usage of CXX_EXTENSIONS
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.22)
  cmake_policy(SET CMP0128 NEW)
endif()

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, defaulting to RelWithDebInfo")
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build" FORCE)
endif()

find_package(OpenMC REQUIRED)

# Find Imgui
add_subdirectory(vendor/imguiwrap)

# Find the required packages
find_package(glfw3 REQUIRED)
find_package (OpenGL REQUIRED)

add_executable(omc-render main.cpp vendor/glad/src/glad.c)
target_compile_definitions(omc-render PUBLIC IMGUI_IMPL_OPENGL_LOADER_CUSTOM)
target_include_directories(omc-render PUBLIC vendor/glad/include)
target_link_libraries(omc-render PUBLIC OpenGL::GL OpenMC::libopenmc glfw imguiwrap)

target_compile_features(omc-render PUBLIC cxx_std_14)
set(CMAKE_CXX_FLAGS "-pedantic-errors")
