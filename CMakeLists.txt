cmake_minimum_required(VERSION 3.20)
project(shallowWaters_GPU CUDA)

set(CMAKE_CUDA_STANDARD 14)

add_executable(shallowWaters_GPU main.cu windows.h windows.cpp swe_cuda.cu textures.h textures.cpp SWE.h SWE.cpp)

set_target_properties(shallowWaters_GPU PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON)
