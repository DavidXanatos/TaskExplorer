/*
 * Task Explorer -
 *   qt port of the Extended Service Plugin
 *
 * Copyright (C) 2011-2015 wj32
 * Copyright (C) 2019 David Xanatos
 *
 * This file is part of Task Explorer and contains Process Hacker code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stdafx.h"
#include "WinSvcTrigger.h"
#ifdef WIN32
#include "../../API/Windows/ProcessHacker/PhSvc.h"
#endif
#include "../../../MiscHelpers/Common/ComboInputDialog.h"
#include "../../../MiscHelpers/Common/MultiLineInputDialog.h"


CWinSvcTrigger::CWinSvcTrigger(QWidget *parent)
	: QDialog(parent)
{
	ui.setupUi(this);

	m_pInfo = NULL;

	m_LastSelectedType = 0;
	m_NoFixServiceTriggerControls = false;

	ui.datas->setHeaderLabels(tr("Data").split("|"));

    for (int i = 0; i < 8; i++)
		ui.type->addItem(QString::fromWCharArray(TypeEntries[i].Name), (quint32)TypeEntries[i].Type);

	ui.action->addItem(tr("Start"), SERVICE_TRIGGER_ACTION_SERVICE_START);
	ui.action->addItem(tr("Stop"), SERVICE_TRIGGER_ACTION_SERVICE_STOP);

	connect(ui.type, SIGNAL(currentIndexChanged(int)), this, SLOT(FixServiceTriggerControls()));
	connect(ui.subType, SIGNAL(currentIndexChanged(int)), this, SLOT(FixServiceTriggerControls()));

	connect(ui.datas, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnData(QTreeWidgetItem*, int)));
	connect(ui.newBtn, SIGNAL(pressed()), this, SLOT(OnNewTrigger()));
	connect(ui.editBtn, SIGNAL(pressed()), this, SLOT(OnEditTrigger()));
	connect(ui.deleteBtn, SIGNAL(pressed()), this, SLOT(OnDeleteTrigger()));

	connect(ui.buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

CWinSvcTrigger::~CWinSvcTrigger()
{
	if(m_pInfo)
		EspDestroyTriggerInfo((PES_TRIGGER_INFO)m_pInfo);
}

void CWinSvcTrigger::InitInfo()
{
	if (m_pInfo) {
		ASSERT(0);
		return;
	}

	PES_TRIGGER_INFO info = EspCreateTriggerInfo(NULL);
	info->Type = SERVICE_TRIGGER_TYPE_IP_ADDRESS_AVAILABILITY;
	info->SubtypeBuffer = NetworkManagerFirstIpAddressArrivalGuid;
	info->Subtype = &info->SubtypeBuffer;
	info->Action = SERVICE_TRIGGER_ACTION_SERVICE_START;

	m_pInfo = info;

	SetInfo();
}

void CWinSvcTrigger::FixServiceTriggerControls()
{
	if (m_NoFixServiceTriggerControls)
		return;
	m_NoFixServiceTriggerControls = true;

	ULONG type = ui.type->currentData().toUInt();

    if (m_LastSelectedType != type)
    {
        // Change the contents of the subtype combo box based on the type.

		ui.subType->clear();

        switch (type)
        {
        case SERVICE_TRIGGER_TYPE_DEVICE_INTERFACE_ARRIVAL:
        case SERVICE_TRIGGER_TYPE_CUSTOM_SYSTEM_STATE_CHANGE:
            {
				ui.subType->addItem(tr("Custom"));
            }
            break;
        case SERVICE_TRIGGER_TYPE_CUSTOM:
            {
                PETW_PUBLISHER_ENTRY entries;
                ULONG numberOfEntries;

				ui.subType->addItem(tr("Custom"), true);

                // Display a list of publishers.
                if (EspEnumerateEtwPublishers(&entries, &numberOfEntries))
                {
                    // Sort the list by name.
                    //qsort(entries, numberOfEntries, sizeof(ETW_PUBLISHER_ENTRY), EtwPublisherByNameCompareFunction);

                    for (ULONG i = 0; i < numberOfEntries; i++)
                    {
						ui.subType->addItem(QString::fromWCharArray(entries[i].PublisherName->Buffer));
                        PhDereferenceObject(entries[i].PublisherName);
                    }

                    PhFree(entries);
                }
            }
            break;
        default:
            for (int i = 0; i < 20; i++)
            {
                if (SubTypeEntries[i].Type == type && SubTypeEntries[i].Guid && SubTypeEntries[i].Guid != &SubTypeUnknownGuid)
                {
					ui.subType->addItem(QString::fromWCharArray(SubTypeEntries[i].Name));
                }
            }
            break;
        }

        m_LastSelectedType = type;
    }

    if (ui.subType->currentData().toBool() == true)
    {
		ui.custom->setEnabled(true);
		ui.custom->setText(m_LastCustomSubType);
    }
    else
    {
        if (ui.custom->isEnabled())
        {
            ui.custom->setEnabled(false);
			m_LastCustomSubType = ui.custom->text();
			ui.custom->setText("");
        }
    }

	m_NoFixServiceTriggerControls = false;
}

bool EspSetTriggerData(const QString& Value, PES_TRIGGER_DATA Data)
{
	// todo: add for every type a sanity check
	// todo: handle number input as hex
	if (Data->Type == SERVICE_TRIGGER_DATA_TYPE_STRING)
	{
		if(Data->String)
			PhDereferenceObject(Data->String);

		PPH_STRING EditingValue = CastQString(Value);
		Data->String = EspConvertNewLinesToNulls(EditingValue);
		PhDereferenceObject(EditingValue);
	}
    else if (Data->Type == SERVICE_TRIGGER_DATA_TYPE_BINARY)
    {
		if(Data->Binary)
			PhFree(Data->Binary);

		QByteArray binValue = QByteArray::fromHex(Value.toLatin1());
		Data->BinaryLength = binValue.length();
		Data->Binary = PhAllocateCopy(binValue.data(), binValue.length());
    }
    else if (Data->Type == SERVICE_TRIGGER_DATA_TYPE_LEVEL)
    {
		Data->Byte = Value.toShort();
    }
    else if (Data->Type == SERVICE_TRIGGER_DATA_TYPE_KEYWORD_ANY)
    {
		Data->UInt64 = Value.toULongLong();
    }
    else if (Data->Type == SERVICE_TRIGGER_DATA_TYPE_KEYWORD_ALL)
    {
		Data->UInt64 = Value.toULongLong();
    }
    else
    {
		return false;
    }

	return false;
}

QString EspFormatTriggerData(PES_TRIGGER_DATA Data, bool bForDisplay = true)
{
	QString Text = "";
	// todo: display numbers as hex
    if (Data->Type == SERVICE_TRIGGER_DATA_TYPE_STRING)
    {
        // This check works for both normal strings and multistrings.
        if (Data->String->Length != 0)
        {
			// todo: Prepare the text for display by replacing null characters with spaces.
			Text = CastPhString(Data->String, false);
        }
        else if (bForDisplay)
        {
			Text = CWinSvcTrigger::tr("(empty string)");
        }
    }
    else if (Data->Type == SERVICE_TRIGGER_DATA_TYPE_BINARY)
    {
		if (bForDisplay)
			Text = CWinSvcTrigger::tr("(binary data) ");
		Text.append(QByteArray((char*)Data->Binary, Data->BinaryLength).toHex());
    }
    else if (Data->Type == SERVICE_TRIGGER_DATA_TYPE_LEVEL)
    {
		if (bForDisplay)
			Text = CWinSvcTrigger::tr("(level) ");
		Text.append(QString::number(Data->Byte));
    }
    else if (Data->Type == SERVICE_TRIGGER_DATA_TYPE_KEYWORD_ANY)
    {
		if (bForDisplay)
			Text = CWinSvcTrigger::tr("(keyword any) ");
		Text.append(QString::number(Data->UInt64));
    }
    else if (Data->Type == SERVICE_TRIGGER_DATA_TYPE_KEYWORD_ALL)
    {
		if (bForDisplay)
			Text = CWinSvcTrigger::tr("(keyword all) ");
		Text.append(QString::number(Data->UInt64));
    }
    else
    {
		if (bForDisplay)
			Text = CWinSvcTrigger::tr("(unknown type)");
    }
	return Text;
}

void CWinSvcTrigger::SetInfo(void* pInfo)
{
	if (m_pInfo) {
		EspDestroyTriggerInfo((PES_TRIGGER_INFO)m_pInfo);
		ui.datas->clear();
	}

	PES_TRIGGER_INFO info = (PES_TRIGGER_INFO)pInfo;
	
	m_pInfo = EspCloneTriggerInfo(info);

	SetInfo();
}

void CWinSvcTrigger::SetInfo()
{
	PES_TRIGGER_INFO info = (PES_TRIGGER_INFO)m_pInfo;

	m_LastSelectedType = 0;

	if (info->Subtype)
		m_LastCustomSubType = CastPhString(PhFormatGuid(info->Subtype));
	else
		m_LastCustomSubType = QString();

	ui.type->setCurrentIndex(ui.type->findData((quint32)info->Type));
	ui.action->setCurrentIndex(ui.action->findData((quint32)info->Action));

	FixServiceTriggerControls();

    if (info->Type != SERVICE_TRIGGER_TYPE_CUSTOM)
    {
        for (int i = 0; i < 20; i++)
        {
            if (
                SubTypeEntries[i].Type == info->Type &&
                SubTypeEntries[i].Guid &&
                info->Subtype &&
                IsEqualGUID(*(_GUID*)&SubTypeEntries[i].Guid, *(_GUID*)&info->Subtype)
                )
            {
				ui.subType->setCurrentIndex(ui.subType->findText(QString::fromWCharArray(SubTypeEntries[i].Name)));
                break;
            }
        }
    }
    else
    {
        if (info->Subtype)
        {
            PPH_STRING publisherName;

            // Try to select the publisher name in the subtype list.
            publisherName = EspLookupEtwPublisherName(info->Subtype);
			ui.subType->setCurrentIndex(ui.subType->findText(QString::fromWCharArray(publisherName->Buffer)));
            PhDereferenceObject(publisherName);
        }
    }

    // Call a second time since the state of the custom subtype text box may have changed.
    FixServiceTriggerControls();

	if (info->DataList)
	{
		for (int i = 0; i < info->DataList->Count; i++)
		{
			PES_TRIGGER_DATA data = (PES_TRIGGER_DATA)info->DataList->Items[i];

			QTreeWidgetItem* pItem = new QTreeWidgetItem();
			pItem->setData(0, Qt::UserRole, (quint64)data);
			ui.datas->addTopLevelItem(pItem);
			pItem->setText(0, EspFormatTriggerData(data));
		}
	}
}


void CWinSvcTrigger::closeEvent(QCloseEvent *e)
{
	this->deleteLater();
}

void CWinSvcTrigger::accept()
{
	PH_AUTO_POOL autoPool;
    PhInitializeAutoPool(&autoPool);

	PES_TRIGGER_INFO info = (PES_TRIGGER_INFO)m_pInfo;

	info->Type = ui.type->currentData().toUInt();
	wstring customStr = ui.custom->text().toStdWString();

    if (ui.subType->currentData().toBool() != true)
    {
        if (info->Type != SERVICE_TRIGGER_TYPE_CUSTOM)
        {
            for (int i = 0; i < 20; i++)
            {
                if (
                    SubTypeEntries[i].Type == info->Type &&
                    QString::fromWCharArray(SubTypeEntries[i].Name).compare(ui.subType->currentText(), Qt::CaseInsensitive) == 0
                    )
                {
                    info->SubtypeBuffer = *SubTypeEntries[i].Guid;
                    info->Subtype = &info->SubtypeBuffer;
                    break;
                }
            }
        }
        else
        {
            if (!EspLookupEtwPublisherGuid((wchar_t*)customStr.c_str(), &info->SubtypeBuffer))
            {
				QMessageBox::warning(NULL, "TaskExplorer", tr("Unable to find the ETW publisher GUID."));
                //PhShowError(hwndDlg, L"Unable to find the ETW publisher GUID.");
                goto DoNotClose;
            }

            info->Subtype = &info->SubtypeBuffer;
        }
    }
    else
    {
        UNICODE_STRING guidString;

		guidString.Buffer = (wchar_t*)customStr.c_str();
		guidString.Length = customStr.size();

        // Trim whitespace.

        while (guidString.Length != 0 && *guidString.Buffer == ' ')
        {
            guidString.Buffer++;
            guidString.Length -= 2;
        }

        while (guidString.Length != 0 && guidString.Buffer[guidString.Length / 2 - 1] == ' ')
        {
            guidString.Length -= 2;
        }

        if (NT_SUCCESS(RtlGUIDFromString(&guidString, &info->SubtypeBuffer)))
        {
            info->Subtype = &info->SubtypeBuffer;
        }
        else
        {
            QMessageBox::warning(NULL, "TaskExplorer", tr("The custom subtype is invalid. Please ensure that the string is a valid GUID: \"{x-x-x-x-x}\"."));
            goto DoNotClose;
        }
    }

	info->Action = ui.action->currentData().toInt();

    if (
        info->DataList &&
        info->DataList->Count != 0 &&
        info->Type != SERVICE_TRIGGER_TYPE_DEVICE_INTERFACE_ARRIVAL &&
        info->Type != SERVICE_TRIGGER_TYPE_FIREWALL_PORT_EVENT &&
        info->Type != SERVICE_TRIGGER_TYPE_NETWORK_ENDPOINT &&
        info->Type != SERVICE_TRIGGER_TYPE_CUSTOM
        )
    {
        // This trigger has data items, but the trigger type doesn't allow them.
		if(QMessageBox("TaskExplorer", tr("The trigger type \"%1\" does not allow data items to be configured. If you continue, they will be removed.").arg(ui.type->currentText()), QMessageBox::Question, QMessageBox::Ok, QMessageBox::Cancel | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Ok)
            goto DoNotClose;

        for (int i = 0; i < info->DataList->Count; i++)
        {
            EspDestroyTriggerData((PES_TRIGGER_DATA)info->DataList->Items[i]);
        }

        PhClearReference((PVOID*)&info->DataList);
    }

    this->close();

DoNotClose:
    PhDeleteAutoPool(&autoPool);
}

void CWinSvcTrigger::reject()
{
	this->close();
}

void CWinSvcTrigger::OnData(QTreeWidgetItem *item, int column)
{
	PES_TRIGGER_INFO info = (PES_TRIGGER_INFO)m_pInfo;

	PES_TRIGGER_DATA data = (PES_TRIGGER_DATA)item->data(0, Qt::UserRole).toULongLong();
    ULONG index = PhFindItemList(info->DataList, data);
	if (index == -1)
		return;

	CMultiLineInputDialog valueDialog(this);
	//valueDialog.setText(tr("Enter value of type %1:").arg(typeDialog.value()));
	valueDialog.setText(tr("Enter value"));
	valueDialog.setValue(EspFormatTriggerData(data, false));

	if (!valueDialog.exec())
		return;

	QString value = valueDialog.value();

	EspSetTriggerData(value, data);
}

void CWinSvcTrigger::OnNewTrigger()
{
	PES_TRIGGER_INFO info = (PES_TRIGGER_INFO)m_pInfo;

	CComboInputDialog typeDialog(this);
	typeDialog.setText(tr("Sellect data type:"));
	typeDialog.addItem(tr("String"), SERVICE_TRIGGER_DATA_TYPE_STRING);
	typeDialog.addItem(tr("Binary data"), SERVICE_TRIGGER_DATA_TYPE_BINARY);
	typeDialog.addItem(tr("Level"), SERVICE_TRIGGER_DATA_TYPE_LEVEL);
	typeDialog.addItem(tr("Keyword any"), SERVICE_TRIGGER_DATA_TYPE_KEYWORD_ANY);
	typeDialog.addItem(tr("Keyword all"), SERVICE_TRIGGER_DATA_TYPE_KEYWORD_ALL);

	if (!typeDialog.exec())
		return;
	
	CMultiLineInputDialog valueDialog(this);
	//valueDialog.setText(tr("Enter value of type %1:").arg(typeDialog.value()));
	valueDialog.setText(tr("Enter value"));

	if (!valueDialog.exec())
		return;

	ULONG type = typeDialog.data().toUInt();
	QString value = valueDialog.value();

	PES_TRIGGER_DATA data = EspCreateTriggerData(NULL);
	data->Type = type;
	data->String = NULL;
	EspSetTriggerData(value, data);

	if (!info->DataList)
		info->DataList = PhCreateList(4);
	PhAddItemList(info->DataList, data);

	QTreeWidgetItem* pItem = new QTreeWidgetItem();
	pItem->setData(0, Qt::UserRole, (quint64)data);
	ui.datas->addTopLevelItem(pItem);
	pItem->setText(0, EspFormatTriggerData(data));
}

void CWinSvcTrigger::OnEditTrigger()
{
	QTreeWidgetItem *item = ui.datas->currentItem();
	if (!item)
		return;
	OnData(item, 0);
}

void CWinSvcTrigger::OnDeleteTrigger()
{
	QTreeWidgetItem *item = ui.datas->currentItem();
	if (!item)
		return;

	if(QMessageBox("TaskExplorer", tr("Do you want to delete the sellected data"), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
		return;

	PES_TRIGGER_INFO info = (PES_TRIGGER_INFO)m_pInfo;

	PES_TRIGGER_DATA data = (PES_TRIGGER_DATA)item->data(0, Qt::UserRole).toULongLong();
    ULONG index = PhFindItemList(info->DataList, data);
    if (index != -1)
    {
        EspDestroyTriggerData(data);
        PhRemoveItemList(info->DataList, index);
        delete item;
    }
}
