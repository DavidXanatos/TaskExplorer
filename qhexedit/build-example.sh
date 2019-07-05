if [ ! -d build ];then
    mkdir build
else
    rm -rf build/*
fi

cd build
qmake ../example/qhexedit.pro
make
cd ..
