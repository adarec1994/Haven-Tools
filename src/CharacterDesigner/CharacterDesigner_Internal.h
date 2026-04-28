// Character-Designer-internal header. Only include from files inside
// src/CharacterDesigner/. Public consumers should #include
// "CharacterDesigner.h" instead.

#pragma once

#include "ui_internal.h"
#include "MorphLoader.h"

// Helpers implemented in CharacterDesigner.cpp. Promoted from file-static
// to module-internal so the UI/*.cpp files can reuse them.
bool cdMaterialExists(AppState& state, const std::string& matName);
int  cdGetMaxMaterialStyle(AppState& state, const std::string& baseName);
void cdLoadSelectedMorphPreset(AppState& state);
void cdLoadFaceMorphTargets(AppState& state, AppState::CharacterDesigner::FaceMorphCategory& cat);

// Per-tab draw functions, implemented in UI/<TabName>Tab.cpp and called
// from UI/CharacterDesigner_UI.cpp.
void drawHeadTab(AppState& state);
void drawFaceTab(AppState& state);
void drawArmorTab(AppState& state);
void drawClothesTab(AppState& state);
void drawBootsTab(AppState& state);
void drawGlovesTab(AppState& state);
void drawHelmetTab(AppState& state);
void drawWeaponsTab(AppState& state);
