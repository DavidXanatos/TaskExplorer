#
# qtsingleapp (Qt Solutions) - single-instance application guard.
#
# The old qtsingleapp.pri omitted qtlockedfile.cpp and both of its platform
# variants entirely, so the qmake build could never have linked. All four are
# listed here.
#

set(QTSINGLEAPP_SOURCES
    src/qtlocalpeer.cpp
    src/qtlocalpeer.h
    src/qtlockedfile.cpp
    src/qtlockedfile.h
    src/qtsingleapplication.cpp
    src/qtsingleapplication.h
    src/qtsinglecoreapplication.cpp
    src/qtsinglecoreapplication.h
)

if(WIN32)
    list(APPEND QTSINGLEAPP_SOURCES src/qtlockedfile_win.cpp)
else()
    list(APPEND QTSINGLEAPP_SOURCES src/qtlockedfile_unix.cpp)
endif()

add_library(qtsingleapp STATIC ${QTSINGLEAPP_SOURCES})

target_compile_definitions(qtsingleapp PRIVATE QT_QTSINGLEAPPLICATION_EXPORT)

target_include_directories(qtsingleapp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(qtsingleapp PUBLIC
    Qt6::Core
    Qt6::Network
    Qt6::Widgets
)
