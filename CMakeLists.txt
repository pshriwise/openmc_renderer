cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(openmc C CXX)


# Enable correct usage of CXX_EXTENSIONS
if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.22)
  cmake_policy(SET CMP0128 NEW)
endif()

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, defaulting to RelWithDebInfo")
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build" FORCE)
endif()

find_package(OpenMC REQUIRED)

add_executable(omc-render main.cpp)

target_link_libraries(omc-render OpenMC::libopenmc)
target_compile_features(omc-render PUBLIC cxx_std_14)
set(CMAKE_CXX_FLAGS "-pedantic-errors")




