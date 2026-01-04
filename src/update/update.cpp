#include "update.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <cstdlib>

static std::wstring Widen(const std::string& s)
{
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), len);
    return w;
}

static std::wstring GetSelfPathW()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return path;
}

static bool FileExistsW(const std::wstring& p)
{
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static bool WaitForPidAndSwap(DWORD pid, const std::wstring& curExe, const std::wstring& newExe)
{
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (h) {
        WaitForSingleObject(h, INFINITE);
        CloseHandle(h);
    } else {
        Sleep(1500);
    }

    std::wstring bak = curExe + L".bak";
    MoveFileExW(bak.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    MoveFileExW(curExe.c_str(), bak.c_str(), MOVEFILE_REPLACE_EXISTING);

    if (!MoveFileExW(newExe.c_str(), curExe.c_str(), MOVEFILE_REPLACE_EXISTING))
        return false;

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring cmd = L"\"" + curExe + L"\"";
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
        return false;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

bool Update::HandleUpdaterMode(int argc, char** argv)
{
    if (argc < 5) return false;
    if (std::string(argv[1]) != "--apply-update") return false;

    DWORD pid = (DWORD)std::stoul(argv[2]);
    std::wstring curExe = Widen(argv[3]);
    std::wstring newExe = Widen(argv[4]);

    if (curExe.empty() || newExe.empty()) return true;
    if (!FileExistsW(newExe)) return true;

    WaitForPidAndSwap(pid, curExe, newExe);
    return true;
}

bool Update::DownloadAndApplyLatest()
{
    std::wstring curExe = GetSelfPathW();
    std::wstring newExe = curExe + L".new";
    if (curExe.empty()) return false;

    std::wstring curlCmdW =
        L"curl -L -o \"" + newExe + L"\" "
        L"\"https://github.com/adarec1994/Haven-Tools/releases/latest/download/HavenTools.exe\"";

    std::string curlCmd(curlCmdW.begin(), curlCmdW.end());
    int rc = std::system(curlCmd.c_str());
    if (rc != 0) return false;

    if (!FileExistsW(newExe)) return false;

    DWORD pid = GetCurrentProcessId();

    std::wstring cmd =
        L"\"" + curExe + L"\" --apply-update " + std::to_wstring(pid) +
        L" \"" + curExe + L"\"" +
        L" \"" + newExe + L"\"";

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
        return false;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    ExitProcess(0);
    return true;
}
