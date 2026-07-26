#
# qhexedit - the hex editor widget plus the editor front-end that TaskExplorer's
# memory editor is built on. Sources live in src/ and editor/.
#

add_library(qhexedit STATIC
    src/chunks.cpp
    src/chunks.h
    src/commands.cpp
    src/commands.h
    src/qhexedit.cpp
    src/qhexedit.h
    src/qhexedit_global.h

    editor/HexEditor.cpp
    editor/HexEditor.h
    editor/HexEditorOptions.cpp
    editor/HexEditorOptions.h
    editor/HexEditorSearch.cpp
    editor/HexEditorSearch.h

    editor/Resource/HexEditorOptions.ui
    editor/Resource/HexEditorSearch.ui
    editor/Resource/qhexedit.qrc
)

target_compile_definitions(qhexedit PRIVATE QHEXEDIT_EXPORTS)

target_include_directories(qhexedit PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/editor"
)

# AUTOUIC needs to know where the .ui files are so ui_*.h resolve.
set_property(TARGET qhexedit PROPERTY AUTOUIC_SEARCH_PATHS
    "${CMAKE_CURRENT_SOURCE_DIR}/editor/Resource")

target_link_libraries(qhexedit PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

if(WIN32)
    target_link_libraries(qhexedit PRIVATE ntdll advapi32 ole32 user32 shell32 gdi32)
endif()
