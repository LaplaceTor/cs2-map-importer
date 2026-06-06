cmake_minimum_required(VERSION 3.10)
project(cs2importer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

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
    # The WIN32 flag here tells CMake to create a Windows GUI application (no console window)
    add_executable(cs2importer WIN32 ${SOURCES})
else()
    add_executable(cs2importer ${SOURCES})
endif()

if(WIN32)
    set_property(TARGET cs2importer PROPERTY VS_PACKAGE_REFERENCES 
        "Microsoft.Windows.CppWinRT_3.0.260520.1" 
        "Microsoft.WindowsAppSDK_2.1.3"
    )
    set_property(TARGET cs2importer PROPERTY VS_GLOBAL_TargetFrameworkMoniker "native,Version=v0.0")

    # Enable Windows App SDK Self Contained/Bootstrap
    set_property(TARGET cs2importer PROPERTY VS_GLOBAL_WindowsPackageType "None")
    set_property(TARGET cs2importer PROPERTY VS_GLOBAL_WindowsAppSDKSelfContained "true")
endif()

target_include_directories(cs2importer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(WIN32)
    # The WindowsAppSDK nuget package properties handle the linking automatically
    # if we set WindowsPackageType and WindowsAppSDKSelfContained properties.
    # Alternatively, we can explicitly link Microsoft.WindowsAppRuntime.Bootstrap.lib
    # but the NuGet target properties are the recommended approach for CMake+WinAppSDK unpackaged apps.
    target_link_libraries(cs2importer PRIVATE "Microsoft.WindowsAppRuntime.Bootstrap.lib")
endif()
