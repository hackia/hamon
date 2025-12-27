cmake_minimum_required(VERSION 3.31)
project(hamon)

set(CMAKE_CXX_STANDARD 26)
find_package(Intl REQUIRED)
set(GCC_WARNING_FLAGS
        -Wall -Wextra -Wpedantic -Wshadow -Wformat=2
        -Wconversion -Wsign-conversion -Werror
)
add_library(cube STATIC
        src/HamonCube.cpp
        src/HamonNode.cpp
        src/Hamon.cpp
        src/Make.cpp
)

target_link_libraries(cube PUBLIC Intl::Intl)
target_compile_options(cube PRIVATE ${GCC_WARNING_FLAGS})

add_executable(hamon apps/hamon/main.cpp)
target_link_libraries(hamon PRIVATE cube)
target_compile_options(hamon PRIVATE ${GCC_WARNING_FLAGS})

install(TARGETS hamon DESTINATION bin)
install(FILES include/HamonCube.hpp include/Make.hpp include/HamonNode.hpp include/Hamon.hpp DESTINATION include)
install(TARGETS cube DESTINATION lib)

enable_testing()

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)
add_executable(hamon_tests
        tests/test_hamon.cpp
        tests/test_hamon_cube.cpp
        tests/test_hamon_node.cpp
)
target_link_libraries(hamon_tests PRIVATE cube gtest_main)
include(GoogleTest)
gtest_discover_tests(hamon_tests)
