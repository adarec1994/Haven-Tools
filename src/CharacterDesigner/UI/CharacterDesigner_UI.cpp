// Top-level Character Designer window. Holds the race/gender radios, the
// state-to-RenderSettings copy, and the tab bar. Each tab's body lives in
// its own UI/<n>Tab.cpp.

#include "CharacterDesigner_Internal.h"

void drawCharacterDesigner(AppState& state, ImGuiIO& io) {
    buildMaterialCache(state);  // idempotent; early-outs if already built

    auto& cd = state.charDesigner;

    // Push designer state into the live render settings before any drawing,
    // so changes show up the same frame they're made.
    memcpy(state.renderSettings.eyeColor,      cd.eyeColor,         sizeof(float) * 3);
    state.renderSettings.ageAmount =           cd.ageAmount;
    memcpy(state.renderSettings.stubbleAmount, cd.stubbleAmount,    sizeof(float) * 4);
    memcpy(state.renderSettings.tattooAmount,  cd.tattooAmount,     sizeof(float) * 3);
    memcpy(state.renderSettings.tattooColor1,  cd.tattooColor1,     sizeof(float) * 3);
    memcpy(state.renderSettings.tattooColor2,  cd.tattooColor2,     sizeof(float) * 3);
    memcpy(state.renderSettings.tattooColor3,  cd.tattooColor3,     sizeof(float) * 3);
    memcpy(state.renderSettings.headZone1,     cd.headTintZone1,    sizeof(float) * 3);
    memcpy(state.renderSettings.headZone2,     cd.headTintZone2,    sizeof(float) * 3);
    memcpy(state.renderSettings.headZone3,     cd.headTintZone3,    sizeof(float) * 3);
    memcpy(state.renderSettings.armorZone1,    cd.armorTintZone1,   sizeof(float) * 3);
    memcpy(state.renderSettings.armorZone2,    cd.armorTintZone2,   sizeof(float) * 3);
    memcpy(state.renderSettings.armorZone3,    cd.armorTintZone3,   sizeof(float) * 3);
    memcpy(state.renderSettings.armorZone4,    cd.armorTintZone4,   sizeof(float) * 3);
    memcpy(state.renderSettings.armorSpecZone1, cd.armorTintSpecZone1, sizeof(float) * 3);
    memcpy(state.renderSettings.armorSpecZone2, cd.armorTintSpecZone2, sizeof(float) * 3);
    memcpy(state.renderSettings.armorSpecZone3, cd.armorTintSpecZone3, sizeof(float) * 3);
    memcpy(state.renderSettings.armorSpecZone4, cd.armorTintSpecZone4, sizeof(float) * 3);
    memcpy(state.renderSettings.armorDiffOpacity, cd.armorTintDiffOpacity, sizeof(float) * 4);
    memcpy(state.renderSettings.armorSpecOpacity, cd.armorTintSpecOpacity, sizeof(float) * 4);
    // Use additive game-math when a preset is loaded; multiplicative for manual editing.
    state.renderSettings.armorTintReplaceFlag = cd.armorTintPreset.empty() ? 0.0f : 1.0f;
    memcpy(state.renderSettings.clothesZone1,  cd.clothesTintZone1, sizeof(float) * 3);
    memcpy(state.renderSettings.clothesZone2,  cd.clothesTintZone2, sizeof(float) * 3);
    memcpy(state.renderSettings.clothesZone3,  cd.clothesTintZone3, sizeof(float) * 3);
    memcpy(state.renderSettings.bootsZone1,    cd.bootsTintZone1,   sizeof(float) * 3);
    memcpy(state.renderSettings.bootsZone2,    cd.bootsTintZone2,   sizeof(float) * 3);
    memcpy(state.renderSettings.bootsZone3,    cd.bootsTintZone3,   sizeof(float) * 3);
    memcpy(state.renderSettings.glovesZone1,   cd.glovesTintZone1,  sizeof(float) * 3);
    memcpy(state.renderSettings.glovesZone2,   cd.glovesTintZone2,  sizeof(float) * 3);
    memcpy(state.renderSettings.glovesZone3,   cd.glovesTintZone3,  sizeof(float) * 3);
    memcpy(state.renderSettings.helmetZone1,   cd.helmetTintZone1,  sizeof(float) * 3);
    memcpy(state.renderSettings.helmetZone2,   cd.helmetTintZone2,  sizeof(float) * 3);
    memcpy(state.renderSettings.helmetZone3,   cd.helmetTintZone3,  sizeof(float) * 3);

    ImGui::SetNextWindowSize(ImVec2(350, 550), ImGuiCond_FirstUseEver);
    ImGui::Begin("Character Designer", nullptr, ImGuiWindowFlags_NoCollapse);

    // Race + gender. Changing either resets the model + lists wholesale.
    ImGui::Text("Race:");
    ImGui::SameLine();
    bool raceChanged = false;
    if (ImGui::RadioButton("Human", cd.race == 0)) { cd.race = 0; raceChanged = true; }
    ImGui::SameLine();
    if (ImGui::RadioButton("Elf",   cd.race == 1)) { cd.race = 1; raceChanged = true; }
    ImGui::SameLine();
    if (ImGui::RadioButton("Dwarf", cd.race == 2)) { cd.race = 2; raceChanged = true; }

    ImGui::Text("Gender:");
    ImGui::SameLine();
    bool genderChanged = false;
    if (ImGui::RadioButton("Male",   cd.isMale))  { cd.isMale = true;  genderChanged = true; }
    ImGui::SameLine();
    if (ImGui::RadioButton("Female", !cd.isMale)) { cd.isMale = false; genderChanged = true; }

    if (raceChanged || genderChanged) {
        cd.listsBuilt = false;
        cd.needsRebuild = true;
        cd.animsLoaded = false;
        cd.partCache.clear();
        cd.morphLoaded = false;
        cd.availableMorphPresets.clear();
        cd.selectedMorphPreset = -1;
        cd.baseHeadVertices.clear();
        cd.baseEyesVertices.clear();
        cd.baseLashesVertices.clear();
        cd.headMeshIndex = -1;
        cd.eyesMeshIndex = -1;
        cd.lashesMeshIndex = -1;
        cd.selectedHead = 0;
        cd.selectedHair = 0;
        cd.selectedBeard = -1;
        cd.selectedArmor = 0;
        cd.selectedBoots = 0;
        cd.selectedGloves = 0;
        cd.selectedHelmet = -1;
        cd.faceMorphsBuilt = false;
        for (auto& cat : cd.faceCategories) {
            cat.selected = 0;
            cat.headTarget.clear();
            cat.eyesTarget.clear();
            cat.lashesTarget.clear();
        }
    }

    buildCharacterLists(state);
    ImGui::Separator();

    if (ImGui::BeginTabBar("EquipTabs")) {
        if (ImGui::BeginTabItem("Head"))    { drawHeadTab(state);    ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Face"))    { drawFaceTab(state);    ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Armor"))   { drawArmorTab(state);   ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Clothes")) { drawClothesTab(state); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Boots"))   { drawBootsTab(state);   ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Gloves"))  { drawGlovesTab(state);  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Helmet"))  { drawHelmetTab(state);  ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Weapons")) { drawWeaponsTab(state); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    ImGui::End();

    // Animation tick + deferred model rebuild after any tab changed something.
    if (state.animPlaying && state.currentAnim.duration > 0) {
        state.animTime += io.DeltaTime * state.animSpeed;
        if (state.animTime > state.currentAnim.duration) state.animTime = 0.0f;
    }
    if (cd.needsRebuild && state.modelErfsLoaded) {
        loadCharacterModel(state);
    }
}
