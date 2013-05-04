cmake_minimum_required (VERSION 2.6)
project(examples)
set (PROJECT_NAME examples)
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

set(ROSENBROCK_BINARY "rosenbrock")
set(ROSENBROCK_SOURCES "example/rosenbrock.cpp")

set(EVALUATION_BINARY "evaluation")
set(EVALUATION_SOURCES "example/evaluation.cpp")

ADD_LIBRARY( 
	pao
	src/Optimizer.cpp
	src/ParticleSwarmOptimization.cpp
)

 
# add the executables
add_executable(${ROSENBROCK_BINARY} ${ROSENBROCK_SOURCES})
target_link_libraries( ${ROSENBROCK_BINARY} pao pthread rt)

add_executable(${EVALUATION_BINARY} ${EVALUATION_SOURCES})
target_link_libraries( ${EVALUATION_BINARY} pao pthread rt)
