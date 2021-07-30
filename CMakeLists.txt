cmake_minimum_required(VERSION 3.20)
project(shallowWaters_GPU CUDA)

set(CMAKE_CUDA_STANDARD 14)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(GLUT REQUIRED)

add_executable(shallowWaters_GPU main.cu windows.cpp swe_cuda.cu textures.cpp SWE.cpp)

set_target_properties(shallowWaters_GPU PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON)

include_directories( ${OPENGL_INCLUDE_DIRS}  ${GLUT_INCLUDE_DIRS} )

target_link_libraries(shallowWaters_GPU ${OPENGL_LIBRARIES} ${GLUT_LIBRARY})
