cmake_minimum_required(VERSION 3.10)
set(PROJECT_NAME "Spider")
project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 17)

if(WIN32)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Wall")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -g")
endif(WIN32)

find_package(Vulkan REQUIRED)


add_subdirectory(Tools/Shader-Reflector)
