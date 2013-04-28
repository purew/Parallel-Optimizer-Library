cmake_minimum_required (VERSION 2.6)
project(example_rosenbrock)
set (PROJECT_NAME example_rosenbrock)
SET(CMAKE_INCLUDE_CURRENT_DIR TRUE)

# The version number.
set (VERSION_MAJOR 0)
set (VERSION_MINOR 1)


set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -Wno-write-strings")
set(CMAKE_CXX_FLAGS_DEBUG "-g -fno-inline")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")


include_directories(
	"include"	
	)

FILE(GLOB_RECURSE SOURCES example/*.cpp)

ADD_LIBRARY( 
	pao
	src/Optimizer.cpp
	src/ParticleSwarmOptimization.cpp
)

 
# add the executable
add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries( ${PROJECT_NAME} pao pthread rt)
