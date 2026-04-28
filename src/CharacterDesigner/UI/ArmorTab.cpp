// Armor tab of the Character Designer window.
//
// Extracted from drawCharacterDesigner(); called from CharacterDesigner_UI.cpp.

#include "CharacterDesigner_Internal.h"
#include <algorithm>

void drawArmorTab(AppState& state) {
    auto& cd = state.charDesigner;

    ImGui::TextDisabled("Body Armor:");
    for (int i = 0; i < (int)cd.armors.size(); i++) {
        bool selected = (cd.selectedArmor == i && cd.selectedRobe < 0 && cd.selectedClothes < 0);
        if (ImGui::Selectable(cd.armors[i].second.c_str(), selected)) {
            cd.selectedArmor = i;
            cd.selectedRobe = -1;
            cd.selectedClothes = -1;
            cd.armorStyle = 0;
            cd.needsRebuild = true;
        }
    }

    if (!cd.robes.empty()) {
        ImGui::Separator();
        ImGui::TextDisabled("Robes:");
        for (int i = 0; i < (int)cd.robes.size(); i++) {
            bool selected = (cd.selectedRobe == i);
            if (ImGui::Selectable(cd.robes[i].second.c_str(), selected)) {
                cd.selectedRobe = i;
                cd.selectedClothes = -1;
                cd.selectedArmor = -1;
                cd.armorStyle = 0;
                cd.needsRebuild = true;
            }
        }
    }

    if (state.hasModel && cd.selectedArmor >= 0 && cd.selectedRobe < 0 && cd.selectedClothes < 0) {
        for (const auto& mat : state.currentModel.materials) {
            std::string matLower = mat.name;
            std::transform(matLower.begin(), matLower.end(), matLower.begin(), ::tolower);

            if (matLower.find("_arm_") == std::string::npos &&
                matLower.find("_mas") == std::string::npos &&
                matLower.find("_med") == std::string::npos &&
                matLower.find("_hvy") == std::string::npos &&
                matLower.find("_lgt") == std::string::npos) {
                continue;
            }

            if (matLower.back() == 'a') {
                std::string baseName = matLower.substr(0, matLower.size() - 1);
                if (cdMaterialExists(state, baseName + "b")) {
                    int maxStyle = cdGetMaxMaterialStyle(state, baseName);
                    if (maxStyle > 0) {
                        ImGui::Separator();
                        char styleChar = 'A' + cd.armorStyle;
                        std::string styleLabel = std::string(1, styleChar);
                        ImGui::Text("Style:");
                        ImGui::SameLine();
                        if (ImGui::SliderInt("##armorstyle", &cd.armorStyle, 0, maxStyle, styleLabel.c_str())) {
                            cd.needsRebuild = true;
                        }
                        break;
                    }
                }
            }
        }
    }

    ImGui::Separator();

    // ---- Armor tint preset dropdown ----
    // TNT layout (Vector4f per slot, .a is padding for slots 0-7):
    //   [0] TINT_MASK_DIFFUSE_R   -> diffuse for tint mask R channel  (zone 1)
    //   [1] TINT_MASK_DIFFUSE_G   -> diffuse for tint mask G channel  (zone 2)
    //   [2] TINT_MASK_DIFFUSE_B   -> diffuse for tint mask B channel  (zone 3)
    //   [3] TINT_MASK_SPECULAR_R  -> specular for tint mask R channel (zone 1)
    //   [4] TINT_MASK_SPECULAR_G  -> specular for tint mask G channel (zone 2)
    //   [5] TINT_MASK_SPECULAR_B  -> specular for tint mask B channel (zone 3)
    //   [6] TINT_MASK_DIFFUSE_A   -> diffuse for tint mask A channel  (zone 4)
    //   [7] TINT_MASK_SPECULAR_A  -> specular for tint mask A channel (zone 4)
    //   [8] TINT_MASK_DIFFUSE_OPACITY  -> rgba = per-zone diffuse opacity
    //   [9] TINT_MASK_SPECULAR_OPACITY -> rgba = per-zone specular opacity
    static std::vector<std::string> s_armorTints;
    static size_t s_armorTintsCacheSize = (size_t)-1;
    size_t curCacheSize = state.tintCache.getTintNames().size();
    if (curCacheSize != s_armorTintsCacheSize) {
        s_armorTints = state.tintCache.getTintNames();
        std::sort(s_armorTints.begin(), s_armorTints.end());
        s_armorTintsCacheSize = curCacheSize;
    }

    auto setIdentity = [&]() {
        for (int i = 0; i < 3; i++) {
            cd.armorTintZone1[i]     = 1.0f;
            cd.armorTintZone2[i]     = 1.0f;
            cd.armorTintZone3[i]     = 1.0f;
            cd.armorTintZone4[i]     = 1.0f;
            cd.armorTintSpecZone1[i] = 1.0f;
            cd.armorTintSpecZone2[i] = 1.0f;
            cd.armorTintSpecZone3[i] = 1.0f;
            cd.armorTintSpecZone4[i] = 1.0f;
        }
        for (int i = 0; i < 4; i++) {
            cd.armorTintDiffOpacity[i] = 1.0f;
            cd.armorTintSpecOpacity[i] = 1.0f;
        }
    };

    auto applyPreset = [&](const std::string& name) {
        const TintData* t = state.tintCache.getTint(name);
        if (!t) { setIdentity(); return; }
        // Slot indexing matches GFF field IDs 14000..14009 (loaded by ID, not
        // binary order, so this is consistent across all TNT files):
        //   colors[0] = DIFFUSE_R  (zone 1 diffuse — mask.r region)
        //   colors[1] = DIFFUSE_G  (zone 2 diffuse — mask.g region)
        //   colors[2] = DIFFUSE_B  (zone 3 diffuse — mask.b region)
        //   colors[3] = SPECULAR_R (zone 1 specular)
        //   colors[4] = SPECULAR_G (zone 2 specular)
        //   colors[5] = SPECULAR_B (zone 3 specular)
        //   colors[6] = DIFFUSE_A  (zone 4 diffuse — mask.a region)
        //   colors[7] = SPECULAR_A (zone 4 specular)
        //   colors[8] = DIFFUSE_OPACITY  (vec4: per-zone opacity for diffuse)
        //   colors[9] = SPECULAR_OPACITY (vec4: per-zone opacity for specular)
        // Diffuse zones (slots 0, 1, 2, 6):
        cd.armorTintZone1[0] = t->colors[0].r; cd.armorTintZone1[1] = t->colors[0].g; cd.armorTintZone1[2] = t->colors[0].b;
        cd.armorTintZone2[0] = t->colors[1].r; cd.armorTintZone2[1] = t->colors[1].g; cd.armorTintZone2[2] = t->colors[1].b;
        cd.armorTintZone3[0] = t->colors[2].r; cd.armorTintZone3[1] = t->colors[2].g; cd.armorTintZone3[2] = t->colors[2].b;
        cd.armorTintZone4[0] = t->colors[6].r; cd.armorTintZone4[1] = t->colors[6].g; cd.armorTintZone4[2] = t->colors[6].b;
        // Specular zones (slots 3, 4, 5, 7):
        cd.armorTintSpecZone1[0] = t->colors[3].r; cd.armorTintSpecZone1[1] = t->colors[3].g; cd.armorTintSpecZone1[2] = t->colors[3].b;
        cd.armorTintSpecZone2[0] = t->colors[4].r; cd.armorTintSpecZone2[1] = t->colors[4].g; cd.armorTintSpecZone2[2] = t->colors[4].b;
        cd.armorTintSpecZone3[0] = t->colors[5].r; cd.armorTintSpecZone3[1] = t->colors[5].g; cd.armorTintSpecZone3[2] = t->colors[5].b;
        cd.armorTintSpecZone4[0] = t->colors[7].r; cd.armorTintSpecZone4[1] = t->colors[7].g; cd.armorTintSpecZone4[2] = t->colors[7].b;
        // Opacities: slot [8].rgba -> per-zone diffuse, slot [9].rgba -> per-zone spec
        cd.armorTintDiffOpacity[0] = t->colors[8].r;
        cd.armorTintDiffOpacity[1] = t->colors[8].g;
        cd.armorTintDiffOpacity[2] = t->colors[8].b;
        cd.armorTintDiffOpacity[3] = t->colors[8].a;
        cd.armorTintSpecOpacity[0] = t->colors[9].r;
        cd.armorTintSpecOpacity[1] = t->colors[9].g;
        cd.armorTintSpecOpacity[2] = t->colors[9].b;
        cd.armorTintSpecOpacity[3] = t->colors[9].a;
    };

    ImGui::Text("Tint Preset:");
    ImGui::SameLine();
    const char* preview = cd.armorTintPreset.empty() ? "(none)" : cd.armorTintPreset.c_str();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::BeginCombo("##armortintpreset", preview)) {
        if (ImGui::Selectable("(none)", cd.armorTintPreset.empty())) {
            cd.armorTintPreset.clear();
            setIdentity();
        }
        for (const auto& name : s_armorTints) {
            bool selected = (cd.armorTintPreset == name);
            if (ImGui::Selectable(name.c_str(), selected)) {
                cd.armorTintPreset = name;
                applyPreset(name);
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::TextDisabled("Diffuse:");
    ImGui::ColorEdit3("Color 1##armor",  cd.armorTintZone1, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit3("Color 2##armor",  cd.armorTintZone2, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit3("Color 3##armor",  cd.armorTintZone3, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit3("Color 4##armor",  cd.armorTintZone4, ImGuiColorEditFlags_NoInputs);

    ImGui::TextDisabled("Specular:");
    ImGui::ColorEdit3("Spec 1##armor",   cd.armorTintSpecZone1, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit3("Spec 2##armor",   cd.armorTintSpecZone2, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit3("Spec 3##armor",   cd.armorTintSpecZone3, ImGuiColorEditFlags_NoInputs);
    ImGui::ColorEdit3("Spec 4##armor",   cd.armorTintSpecZone4, ImGuiColorEditFlags_NoInputs);

    if (ImGui::TreeNode("Opacity")) {
        ImGui::TextDisabled("How strongly each zone's tint is applied (0 = off).");
        ImGui::SliderFloat4("Diffuse##op",  cd.armorTintDiffOpacity, 0.0f, 1.0f);
        ImGui::SliderFloat4("Specular##op", cd.armorTintSpecOpacity, 0.0f, 1.0f);
        ImGui::TreePop();
    }

}
