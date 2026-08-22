qt_add_library(cs2importer_qml STATIC)

qt_add_qml_module(cs2importer_qml
    URI cs2importer
    VERSION 1.0
    QML_FILES
        Main.qml
)

target_link_libraries(cs2importer_qml PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)
