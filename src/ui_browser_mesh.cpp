#include "ui_internal.h"

void drawMeshBrowserWindow(AppState& state) {
    loadMeshDatabase(state);
    ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
    ImGui::Begin("Mesh Browser", &state.showMeshBrowser);
    if (state.meshBrowser.allMeshes.empty()) {
        ImGui::TextDisabled("No mesh database loaded.");
        ImGui::TextDisabled("Place model_names.csv in exe directory.");
        ImGui::End();
        return;
    }
    ImGui::Checkbox("Categorized", &state.meshBrowser.categorized);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    if (ImGui::BeginCombo("Category", state.meshBrowser.categories[state.meshBrowser.selectedCategory].c_str())) {
        for (size_t i = 0; i < state.meshBrowser.categories.size(); i++) {
            bool selected = (state.meshBrowser.selectedCategory == (int)i);
            if (ImGui::Selectable(state.meshBrowser.categories[i].c_str(), selected)) {
                state.meshBrowser.selectedCategory = (int)i;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (ImGui::BeginTabBar("LODTabs")) {
        const char* lodNames[] = {"LOD 0", "LOD 1", "LOD 2", "LOD 3"};
        for (int lod = 0; lod < 4; lod++) {
            if (ImGui::BeginTabItem(lodNames[lod])) {
                state.meshBrowser.selectedLod = lod;
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    ImGui::InputText("Filter", state.meshBrowser.meshFilter, sizeof(state.meshBrowser.meshFilter));
    std::string filterLower = state.meshBrowser.meshFilter;
    std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
    std::string selectedCat = state.meshBrowser.categories[state.meshBrowser.selectedCategory];
    std::vector<const MeshEntry*> filtered;
    for (const auto& entry : state.meshBrowser.allMeshes) {
        if (entry.lod != state.meshBrowser.selectedLod) continue;
        if (state.meshBrowser.categorized && selectedCat != "All" && entry.category != selectedCat) continue;
        std::string displayName = entry.mshName.empty() ? entry.mshFile : entry.mshName;
        std::string displayLower = displayName;
        std::transform(displayLower.begin(), displayLower.end(), displayLower.begin(), ::tolower);
        if (!filterLower.empty() && displayLower.find(filterLower) == std::string::npos) continue;
        filtered.push_back(&entry);
    }
    ImGui::Text("%zu meshes", filtered.size());
    ImGui::Separator();
    ImGui::BeginChild("MeshList", ImVec2(0, 0), true);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(filtered.size()));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            const MeshEntry* entry = filtered[i];
            std::string displayName = entry->mshName.empty() ? entry->mshFile : entry->mshName;
            char label[512];
            if (state.meshBrowser.categorized || selectedCat == "All") {
                snprintf(label, sizeof(label), "%s##%d", displayName.c_str(), i);
            } else {
                snprintf(label, sizeof(label), "[%s] %s##%d", entry->category.c_str(), displayName.c_str(), i);
            }
            bool selected = (state.meshBrowser.selectedMeshIndex == i);
            if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                state.meshBrowser.selectedMeshIndex = i;
                if (ImGui::IsMouseDoubleClicked(0)) {
                    std::string mshFile = entry->mshFile;
                    std::string mshLower = mshFile;
                    std::transform(mshLower.begin(), mshLower.end(), mshLower.begin(), ::tolower);
                    if (state.showHeadSelector && state.pendingBodyMsh != mshFile) {
                        state.showHeadSelector = false;
                    }
                    auto heads = findAssociatedHeads(state, mshFile);
                    auto eyes = findAssociatedEyes(state, mshFile);
                    state.currentModelAnimations = entry->animations;
                    for (const auto& erfPath : state.erfFiles) {
                        ERFFile erf;
                        if (erf.open(erfPath)) {
                            for (size_t entryIdx = 0; entryIdx < erf.entries().size(); entryIdx++) {
                                const auto& erfEntry = erf.entries()[entryIdx];
                                std::string entryLower = erfEntry.name;
                                std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
                                if (entryLower == mshLower) {
                                    state.currentErf = std::make_unique<ERFFile>();
                                    state.currentErf->open(erfPath);
                                    if (loadModelFromEntry(state, erfEntry)) {
                                        state.statusMessage = "Loaded: " + displayName;
                                        if (!heads.empty()) {
                                            loadAndMergeHead(state, heads[0].first);
                                            state.statusMessage += " + " + heads[0].second;
                                            if (heads.size() > 1) {
                                                state.availableHeads.clear();
                                                state.availableHeadNames.clear();
                                                for (const auto& h : heads) {
                                                    state.availableHeads.push_back(h.first);
                                                    state.availableHeadNames.push_back(h.second);
                                                }
                                                state.pendingBodyMsh = mshFile;
                                                state.pendingBodyEntry.erfIdx = 0;
                                                for (size_t ei = 0; ei < state.erfFiles.size(); ei++) {
                                                    if (state.erfFiles[ei] == erfPath) {
                                                        state.pendingBodyEntry.erfIdx = ei;
                                                        break;
                                                    }
                                                }
                                                state.pendingBodyEntry.entryIdx = entryIdx;
                                                state.pendingBodyEntry.name = erfEntry.name;
                                                state.selectedHeadIndex = 0;
                                                state.showHeadSelector = true;
                                            }
                                        }
                                        if (!eyes.empty()) {
                                            loadAndMergeHead(state, eyes[0].first);
                                            state.statusMessage += " + " + eyes[0].second;
                                        }
                                        state.showRenderSettings = true;
                                    } else {
                                        state.statusMessage = "Failed to load: " + displayName;
                                    }
                                    goto done_search;
                                }
                            }
                        }
                    }
                    done_search:;
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("File: %s", entry->mshFile.c_str());
                if (!entry->mshName.empty()) ImGui::Text("Name: %s", entry->mshName.c_str());
                ImGui::Text("Category: %s", entry->category.c_str());
                ImGui::Text("LOD: %d", entry->lod);
                ImGui::EndTooltip();
            }
        }
    }
    ImGui::EndChild();
    ImGui::End();
}