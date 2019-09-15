#include "stdafx.h"
#include "WsWatchDialog.h"
#include "../Common/Settings.h"
#include "../API/Windows/ProcessHacker.h"
#include "../API/Windows/WindowsAPI.h"

struct SWorkingSetWatch
{
	SWorkingSetWatch()
	{
		ProcessId = NULL;

		ProcessHandle = NULL;
		Buffer = NULL;
		BufferSize = 0;
	}

	HANDLE ProcessId;

    HANDLE ProcessHandle;
    PVOID Buffer;
    ULONG BufferSize;
};

class CSWorkingSetItem : public QTreeWidgetItem
{
public:
	CSWorkingSetItem(QTreeWidget* parent = NULL) : QTreeWidgetItem(parent) {}

private:
	virtual bool operator< (const QTreeWidgetItem &other) const
	{
		int column = treeWidget()->sortColumn();
		return data(column, Qt::UserRole).toUInt() < other.data(column, Qt::UserRole).toUInt();
	}
};

CWsWatchDialog::CWsWatchDialog(const CProcessPtr& pProcess, QWidget *parent)
	: QMainWindow(parent)
{
	this->setWindowTitle("Working Set Watch");

	m_pMainWidget = new QWidget();
    m_pMainWidget->setMinimumSize(QSize(430, 210));

    m_pMainLayout = new QGridLayout();
	m_pMainWidget->setLayout(m_pMainLayout);

	m_pInfoLabel = new QLabel();
    m_pInfoLabel->setWordWrap(true);
	m_pInfoLabel->setText(tr("Working set watch allows you to monitor page faults that occur in a process. "
		"You must enable WS watch for the process to start the monitoring. Once WS watch is enabled, it cannot be disabled."));
    m_pMainLayout->addWidget(m_pInfoLabel, 0, 0, 1, 4);

	m_pEnableBtn = new QPushButton();
	m_pEnableBtn->setText(tr("Enable"));
    m_pMainLayout->addWidget(m_pEnableBtn, 1, 0, 1, 1);

	m_pEnabledLbl = new QLabel();
	m_pEnabledLbl->setText(tr("WS watch is enabled."));
    m_pMainLayout->addWidget(m_pEnabledLbl, 1, 1, 1, 1);

	m_pMainLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum), 1, 2, 1, 1);

	m_pMainLayout->addWidget(new QLabel(tr("Page faults:")), 2, 0, 1, 4);

	m_pFaultList = new CPanelWidgetEx();
	m_pFaultList->GetTree()->setHeaderLabels(tr("Count|Instruction").split("|"));
	m_pFaultList->GetTree()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pFaultList->GetTree()->setSortingEnabled(true);
    m_pMainLayout->addWidget(m_pFaultList, 3, 0, 1, 4);

    m_pButtonBox = new QDialogButtonBox();
    m_pButtonBox->setStandardButtons(QDialogButtonBox::Close);
    m_pMainLayout->addWidget(m_pButtonBox, 4, 0, 1, 4);

	this->setCentralWidget(m_pMainWidget);

	connect(m_pEnableBtn, SIGNAL(pressed()), this, SLOT(OnEnable()));
	//connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));

	restoreGeometry(theConf->GetBlob("WsWatchWindow/Window_Geometry"));
	m_pFaultList->GetTree()->header()->restoreState(theConf->GetBlob("WsWatchWindow/FaultList_Columns"));

	m_TimerId = -1;

	m = new SWorkingSetWatch();
	m->ProcessId = (HANDLE)pProcess->GetProcessId();

	if (!NT_SUCCESS(PhOpenProcess(&m->ProcessHandle, PROCESS_QUERY_INFORMATION, m->ProcessId)))
    {
		QMessageBox::critical(this, tr("Working Set Watch"), tr("Unable to open the process."));
		this->close();
		return;
    }

    m->BufferSize = 0x2000;
    m->Buffer = PhAllocate(m->BufferSize);

	if (Refresh())
	{
		m_TimerId = startTimer(1000);
		m_pEnableBtn->setEnabled(false);
	}
	else
		m_pEnabledLbl->setVisible(false);
}

