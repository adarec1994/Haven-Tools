// Head tab of the Character Designer window.
//
// Extracted from drawCharacterDesigner(); called from CharacterDesigner_UI.cpp.

#include "CharacterDesigner_Internal.h"

void drawHeadTab(AppState& state) {
    auto& cd = state.charDesigner;

    if (!cd.availableMorphPresets.empty()) {
        ImGui::Text("Face Presets:");

        std::string currentPreset = (cd.selectedMorphPreset < 0) ? "Default" :
            (cd.selectedMorphPreset < (int)cd.availableMorphPresets.size()
                ? cd.availableMorphPresets[cd.selectedMorphPreset].displayName : "Default");

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 30);
        if (ImGui::BeginCombo("##morphpreset", currentPreset.c_str())) {
            bool defaultSelected = (cd.selectedMorphPreset < 0);
            if (ImGui::Selectable("Default", defaultSelected)) {
                cd.selectedMorphPreset = -1;
                cd.morphLoaded = false;
                cd.morphData = MorphData();
                cd.baseHeadVertices.clear();
                cd.baseEyesVertices.clear();
                cd.baseLashesVertices.clear();
                cd.needsRebuild = true;
            }
            if (defaultSelected) ImGui::SetItemDefaultFocus();

            ImGui::Separator();

            for (int i = 0; i < (int)cd.availableMorphPresets.size(); i++) {
                bool selected = (cd.selectedMorphPreset == i);
                if (ImGui::Selectable(cd.availableMorphPresets[i].displayName.c_str(), selected)) {
                    cd.selectedMorphPreset = i;
                    cdLoadSelectedMorphPreset(state);
                    cd.needsRebuild = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::Button("X##resetpreset")) {
            cd.selectedMorphPreset = -1;
            cd.morphLoaded = false;
            cd.morphData = MorphData();
            cd.baseHeadVertices.clear();
            cd.baseEyesVertices.clear();
            cd.baseLashesVertices.clear();
            cd.needsRebuild = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reset to Default");
        }

        ImGui::Separator();
    }

    ImGui::ColorEdit3("Skin Color", state.renderSettings.skinColor, ImGuiColorEditFlags_NoInputs);
    ImGui::Separator();

    ImGui::Text("Hair:");
    if (!cd.hairs.empty()) {
        std::string currentHair = (cd.selectedHair >= 0 && cd.selectedHair < (int)cd.hairs.size())
            ? cd.hairs[cd.selectedHair].second : "None";
        int hairIdx = cd.selectedHair;
        if (ImGui::SliderInt("##hair", &hairIdx, 0, (int)cd.hairs.size() - 1, currentHair.c_str())) {
            cd.selectedHair = hairIdx;
            cd.selectedHelmet = -1;
            cd.needsRebuild = true;
        }
    }
    ImGui::ColorEdit3("Hair Color", state.renderSettings.hairColor, ImGuiColorEditFlags_NoInputs);

    if (cd.isMale && !cd.beards.empty()) {
        ImGui::Separator();
        ImGui::Text("Beard:");
        std::string currentBeard = (cd.selectedBeard < 0) ? "None" :
            (cd.selectedBeard < (int)cd.beards.size() ? cd.beards[cd.selectedBeard].second : "None");
        int beardIdx = cd.selectedBeard + 1;
        int maxBeard = (int)cd.beards.size();
        std::string beardLabel = (beardIdx == 0) ? "None" : currentBeard;
        if (ImGui::SliderInt("##beard", &beardIdx, 0, maxBeard, beardLabel.c_str())) {
            cd.selectedBeard = beardIdx - 1;
            cd.selectedHelmet = -1;
            cd.needsRebuild = true;
        }
        ImGui::Text("Stubble:");
        ImGui::SliderFloat("Style 1##stubble", &cd.stubbleAmount[0], 0.0f, 1.0f);
        ImGui::SliderFloat("Style 2##stubble", &cd.stubbleAmount[1], 0.0f, 1.0f);
        ImGui::SliderFloat("Style 3##stubble", &cd.stubbleAmount[2], 0.0f, 1.0f);
        ImGui::SliderFloat("Style 4##stubble", &cd.stubbleAmount[3], 0.0f, 1.0f);
    }
    ImGui::Separator();

    ImGui::ColorEdit3("Eye Color", cd.eyeColor, ImGuiColorEditFlags_NoInputs);
    ImGui::Separator();

    ImGui::SliderFloat("Age", &cd.ageAmount, 0.0f, 1.0f);
    ImGui::Separator();

    ImGui::Text("Tattoo:");
    if (!cd.tattoos.empty()) {
        std::string currentTattoo = (cd.selectedTattoo < 0) ? "None" :
            (cd.selectedTattoo < (int)cd.tattoos.size() ? cd.tattoos[cd.selectedTattoo].second : "None");
        if (ImGui::BeginCombo("##tattooselect", currentTattoo.c_str())) {
            for (int i = 0; i < (int)cd.tattoos.size(); i++) {
                bool selected = (cd.selectedTattoo == i) || (i == 0 && cd.selectedTattoo < 0);
                if (ImGui::Selectable(cd.tattoos[i].second.c_str(), selected)) {
                    cd.selectedTattoo = (i == 0) ? -1 : i;
                    state.renderSettings.selectedTattoo = cd.selectedTattoo;
                    cd.needsRebuild = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }
    ImGui::SliderFloat("Style 1##tattoo", &cd.tattooAmount[0], 0.0f, 1.0f);
    ImGui::ColorEdit3("Color 1##tattoo", cd.tattooColor1, ImGuiColorEditFlags_NoInputs);
    ImGui::SliderFloat("Style 2##tattoo", &cd.tattooAmount[1], 0.0f, 1.0f);
    ImGui::ColorEdit3("Color 2##tattoo", cd.tattooColor2, ImGuiColorEditFlags_NoInputs);
    ImGui::SliderFloat("Style 3##tattoo", &cd.tattooAmount[2], 0.0f, 1.0f);
    ImGui::ColorEdit3("Color 3##tattoo", cd.tattooColor3, ImGuiColorEditFlags_NoInputs);
    ImGui::Separator();

    ImGui::Text("Makeup:");
    ImGui::ColorEdit3("Lips", cd.headTintZone1, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit3("Eyeshadow", cd.headTintZone2, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit3("Blush", cd.headTintZone3, ImGuiColorEditFlags_NoInputs);
}
