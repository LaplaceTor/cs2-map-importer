find_package(Qt6 REQUIRED COMPONENTS Core Test)

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
