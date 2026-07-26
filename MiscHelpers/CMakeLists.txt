#
# MiscHelpers - shared Qt widgets/utilities.
#
# Note on the Archive/ subtree: ArchiveHelper.cpp is a unity translation unit
# that #includes Archive.cpp, ArchiveExtractor.cpp, ArchiveInterface.cpp,
# ArchiveOpener.cpp and ArchiveUpdater.cpp, which is why those five are not
# listed separately (the Visual Studio project excluded them for the same
# reason). The whole subtree is a 7-Zip wrapper built on the Win32 COM shims in
# Archive/7z/CPP/Common/MyWindows.h and is Windows-only for now; on Linux it is
# left out and the two call sites in TaskExplorer are guarded instead.
#

set(MISCHELPERS_SOURCES
    MiscHelpers.cpp
    MiscHelpers.h
    mischelpers_global.h
    stdafx.cpp
    stdafx.h

    Common/CheckableMessageBox.cpp
    Common/CheckableMessageBox.h
    Common/CollapsibleGroupBox.cpp
    Common/CollapsibleGroupBox.h
    Common/ComboInputDialog.cpp
    Common/ComboInputDialog.h
    Common/Common.cpp
    Common/Compat.cpp
    Common/Compat.h
    Common/Common.h
    Common/CustomStyles.h
    Common/CustomTheme.cpp
    Common/CustomTheme.h
    Common/DebugHelpers.cpp
    Common/DebugHelpers.h
    Common/ExitDialog.h
    Common/Finder.cpp
    Common/Finder.h
    Common/FlexError.h
    Common/FlowLayout.cpp
    Common/FlowLayout.h
    Common/HistoryGraph.h
    Common/ItemChooser.cpp
    Common/ItemChooser.h
    Common/KeyValueInputDialog.cpp
    Common/KeyValueInputDialog.h
    Common/ListItemModel.cpp
    Common/ListItemModel.h
    Common/MT/ThreadLock.cpp
    Common/MultiLineInputDialog.cpp
    Common/MultiLineInputDialog.h
    Common/NetworkAccessManager.cpp
    Common/ObjectTracker.cpp
    Common/OtherFunctions.cpp
    Common/PanelView.cpp
    Common/PanelView.h
    Common/ProgressDialog.h
    Common/qRC4.cpp
    Common/qRC4.h
    Common/Settings.cpp
    Common/Settings.h
    Common/SettingsWidgets.cpp
    Common/SettingsWidgets.h
    Common/SmartGridWidget.cpp
    Common/SmartGridWidget.h
    Common/SortFilterProxyModel.h
    Common/SplitTreeView.cpp
    Common/SplitTreeView.h
    Common/TreeItemModel.cpp
    Common/TreeItemModel.h
    Common/TreeViewEx.h
    Common/TreeWidgetEx.h
    Common/Xml.cpp
    Common/Xml.h
)

if(WIN32)
    list(APPEND MISCHELPERS_SOURCES
        Archive/ArchiveFS.cpp
        Archive/ArchiveFS.h
        Archive/ArchiveHelper.cpp
        Archive/ArchiveThread.cpp
        Archive/CachedArchive.cpp
        Archive/SplitFile.cpp
    )
endif()

add_library(MiscHelpers STATIC ${MISCHELPERS_SOURCES})

target_compile_definitions(MiscHelpers PRIVATE MISCHELPERS_LIB)

target_include_directories(MiscHelpers PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(MiscHelpers PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    qextwidgets
)

if(WIN32)
    # ArchiveFS.h pulls in <private/qabstractfileengine_p.h>.
    te_target_qt_private_includes(MiscHelpers QtCore)
    target_link_libraries(MiscHelpers PRIVATE user32 shell32 oleaut32)
endif()

target_precompile_headers(MiscHelpers PRIVATE stdafx.h)
