cmake_minimum_required(VERSION 3.20)

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0.19041.0)

project(cs2importer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# C++/WinRT and WinUI 3 require /await or /std:c++20 coroutines
add_compile_options(/await)

# Define source files
set(SOURCES
    src/main.cpp
    src/appcore.cpp
    src/appcore.h
    src/mapimporter.cpp
    src/mapimporter.h
    src/pch.h
    src/pch.cpp
    src/App.xaml
    src/App.xaml.cpp
    src/App.xaml.h
    src/App.idl
    src/MainWindow.xaml
    src/MainWindow.xaml.cpp
    src/MainWindow.xaml.h
    src/MainWindow.idl
    src/res/app.rc
    app.manifest
)

add_executable(cs2importer WIN32 ${SOURCES})

# Windows App SDK and C++/WinRT NuGet references for Visual Studio generator
set_target_properties(cs2importer PROPERTIES
    VS_PACKAGE_REFERENCES "Microsoft.WindowsAppSDK_1.4.231219000;Microsoft.Windows.CppWinRT_2.0.230706.1"
    VS_GLOBAL_KEYWORD "Win32Proj"
    VS_GLOBAL_ROOTNAMESPACE "cs2importer"
    VS_GLOBAL_PROJECT_TYPES "8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942"
    VS_GLOBAL_DefaultLanguage "en-US"
    VS_GLOBAL_TargetFramework "native10.0"


    VS_GLOBAL_TargetPlatformVersion "10.0.19041.0"
    VS_GLOBAL_TargetPlatformMinVersion "10.0.17763.0"
    VS_GLOBAL_AppxPackage "false"
    VS_GLOBAL_WindowsPackageType "None"
    VS_GLOBAL_WindowsAppSDKSelfContained "true"
    VS_DPI_AWARE "PerMonitor"
)

# Set precompiled headers
target_precompile_headers(cs2importer PRIVATE src/pch.h)

# Define symbols
target_compile_definitions(cs2importer PRIVATE
    WIN32
    _WINDOWS
    UNICODE
    _UNICODE
    _WINRT_DLL
    WINRT_LEAN_AND_MEAN
)

target_include_directories(cs2importer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Configure the manifest


# Set App.xaml as ApplicationDefinition
set_property(SOURCE src/App.xaml PROPERTY VS_XAML_TYPE "ApplicationDefinition")

# Add the app manifest correctly
set_target_properties(cs2importer PROPERTIES
    VS_DPI_AWARE "PerMonitor"
    VS_GLOBAL_ApplicationManifest "app.manifest"
)
