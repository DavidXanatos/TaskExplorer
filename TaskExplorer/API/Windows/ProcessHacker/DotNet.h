#pragma once

class QTreeWidgetEx;
class CWinProcess;

void InitDotNetStatTree(QTreeWidgetEx* pTree, QMap<int, QTreeWidgetItem*>& PerfCounters);

void UpdateDotNetStatTree(CWinProcess* pProcess, const QMap<int, QTreeWidgetItem*>& PerfCounters);