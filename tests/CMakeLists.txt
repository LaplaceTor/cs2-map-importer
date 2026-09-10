cmake_minimum_required(VERSION 3.28)
project(cs2importer_tests VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network Test)

qt_standard_project_setup(REQUIRES 6.8)

enable_testing()

if(NOT TARGET cs2importer_core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/Core ${CMAKE_CURRENT_BINARY_DIR}/src/Core)
endif()
if(NOT TARGET cs2importer_domain)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/Domain ${CMAKE_CURRENT_BINARY_DIR}/src/Domain)
endif()
if(NOT TARGET cs2importer_workflow)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/Workflow ${CMAKE_CURRENT_BINARY_DIR}/src/Workflow)
endif()
if(NOT TARGET cs2importer_application)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/Application ${CMAKE_CURRENT_BINARY_DIR}/src/Application)
endif()
if(NOT TARGET cs2importer_ui)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/UI ${CMAKE_CURRENT_BINARY_DIR}/src/UI)
endif()

qt_add_executable(test_async_task_logging
    TestAsyncTaskLogging.cpp
)

target_link_libraries(test_async_task_logging PRIVATE
    cs2importer_ui
    cs2importer_application
    cs2importer_domain
    cs2importer_core
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)

target_include_directories(test_async_task_logging PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(NAME test_async_task_logging COMMAND test_async_task_logging)
