cmake_minimum_required(VERSION 3.10)

# Set the project name
project(OpenGLCube)

# Find the required packages
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
find_package(glfw3 REQUIRED)

# Add the executable
add_executable(cube cube.cpp)

# Link the libraries
target_link_libraries(cube PRIVATE OpenGL::GL GLUT::GLUT glfw GLU)