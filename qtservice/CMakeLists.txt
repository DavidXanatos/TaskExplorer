#
# qtservice (Qt Solutions) - service/daemon wrapper. Upstream already ships a
# Unix backend (qtservice_unix.cpp + the qtunix*socket helpers); the old qmake
# project just never selected it because the build was Windows-only.
#

set(QTSERVICE_SOURCES
    src/qtservice.cpp
    src/qtservice.h
    src/qtservice_p.h
    src2/adminauthorization.cpp
    src2/adminauthorization.h
)

if(WIN32)
    list(APPEND QTSERVICE_SOURCES
        src/qtservice_win.cpp
        src2/adminauthorization_win.cpp
    )
else()
    list(APPEND QTSERVICE_SOURCES
        src/qtservice_unix.cpp
        src/qtunixserversocket.cpp
        src/qtunixserversocket.h
        src/qtunixsocket.cpp
        src/qtunixsocket.h
    )
    if(APPLE)
        list(APPEND QTSERVICE_SOURCES src2/adminauthorization_mac.cpp)
    else()
        list(APPEND QTSERVICE_SOURCES src2/adminauthorization_x11.cpp)
    endif()
endif()

add_library(qtservice STATIC ${QTSERVICE_SOURCES})

target_compile_definitions(qtservice PRIVATE QT_QTSERVICE_EXPORT)

target_include_directories(qtservice PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src2"
)

target_link_libraries(qtservice PUBLIC
    Qt6::Core
    Qt6::Network
    Qt6::Widgets
)

if(WIN32)
    target_link_libraries(qtservice PRIVATE ntdll advapi32 ole32 user32 shell32 gdi32)
endif()
