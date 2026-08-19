qt_add_executable(core_logging_test
    CoreLoggingTest/tst_CoreLogging.cpp
)

target_link_libraries(core_logging_test PRIVATE
    cs2importer_core
    Qt6::Core
    Qt6::Test
)

target_include_directories(core_logging_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
