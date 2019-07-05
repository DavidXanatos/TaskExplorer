QHexEdit2
=========
![Application QHexEdit2 in Action](http://simsys.github.io/qhexedit.png)


QHexEdit is a hex editor widget written in C++ for the Qt (Qt4, Qt5) framework. It is a simple editor for binary data, just like QPlainTextEdit is for text data. There are sip configuration files included, so it is easy to create bindings for PyQt and you can use this widget also in python 2 and 3.

QHexEdit takes the data of a QByteArray (setData()) and shows it. You can use the mouse or the keyboard to navigate inside the widget. If you hit the keys (0..9, a..f) you will change the data. Changed data is highlighted and can be accessed via data().

Normally QHexEdit works in the overwrite mode. You can set overwrite mode(false) and insert data. In this case the size of data() increases. It is also possible to delete bytes (del or backspace), here the size of data decreases.

You can select data with keyboard hits or mouse movements. The copy-key will copy the selected data into the clipboard. The cut-key copies also but deletes it afterwards. In overwrite mode, the paste function overwrites the content of the (does not change the length) data. In insert mode, clipboard data will be inserted. The clipboard content is expected in ASCII Hex notation. Unknown characters will be ignored.

QHexEdit comes with undo/redo functionality. All changes can be undone, by pressing the undo-key (usually ctr-z). They can also be redone afterwards. The undo/redo framework is cleared, when setData() sets up a new content for the editor. You can search data inside the content with indexOf() and lastIndexOf(). The replace() function is to change located subdata. This 'replaced' data can also be undone by the undo/redo framework.

QHexEdit is based on QIODevice, that's why QHexEdit can handle big amounts of data. The size of edited data can be more then two gigabytes without any restrictions.

## Using QHexEdit
You can read the documentation of the project [here](http://simsys.github.io/). You find in the sources a [C++ example](https://github.com/Simsys/qhexedit2/tree/master/example), that shows how tu use the QHexedit widget. There is also a [python example](https://github.com/Simsys/qhexedit2/tree/master/python/python3_pyqt5) available.

## Contributing to QHexEdit
We love to receive contributions. You can submit bug reports [here](https://github.com/Simsys/qhexedit2/issues). If you are a developer, you can pick up a work item and start to realize super exciting features or fix bugs. We also like to receive enhancement proposals or translation support.

## License
```
Copyright (C) 2015-2016 Winfried Simon

This software may be used under the terms of the GNU Lesser General 
Public License version 2.1 as published by the Free Software Foundation 
and appearing in the file license.txt included in the packaging of this file.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
```
