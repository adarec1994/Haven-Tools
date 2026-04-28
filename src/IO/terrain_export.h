#pragma once
#include "types.h"
#include <string>

struct LevelExportOptions {
    bool useFbx = false;
};

void startLevelExport(AppState& state, const std::string& outputDir, const LevelExportOptions& options);
void tickLevelExport(AppState& state);