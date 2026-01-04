#include "update.h"
#include "../version.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "version.lib")

static std::atomic<bool> g_busy{ false };
static std::atomic<bool> g_error{ false };
static std::atomic<float> g_progress{ 0.0f };

static CRITICAL_SECTION g_statusCs;
static bool g_statusCsInit = false;
static std::string g_statusText;

static std::atomic<bool> g_checkDone{ false };
static std::atomic<bool> g_updateAvailable{ false };
static std::atomic<bool> g_promptDismissed{ false };

static CRITICAL_SECTION g_latestCs;
static bool g_latestCsInit = false;
static std::string g_latestVersion;

static std::wstring Widen(const std::string& s)
{
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), len);
    return w;
}

static std::string Narrow(const std::wstring& w)
{
    if (w.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), len, nullptr, nullptr);
    return s;
}

static void EnsureStatusCs()
{
    if (!g_statusCsInit) {
        InitializeCriticalSection(&g_statusCs);
        g_statusCsInit = true;
    }
}

static void EnsureLatestCs()
{
    if (!g_latestCsInit) {
        InitializeCriticalSection(&g_latestCs);
        g_latestCsInit = true;
    }
}

static void SetStatus(const char* s)
{
    EnsureStatusCs();
    EnterCriticalSection(&g_statusCs);
    g_statusText = s ? s : "";
    LeaveCriticalSection(&g_statusCs);
}

bool Update::IsBusy() { return g_busy.load(); }
bool Update::HadError() { return g_error.load(); }
float Update::GetProgress() { return g_progress.load(); }

const char* Update::GetStatusText()
{
    EnsureStatusCs();
    static thread_local std::string tmp;
    EnterCriticalSection(&g_statusCs);
    tmp = g_statusText;
    LeaveCriticalSection(&g_statusCs);
    return tmp.c_str();
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

static bool SplitUrl(const std::wstring& url, bool& https, std::wstring& host, std::wstring& pathAndQuery)
{
    https = false;
    host.clear();
    pathAndQuery.clear();

    std::wstring u = url;
    if (u.rfind(L"https://", 0) == 0) {
        https = true;
        u = u.substr(8);
    } else if (u.rfind(L"http://", 0) == 0) {
        https = false;
        u = u.substr(7);
    } else {
        return false;
    }

    size_t slash = u.find(L'/');
    if (slash == std::wstring::npos) {
        host = u;
        pathAndQuery = L"/";
    } else {
        host = u.substr(0, slash);
        pathAndQuery = u.substr(slash);
        if (pathAndQuery.empty()) pathAndQuery = L"/";
    }
    return !host.empty();
}

static bool HttpGetToMemory(const std::wstring& url, std::string& out)
{
    out.clear();

    std::wstring cur = url;
    for (int redirects = 0; redirects < 5; redirects++) {
        bool https = false;
        std::wstring host, path;
        if (!SplitUrl(cur, https, host, path)) return false;

        HINTERNET hSession = WinHttpOpen(L"HavenToolsUpdater/1.0",
                                        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                        WINHTTP_NO_PROXY_NAME,
                                        WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return false;

        DWORD redir = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(hSession, WINHTTP_OPTION_REDIRECT_POLICY, &redir, sizeof(redir));

        HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                                nullptr, WINHTTP_NO_REFERER,
                                                WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                https ? WINHTTP_FLAG_SECURE : 0);
        if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

        WinHttpAddRequestHeaders(hRequest, L"User-Agent: HavenToolsUpdater/1.0\r\n", (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

        BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                     WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        if (!ok) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

        ok = WinHttpReceiveResponse(hRequest, nullptr);
        if (!ok) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

        DWORD status = 0;
        DWORD statusSize = sizeof(status);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);

        if (status >= 300 && status < 400) {
            wchar_t loc[2048];
            DWORD locSize = sizeof(loc);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, &loc, &locSize, WINHTTP_NO_HEADER_INDEX)) {
                cur = std::wstring(loc, locSize / sizeof(wchar_t));
                if (!cur.empty() && cur.back() == L'\0') cur.pop_back();
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                continue;
            }
        }

        std::vector<char> buf;
        DWORD avail = 0;
        for (;;) {
            avail = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &avail)) break;
            if (avail == 0) break;
            size_t old = buf.size();
            buf.resize(old + avail);
            DWORD read = 0;
            if (!WinHttpReadData(hRequest, buf.data() + old, avail, &read)) break;
            buf.resize(old + read);
        }

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        out.assign(buf.begin(), buf.end());
        return !out.empty();
    }
    return false;
}

