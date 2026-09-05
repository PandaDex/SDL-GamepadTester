cmake_minimum_required(VERSION 3.16)
project(InputTester)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

add_subdirectory(external/SDL EXCLUDE_FROM_ALL)

if(WIN32)
add_executable(InputTester WIN32 main.c)
else()
add_executable(InputTester main.c)
endif()

target_link_libraries(InputTester PRIVATE SDL3::SDL3)
