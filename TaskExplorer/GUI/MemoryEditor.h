#pragma once
#include "../qhexedit/editor/HexEditor.h"
//#include "../NeoFramework/editor/HexEditor.h"

class CMemoryEditor: public QHexEditor
{
    Q_OBJECT

public:
    CMemoryEditor(QWidget *parent = Q_NULLPTR);
	virtual ~CMemoryEditor();

	void setDevice(QIODevice* pDevice, quint64 BaseAddress = 0);


protected:
	virtual bool open();
	virtual bool save();

	virtual void readSettings();
	virtual void writeSettings();

	QIODevice* m_pDevice;
};