static bool ExtractLatestTagName(const std::string& json, std::string& tag)
{
    tag.clear();
    const std::string key = "\"tag_name\"";
    size_t p = json.find(key);
    if (p == std::string::npos) return false;
    p = json.find(':', p);
    if (p == std::string::npos) return false;
    p = json.find('"', p);
    if (p == std::string::npos) return false;
    size_t e = json.find('"', p + 1);
    if (e == std::string::npos) return false;
    tag = json.substr(p + 1, e - (p + 1));
    return !tag.empty();
}

static void NormalizeTagToVersion(std::string& v)
{
    while (!v.empty() && std::isspace((unsigned char)v.front())) v.erase(v.begin());
    while (!v.empty() && std::isspace((unsigned char)v.back())) v.pop_back();
    if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) v.erase(v.begin());
}

static std::vector<int> ParseVer(const std::string& s)
{
    std::vector<int> parts;
    int cur = 0;
    bool any = false;
    for (char c : s) {
        if (c >= '0' && c <= '9') {
            cur = cur * 10 + (c - '0');
            any = true;
        } else if (c == '.') {
            parts.push_back(any ? cur : 0);
            cur = 0;
            any = false;
        } else {
            break;
        }
    }
    parts.push_back(any ? cur : 0);
    while (!parts.empty() && parts.back() == 0) parts.pop_back();
    return parts;
}

static bool IsRemoteNewer(const std::string& remote, const std::string& local)
{
    auto r = ParseVer(remote);
    auto l = ParseVer(local);
    size_t n = (std::max)(r.size(), l.size());
    r.resize(n, 0);
    l.resize(n, 0);
    for (size_t i = 0; i < n; i++) {
        if (r[i] > l[i]) return true;
        if (r[i] < l[i]) return false;
    }
    return false;
}

static bool DownloadUrlToFileWithProgress(const std::wstring& url, const std::wstring& outPath)
{
    std::wstring cur = url;

    for (int redirects = 0; redirects < 5; redirects++) {
        bool https = false;
        std::wstring host, path;
        if (!SplitUrl(cur, https, host, path)) return false;

        HINTERNET hSession = WinHttpOpen(L"HavenToolsUpdater/1.0",
                                        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                        WINHTTP_NO_PROXY_NAME,
                                        WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return false;

        DWORD redir = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(hSession, WINHTTP_OPTION_REDIRECT_POLICY, &redir, sizeof(redir));

        HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                                nullptr, WINHTTP_NO_REFERER,
                                                WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                https ? WINHTTP_FLAG_SECURE : 0);
        if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

        WinHttpAddRequestHeaders(hRequest, L"User-Agent: HavenToolsUpdater/1.0\r\n", (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

        BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                     WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        if (!ok) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

        ok = WinHttpReceiveResponse(hRequest, nullptr);
        if (!ok) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

        DWORD status = 0;
        DWORD statusSize = sizeof(status);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);

        if (status >= 300 && status < 400) {
            wchar_t loc[2048];
            DWORD locSize = sizeof(loc);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, &loc, &locSize, WINHTTP_NO_HEADER_INDEX)) {
                cur = std::wstring(loc, locSize / sizeof(wchar_t));
                if (!cur.empty() && cur.back() == L'\0') cur.pop_back();
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                continue;
            }
        }

        ULONGLONG contentLen = 0;
        {
            wchar_t cl[64];
            DWORD clSize = sizeof(cl);
            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, &cl, &clSize, WINHTTP_NO_HEADER_INDEX)) {
                contentLen = _wtoi64(cl);
            }
        }

        HANDLE hFile = CreateFileW(outPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        std::vector<unsigned char> buffer(64 * 1024);
        ULONGLONG total = 0;

        for (;;) {
            DWORD avail = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &avail)) { CloseHandle(hFile); DeleteFileW(outPath.c_str()); break; }
            if (avail == 0) break;

            while (avail > 0) {
                DWORD chunk = (DWORD)((std::min)(avail, (DWORD)buffer.size()));
                DWORD read = 0;
                if (!WinHttpReadData(hRequest, buffer.data(), chunk, &read)) { CloseHandle(hFile); DeleteFileW(outPath.c_str()); avail = 0; break; }
                if (read == 0) { avail = 0; break; }

                DWORD written = 0;
                if (!WriteFile(hFile, buffer.data(), read, &written, nullptr) || written != read) {
                    CloseHandle(hFile);
                    DeleteFileW(outPath.c_str());
                    avail = 0;
                    break;
                }

                total += read;
                avail -= read;

                if (contentLen > 0) {
                    float p = (float)((double)total / (double)contentLen);
                    if (p < 0.0f) p = 0.0f;
                    if (p > 1.0f) p = 1.0f;
                    g_progress.store(p);
                }
            }
        }

        CloseHandle(hFile);

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        if (!FileExistsW(outPath)) return false;
        if (contentLen > 0 && total == 0) return false;
        return true;
    }

    return false;
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
    EnsureStatusCs();
    if (g_busy.exchange(true)) return false;

    g_error.store(false);
    g_progress.store(0.0f);
    SetStatus("Downloading update...");

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

    return true;
}

