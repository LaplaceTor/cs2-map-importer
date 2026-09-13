cmake_minimum_required(VERSION 3.28)
project(cs2importer_tests VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Test)

qt_standard_project_setup(REQUIRES 6.8)

enable_testing()

if(NOT TARGET cs2importer_core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/Core ${CMAKE_CURRENT_BINARY_DIR}/src/Core)
endif()

qt_add_executable(test_core_logging
    TestCoreLogging.cpp
)

target_link_libraries(test_core_logging PRIVATE
    cs2importer_core
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_core_logging PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(NAME test_core_logging COMMAND test_core_logging)
