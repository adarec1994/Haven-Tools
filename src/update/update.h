#pragma once

namespace Update
{
    bool HandleUpdaterMode(int argc, char** argv);

    void StartDownloadAndApplyLatest();
    bool IsBusy();
    float GetProgress();
    const char* GetStatusText();
    bool HadError();
}
