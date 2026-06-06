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

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(WIN_ARCH "win10-x64")
    else()
        set(WIN_ARCH "win10-x86")
    endif()

    if(DEFINED ENV{USERPROFILE})
        set(NUGET_ROOT "$ENV{USERPROFILE}/.nuget/packages")
    else()
        set(NUGET_ROOT "C:/Users/runneradmin/.nuget/packages") # Fallback for GitHub Actions
    endif()

    set(BOOTSTRAP_LIB_PATH "${NUGET_ROOT}/microsoft.windowsappsdk/2.1.3/build/native/lib/${WIN_ARCH}")

    target_link_directories(cs2importer PRIVATE "${BOOTSTRAP_LIB_PATH}")

    target_link_libraries(cs2importer PRIVATE Microsoft.WindowsAppRuntime.Bootstrap.lib)
endif()

target_include_directories(cs2importer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
