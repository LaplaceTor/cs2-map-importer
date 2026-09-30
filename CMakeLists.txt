cmake_minimum_required(VERSION 3.28)

if(POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0177 NEW)
endif()
if(POLICY CMP0194)
    cmake_policy(SET CMP0194 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0194 NEW)
endif()

project(cs2importer VERSION 2.0.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "CS2 Map Importer is only supported on Windows.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries by default" FORCE)

include(GNUInstallDirs)
set(CMAKE_INSTALL_BINDIR ".")
set(CMAKE_INSTALL_LIBDIR ".")

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 LinguistTools)

qt_standard_project_setup(REQUIRES 6.8)

set(QT_DEPLOY_BIN_DIR ".")
set(QT_DEPLOY_LIB_DIR ".")
set(QT_DEPLOY_PLUGINS_DIR "plugins")
set(QT_DEPLOY_QML_DIR "qml")

option(CS2IMPORTER_DEPLOY_QT_RUNTIME "Deploy Qt runtime using Qt CMake deployment API on Windows" ON)

add_subdirectory(src)

