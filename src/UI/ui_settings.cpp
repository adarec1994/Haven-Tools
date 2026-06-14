#include "ui_internal.h"

static const char* SETTINGS_FILE = "haventools_settings.ini";

void saveSettings(const AppState& state) {
    std::ofstream f(SETTINGS_FILE);
    if (f.is_open()) {
        f << "lastDialogPath=" << state.lastDialogPath << "\n";
        f << "selectedFolder=" << state.selectedFolder << "\n";
        f << "overrideFolder=" << state.overrideFolder << "\n";
        f << "lastErfPath=" << state.lastErfPath << "\n";
        for (const auto& erfPath : state.extraErfPaths) {
            f << "extraErfPath=" << erfPath << "\n";
        }
        f << "lastRunVersion=" << state.lastRunVersion << "\n";
        f << "uiFontSize=" << state.uiFontSize << "\n";
        f << "kb_moveForward=" << (int)state.keybinds.moveForward << "\n";
        f << "kb_moveBackward=" << (int)state.keybinds.moveBackward << "\n";
        f << "kb_moveLeft=" << (int)state.keybinds.moveLeft << "\n";
        f << "kb_moveRight=" << (int)state.keybinds.moveRight << "\n";
        f << "kb_panUp=" << (int)state.keybinds.panUp << "\n";
        f << "kb_panDown=" << (int)state.keybinds.panDown << "\n";
        f << "kb_deselectBone=" << (int)state.keybinds.deselectBone << "\n";
        f << "kb_deleteObject=" << (int)state.keybinds.deleteObject << "\n";
        f << "kb_boneRotate=" << (int)state.keybinds.boneRotate << "\n";
        f << "kb_boneGrab=" << (int)state.keybinds.boneGrab << "\n";
    }
}

static int safeStoi(const std::string& val, int fallback = 0) {
    if (val.empty()) return fallback;
    try { return std::stoi(val); }
    catch (...) { return fallback; }
}

static float safeStof(const std::string& val, float fallback = 0.0f) {
    if (val.empty()) return fallback;
    try { return std::stof(val); }
    catch (...) { return fallback; }
}

static void addUniqueErfPath(std::vector<std::string>& paths, const std::string& path) {
    if (path.empty()) return;
    if (std::find(paths.begin(), paths.end(), path) == paths.end()) {
        paths.push_back(path);
    }
}

void loadSettings(AppState& state) {
    std::ifstream f(SETTINGS_FILE);
    if (!f.is_open()) return;
    try {
        state.loadedErfPath.clear();
        state.extraErfPaths.clear();
        std::string line;
        while (std::getline(f, line)) {
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);

            if (key == "lastDialogPath") state.lastDialogPath = val;
            else if (key == "selectedFolder") { state.selectedFolder = val; state.gffViewer.gamePath = val; }
            else if (key == "overrideFolder") { state.overrideFolder = val; state.gffViewer.overridePath = val; }
            else if (key == "loadedErfPath") addUniqueErfPath(state.extraErfPaths, val);
            else if (key == "extraErfPath") addUniqueErfPath(state.extraErfPaths, val);
            else if (key == "lastErfPath") state.lastErfPath = val;
            else if (key == "lastRunVersion") state.lastRunVersion = val;
            else if (key == "uiFontSize") state.uiFontSize = clampUIFontSize(safeStof(val, UI_FONT_SIZE_DEFAULT));
            else if (key == "kb_moveForward") state.keybinds.moveForward = (ImGuiKey)safeStoi(val);
            else if (key == "kb_moveBackward") state.keybinds.moveBackward = (ImGuiKey)safeStoi(val);
            else if (key == "kb_moveLeft") state.keybinds.moveLeft = (ImGuiKey)safeStoi(val);
            else if (key == "kb_moveRight") state.keybinds.moveRight = (ImGuiKey)safeStoi(val);
            else if (key == "kb_panUp") state.keybinds.panUp = (ImGuiKey)safeStoi(val);
            else if (key == "kb_panDown") state.keybinds.panDown = (ImGuiKey)safeStoi(val);
            else if (key == "kb_deselectBone") state.keybinds.deselectBone = (ImGuiKey)safeStoi(val);
            else if (key == "kb_deleteObject") state.keybinds.deleteObject = (ImGuiKey)safeStoi(val);
            else if (key == "kb_boneRotate") state.keybinds.boneRotate = (ImGuiKey)safeStoi(val);
            else if (key == "kb_boneGrab") state.keybinds.boneGrab = (ImGuiKey)safeStoi(val);
        }
    } catch (...) {
    }
}
