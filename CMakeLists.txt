cmake_minimum_required(VERSION 4.0)
project(kpack)

set(CMAKE_CXX_STANDARD 26)

add_library(kpack STATIC Kpack.cpp Kpack.h)

add_executable(pack main.cpp)
target_link_libraries(pack kpack)

install(TARGETS pack DESTINATION bin)
install(FILES Kpack.h DESTINATION include)
install(TARGETS kpack DESTINATION lib)
