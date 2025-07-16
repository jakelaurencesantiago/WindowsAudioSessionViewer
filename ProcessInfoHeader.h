#pragma once


#include <Windows.h>
#include <Psapi.h>
#include <strsafe.h>

#pragma comment(lib, "Version.lib")

struct LANGANDCODEPAGE {
	WORD wLanguage;
	WORD wCodePage;
};

struct ProcessInformation {
	std::wstring processDisplayName;
	std::wstring processBaseName;
	std::wstring processPath;
};

BOOL GetProcessInfo(DWORD processId, ProcessInformation* processInfo) {
	WCHAR szFileName[MAX_PATH];
	WCHAR szFilePath[MAX_PATH];

	HMODULE hBaseMode;
	DWORD cbBaseNeeded;

	ZeroMemory(szFileName, MAX_PATH);
	ZeroMemory(szFilePath, MAX_PATH);

	HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION |
		PROCESS_VM_READ,
		FALSE, processId);

	if (NULL == hProcess) {
		return FALSE;
	}

	if (EnumProcessModules(hProcess, &hBaseMode, sizeof(hBaseMode),
		&cbBaseNeeded))
	{
		GetModuleBaseName(hProcess, NULL, szFileName, sizeof(szFileName) / sizeof(WCHAR));

		GetModuleFileNameEx(hProcess, NULL, szFilePath, sizeof(szFilePath) / sizeof(WCHAR));

		processInfo->processBaseName = std::wstring(szFileName);
		processInfo->processPath = std::wstring(szFilePath);


		UINT dwBytes, cbTranslate;
		LANGANDCODEPAGE* lpTranslate;
		DWORD dwFileInfoSize = GetFileVersionInfoSize(szFilePath, (DWORD*)&dwBytes);
		if (dwFileInfoSize > 0) {
			LPVOID lpData = (LPVOID)malloc(dwFileInfoSize);
			if (lpData) {
				ZeroMemory(lpData, dwFileInfoSize);
				if (GetFileVersionInfo(szFilePath, 0, dwFileInfoSize, lpData)) {
					VerQueryValue(lpData, L"\\VarFileInfo\\Translation",
						(LPVOID*)&lpTranslate, &cbTranslate);

					WCHAR strSubBlock[MAX_PATH] = { 0 };
					WCHAR* lpBuffer;

					for (int i = 0; i < (cbTranslate / sizeof(struct LANGANDCODEPAGE)); i++)
					{
						StringCchPrintf(strSubBlock, 50,
							L"\\StringFileInfo\\%04x%04x\\FileDescription",
							lpTranslate[i].wLanguage,
							lpTranslate[i].wCodePage);
						VerQueryValue(lpData,
							strSubBlock,
							(void**)&lpBuffer,
							&dwBytes);

						if (dwBytes > 0 && lpBuffer && lstrlenW(lpBuffer) > 0) {
							processInfo->processDisplayName = std::wstring(lpBuffer);
						}
						break;
					}
				}
				free(lpData);
			}
		}
	}
	CloseHandle(hProcess);

	return TRUE;
}