cmake_minimum_required(VERSION 3.31.6)
project(hamon)

set(CMAKE_CXX_STANDARD 26)
set(GCC_WARNING_FLAGS
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wformat=2
        -Wconversion
        -Wsign-conversion
        -Werror
)

find_package(Gettext REQUIRED)
find_path(INTL_INCLUDE_DIR libintl.h PATHS /usr/local/include /usr/include)
find_library(INTL_LIBRARY NAMES intl libintl PATHS /usr/local/lib /usr/lib)

add_library(cube STATIC src/HamonCube.cpp include/HamonCube.hpp src/HamonNode.cpp include/HamonNode.hpp src/Hamon.cpp include/Hamon.hpp src/Make.cpp include/Make.hpp)
target_compile_options(cube PRIVATE ${GCC_WARNING_FLAGS})

add_executable(hamon apps/hamon/main.cpp)
target_compile_options(hamon PRIVATE ${GCC_WARNING_FLAGS})
target_link_libraries(hamon cube)
target_include_directories(hamon PRIVATE ${INTL_INCLUDE_DIR})
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
add_executable(
        hamon_tests
        tests/test_hamon.cpp
        tests/test_hamon_cube.cpp
        tests/test_hamon_node.cpp
)
target_compile_options(hamon_tests PRIVATE ${GCC_WARNING_FLAGS})
target_link_libraries(
        hamon_tests
        PRIVATE
        cube
        gtest_main
)

include(GoogleTest)
gtest_discover_tests(hamon_tests)
add_custom_command(
        TARGET hamon_tests
        POST_BUILD
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running tests automatically after build..."
)
