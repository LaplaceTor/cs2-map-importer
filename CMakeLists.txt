cmake_minimum_required(VERSION 3.20)
project(cs2importer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    src/main.cpp
    src/ui.cpp
    src/ui.h
    src/appcore.cpp
    src/appcore.h
    src/mapimporter.cpp
    src/mapimporter.h
)

if(WIN32)
    add_executable(cs2importer WIN32 ${SOURCES})
else()
    add_executable(cs2importer ${SOURCES})
endif()

# We need C++17 at least
set_target_properties(cs2importer PROPERTIES CXX_STANDARD 17)

# Setup package references for WinUI 3 (WindowsAppSDK and CppWinRT)
set_target_properties(cs2importer PROPERTIES
    VS_GLOBAL_UseWinUI "true"
    VS_PACKAGE_REFERENCES "Microsoft.WindowsAppSDK_1.5.240428000;Microsoft.Windows.SDK.BuildTools_10.0.22621.3233;Microsoft.Windows.CppWinRT_2.0.240111.5"
    VS_GLOBAL_WindowsPackageType "None"
    VS_GLOBAL_WindowsAppSDKSelfContained "true"
    VS_GLOBAL_AppxPackage "false"
)

target_include_directories(cs2importer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
