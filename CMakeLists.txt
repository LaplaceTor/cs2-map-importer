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

# We need C++/WinRT and WinUI 3 dependencies, usually added via NuGet in VS or through Windows SDKs
# CMake supports VS_PACKAGE_REFERENCES to pull NuGet packages into the generated vcxproj
if(WIN32)
    set_property(TARGET cs2importer PROPERTY VS_PACKAGE_REFERENCES "Microsoft.Windows.CppWinRT_2.0.221121.5" "Microsoft.WindowsAppSDK_1.4.230913002")


    # Workaround for MSBuild Sequence contains no elements bug
    set_property(TARGET cs2importer PROPERTY VS_GLOBAL_TargetFramework "native")

    # We must enable exception handling to use C++/WinRT exceptions smoothly
    target_compile_options(cs2importer PRIVATE /EHsc)

    # Link the Windows App SDK bootstrap library
    target_link_libraries(cs2importer PRIVATE Microsoft.WindowsAppSDK.Bootstrap)
endif()

target_include_directories(cs2importer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
