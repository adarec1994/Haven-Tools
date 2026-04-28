// Weapons tab of the Character Designer window.
//
// Extracted from drawCharacterDesigner(); called from CharacterDesigner_UI.cpp.

#include "CharacterDesigner_Internal.h"

void drawWeaponsTab(AppState& state) {
    auto& cd = state.charDesigner;

    const char* weaponStyles[] = {
        "None",
        "Dual Swords",
        "Dual Daggers",
        "Sword + Shield",
        "Dagger + Shield",
        "Staff",
        "Greatsword",
        "Greataxe",
        "Maul"
    };

    if (ImGui::Combo("Style", &cd.weaponStyle, weaponStyles, IM_ARRAYSIZE(weaponStyles))) {
        cd.selectedMainHandWeapon = -1;
        cd.selectedOffHandWeapon = -1;
        cd.needsRebuild = true;
    }

    ImGui::Separator();

    if (cd.weaponStyle == 1) {
        ImGui::Text("Main Hand:");
        ImGui::BeginChild("MainSwords", ImVec2(0, 150), true);
        for (int i = 0; i < (int)cd.swords.size(); i++) {
            bool selected = (cd.selectedMainHandWeapon == i);
            if (ImGui::Selectable((cd.swords[i].second + "##main").c_str(), selected)) {
                cd.selectedMainHandWeapon = i;
                cd.needsRebuild = true;
            }
        }
        ImGui::EndChild();
        ImGui::Text("Off Hand:");
        ImGui::BeginChild("OffSwords", ImVec2(0, 150), true);
        for (int i = 0; i < (int)cd.swords.size(); i++) {
            bool selected = (cd.selectedOffHandWeapon == i);
            if (ImGui::Selectable((cd.swords[i].second + "##off").c_str(), selected)) {
                cd.selectedOffHandWeapon = i;
                cd.needsRebuild = true;
            }
        }
        ImGui::EndChild();
    }
    else if (cd.weaponStyle == 2) {
        ImGui::Text("Main Hand:");
        ImGui::BeginChild("MainDaggers", ImVec2(0, 150), true);
        for (int i = 0; i < (int)cd.daggers.size(); i++) {
            bool selected = (cd.selectedMainHandWeapon == i);
            if (ImGui::Selectable((cd.daggers[i].second + "##main").c_str(), selected)) {
                cd.selectedMainHandWeapon = i;
                cd.needsRebuild = true;
            }
        }
        ImGui::EndChild();
        ImGui::Text("Off Hand:");
        ImGui::BeginChild("OffDaggers", ImVec2(0, 150), true);
        for (int i = 0; i < (int)cd.daggers.size(); i++) {
            bool selected = (cd.selectedOffHandWeapon == i);
            if (ImGui::Selectable((cd.daggers[i].second + "##off").c_str(), selected)) {
                cd.selectedOffHandWeapon = i;
                cd.needsRebuild = true;
            }
        }
        ImGui::EndChild();
    }
    else if (cd.weaponStyle == 3) {
        ImGui::Text("Sword:");
        ImGui::BeginChild("MainSword", ImVec2(0, 150), true);
        for (int i = 0; i < (int)cd.swords.size(); i++) {
            bool selected = (cd.selectedMainHandWeapon == i);
            if (ImGui::Selectable((cd.swords[i].second + "##main").c_str(), selected)) {
                cd.selectedMainHandWeapon = i;
                cd.needsRebuild = true;
            }
        }
        ImGui::EndChild();
        ImGui::Text("Shield:");
        ImGui::BeginChild("OffShield", ImVec2(0, 150), true);
        for (int i = 0; i < (int)cd.shields.size(); i++) {
            bool selected = (cd.selectedOffHandWeapon == i);
            if (ImGui::Selectable((cd.shields[i].second + "##off").c_str(), selected)) {
                cd.selectedOffHandWeapon = i;
                cd.needsRebuild = true;
            }
        }
        ImGui::EndChild();
    }
    else if (cd.weaponStyle == 4) {
        ImGui::Text("Dagger:");
        ImGui::BeginChild("MainDagger", ImVec2(0, 150), true);
        for (int i = 0; i < (int)cd.daggers.size(); i++) {
            bool selected = (cd.selectedMainHandWeapon == i);
            if (ImGui::Selectable((cd.daggers[i].second + "##main").c_str(), selected)) {
                cd.selectedMainHandWeapon = i;
                cd.needsRebuild = true;
            }
        }
        ImGui::EndChild();
        ImGui::Text("Shield:");
        ImGui::BeginChild("OffShield2", ImVec2(0, 150), true);
        for (int i = 0; i < (int)cd.shields.size(); i++) {
            bool selected = (cd.selectedOffHandWeapon == i);
            if (ImGui::Selectable((cd.shields[i].second + "##off").c_str(), selected)) {
                cd.selectedOffHandWeapon = i;
                cd.needsRebuild = true;
            }
        }
        ImGui::EndChild();
    }
    else if (cd.weaponStyle == 5) {
        ImGui::BeginChild("Staves", ImVec2(0, 300), true);
        for (int i = 0; i < (int)cd.staves.size(); i++) {
            bool selected = (cd.selectedMainHandWeapon == i);
            if (ImGui::Selectable(cd.staves[i].second.c_str(), selected)) {
                cd.selectedMainHandWeapon = i;
                cd.needsRebuild = true;
            }
        }
        ImGui::EndChild();
    }
    else if (cd.weaponStyle == 6) {
        ImGui::BeginChild("Greatswords", ImVec2(0, 300), true);
        for (int i = 0; i < (int)cd.greatswords.size(); i++) {
            bool selected = (cd.selectedMainHandWeapon == i);
            if (ImGui::Selectable(cd.greatswords[i].second.c_str(), selected)) {
                cd.selectedMainHandWeapon = i;
                cd.needsRebuild = true;
            }
        }
        ImGui::EndChild();
    }
    else if (cd.weaponStyle == 7) {
        ImGui::BeginChild("Greataxes", ImVec2(0, 300), true);
        for (int i = 0; i < (int)cd.greataxes.size(); i++) {
            bool selected = (cd.selectedMainHandWeapon == i);
            if (ImGui::Selectable(cd.greataxes[i].second.c_str(), selected)) {
                cd.selectedMainHandWeapon = i;
                cd.needsRebuild = true;
            }
        }
        ImGui::EndChild();
    }
    else if (cd.weaponStyle == 8) {
        ImGui::BeginChild("Mauls", ImVec2(0, 300), true);
        for (int i = 0; i < (int)cd.mauls.size(); i++) {
            bool selected = (cd.selectedMainHandWeapon == i);
            if (ImGui::Selectable(cd.mauls[i].second.c_str(), selected)) {
                cd.selectedMainHandWeapon = i;
                cd.needsRebuild = true;
            }
        }
        ImGui::EndChild();
    }

}
