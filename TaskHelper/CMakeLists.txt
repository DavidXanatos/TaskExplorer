#
# TaskHelper - the privileged worker TaskExplorer spawns on demand.
#
# Deliberately has no Qt dependency: it is small, it may be launched through
# pkexec as root, and it must keep working in contexts where a Qt runtime is not
# usable. The wire protocol is shared with the GUI through the STL-only CVariant
# in TaskExplorer/Common, which compiles here because TaskHelper's own Qt-free
# stdafx.h comes first on the include path.
#
# The Windows helper (Main.cpp) is not built here; it is Windows-only and still
# built by TaskHelper.vcxproj.
#

if(NOT UNIX OR APPLE)
    return()
endif()

add_executable(TaskHelper
    LinuxMain.cpp

    # Shared, STL-only serialisation. Same sources the GUI uses, so the two
    # cannot drift apart in wire format.
    ../TaskExplorer/Common/Variant.cpp
    ../TaskExplorer/Common/Buffer.cpp

    # The Qt-free platform shim Exception.h needs.
    ../MiscHelpers/Common/Compat.cpp
)

#
# This directory first, so that the "stdafx.h" included by Variant.cpp and
# Buffer.cpp resolves to TaskHelper's Qt-free one rather than TaskExplorer's
# Qt-heavy header.
#
target_include_directories(TaskHelper PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/TaskExplorer
)

#
# libdw (elfutils) for stack unwinding. A stable dependency: SONAME libdw.so.1
# with symbol versioning since 2007, so this does not fragment builds across
# distributions.
#
find_library(TE_LIBDW NAMES dw libdw REQUIRED)
find_library(TE_LIBELF NAMES elf libelf REQUIRED)
target_link_libraries(TaskHelper PRIVATE ${TE_LIBDW} ${TE_LIBELF})

set_target_properties(TaskHelper PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${TE_OUTPUT_DIR}"
)
