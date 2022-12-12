################################################################
# Qwt Widget Library
# Copyright (C) 1997   Josef Wilgen
# Copyright (C) 2002   Uwe Rathmann
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the Qwt License, Version 1.0
################################################################

######################################################################
# qmake internal options
######################################################################

CONFIG           += qt
CONFIG           += warn_on
CONFIG           += no_keywords
CONFIG           += silent
CONFIG           -= depend_includepath

# CONFIG += sanitize
# CONFIG += pedantic

# older Qt headers result in tons of warnings with modern compilers and flags
unix:lessThan(QT_MAJOR_VERSION, 5) CONFIG += qtsystemincludes

# CONFIG += c++11

c++11 {
    CONFIG           += strict_c++
}

sanitize {

    CONFIG += sanitizer
    CONFIG += sanitize_address
    #CONFIG *= sanitize_memory
    CONFIG *= sanitize_undefined
}

# Include the generated moc files in the corresponding cpp file
# what increases the compile time significantly

DEFINES += QWT_MOC_INCLUDE=1
# DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000

######################################################################
# release/debug mode
######################################################################

win32 {
    # On Windows you can't mix release and debug libraries.
    # The designer is built in release mode. If you like to use it
    # you need a release version. For your own application development you
    # might need a debug version.
    # Enable debug_and_release + build_all if you want to build both.

    CONFIG           += debug_and_release
    CONFIG           += build_all
}
else {

    CONFIG           += release

    VER_MAJ           = $${QWT_VER_MAJ}
    VER_MIN           = $${QWT_VER_MIN}
    VER_PAT           = $${QWT_VER_PAT}
    VERSION           = $${QWT_VERSION}
}

linux {

    pedantic {

        DEFINES += QT_STRICT_ITERATORS
        # DEFINES += __STRICT_ANSI__

        # Qt headers do not stand pedantic checks, so it's better
        # to exclude them by declaring them as system includes

        QMAKE_CXXFLAGS += \
            -isystem $$[QT_INSTALL_HEADERS] \
            -isystem $$[QT_INSTALL_HEADERS]/QtCore \
            -isystem $$[QT_INSTALL_HEADERS]/QtGui \
            -isystem $$[QT_INSTALL_HEADERS]/QtWidgets
    }

    linux-g++ | linux-g++-64 {

        # CONFIG           += separate_debug_info

        sanitize_undefined {
        
            GCC_VERSION = $$system("$$QMAKE_CXX -dumpversion")
            equals(GCC_VERSION,4) || contains(GCC_VERSION, 4.* ) {

                CONFIG -= sanitize_undefined
            }
        }

        c++11 {
            QMAKE_CXXFLAGS *= -Wsuggest-override
        }

        pedantic {

            CONFIG -= warn_on

            QMAKE_CXXFLAGS *= -Wall
            QMAKE_CXXFLAGS *= -Wextra
            QMAKE_CXXFLAGS *= -Wpedantic

            QMAKE_CXXFLAGS   *= -Wcast-qual
            QMAKE_CXXFLAGS   *= -Wcast-align
            QMAKE_CXXFLAGS   *= -Wlogical-op
            QMAKE_CXXFLAGS   *= -Wredundant-decls
            QMAKE_CXXFLAGS   *= -Wformat
            QMAKE_CXXFLAGS   *= -Wshadow 
            QMAKE_CXXFLAGS   *= -Woverloaded-virtual

            # checks qwt code does not pass, but should be able to
            # QMAKE_CXXFLAGS   *= -Wconversion 

            # checks qwt code does not pass

            # QMAKE_CXXFLAGS   *= -Wuseless-cast
            # QMAKE_CXXFLAGS   *= -Wmissing-declarations
            # QMAKE_CXXFLAGS   *= -Winline
            # QMAKE_CXXFLAGS   *= -Wdouble-promotion
            # QMAKE_CXXFLAGS   *= -Wfloat-equal 
            # QMAKE_CXXFLAGS   *= -Wpadded
            # QMAKE_CXXFLAGS   *= -Waggregate-return
            # QMAKE_CXXFLAGS   *= -Wzero-as-null-pointer-constant
        }

        # --- optional optimzations

        # qwt code doesn't check errno after calling math functions
        # so it is perfectly safe to disable it in favor of better performance
        QMAKE_CXXFLAGS   *= -fno-math-errno

        # qwt code doesn't rely ( at least intends not to do )
        # on an exact implementation of IEEE or ISO rules/specifications
        QMAKE_CXXFLAGS   *= -funsafe-math-optimizations

        # also enables -fno-math-errno and -funsafe-math-optimizations
        # QMAKE_CXXFLAGS   *= -ffast-math

        # QMAKE_CXXFLAGS_DEBUG  *= -Og # since gcc 4.8

        # QMAKE_CXXFLAGS_RELEASE  *= -O3
        # QMAKE_CXXFLAGS_RELEASE  *= -Ofast
        # QMAKE_CXXFLAGS_RELEASE  *= -Os

        # when using the gold linker ( Qt < 4.8 ) - might be
        # necessary on non linux systems too
        #QMAKE_LFLAGS += -lrt

        sanitize {

            # QMAKE_CXXFLAGS *= -fsanitize-address-use-after-scope
            # QMAKE_LFLAGS *= -fsanitize-address-use-after-scope
        }
    }

    linux-clang {

        # workaround for a clang 3.8 bug
        # DEFINES += __STRICT_ANSI__

        # QMAKE_CXXFLAGS_RELEASE  *= -O3
    }

    # QMAKE_CXXFLAGS   *= -Werror
}

qtsystemincludes {
    
    # mark Qt directories as a system directories - usually to get rid
    # of compiler warnings in Qt headers of old Qt versions
    # when being built with modern compilers

    QMAKE_CXXFLAGS += \
        -isystem $$[QT_INSTALL_HEADERS]/QtCore \
        -isystem $$[QT_INSTALL_HEADERS]/QtGui
}

######################################################################
# paths for building qwt
######################################################################

MOC_DIR      = moc
RCC_DIR      = resources

!debug_and_release {

    # in case of debug_and_release object files
    # are built in the release and debug subdirectories
    OBJECTS_DIR       = obj
}
