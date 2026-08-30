cmake_minimum_required(VERSION 3.28)
project(cs2importer_tests VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network Test)

qt_standard_project_setup(REQUIRES 6.8)

enable_testing()

if(NOT TARGET cs2importer_core)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/Core ${CMAKE_CURRENT_BINARY_DIR}/src/Core)
endif()
if(NOT TARGET cs2importer_domain)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/Domain ${CMAKE_CURRENT_BINARY_DIR}/src/Domain)
endif()
if(NOT TARGET cs2importer_application)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/Application ${CMAKE_CURRENT_BINARY_DIR}/src/Application)
endif()
if(NOT TARGET cs2importer_ui)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../src/UI ${CMAKE_CURRENT_BINARY_DIR}/src/UI)
endif()

set(GAMEINFO_TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../gameinfotestfile")
set(SOUNDSCRIPTS_TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../source1soundscripts")

qt_add_executable(test_logmanager
    TestLogManager.cpp
)

target_link_libraries(test_logmanager PRIVATE
    cs2importer_core
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_logmanager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(NAME test_logmanager COMMAND test_logmanager)

qt_add_executable(test_logging_stress
    LoggingStressTest.cpp
)

target_link_libraries(test_logging_stress PRIVATE
    cs2importer_core
    Qt6::Core
)

target_include_directories(test_logging_stress PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(NAME test_logging_stress COMMAND test_logging_stress)

qt_add_executable(test_keyvalues
    TestKeyValues.cpp
)

target_link_libraries(test_keyvalues PRIVATE
    cs2importer_core
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_keyvalues PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(NAME test_keyvalues COMMAND test_keyvalues)

qt_add_executable(test_gameinfo
    TestGameInfo.cpp
)

target_link_libraries(test_gameinfo PRIVATE
    cs2importer_domain
    cs2importer_core
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_gameinfo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_compile_definitions(test_gameinfo PRIVATE
    "GAMEINFO_TEST_DIR=\"${GAMEINFO_TEST_DIR}\""
)

add_test(NAME test_gameinfo COMMAND test_gameinfo)

qt_add_executable(test_asset
    TestAsset.cpp
)

target_link_libraries(test_asset PRIVATE
    cs2importer_domain
    cs2importer_core
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_asset PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(NAME test_asset COMMAND test_asset)

qt_add_executable(test_environment
    TestEnvironment.cpp
)

target_link_libraries(test_environment PRIVATE
    cs2importer_application
    cs2importer_domain
    cs2importer_core
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_environment PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_compile_definitions(test_environment PRIVATE
    "GAMEINFO_TEST_DIR=\"${GAMEINFO_TEST_DIR}\""
)

add_test(NAME test_environment COMMAND test_environment)

qt_add_executable(test_uiviewmodels
    TestUiViewModels.cpp
)

target_link_libraries(test_uiviewmodels PRIVATE
    cs2importer_ui
    cs2importer_application
    cs2importer_domain
    cs2importer_core
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Test
)

target_include_directories(test_uiviewmodels PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_compile_definitions(test_uiviewmodels PRIVATE
    "GAMEINFO_TEST_DIR=\"${GAMEINFO_TEST_DIR}\""
)

add_test(NAME test_uiviewmodels COMMAND test_uiviewmodels)

qt_add_executable(test_filelease
    TestFileLease.cpp
)

target_link_libraries(test_filelease PRIVATE
    cs2importer_application
    cs2importer_domain
    cs2importer_core
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_filelease PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(NAME test_filelease COMMAND test_filelease)

qt_add_executable(test_async_task_logging
    TestAsyncTaskLogging.cpp
)

target_link_libraries(test_async_task_logging PRIVATE
    cs2importer_ui
    cs2importer_application
    cs2importer_domain
    cs2importer_core
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)

target_include_directories(test_async_task_logging PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(NAME test_async_task_logging COMMAND test_async_task_logging)

qt_add_executable(test_error
    TestError.cpp
)

target_link_libraries(test_error PRIVATE
    cs2importer_domain
    cs2importer_core
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_error PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(NAME test_error COMMAND test_error)

qt_add_executable(test_logging_infrastructure
    TestLoggingInfrastructure.cpp
)

target_link_libraries(test_logging_infrastructure PRIVATE
    cs2importer_ui
    cs2importer_application
    cs2importer_domain
    cs2importer_core
    Qt6::Core
    Qt6::Gui
    Qt6::Test
)

target_include_directories(test_logging_infrastructure PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(NAME test_logging_infrastructure COMMAND test_logging_infrastructure)

qt_add_executable(test_soundscape_domain
    TestSoundscapeDomain.cpp
)

target_link_libraries(test_soundscape_domain PRIVATE
    cs2importer_domain
    cs2importer_core
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_soundscape_domain PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_compile_definitions(test_soundscape_domain PRIVATE
    "SOUNDSCRIPTS_TEST_DIR=\"${SOUNDSCRIPTS_TEST_DIR}\""
)

add_test(NAME test_soundscape_domain COMMAND test_soundscape_domain)

qt_add_executable(test_soundscape_application
    TestSoundscapeApplication.cpp
)

target_link_libraries(test_soundscape_application PRIVATE
    cs2importer_application
    cs2importer_domain
    cs2importer_core
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_soundscape_application PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(NAME test_soundscape_application COMMAND test_soundscape_application)




