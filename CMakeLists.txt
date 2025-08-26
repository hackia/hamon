cmake_minimum_required(VERSION 4.0)
project(hamon)

set(CMAKE_CXX_STANDARD 26)

add_library(cube STATIC HamonCube.cpp HamonCube.h HamonNode.cpp HamonNode.h)

add_executable(hamon main.cpp)
target_link_libraries(hamon cube)

install(TARGETS hamon DESTINATION bin)
install(FILES HamonCube.h DESTINATION include)
install(TARGETS cube DESTINATION lib)
