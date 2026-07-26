cmake_minimum_required(VERSION 3.21)

project(TaskExplorer
    VERSION 1.8.0
    DESCRIPTION "TaskExplorer - an advanced task manager"
    LANGUAGES C CXX
)

#
# TaskExplorer was originally a Windows-only Visual Studio project. The build is
# being ported to CMake so that the same description drives both the Windows
# build and the new Linux port (API/Linux). The legacy .pro/.pri qmake files
# have been removed; the .vcxproj files remain as the reference for what the
# Windows build used to contain but are no longer the primary build.
#

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

#
# Platform identification. TE_PLATFORM is only used to name the output
# directory so that a Linux build does not collide with the Windows one.
#
if(WIN32)
    set(TE_PLATFORM "windows")
elseif(APPLE)
    set(TE_PLATFORM "macos")
elseif(UNIX)
    set(TE_PLATFORM "linux")
else()
    set(TE_PLATFORM "unknown")
endif()

set(TE_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/Bin/${TE_PLATFORM}-${CMAKE_SYSTEM_PROCESSOR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${TE_OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${TE_OUTPUT_DIR}")
#
# Static archives stay in the build tree. The output directory's lib/ folder is
# what gets shipped - bundled Qt and friends, see Installer/deploy-linux.sh -
# and mixing our build-time .a files into it only confuses matters.
#
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/archives")

#
# Self-contained Linux builds.
#
# The goal is one binary that runs across a wide range of distributions rather
# than a build per distro and per distro version. Two things are needed for
# that: the libraries that cannot be relied upon are shipped alongside the
# executable (Installer/deploy-linux.sh), and the executables look for them
# relative to themselves rather than at an absolute path on the build machine.
#
# $ORIGIN is that relative reference - the loader expands it to the directory
# holding the executable, so lib/ travels with the application wherever it is
# unpacked, with no wrapper script and no LD_LIBRARY_PATH.
#
# BUILD_WITH_INSTALL_RPATH makes this apply to the build output too, not just to
# an install tree, because the build output *is* what ships here. The side
# effect is that the binary stops finding the build machine's Qt, which is the
# point: if it runs, it is genuinely self-contained.
#
option(TE_BUNDLE_QT "Bundle Qt and other runtime libraries into the output directory" ON)

if(UNIX AND NOT APPLE AND TE_BUNDLE_QT)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/lib")

    #
    # Emit the old DT_RPATH rather than DT_RUNPATH, which is what
    # --disable-new-dtags means.
    #
    # The difference matters here: DT_RUNPATH applies only to the object that
    # carries it, while DT_RPATH is inherited by everything further down the
    # dependency chain. Several bundled libraries depend on other bundled
    # libraries and carry no search path of their own - libdw needs libelf,
    # libxkbcommon-x11 needs libxkbcommon, libxcb-image needs libxcb-util. With
    # DT_RUNPATH those inner dependencies are resolved without our lib/ in the
    # search path, so they quietly come from the system instead, and the bundle
    # only turns out to be incomplete on a machine that lacks them - exactly the
    # failure this is all meant to prevent.
    #
    # The alternative is rewriting each copied library's RUNPATH with patchelf,
    # which would add a build dependency that is not installed everywhere.
    #
    add_link_options(-Wl,--disable-new-dtags)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

#
# The helper libraries are built statically. Every one of them guards its
# Q_DECL_EXPORT/Q_DECL_IMPORT macros behind BUILD_STATIC, so defining it
# globally turns the export macros into no-ops - which is what a static build
# needs on Windows and is harmless everywhere else.
#
add_compile_definitions(BUILD_STATIC)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Concurrent
    PrintSupport
    Svg
    OpenGL
    OpenGLWidgets
    LinguistTools
)

#
# Qt private headers.
#
# qextwidgets subclasses QTabBar/QTabWidget and includes private/qwidget_p.h,
# private/qtextengine_p.h and private/qlayoutengine_p.h. MiscHelpers' 7-Zip
# file engine needs private/qabstractfileengine_p.h.
#
# The obvious way to get these is the Qt6::CorePrivate / GuiPrivate /
# WidgetsPrivate targets. On Linux, though, Qt6GuiPrivateConfig.cmake performs a
# hard find_dependency(XKB) and refuses to load without libxkbcommon-dev
# installed - even though the headers themselves ship with the Qt toolchain and
# nothing here needs to link against xkbcommon. The Windows build has no such
# dependency, which is why this never came up under Visual Studio.
#
# Pointing at the include directories directly gives the same result without
# dragging in an extra system package.
#
get_filename_component(TE_QT_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)
set(TE_QT_INCLUDE_DIR "${TE_QT_PREFIX}/include")

# Adds the private include paths for the named Qt modules (Core, Gui, Widgets)
# to a target. Both levels are needed: <QtGui/private/x_p.h> resolves against
# the version directory, plain <private/x_p.h> against the module directory
# inside it.
function(te_target_qt_private_includes target)
    set(_dirs "")
    foreach(_module IN LISTS ARGN)
        set(_versioned "${TE_QT_INCLUDE_DIR}/${_module}/${Qt6_VERSION}")
        if(NOT IS_DIRECTORY "${_versioned}")
            message(FATAL_ERROR
                "Qt private headers for ${_module} not found at ${_versioned}. "
                "Check that the Qt installation at ${TE_QT_PREFIX} is complete.")
        endif()
        list(APPEND _dirs "${_versioned}" "${_versioned}/${_module}")
    endforeach()
    target_include_directories(${target} PUBLIC ${_dirs})
endfunction()

if(UNIX AND NOT APPLE)
    # The Linux service backend talks to systemd over D-Bus.
    find_package(Qt6 REQUIRED COMPONENTS DBus)

    #
    # X11 is optional. It backs the window list, which is enumerated through
    # the EWMH protocol on the root window.
    #
    # Without it TaskExplorer still builds and runs; the window views are just
    # empty - which is also what happens at runtime under a Wayland session,
    # since Wayland deliberately provides no way for one client to enumerate
    # another client's windows.
    #
    find_package(X11)
    if(X11_FOUND)
        set(TE_HAVE_X11 ON)
        message(STATUS "TaskExplorer: X11 found - window enumeration enabled")
    else()
        message(STATUS "TaskExplorer: X11 not found - window views will be empty")
    endif()
endif()

qt_standard_project_setup()

#
# MSVC needs a couple of legacy switches that the original project set.
#
if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _UNICODE UNICODE _ENABLE_EXTENDED_ALIGNED_STORAGE)
    add_compile_options(/MP /bigobj)
else()
    # The codebase leans on a number of constructs GCC/Clang are noisy about but
    # which are intentional; keep the signal-to-noise ratio usable during the port.
    add_compile_options(
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-unused-function
        -Wno-reorder
        -Wno-switch
        -fPIC
    )
endif()

add_subdirectory(qextwidgets)
add_subdirectory(qhexedit)
add_subdirectory(qwt)
add_subdirectory(qtservice)
add_subdirectory(qtsingleapp)
add_subdirectory(MiscHelpers)
add_subdirectory(TaskExplorer)

# The privileged worker. Qt-free, built only on Linux; see TaskHelper/CMakeLists.txt.
add_subdirectory(TaskHelper)

message(STATUS "TaskExplorer: platform=${TE_PLATFORM} output=${TE_OUTPUT_DIR}")
