cmake_minimum_required(VERSION 3.4)
project(CacheLab C)

set(CMAKE_C_STANDARD 11)

add_executable(CacheLab csim.c cachelab.c cachelab.h)