if [ ! -d build ];then
    mkdir build
else
    rm -rf build/*
fi

cd build

# for Qt4 use qt4-qmake
# /usr/share/qt4/bin/qmake ../src/qhexedit.pro

# for Qt5 use qt5-qmake
/usr/lib/x86_64-linux-gnu/qt5/bin/qmake ../src/qhexedit.pro

sudo make
cd ..

# for PyQt4 use python2 and PyQt4
# sudo python2 setup.py install

# for PyQt5 use python3 and PyQt5
sudo python3 setup.py install
