#pragma once
#include <string>

namespace Update
{
    bool HandleUpdaterMode(int argc, char** argv);

    bool DownloadAndApplyLatest();
}
