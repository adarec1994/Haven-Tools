#pragma once
#include <string>

namespace Update
{
    bool HandleUpdaterMode(int argc, char** argv);

    bool DownloadAndApplyLatest();

    bool IsBusy();
    bool HadError();
    float GetProgress();
    const char* GetStatusText();

    const char* GetInstalledVersionText();

    void StartCheckForUpdates();
    bool IsCheckDone();
    bool IsUpdateAvailable();
    const char* GetLatestVersionText();
    void DismissUpdatePrompt();
    bool WasUpdatePromptDismissed();
    void StartAutoCheckAndUpdate();
}