const char* Update::GetInstalledVersionText()
{
    static std::string s;
    s = APP_VERSION;
    return s.c_str();
}

void Update::StartCheckForUpdates()
{
    EnsureStatusCs();
    EnsureLatestCs();

    if (g_busy.exchange(true)) return;

    g_error.store(false);
    g_progress.store(0.0f);
    g_checkDone.store(false);
    g_updateAvailable.store(false);
    g_promptDismissed.store(false);

    EnterCriticalSection(&g_latestCs);
    g_latestVersion.clear();
    LeaveCriticalSection(&g_latestCs);

    SetStatus("Checking updates...");

    std::thread([]{
        std::string json;
        std::string tag;

        const std::wstring api = L"https://api.github.com/repos/adarec1994/Haven-Tools/releases/latest";
        if (!HttpGetToMemory(api, json) || !ExtractLatestTagName(json, tag)) {
            g_error.store(true);
            SetStatus("Update check failed");
            g_checkDone.store(true);
            g_busy.store(false);
            return;
        }

        NormalizeTagToVersion(tag);

        EnsureLatestCs();
        EnterCriticalSection(&g_latestCs);
        g_latestVersion = tag;
        LeaveCriticalSection(&g_latestCs);

        std::string local = APP_VERSION;
        if (IsRemoteNewer(tag, local)) {
            g_updateAvailable.store(true);
            std::string msg = "Update available: " + tag;
            SetStatus(msg.c_str());
        } else {
            SetStatus("Up to date");
        }

        g_progress.store(1.0f);
        g_checkDone.store(true);
        g_busy.store(false);
    }).detach();
}

bool Update::IsCheckDone()
{
    return g_checkDone.load();
}

bool Update::IsUpdateAvailable()
{
    return g_updateAvailable.load() && !g_promptDismissed.load();
}

const char* Update::GetLatestVersionText()
{
    EnsureLatestCs();
    static thread_local std::string tmp;
    EnterCriticalSection(&g_latestCs);
    tmp = g_latestVersion;
    LeaveCriticalSection(&g_latestCs);
    return tmp.c_str();
}

void Update::DismissUpdatePrompt()
{
    g_promptDismissed.store(true);
}

bool Update::WasUpdatePromptDismissed()
{
    return g_promptDismissed.load();
}

void Update::StartAutoCheckAndUpdate()
{
    static std::atomic<bool> once{ false };
    bool expected = false;
    if (!once.compare_exchange_strong(expected, true)) return;
    StartCheckForUpdates();
}
