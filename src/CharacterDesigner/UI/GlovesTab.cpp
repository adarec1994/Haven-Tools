// Gloves tab of the Character Designer window.
//
// Extracted from drawCharacterDesigner(); called from CharacterDesigner_UI.cpp.

#include "CharacterDesigner_Internal.h"

void drawGlovesTab(AppState& state) {
    auto& cd = state.charDesigner;

    bool noneSelected = (cd.selectedGloves < 0);
    if (ImGui::Selectable("None", noneSelected)) {
        cd.selectedGloves = -1;
        cd.needsRebuild = true;
    }
    for (int i = 0; i < (int)cd.gloves.size(); i++) {
        bool selected = (cd.selectedGloves == i);
        if (ImGui::Selectable(cd.gloves[i].second.c_str(), selected)) {
            cd.selectedGloves = i;
            cd.glovesStyle = 0;
            cd.needsRebuild = true;
        }
    }

    if (state.hasModel && cd.selectedGloves >= 0) {
        for (const auto& mat : state.currentModel.materials) {
            std::string matName = mat.name;
            std::string matLower = matName;
            std::transform(matLower.begin(), matLower.end(), matLower.begin(), ::tolower);

            if (matLower.find("_glv_") != std::string::npos || matLower.find("_glove") != std::string::npos) {
                if (!matName.empty() && matName.back() >= 'a' && matName.back() <= 'z') {
                    std::string baseName = matName.substr(0, matName.size() - 1);
                    int maxStyle = cdGetMaxMaterialStyle(state, baseName);

                    if (maxStyle > 0) {
                        ImGui::Separator();
                        char styleChar = 'A' + cd.glovesStyle;
                        std::string styleLabel = std::string(1, styleChar);
                        ImGui::Text("Style:");
                        ImGui::SameLine();
                        if (ImGui::SliderInt("##glovesstyle", &cd.glovesStyle, 0, maxStyle, styleLabel.c_str())) {
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
    ImGui::ColorEdit3("Color 1##gloves", cd.glovesTintZone1, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit3("Color 2##gloves", cd.glovesTintZone2, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit3("Color 3##gloves", cd.glovesTintZone3, ImGuiColorEditFlags_NoInputs);

}
