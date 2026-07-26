#
# qextwidgets - QTabBar/QTabWidget subclasses that reach into Qt widget
# internals, hence the *Private Qt modules.
#

add_library(qextwidgets STATIC
    qtabbarex.cpp
    qtabbarex.h
    qtabbarex_p.h
    qtabwidgetex.cpp
    qtabwidgetex.h
    qtabwidgetex_p.h
    qextwidgets_global.h
)

target_compile_definitions(qextwidgets PRIVATE QEXTWIDGETS_LIB)

#
# Q_DECLARE_PRIVATE means the generated moc code dereferences QTabBarExPrivate /
# QTabWidgetExPrivate, so moc's output has to see their full definitions - the
# public headers only forward declare them. moc's -f option forces the extra
# #include into every file it generates for this target.
#
# The old qmake project did the same thing via:
#   QMAKE_MOC_OPTIONS += -f ../../qextwidgets/qtabbarex_p.h
#   QMAKE_MOC_OPTIONS += -f ../../qextwidgets/qtabwidgetex_p.h
#
set_property(TARGET qextwidgets PROPERTY AUTOMOC_MOC_OPTIONS
    "-f${CMAKE_CURRENT_SOURCE_DIR}/qtabbarex_p.h"
    "-f${CMAKE_CURRENT_SOURCE_DIR}/qtabwidgetex_p.h"
)

target_include_directories(qextwidgets PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(qextwidgets PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

te_target_qt_private_includes(qextwidgets QtCore QtGui QtWidgets)

if(WIN32)
    target_link_libraries(qextwidgets PRIVATE ntdll advapi32 ole32 user32 shell32 gdi32)
endif()
