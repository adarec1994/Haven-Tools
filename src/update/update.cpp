#include "update.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <atomic>
#include <thread>
#include <vector>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")

static std::atomic<bool>  g_busy{false};
static std::atomic<bool>  g_error{false};
static std::atomic<float> g_progress{0.0f};
static std::string        g_status = "Idle";
static CRITICAL_SECTION   g_statusCs;

static void SetStatus(const char* s)
{
    EnterCriticalSection(&g_statusCs);
    g_status = s;
    LeaveCriticalSection(&g_statusCs);
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

    auto widen = [](const std::string& s) -> std::wstring {
        if (s.empty()) return L"";
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
        std::wstring w(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), len);
        return w;
    };

    std::wstring curExe = widen(argv[3]);
    std::wstring newExe = widen(argv[4]);

    if (curExe.empty() || newExe.empty()) return true;
    if (!FileExistsW(newExe)) return true;

    WaitForPidAndSwap(pid, curExe, newExe);
    return true;
}

static bool DownloadUrlToFileWithProgress(const wchar_t* url, const std::wstring& outPath)
{
    g_progress.store(0.0f, std::memory_order_relaxed);

    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    uc.dwSchemeLength = (DWORD)-1;
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;

    if (!WinHttpCrackUrl(url, 0, 0, &uc)) return false;

    std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
    std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength && uc.lpszExtraInfo)
        path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);

    HINTERNET hSession = WinHttpOpen(L"HavenTools-Updater/1.0",
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hSession, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    DWORD flags = 0;
    if (uc.nScheme == INTERNET_SCHEME_HTTPS) flags |= WINHTTP_FLAG_SECURE;

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                           nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    unsigned long long total = 0;
    DWORD len = sizeof(total);
    if (!WinHttpQueryHeaders(hRequest,
                            WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &total, &len, WINHTTP_NO_HEADER_INDEX)) {
        total = 0;
    }

    HANDLE out = CreateFileW(outPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out == INVALID_HANDLE_VALUE) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    unsigned long long downloaded = 0;
    std::vector<char> buffer(1 << 16);

    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &avail)) {
            CloseHandle(out);
            DeleteFileW(outPath.c_str());
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }
        if (avail == 0) break;

        while (avail > 0) {
            DWORD toRead = (DWORD)std::min<size_t>(buffer.size(), (size_t)avail);
            DWORD read = 0;

            if (!WinHttpReadData(hRequest, buffer.data(), toRead, &read) || read == 0) {
                CloseHandle(out);
                DeleteFileW(outPath.c_str());
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return false;
            }

            DWORD written = 0;
            if (!WriteFile(out, buffer.data(), read, &written, nullptr) || written != read) {
                CloseHandle(out);
                DeleteFileW(outPath.c_str());
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return false;
            }

            downloaded += read;
            avail -= read;

            if (total > 0) {
                float p = (float)((double)downloaded / (double)total);
                if (p < 0.0f) p = 0.0f;
                if (p > 1.0f) p = 1.0f;
                g_progress.store(p, std::memory_order_relaxed);
            } else {
                float p = g_progress.load(std::memory_order_relaxed);
                p += 0.0025f;
                if (p > 0.95f) p = 0.95f;
                g_progress.store(p, std::memory_order_relaxed);
            }
        }
    }

    CloseHandle(out);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    g_progress.store(1.0f, std::memory_order_relaxed);
    return true;
}

void Update::StartDownloadAndApplyLatest()
{
    if (g_busy.exchange(true)) return;

    static bool csInit = false;
    if (!csInit) {
        InitializeCriticalSection(&g_statusCs);
        csInit = true;
    }

    g_error.store(false);
    g_progress.store(0.0f);
    SetStatus("Downloading...");

    std::thread([]{
        const wchar_t* url = L"https://github.com/adarec1994/Haven-Tools/releases/latest/download/HavenTools.exe";

        std::wstring curExe = GetSelfPathW();
        std::wstring newExe = curExe + L".new";

        bool ok = DownloadUrlToFileWithProgress(url, newExe);
        if (!ok || !FileExistsW(newExe)) {
            g_error.store(true);
            SetStatus("Download failed");
            g_busy.store(false);
            return;
        }

        SetStatus("Applying update...");

        DWORD pid = GetCurrentProcessId();

        std::wstring cmd =
            L"\"" + curExe + L"\" --apply-update " + std::to_wstring(pid) +
            L" \"" + curExe + L"\"" +
            L" \"" + newExe + L"\"";

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            g_error.store(true);
            SetStatus("Failed to restart");
            g_busy.store(false);
            return;
        }

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        ExitProcess(0);
    }).detach();
}

bool Update::IsBusy()
{
    return g_busy.load();
}

float Update::GetProgress()
{
    return g_progress.load();
}

const char* Update::GetStatusText()
{
    EnterCriticalSection(&g_statusCs);
    static std::string copy;
    copy = g_status;
    LeaveCriticalSection(&g_statusCs);
    return copy.c_str();
}

bool Update::HadError()
{
    return g_error.load();
}
