// Helmet tab of the Character Designer window.
//
// Extracted from drawCharacterDesigner(); called from CharacterDesigner_UI.cpp.

#include "CharacterDesigner_Internal.h"

void drawHelmetTab(AppState& state) {
    auto& cd = state.charDesigner;

    bool noHelmet = (cd.selectedHelmet == -1);
    if (ImGui::Selectable("Remove Helmet", noHelmet)) {
        if (cd.selectedHelmet >= 0) {
            cd.selectedHair = cd.rememberedHair;
        }
        cd.selectedHelmet = -1;
        cd.needsRebuild = true;
    }
    if (!cd.helmets.empty()) {
        ImGui::Separator();
        for (int i = 0; i < (int)cd.helmets.size(); i++) {
            bool selected = (cd.selectedHelmet == i);
            if (ImGui::Selectable(cd.helmets[i].second.c_str(), selected)) {
                if (cd.selectedHelmet < 0) {
                    cd.rememberedHair = cd.selectedHair;
                }
                cd.selectedHelmet = i;
                cd.needsRebuild = true;
            }
        }
    }

    ImGui::Separator();
    ImGui::Text("Colors:");
    ImGui::ColorEdit3("Color 1##helmet", cd.helmetTintZone1, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit3("Color 2##helmet", cd.helmetTintZone2, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit3("Color 3##helmet", cd.helmetTintZone3, ImGuiColorEditFlags_NoInputs);

}
