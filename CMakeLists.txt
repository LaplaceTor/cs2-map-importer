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

    # 3. 收集所有可能的 NuGet 根目录（本地、GitHub Actions、自定义缓存）
    if(DEFINED ENV{NUGET_PACKAGES})
        set(NUGET_ROOT "$ENV{ENV_NUGET_PACKAGES}")
    elseif(DEFINED ENV{USERPROFILE})
        set(NUGET_ROOT "$ENV{USERPROFILE}/.nuget/packages")
    else()
        set(NUGET_ROOT "C:/Users/runneradmin/.nuget/packages")
    endif()

    message(STATUS "--- 开始模糊匹配任意版本的 Bootstrap 引导库 ---")
    message(STATUS "正在检索目录: ${NUGET_ROOT}")

    # 4. 全局通配符地毯式搜索：去掉所有版本号硬编码！
    # 只要文件夹名字包含 microsoft.windowsappsdk，不管是 main 还是 foundation，全都检索
    file(GLOB_RECURSE ALL_FOUND_LIBS 
         "${NUGET_ROOT}/microsoft.windowsappsdk*/Microsoft.WindowsAppRuntime.Bootstrap.lib"
         "${CMAKE_CURRENT_SOURCE_DIR}/packages/Microsoft.WindowsAppSDK*/Microsoft.WindowsAppRuntime.Bootstrap.lib"
    )

    # 5. 过滤出符合当前编译架构（如 x64 或 win10-x64）的所有可用库
    set(MATCHED_LIBS "")
    foreach(LIB_PATH ${ALL_FOUND_LIBS})
        if(LIB_PATH MATCHES "/${WIN_ARCH}/" OR LIB_PATH MATCHES "/win10-${WIN_ARCH}/")
            list(APPEND MATCHED_LIBS "${LIB_PATH}")
        endif()
    endforeach()

    # 6. 如果找到了多个版本（比如本地积攒了多个历史版本的 NuGet 包），自动选择最后一个（通常是最新版本）
    if(MATCHED_LIBS)
        list(LENGTH MATCHED_LIBS LIBS_COUNT)
        math(EXPR LAST_INDEX "${LIBS_COUNT} - 1")
        list(GET MATCHED_LIBS ${LAST_INDEX} BOOTSTRAP_LIB_PATH)
        
        message(STATUS "【成功】通过模糊匹配找到目标引导库: ${BOOTSTRAP_LIB_PATH}")
        
        # 提取该文件所在的目录并传给链接器
        get_filename_component(BOOTSTRAP_DIR "${BOOTSTRAP_LIB_PATH}" DIRECTORY)
        target_link_directories(cs2importer PRIVATE "${BOOTSTRAP_DIR}")
        
        # 链接核心库文件
        target_link_libraries(cs2importer PRIVATE Microsoft.WindowsAppRuntime.Bootstrap.lib)
    else()
        # 彻底找不到时的友好报错提示
        message(FATAL_ERROR "【致命错误】在任何版本的 SDK 或 Foundation 文件夹中都未找到 Microsoft.WindowsAppRuntime.Bootstrap.lib！\n"
        "请确保你在流水线中执行了正确的 nuget restore / dotnet restore。\n"
        "或者在本地执行了安装命令以生成依赖文件夹。")
    endif()
endif()

target_include_directories(cs2importer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
