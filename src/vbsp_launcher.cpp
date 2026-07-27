#include <windows.h>
#include <stdio.h>
#include <string>

int wmain(int argc, wchar_t* argv[])
{
    // 1. Get current directory and set target path
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    std::wstring dirPath = exePath;
    size_t lastSlash = dirPath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        dirPath = dirPath.substr(0, lastSlash);
    }

    std::wstring targetExe = dirPath + L"\\vbsp_original.exe";
    std::wstring hookDll = dirPath + L"\\vbsp_hook.dll";

    // 2. Get the full command line to forward to vbsp_original.exe
    // We should skip the first argument (our own exe path) and get the rest of the arguments.
    // Or we can just get GetCommandLineW(), but wait, GetCommandLineW() includes the exe name as argv[0].
    // If we replace the exe name in GetCommandLineW() with vbsp_original.exe, it is much easier and safer!
    std::wstring commandLine = GetCommandLineW();

    // Replace the first token in the command line (our own executable name) with vbsp_original.exe
    // Handle quoted and unquoted executable names
    std::wstring newCommandLine;
    if (commandLine[0] == L'"') {
        size_t nextQuote = commandLine.find(L'"', 1);
        if (nextQuote != std::wstring::npos) {
            newCommandLine = L"\"" + targetExe + L"\"" + commandLine.substr(nextQuote + 1);
        } else {
            newCommandLine = L"\"" + targetExe + L"\" " + commandLine;
        }
    } else {
        size_t firstSpace = commandLine.find(L' ');
        if (firstSpace != std::wstring::npos) {
            newCommandLine = L"\"" + targetExe + L"\"" + commandLine.substr(firstSpace);
        } else {
            newCommandLine = L"\"" + targetExe + L"\"";
        }
    }

    // 3. Create the target process in SUSPENDED state
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    // CreateProcessW might modify the commandLine string, so we must make a mutable copy
    wchar_t* mutableCmdLine = new wchar_t[newCommandLine.length() + 1];
    wcscpy(mutableCmdLine, newCommandLine.c_str());

    BOOL success = CreateProcessW(
        targetExe.c_str(),
        mutableCmdLine,
        NULL,
        NULL,
        FALSE,
        CREATE_SUSPENDED,
        NULL,
        NULL,
        &si,
        &pi
    );

    delete[] mutableCmdLine;

    if (!success) {
        fwprintf(stderr, L"Failed to launch vbsp_original.exe. Error code: %d\n", GetLastError());
        return 1;
    }

    // 4. Convert DLL path to absolute ANSI path for LoadLibraryA
    char dllPathAnsi[MAX_PATH];
    int len = WideCharToMultiByte(CP_ACP, 0, hookDll.c_str(), -1, dllPathAnsi, MAX_PATH, NULL, NULL);
    if (len <= 0) {
        fwprintf(stderr, L"Failed to convert DLL path to ANSI.\n");
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    // 5. Allocate memory in target process and write the DLL path
    LPVOID remoteMem = VirtualAllocEx(pi.hProcess, NULL, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) {
        fwprintf(stderr, L"Failed to allocate memory in target process. Error code: %d\n", GetLastError());
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    if (!WriteProcessMemory(pi.hProcess, remoteMem, dllPathAnsi, len, NULL)) {
        fwprintf(stderr, L"Failed to write DLL path to target process. Error code: %d\n", GetLastError());
        VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    // 6. Get the address of LoadLibraryA in kernel32.dll
    LPVOID loadLibraryAddr = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    if (!loadLibraryAddr) {
        fwprintf(stderr, L"Failed to get LoadLibraryA address.\n");
        VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    // 7. Call LoadLibraryA inside the target process using CreateRemoteThread
    HANDLE remoteThread = CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddr, remoteMem, 0, NULL);
    if (!remoteThread) {
        fwprintf(stderr, L"Failed to create remote thread. Error code: %d\n", GetLastError());
        VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    // Wait for the remote thread to finish
    WaitForSingleObject(remoteThread, INFINITE);
    CloseHandle(remoteThread);

    // Free the allocated memory in the target process
    VirtualFreeEx(pi.hProcess, remoteMem, 0, MEM_RELEASE);

    // 8. Resume the main thread of the target process
    ResumeThread(pi.hThread);

    // 9. Wait for target process to finish
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return (int)exitCode;
}
