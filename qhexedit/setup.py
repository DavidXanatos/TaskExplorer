#!/usr/bin/env python

from distutils.core import setup, Extension
from distutils import log

import os
import subprocess
import sipdistutils
import sipconfig

cfg = sipconfig.Configuration()
pyqt_sip_dir = cfg.default_sip_dir

try:
    import PyQt5
    PyQt_Version = 'PyQt5'
except:
    PyQt_Version = 'PyQt4'


include_dirs = ['src']


class build_pyqt_ext(sipdistutils.build_ext):
    description = "Build a qhexedit PyQt extension."

    user_options = sipdistutils.build_ext.user_options + [(
        "required",
        None,
        "qhexedit is required (failure to build will raise an error)"
    )]

    boolean_options = sipdistutils.build_ext.boolean_options + ["required"]

    def initialize_options(self):
        sipdistutils.build_ext.initialize_options(self)
        self.required = False

    def finalize_options(self):
        if PyQt_Version == 'PyQt5':
            from PyQt5.QtCore import PYQT_CONFIGURATION
        else:
            from PyQt4.QtCore import PYQT_CONFIGURATION
        sipdistutils.build_ext.finalize_options(self)
        self.sip_opts = self.sip_opts + PYQT_CONFIGURATION['sip_flags'].split()
        self.sip_opts.append('-I%s/%s' % (pyqt_sip_dir, PyQt_Version))
        if self.required is not None:
            self.required = True

    def build_extension(self, ext):
        cppsources = (s for s in ext.sources if s.endswith(".cpp"))
        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)
        for source in cppsources:
            header = source.replace(".cpp", ".h")
            if os.path.exists(header):
                moc_file = os.path.basename(header).replace(".h", ".moc")
                call_arg = (
                    "moc",
                    "-o",
                    os.path.join(self.build_temp, moc_file),
                    header
                )
                log.info("Calling: " + " ".join(call_arg))
                try:
                    subprocess.call(call_arg)
                except OSError:
                    raise OSError(
                        "Could not locate 'moc' executable."
                    )
        sipdistutils.build_ext.build_extension(self, ext)

    def run(self):
        try:
            sipdistutils.build_ext.run(self)
        except Exception as ex:
            if self.required:
                raise
            else:
                log.info("Could not build qhexedit extension (%r)\n"
                         "Skipping." % ex)

    # For sipdistutils to find PyQt's .sip files
    def _sip_sipfiles_dir(self):
        return pyqt_sip_dir


# Used Qt libs
if PyQt_Version == 'PyQt5':
    qt_libs = ["QtCore", "QtGui", "QtWidgets"]
else:
    qt_libs = ["QtCore", "QtGui"]


if cfg.qt_framework:
    for lib in qt_libs:
        include_dirs += [os.path.join(cfg.qt_lib_dir,
                                      lib + ".framework", "Headers")]
else:
    if PyQt_Version == 'PyQt5':
        for qt_inc_dir in ('/usr/include/qt', '/usr/include/x86_64-linux-gnu/qt5'):
            include_dirs.append(qt_inc_dir)
            include_dirs += [os.path.join(qt_inc_dir, lib) for lib in qt_libs]
        libraries = ["Qt5" + lib[2:] for lib in qt_libs]
    else:
        for qt_inc_dir in ('/usr/include/qt', '/usr/include/qt4'):
            include_dirs.append(qt_inc_dir)
            include_dirs += [os.path.join(qt_inc_dir, lib) for lib in qt_libs]
        libraries = ["Qt" + lib[2:] for lib in qt_libs]

libraries.append("qhexedit")

dirname = os.path.dirname(__file__)

setup(
    name='QHexEdit',
    version='0.8.6',
    ext_modules=[
        Extension(
            "qhexedit",
            sources=[
                os.path.join(dirname, "src/qhexedit.sip"),
            ],
            include_dirs=include_dirs,
            libraries=libraries,
        )
    ],
    cmdclass={"build_ext": build_pyqt_ext},
)
            