CWsWatchDialog::~CWsWatchDialog()
{
	theConf->SetBlob("WsWatchWindow/Window_Geometry", saveGeometry());
	theConf->SetBlob("WsWatchWindow/FaultList_Columns", m_pFaultList->GetTree()->header()->saveState());

	if(m_TimerId != -1)
		killTimer(m_TimerId);

	if (m->ProcessHandle)
		NtClose(m->ProcessHandle);

	if(m->Buffer)
		PhFree(m->Buffer);

	delete m;
}

void CWsWatchDialog::closeEvent(QCloseEvent *e)
{
	this->deleteLater();
}

/*void CWsWatchDialog::accept()
{

}*/

void CWsWatchDialog::reject()
{
	this->close();
}

void CWsWatchDialog::OnEnable()
{
	NTSTATUS status;
    HANDLE processHandle;

    if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_SET_INFORMATION, m->ProcessId)))
    {
        status = NtSetInformationProcess(processHandle, ProcessWorkingSetWatchEx, NULL, 0);
        NtClose(processHandle);
    }

	m_pEnableBtn->setEnabled(false);
	m_pEnabledLbl->setVisible(true);
	if (NT_SUCCESS(status))
		m_TimerId = startTimer(1000);
	else
		m_pEnabledLbl->setText(tr("Unable to enable WS watch, error: %1").arg(status));
}

void CWsWatchDialog::timerEvent(QTimerEvent *e)
{
	if (e->timerId() != m_TimerId) 
	{
		QMainWindow::timerEvent(e);
		return;
	}

	Refresh();
}

bool CWsWatchDialog::Refresh()
{
	NTSTATUS status;
    ULONG returnLength;
    PPROCESS_WS_WATCH_INFORMATION_EX wsWatchInfo;

    // Query WS watch information.

    if (!m->Buffer)
        return FALSE;

    status = NtQueryInformationProcess(m->ProcessHandle, ProcessWorkingSetWatchEx, m->Buffer, m->BufferSize, &returnLength);

    if (status == STATUS_UNSUCCESSFUL)
    {
        // WS Watch is not enabled.
        return false;
    }

    if (status == STATUS_NO_MORE_ENTRIES)
    {
        // There were no new faults, but we still need to process symbol lookup results.
		return true;
    }

    if (status == STATUS_BUFFER_TOO_SMALL || status == STATUS_INFO_LENGTH_MISMATCH)
    {
        PhFree(m->Buffer);
        m->Buffer = PhAllocate(returnLength);
        m->BufferSize = returnLength;

        status = NtQueryInformationProcess(m->ProcessHandle, ProcessWorkingSetWatchEx, m->Buffer, m->BufferSize, &returnLength);
    }

    if (!NT_SUCCESS(status))
    {
        // Error related to the buffer size. Try again later.
		return false;
    }

    // Update the hashtable and list view.

    wsWatchInfo = (PPROCESS_WS_WATCH_INFORMATION_EX)m->Buffer;

    while (wsWatchInfo->BasicInfo.FaultingPc)
    {
        quint32 newCount;

        // Update the count in the entry for this instruction pointer, or add a new entry if it doesn't exist.

		QTreeWidgetItem* &pItem = m_FailtList[(quint64)wsWatchInfo->BasicInfo.FaultingPc];

        if (pItem)
        {
            newCount = pItem->data(1, Qt::UserRole).toUInt() + 1;
        }
        else
        {
			pItem = new CSWorkingSetItem();
			m_pFaultList->GetTree()->addTopLevelItem(pItem);

			pItem->setData(0, Qt::UserRole, (quint64)wsWatchInfo->BasicInfo.FaultingPc);
			pItem->setText(0, FormatAddress((quint64)wsWatchInfo->BasicInfo.FaultingPc));

			qobject_cast<CWindowsAPI*>(theAPI)->GetSymbolProvider()->GetSymbolFromAddress((quint64)m->ProcessId, (quint64)wsWatchInfo->BasicInfo.FaultingPc, this, SLOT(OnSymbolFromAddress(quint64, quint64, int, const QString&, const QString&, const QString&)));

			newCount = 1;
        }

        pItem->setData(1, Qt::UserRole, newCount);
		pItem->setText(1, FormatNumber(newCount));

        wsWatchInfo++;
    }

	return true;
}

void CWsWatchDialog::OnSymbolFromAddress(quint64 ProcessId, quint64 Address, int ResolveLevel, const QString& StartAddressString, const QString& FileName, const QString& SymbolName)
{
	QTreeWidgetItem* pItem = m_FailtList.value(Address);
	pItem->setText(0, StartAddressString);
}