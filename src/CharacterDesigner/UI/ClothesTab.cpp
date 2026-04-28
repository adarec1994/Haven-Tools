// Clothes tab of the Character Designer window.
//
// Extracted from drawCharacterDesigner(); called from CharacterDesigner_UI.cpp.

#include "CharacterDesigner_Internal.h"

void drawClothesTab(AppState& state) {
    auto& cd = state.charDesigner;

    if (cd.clothes.empty()) {
        ImGui::TextDisabled("No clothes available");
    } else {
        for (int i = 0; i < (int)cd.clothes.size(); i++) {
            bool selected = (cd.selectedClothes == i);
            if (ImGui::Selectable(cd.clothes[i].second.c_str(), selected)) {
                cd.selectedClothes = i;
                cd.selectedArmor = -1;
                cd.selectedRobe = -1;
                cd.selectedBoots = -1;
                cd.selectedGloves = -1;
                cd.clothesStyle = 0;
                cd.needsRebuild = true;
            }
        }

        if (state.hasModel && cd.selectedClothes >= 0) {
            for (const auto& mat : state.currentModel.materials) {
                std::string matName = mat.name;
                std::string matLower = matName;
                std::transform(matLower.begin(), matLower.end(), matLower.begin(), ::tolower);

                if (matLower.find("_cth_") != std::string::npos || matLower.find("_clo") != std::string::npos) {
                    if (!matName.empty() && matName.back() >= 'a' && matName.back() <= 'z') {
                        std::string baseName = matName.substr(0, matName.size() - 1);
                        int maxStyle = cdGetMaxMaterialStyle(state, baseName);

                        if (maxStyle > 0) {
                            ImGui::Separator();
                            char styleChar = 'A' + cd.clothesStyle;
                            std::string styleLabel = std::string(1, styleChar);
                            ImGui::Text("Style:");
                            ImGui::SameLine();
                            if (ImGui::SliderInt("##clothesstyle", &cd.clothesStyle, 0, maxStyle, styleLabel.c_str())) {
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
        ImGui::ColorEdit3("Color 1##clothes", cd.clothesTintZone1, ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit3("Color 2##clothes", cd.clothesTintZone2, ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit3("Color 3##clothes", cd.clothesTintZone3, ImGuiColorEditFlags_NoInputs);
    }
}
