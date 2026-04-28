// Face tab of the Character Designer window.
//
// Extracted from drawCharacterDesigner(); called from CharacterDesigner_UI.cpp.

#include "CharacterDesigner_Internal.h"

void drawFaceTab(AppState& state) {
    auto& cd = state.charDesigner;

    if (cd.faceCategories.empty()) {
        ImGui::TextDisabled("No face morphs available");
    } else {
        bool anyChanged = false;
        for (int ci = 0; ci < (int)cd.faceCategories.size(); ci++) {
            auto& cat = cd.faceCategories[ci];
            ImGui::Text("%s:", cat.name.c_str());

            std::string currentLabel = (cat.selected <= 0) ? "Default" :
                ((cat.selected <= (int)cat.variants.size())
                    ? cat.variants[cat.selected - 1].label : "Default");

            int val = cat.selected;
            int maxVal = (int)cat.variants.size();
            std::string sliderLabel = (val == 0) ? "Default" : currentLabel;
            std::string id = "##face_" + cat.code;
            if (ImGui::SliderInt(id.c_str(), &val, 0, maxVal, sliderLabel.c_str())) {
                if (val != cat.selected) {
                    cat.selected = val;
                    cdLoadFaceMorphTargets(state, cat);
                    anyChanged = true;
                }
            }
        }
        if (anyChanged) {
            cd.needsRebuild = true;
        }

        ImGui::Separator();
        if (ImGui::Button("Reset All Face Morphs")) {
            for (auto& cat : cd.faceCategories) {
                cat.selected = 0;
                cat.headTarget.clear();
                cat.eyesTarget.clear();
                cat.lashesTarget.clear();
            }
            cd.needsRebuild = true;
        }
    }
}
