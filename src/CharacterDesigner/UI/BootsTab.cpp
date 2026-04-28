// Boots tab of the Character Designer window.
//
// Extracted from drawCharacterDesigner(); called from CharacterDesigner_UI.cpp.

#include "CharacterDesigner_Internal.h"

void drawBootsTab(AppState& state) {
    auto& cd = state.charDesigner;

    bool noneSelected = (cd.selectedBoots < 0);
    if (ImGui::Selectable("None", noneSelected)) {
        cd.selectedBoots = -1;
        cd.needsRebuild = true;
    }
    for (int i = 0; i < (int)cd.boots.size(); i++) {
        bool selected = (cd.selectedBoots == i);
        if (ImGui::Selectable(cd.boots[i].second.c_str(), selected)) {
            cd.selectedBoots = i;
            cd.bootsStyle = 0;
            cd.needsRebuild = true;
        }
    }

    if (state.hasModel && cd.selectedBoots >= 0) {
        for (const auto& mat : state.currentModel.materials) {
            std::string matName = mat.name;
            std::string matLower = matName;
            std::transform(matLower.begin(), matLower.end(), matLower.begin(), ::tolower);

            if (matLower.find("_boo_") != std::string::npos || matLower.find("_boot") != std::string::npos) {
                if (!matName.empty() && matName.back() >= 'a' && matName.back() <= 'z') {
                    std::string baseName = matName.substr(0, matName.size() - 1);
                    int maxStyle = cdGetMaxMaterialStyle(state, baseName);

                    if (maxStyle > 0) {
                        ImGui::Separator();
                        char styleChar = 'A' + cd.bootsStyle;
                        std::string styleLabel = std::string(1, styleChar);
                        ImGui::Text("Style:");
                        ImGui::SameLine();
                        if (ImGui::SliderInt("##bootsstyle", &cd.bootsStyle, 0, maxStyle, styleLabel.c_str())) {
                            cd.needsRebuild = true;
                        }
                        break;
                    }
                }
            }
        }
    }

    ImGui::Separator();
    ImGui::Text("Colors:");
    ImGui::ColorEdit3("Color 1##boots", cd.bootsTintZone1, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit3("Color 2##boots", cd.bootsTintZone2, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit3("Color 3##boots", cd.bootsTintZone3, ImGuiColorEditFlags_NoInputs);

}
