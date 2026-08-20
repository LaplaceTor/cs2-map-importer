cmake_minimum_required(VERSION 3.28)
project(cs2importer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network)

qt_standard_project_setup()

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
