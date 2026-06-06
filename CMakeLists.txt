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
    # 1. 声明包引用和原生目标框架（保持不变，解决最开始的 NuGet 错误）
    set_property(TARGET cs2importer PROPERTY VS_PACKAGE_REFERENCES 
        "Microsoft.Windows.CppWinRT_3.0.260520.1" 
        "Microsoft.WindowsAppSDK_2.1.3"
    )
    set_property(TARGET cs2importer PROPERTY VS_GLOBAL_TargetFrameworkMoniker "native,Version=v0.0")

    # 2. 判断当前编译的架构
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(WIN_ARCH "x64")
    else()
        set(WIN_ARCH "x86")
    endif()

    # MSIX build dependencies and packaging configuration
    # Microsoft.WindowsAppRuntime.Bootstrap.lib is removed because it's only required for unpackaged apps
endif()

target_include_directories(cs2importer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Add custom target for MSIX packaging
if(WIN32)
    # Define paths for packaging
    set(PACKAGE_DIR "${CMAKE_BINARY_DIR}/MsixPackage")
    set(MSIX_OUTPUT "${CMAKE_BINARY_DIR}/cs2importer.msix")

    # Create the package directory structure and copy files after building the executable
    add_custom_command(
        TARGET cs2importer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PACKAGE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PACKAGE_DIR}/Assets"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:cs2importer>" "${PACKAGE_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/AppxManifest.xml" "${PACKAGE_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Assets" "${PACKAGE_DIR}/Assets"
        COMMENT "Copying files to MSIX staging directory"
    )

    # Provide a custom target to generate the actual MSIX file
    # This assumes makeappx.exe is available in the system PATH
    add_custom_target(msix
        COMMAND MakeAppx pack /d "${PACKAGE_DIR}" /p "${MSIX_OUTPUT}" /o
        DEPENDS cs2importer
        COMMENT "Building MSIX package ${MSIX_OUTPUT}"
    )
endif()
