cmake_minimum_required(VERSION 3.28)
project(cs2importer VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "CS2 Map Importer is only supported on Windows.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
set(CMAKE_INSTALL_BINDIR ".")
set(CMAKE_INSTALL_LIBDIR ".")

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network)

qt_standard_project_setup(REQUIRES 6.8)

set(QT_DEPLOY_BIN_DIR ".")
set(QT_DEPLOY_LIB_DIR ".")
set(QT_DEPLOY_PLUGINS_DIR "plugins")
set(QT_DEPLOY_QML_DIR "qml")

include(CTest)

option(CS2IMPORTER_DEPLOY_QT_RUNTIME "Deploy Qt runtime using Qt CMake deployment API on Windows" ON)
option(CS2IMPORTER_BUILD_TESTS "Build tests for CS2 Map Importer" ON)

# Third-party deps pulled in by sourcepp (minizip-ng) force BUILD_TESTING OFF in
# the cache during add_subdirectory(src); use CS2IMPORTER_BUILD_TESTS which is untouched.
if(CS2IMPORTER_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(src)

if(CS2IMPORTER_BUILD_TESTS)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
