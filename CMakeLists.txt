cmake_minimum_required(VERSION 3.29)
project(testproject)

set(CMAKE_CXX_STANDARD 23)

# Find SDL2 package
find_package(SDL2 REQUIRED)

# Include SDL2 headers
include_directories(${SDL2_INCLUDE_DIRS})

# Create executable
add_executable(testGame testGame.cpp
        testGame.cpp)

# Link SDL2 libraries
target_link_libraries(testGame ${SDL2_LIBRARIES})

add_executable(testproject main.cpp)
