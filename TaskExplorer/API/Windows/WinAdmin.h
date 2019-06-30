#pragma once

bool IsElevated();
int RunElevated(const wstring& Params, bool bGetCode = false);
int RunElevated(const wstring& binaryPath, const wstring& Params, bool bGetCode = false);
int RestartElevated(int &argc, char **argv);

bool IsAutorunEnabled();
bool AutorunEnable(bool is_enable);

bool SkipUacRun(bool test_only);
bool SkipUacEnable(bool is_enable);

void create_process_as_trusted_installer(wstring command_line);