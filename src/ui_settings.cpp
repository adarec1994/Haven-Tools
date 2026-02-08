#include "ui_internal.h"

static const char* SETTINGS_FILE = "haventools_settings.ini";

void saveSettings(const AppState& state) {
    std::ofstream f(SETTINGS_FILE);
    if (f.is_open()) {
        f << "lastDialogPath=" << state.lastDialogPath << "\n";
        f << "selectedFolder=" << state.selectedFolder << "\n";
        f << "lastRunVersion=" << state.lastRunVersion << "\n";
        f << "kb_moveForward=" << (int)state.keybinds.moveForward << "\n";
        f << "kb_moveBackward=" << (int)state.keybinds.moveBackward << "\n";
        f << "kb_moveLeft=" << (int)state.keybinds.moveLeft << "\n";
        f << "kb_moveRight=" << (int)state.keybinds.moveRight << "\n";
        f << "kb_panUp=" << (int)state.keybinds.panUp << "\n";
        f << "kb_panDown=" << (int)state.keybinds.panDown << "\n";
        f << "kb_deselectBone=" << (int)state.keybinds.deselectBone << "\n";
    }
}

void loadSettings(AppState& state) {
    std::ifstream f(SETTINGS_FILE);
    if (f.is_open()) {
        std::string line;
        while (std::getline(f, line)) {
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                std::string key = line.substr(0, eq);
                std::string val = line.substr(eq + 1);

                if (key == "lastDialogPath") state.lastDialogPath = val;
                else if (key == "selectedFolder") state.selectedFolder = val;
                else if (key == "lastRunVersion") state.lastRunVersion = val;
                else if (key == "kb_moveForward") state.keybinds.moveForward = (ImGuiKey)std::stoi(val);
                else if (key == "kb_moveBackward") state.keybinds.moveBackward = (ImGuiKey)std::stoi(val);
                else if (key == "kb_moveLeft") state.keybinds.moveLeft = (ImGuiKey)std::stoi(val);
                else if (key == "kb_moveRight") state.keybinds.moveRight = (ImGuiKey)std::stoi(val);
                else if (key == "kb_panUp") state.keybinds.panUp = (ImGuiKey)std::stoi(val);
                else if (key == "kb_panDown") state.keybinds.panDown = (ImGuiKey)std::stoi(val);
                else if (key == "kb_deselectBone") state.keybinds.deselectBone = (ImGuiKey)std::stoi(val);
            }
        }
    }
}