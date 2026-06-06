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
    # 1. Use VS_PACKAGE_REFERENCES to handle headers and general build props
    set_property(TARGET cs2importer PROPERTY VS_PACKAGE_REFERENCES 
        "Microsoft.Windows.CppWinRT_3.0.260520.1" 
        "Microsoft.WindowsAppSDK_2.1.3"
    )

    # 2. Prevent the "Sequence contains no elements" NuGet error
    set_property(TARGET cs2importer PROPERTY VS_GLOBAL_TargetFrameworkMoniker "native,Version=v0.0")

    # 4. robustly find the NuGet root (Handles GitHub Actions override)
    if(DEFINED ENV{NUGET_PACKAGES})
        set(NUGET_ROOT "$ENV{NUGET_PACKAGES}")
    elseif(DEFINED ENV{USERPROFILE})
        set(NUGET_ROOT "$ENV{USERPROFILE}/.nuget/packages")
    else()
        set(NUGET_ROOT "C:/Users/runneradmin/.nuget/packages")
    endif()

    set(BOOTSTRAP_LIB_PATH "${NUGET_ROOT}/microsoft.windowsappsdk/2.1.3/")

    target_link_directories(cs2importer PRIVATE "${BOOTSTRAP_LIB_PATH}")

    # 7. Link the library (Ensure the name matches the file in that directory)
    target_link_libraries(cs2importer PRIVATE Microsoft.WindowsAppRuntime.Bootstrap.lib)
endif()

target_include_directories(cs2importer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
