cmake_minimum_required(VERSION 3.10)

project(MandelbrotDisplayer)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(OpenMP REQUIRED)

include_directories(include)

add_executable(MandelbrotDisplayer main.cpp glad.c shaderProgram.cpp complexBigNum.cpp)

target_link_libraries(MandelbrotDisplayer ${OPENGL_LIBRARIES} glfw gmp gmpxx OpenMP::OpenMP_CXX)