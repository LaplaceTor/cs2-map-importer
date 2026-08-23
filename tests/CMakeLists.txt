find_package(Qt6 6.8 REQUIRED COMPONENTS Core Test)

qt_add_executable(test_logmanager
    TestLogManager.cpp
)

target_link_libraries(test_logmanager PRIVATE
    cs2importer_core
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_logmanager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME test_logmanager COMMAND test_logmanager)

qt_add_executable(logging_test
    LoggingStressTest.cpp
)

target_link_libraries(logging_test PRIVATE
    cs2importer_core
    Qt6::Core
)

target_include_directories(logging_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME logging_test COMMAND logging_test)

qt_add_executable(test_keyvalues
    TestKeyValues.cpp
)

target_link_libraries(test_keyvalues PRIVATE
    cs2importer_core
    Qt6::Core
    Qt6::Test
)

target_include_directories(test_keyvalues PRIVATE
    ${CMAKE_SOURCE_DIR}/src
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
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(test_gameinfo PRIVATE
    "GAMEINFO_TEST_DIR=\"${CMAKE_SOURCE_DIR}/gameinfotestfile\""
)

add_test(NAME test_gameinfo COMMAND test_gameinfo)


