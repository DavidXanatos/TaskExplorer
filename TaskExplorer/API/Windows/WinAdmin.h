#pragma once

bool IsElevated();
int RunElevated(const std::wstring& Params, bool bGetCode = false);
int RunElevated(const std::wstring& binaryPath, const std::wstring& Params, bool bGetCode = false);
int RestartElevated(int &argc, char **argv);

bool IsAutorunEnabled();
bool AutorunEnable(bool is_enable);

bool SkipUacRun(bool test_only);
bool SkipUacEnable(bool is_enable);

void create_process_as_trusted_installer(std::wstring command_line);