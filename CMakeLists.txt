cmake_minimum_required (VERSION 3.8)
project (rt)

# get rid of annoying MSVC warnings.
add_definitions(-D_CRT_SECURE_NO_WARNINGS -O2)
find_package(OpenMP)
add_executable(rt main.cpp Bitmap.cpp)
if(OpenMP_CXX_FOUND)
    target_link_libraries(rt PUBLIC OpenMP::OpenMP_CXX)
endif()
set (CMAKE_CXX_STANDARD 11)

target_link_libraries(rt ${ALL_LIBS} )

