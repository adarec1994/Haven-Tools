// Public Character Designer API.
//
// Implementation lives across CharacterDesigner.cpp (logic / data) and
// UI/*.cpp (tab-by-tab UI). Internal helpers shared between those files
// live in CharacterDesigner_Internal.h (do not include from outside this
// folder).

#pragma once

struct AppState;
struct ImGuiIO;

// Lifecycle / loading
void preloadCharacterData(AppState& state);
void buildCharacterLists(AppState& state);
void loadCharacterModel(AppState& state);

// Top-level UI
void drawCharacterDesigner(AppState& state, ImGuiIO& io);

// Misc utilities first defined here but called elsewhere
void buildMaterialCache(AppState& state, float startProgress = 0.0f, float endProgress = 1.0f);
void filterEncryptedErfs(AppState& state);
