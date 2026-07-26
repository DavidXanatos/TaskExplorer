#
# qwt 6.3.0 - vendored unmodified. The upstream qmake build is left in place
# (qwt.pro / src.pri and friends); this CMakeLists only describes the subset
# TaskExplorer links against, which is "all of src/".
#
# CONFIGURE_DEPENDS makes CMake re-glob when the directory contents change, so
# the usual objection to globbing does not apply here.
#

file(GLOB QWT_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB QWT_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")

add_library(qwt STATIC ${QWT_SOURCES} ${QWT_HEADERS})

# QWT_MOC_INCLUDE makes qwt's headers moc-friendly. QWT_DLL/QWT_MAKEDLL are
# deliberately not set - this is a static build, so QWT_EXPORT stays empty.
target_compile_definitions(qwt PUBLIC QWT_MOC_INCLUDE)

target_include_directories(qwt PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(qwt PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::PrintSupport
    Qt6::Svg
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

if(WIN32)
    target_link_libraries(qwt PRIVATE user32 shell32 oleaut32)
endif()
