#include "ui_internal.h"

// Helper function to determine ERF source category
static std::string GetErfSource(const std::string& erfPath) {
    std::string pathLower = erfPath;
    std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);

    if (pathLower.find("packages/core_ep1") != std::string::npos ||
        pathLower.find("packages\\core_ep1") != std::string::npos) {
        return "Awakening";
    }
    // Everything in packages/core (including mods added to it) is "Core" by default
    // Imported models will be marked as "Mods" separately
    return "Core";
}

static int s_meshDataSourceFilter = 0; // 0=All, 1=Core, 2=Awakening, 3=Mods
static std::set<std::string> s_importedModels; // Track models imported by Haven-Tools
static bool s_importedModelsLoaded = false;

// Get the path to the imported models list file
static std::string getImportedModelsPath() {
    return (fs::path(getExeDir()) / "imported_models.txt").string();
}

// Load imported models from file
static void loadImportedModels() {
    if (s_importedModelsLoaded) return;
    s_importedModelsLoaded = true;

    std::ifstream file(getImportedModelsPath());
    if (!file) return;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        // Remove carriage return if present
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string lower = line;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        s_importedModels.insert(lower);
    }

    std::cout << "[Browser] Loaded " << s_importedModels.size() << " imported models from file" << std::endl;
}

// Save imported models to file
static void saveImportedModels() {
    std::ofstream file(getImportedModelsPath());
    if (!file) {
        std::cerr << "[Browser] Failed to save imported models list" << std::endl;
        return;
    }

    for (const auto& name : s_importedModels) {
        file << name << "\n";
    }

    std::cout << "[Browser] Saved " << s_importedModels.size() << " imported models to file" << std::endl;
}

// Call this when a model is imported to mark it as a mod
void markModelAsImported(const std::string& modelName) {
    loadImportedModels(); // Ensure loaded first

    std::string nameLower = modelName;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
    s_importedModels.insert(nameLower);

    // Save to file immediately
    saveImportedModels();
}

// Check if a model was imported
static bool isImportedModel(const std::string& modelName) {
    loadImportedModels(); // Ensure loaded first

    std::string nameLower = modelName;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
    return s_importedModels.find(nameLower) != s_importedModels.end();
}

// Remove a model from the imported list
static void unmarkModelAsImported(const std::string& modelName) {
    loadImportedModels();

    std::string nameLower = modelName;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
    s_importedModels.erase(nameLower);

    saveImportedModels();
}

// Delete confirmation state
static bool s_showDeleteConfirm = false;
static std::string s_deleteModelName;
static CachedEntry s_deleteEntry;

// Helper to delete entries from an ERF file
static bool deleteFromERF(const std::string& erfPath, const std::vector<std::string>& namesToDelete) {
    if (!fs::exists(erfPath)) return false;

    // Read entire ERF
    std::vector<uint8_t> erfData;
    {
        std::ifstream in(erfPath, std::ios::binary | std::ios::ate);
        if (!in) return false;
        size_t size = in.tellg();
        in.seekg(0);
        erfData.resize(size);
        in.read(reinterpret_cast<char*>(erfData.data()), size);
    }

    if (erfData.size() < 32) return false;

    auto readU32 = [&](size_t offset) -> uint32_t {
        if (offset + 4 > erfData.size()) return 0;
        return *reinterpret_cast<uint32_t*>(&erfData[offset]);
    };

    auto readUtf16String = [&](size_t offset, size_t charCount) -> std::string {
        std::string result;
        for (size_t i = 0; i < charCount; ++i) {
            size_t pos = offset + i * 2;
            if (pos + 2 > erfData.size()) break;
            uint16_t ch = *reinterpret_cast<uint16_t*>(&erfData[pos]);
            if (ch == 0) break;
            if (ch < 128) result += static_cast<char>(ch);
        }
        return result;
    };

    std::string magic = readUtf16String(0, 4);
    if (magic != "ERF ") return false;

    uint32_t fileCount = readU32(16);
    uint32_t year = readU32(20);
    uint32_t day = readU32(24);
    uint32_t unknown = readU32(28);

    // Build set of names to delete (lowercase)
    std::set<std::string> deleteSet;
    for (const auto& name : namesToDelete) {
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        deleteSet.insert(lower);
    }

    // Parse entries and filter out ones to delete
    struct FileEntry { std::string name; uint32_t offset; uint32_t size; };
    std::vector<FileEntry> keepEntries;

    size_t tableOffset = 32;
    for (uint32_t i = 0; i < fileCount; ++i) {
        size_t entryOff = tableOffset + i * 72;
        if (entryOff + 72 > erfData.size()) break;
        FileEntry e;
        e.name = readUtf16String(entryOff, 32);
        e.offset = readU32(entryOff + 64);
        e.size = readU32(entryOff + 68);

        std::string nameLower = e.name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

        if (deleteSet.find(nameLower) == deleteSet.end()) {
            keepEntries.push_back(e);
        } else {
            std::cout << "[Delete] Removing: " << e.name << std::endl;
        }
    }

    if (keepEntries.size() == fileCount) {
        // Nothing was deleted
        return false;
    }

    // Rebuild ERF with remaining entries
    std::vector<uint8_t> newErf;
    newErf.reserve(erfData.size());

    // Write header (32 bytes)
    for (int i = 0; i < 32; ++i) newErf.push_back(erfData[i]);

    // Update file count
    uint32_t newCount = static_cast<uint32_t>(keepEntries.size());
    *reinterpret_cast<uint32_t*>(&newErf[16]) = newCount;

    // Write file table
    size_t dataStart = 32 + keepEntries.size() * 72;
    uint32_t currentOffset = static_cast<uint32_t>(dataStart);

    for (auto& e : keepEntries) {
        // Entry: 64 bytes name (UTF-16) + 4 bytes offset + 4 bytes size = 72 bytes
        for (size_t c = 0; c < 32; ++c) {
            if (c < e.name.size()) {
                newErf.push_back(static_cast<uint8_t>(e.name[c]));
                newErf.push_back(0);
            } else {
                newErf.push_back(0);
                newErf.push_back(0);
            }
        }

        // Write offset and size
        uint32_t newOffset = currentOffset;
        for (int b = 0; b < 4; ++b) newErf.push_back((newOffset >> (b * 8)) & 0xFF);
        for (int b = 0; b < 4; ++b) newErf.push_back((e.size >> (b * 8)) & 0xFF);

        currentOffset += e.size;
    }

    // Write file data
    for (const auto& e : keepEntries) {
        if (e.offset + e.size <= erfData.size()) {
            for (uint32_t i = 0; i < e.size; ++i) {
                newErf.push_back(erfData[e.offset + i]);
            }
        }
    }

    // Write to file
    std::ofstream out(erfPath, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(newErf.data()), newErf.size());

    std::cout << "[Delete] ERF rebuilt: " << keepEntries.size() << " entries (was " << fileCount << ")" << std::endl;
    return true;
}

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
void drawBrowserWindow(AppState& state) {
    ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("ERF Browser", &state.showBrowser, ImGuiWindowFlags_MenuBar);
    if (ImGui::BeginMenuBar()) {
        if (ImGui::Button("Open Folder")) {
            IGFD::FileDialogConfig config;
            config.path = state.lastDialogPath.empty() ?
                (state.selectedFolder.empty() ? "." : state.selectedFolder) : state.lastDialogPath;
            ImGuiFileDialog::Instance()->OpenDialog("ChooseFolder", "Choose Folder", nullptr, config);
        }
        if (!state.statusMessage.empty()) { ImGui::SameLine(); ImGui::Text("%s", state.statusMessage.c_str()); }
        ImGui::EndMenuBar();
    }

    ImGui::Columns(2, "browser_columns");
    ImGui::Text("Files");
    ImGui::Separator();

    ImGui::BeginChild("ERFList", ImVec2(0, 0), true);
    if (!state.audioFilesLoaded && !state.selectedFolder.empty()) {
        scanAudioFiles(state);
    }

    bool audioSelected = (state.selectedErfName == "[Audio]");
    if (ImGui::Selectable("Audio - Sound Effects", audioSelected)) {
        if (!audioSelected) {
            state.selectedErfName = "[Audio]";
            state.selectedEntryIndex = -1;
            state.mergedEntries.clear();
            state.filteredEntryIndices.clear();
            state.lastContentFilter.clear();
            for (size_t i = 0; i < state.audioFiles.size(); i++) {
                CachedEntry ce;
                if (state.audioFiles[i].find("__HEADER__") == 0) {
                    ce.name = state.audioFiles[i];
                } else {
                    size_t lastSlash = state.audioFiles[i].find_last_of("/\\");
                    ce.name = (lastSlash != std::string::npos) ? state.audioFiles[i].substr(lastSlash + 1) : state.audioFiles[i];
                }
                ce.erfIdx = i;
                ce.entryIdx = 0;
                state.mergedEntries.push_back(ce);
            }
            state.statusMessage = std::to_string(state.audioFiles.size()) + " audio files";
        }
    }

    bool voSelected = (state.selectedErfName == "[VoiceOver]");
    if (ImGui::Selectable("Audio - Voice Over", voSelected)) {
        if (!voSelected) {
            state.selectedErfName = "[VoiceOver]";
            state.selectedEntryIndex = -1;
            state.mergedEntries.clear();
            state.filteredEntryIndices.clear();
            state.lastContentFilter.clear();
            for (size_t i = 0; i < state.voiceOverFiles.size(); i++) {
                CachedEntry ce;
                if (state.voiceOverFiles[i].find("__HEADER__") == 0) {
                    ce.name = state.voiceOverFiles[i];
                } else {
                    size_t lastSlash = state.voiceOverFiles[i].find_last_of("/\\");
                    ce.name = (lastSlash != std::string::npos) ? state.voiceOverFiles[i].substr(lastSlash + 1) : state.voiceOverFiles[i];
                }
                ce.erfIdx = i;
                ce.entryIdx = 0;
                state.mergedEntries.push_back(ce);
            }
            state.statusMessage = std::to_string(state.voiceOverFiles.size()) + " voice over files";
        }
    }

    ImGui::Separator();

    for (const auto& [filename, indices] : state.erfsByName) {
        bool isSelected = (state.selectedErfName == filename);

        // Check if this is modelmeshdata.erf for special handling
        std::string filenameLower = filename;
        std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);
        bool isModelMeshData = (filenameLower == "modelmeshdata.erf");

        if (ImGui::Selectable(filename.c_str(), isSelected)) {
            if (!isSelected) {
                state.selectedErfName = filename;
                state.selectedEntryIndex = -1;
                state.mergedEntries.clear();
                state.filteredEntryIndices.clear();
                state.lastContentFilter.clear();
                s_meshDataSourceFilter = 0; // Reset filter

                std::set<std::string> seenNames;
                for (size_t erfIdx : indices) {
                    std::string source = GetErfSource(state.erfFiles[erfIdx]);

                    ERFFile erf;
                    if (erf.open(state.erfFiles[erfIdx])) {
                        for (size_t entryIdx = 0; entryIdx < erf.entries().size(); entryIdx++) {
                            const std::string& name = erf.entries()[entryIdx].name;
                            if (seenNames.find(name) == seenNames.end()) {
                                seenNames.insert(name);
                                CachedEntry ce;
                                ce.name = name;
                                ce.erfIdx = erfIdx;
                                ce.entryIdx = entryIdx;
                                // Check if this was imported by Haven-Tools
                                if (isImportedModel(name)) {
                                    ce.source = "Mods";
                                } else {
                                    ce.source = source;
                                }
                                state.mergedEntries.push_back(ce);
                            }
                        }
                    }
                }
                state.statusMessage = std::to_string(state.mergedEntries.size()) + " entries from " + std::to_string(indices.size()) + " ERF(s)";
            }
        }

        // Show subcategory filters when modelmeshdata.erf is selected
        if (isSelected && isModelMeshData && !state.mergedEntries.empty()) {
            ImGui::Indent();

            // Count entries per category
            int coreCount = 0, awakCount = 0, modsCount = 0;
            for (const auto& ce : state.mergedEntries) {
                if (ce.source == "Core") coreCount++;
                else if (ce.source == "Awakening") awakCount++;
                else modsCount++;
            }

            char label[64];

            snprintf(label, sizeof(label), "All (%zu)", state.mergedEntries.size());
            if (ImGui::RadioButton(label, s_meshDataSourceFilter == 0)) {
                s_meshDataSourceFilter = 0;
                state.filteredEntryIndices.clear();
                state.lastContentFilter.clear();
            }

            if (coreCount > 0) {
                snprintf(label, sizeof(label), "Core (%d)", coreCount);
                if (ImGui::RadioButton(label, s_meshDataSourceFilter == 1)) {
                    s_meshDataSourceFilter = 1;
                    state.filteredEntryIndices.clear();
                    state.lastContentFilter.clear();
                }
            }

            if (awakCount > 0) {
                snprintf(label, sizeof(label), "Awakening (%d)", awakCount);
                if (ImGui::RadioButton(label, s_meshDataSourceFilter == 2)) {
                    s_meshDataSourceFilter = 2;
                    state.filteredEntryIndices.clear();
                    state.lastContentFilter.clear();
                }
            }

            if (modsCount > 0) {
                snprintf(label, sizeof(label), "Mods (%d)", modsCount);
                if (ImGui::RadioButton(label, s_meshDataSourceFilter == 3)) {
                    s_meshDataSourceFilter = 3;
                    state.filteredEntryIndices.clear();
                    state.lastContentFilter.clear();
                }
            }

            ImGui::Unindent();
        }
    }
    ImGui::EndChild();

    ImGui::NextColumn();

    if (!state.selectedErfName.empty() && !state.mergedEntries.empty()) {
        bool hasTextures = false, hasModels = false;
        bool isAudioCategory = (state.selectedErfName == "[Audio]" || state.selectedErfName == "[VoiceOver]");

        for (const auto& ce : state.mergedEntries) {
            if (ce.name.find("__HEADER__") == 0) continue;
            if (ce.name.size() > 4 && ce.name.substr(ce.name.size() - 4) == ".dds") hasTextures = true;
            if (isModelFile(ce.name)) hasModels = true;
            if (hasTextures && hasModels) break;
        }

        ImGui::Text("Contents (%zu)", state.mergedEntries.size());
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputText("##contentSearch", state.contentFilter, sizeof(state.contentFilter));

        if (isAudioCategory) {
            if (ImGui::Button("Convert All to MP3")) {
                IGFD::FileDialogConfig config;
                #ifdef _WIN32
                char* userProfile = getenv("USERPROFILE");
                if (userProfile) config.path = std::string(userProfile) + "\\Documents";
                else config.path = ".";
                #else
                char* home = getenv("HOME");
                if (home) config.path = std::string(home) + "/Documents";
                else config.path = ".";
                #endif
                ImGuiFileDialog::Instance()->OpenDialog("ConvertAllAudio", "Select Output Folder", nullptr, config);
            }
            ImGui::SameLine();
            if (state.audioPlaying || state.showAudioPlayer) {
                if (ImGui::Button("Stop")) {
                    stopAudio();
                    state.audioPlaying = false;
                    state.showAudioPlayer = false;
                }
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Playing: %s", state.currentAudioName.c_str());
            }
        } else {
            if (ImGui::Button("Dump all files")) {
                IGFD::FileDialogConfig config;
                #ifdef _WIN32
                char* userProfile = getenv("USERPROFILE");
                if (userProfile) config.path = std::string(userProfile) + "\\Documents";
                else config.path = ".";
                #else
                char* home = getenv("HOME");
                if (home) config.path = std::string(home) + "/Documents";
                else config.path = ".";
                #endif
                ImGuiFileDialog::Instance()->OpenDialog("DumpAllFiles", "Select Output Folder", nullptr, config);
            }

            if (hasTextures) {
                ImGui::SameLine();
                if (ImGui::Button("Dump Textures")) {
                    IGFD::FileDialogConfig config;
                    #ifdef _WIN32
                    char* userProfile = getenv("USERPROFILE");
                    if (userProfile) config.path = std::string(userProfile) + "\\Documents";
                    else config.path = ".";
                    #else
                    char* home = getenv("HOME");
                    if (home) config.path = std::string(home) + "/Documents";
                    else config.path = ".";
                    #endif
                    ImGuiFileDialog::Instance()->OpenDialog("DumpTextures", "Select Output Folder", nullptr, config);
                }
            }
            if (hasModels) {
                ImGui::SameLine();
                if (ImGui::Button("Dump Models")) {
                    IGFD::FileDialogConfig config;
                    #ifdef _WIN32
                    char* userProfile = getenv("USERPROFILE");
                    if (userProfile) config.path = std::string(userProfile) + "\\Documents";
                    else config.path = ".";
                    #else
                    char* home = getenv("HOME");
                    if (home) config.path = std::string(home) + "/Documents";
                    else config.path = ".";
                    #endif
                    ImGuiFileDialog::Instance()->OpenDialog("DumpModels", "Select Output Folder", nullptr, config);
                }
            }
        }

        ImGui::Separator();

        std::string currentFilter = state.contentFilter;
        if (currentFilter != state.lastContentFilter || state.filteredEntryIndices.empty()) {
            state.lastContentFilter = currentFilter;
            state.filteredEntryIndices.clear();
            state.filteredEntryIndices.reserve(state.mergedEntries.size());
            std::string filterLower = currentFilter;
            std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);

            // Check if we need source filtering (for modelmeshdata.erf)
            std::string selNameLower = state.selectedErfName;
            std::transform(selNameLower.begin(), selNameLower.end(), selNameLower.begin(), ::tolower);
            bool filterBySource = (selNameLower == "modelmeshdata.erf" && s_meshDataSourceFilter > 0);

            for (int i = 0; i < (int)state.mergedEntries.size(); i++) {
                const auto& ce = state.mergedEntries[i];

                if (ce.name.find("__HEADER__") == 0) {
                    state.filteredEntryIndices.push_back(i);
                    continue;
                }

                // Source filter for modelmeshdata.erf
                if (filterBySource) {
                    if (s_meshDataSourceFilter == 1 && ce.source != "Core") continue;
                    if (s_meshDataSourceFilter == 2 && ce.source != "Awakening") continue;
                    if (s_meshDataSourceFilter == 3 && ce.source != "Mods") continue;
                }

                if (filterLower.empty()) {
                    state.filteredEntryIndices.push_back(i);
                } else {
                    std::string nameLower = ce.name;
                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                    if (nameLower.find(filterLower) != std::string::npos) {
                        state.filteredEntryIndices.push_back(i);
                    }
                }
            }
        }

        ImGui::BeginChild("EntryList", ImVec2(0, 0), true);

        drawVirtualList((int)state.filteredEntryIndices.size(), [&](int i) {
            int idx = state.filteredEntryIndices[i];
            const CachedEntry& ce = state.mergedEntries[idx];

            if (ce.name.find("__HEADER__") == 0) {
                std::string headerTitle = ce.name.substr(10);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                ImGui::Selectable(headerTitle.c_str(), false, ImGuiSelectableFlags_Disabled);
                ImGui::PopStyleColor();
                return;
            }

            bool isModel = isModelFile(ce.name), isMao = isMaoFile(ce.name), isPhy = isPhyFile(ce.name);
            bool isTexture = ce.name.size() > 4 && ce.name.substr(ce.name.size() - 4) == ".dds";
            bool isAudioFile = ce.name.size() > 4 && (ce.name.substr(ce.name.size() - 4) == ".fsb" );
            bool isGda = ce.name.size() > 4 && ce.name.substr(ce.name.size() - 4) == ".gda";

            if (isModel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
            else if (isMao) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.4f, 1.0f));
            else if (isPhy) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 1.0f, 1.0f));
            else if (isTexture) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
            else if (isAudioFile) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
            else if (isGda) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 1.0f, 1.0f));

            char label[256]; snprintf(label, sizeof(label), "%s##%d", ce.name.c_str(), idx);
            if (ImGui::Selectable(label, idx == state.selectedEntryIndex, ImGuiSelectableFlags_AllowDoubleClick)) {
                state.selectedEntryIndex = idx;
                if (ImGui::IsMouseDoubleClicked(0)) {
                    ERFFile erf;
                    if (erf.open(state.erfFiles[ce.erfIdx])) {
                        if (ce.entryIdx < erf.entries().size()) {
                            const auto& entry = erf.entries()[ce.entryIdx];
                            if (isModel) {
                                if (state.showHeadSelector && state.pendingBodyMsh != ce.name) {
                                    state.showHeadSelector = false;
                                }
                                auto heads = findAssociatedHeads(state, ce.name);
                                auto eyes = findAssociatedEyes(state, ce.name);
                                state.currentErf = std::make_unique<ERFFile>();
                                state.currentErf->open(state.erfFiles[ce.erfIdx]);
                                state.currentModelAnimations.clear();
                                loadMeshDatabase(state);
                                std::string mshLower = ce.name;
                                std::transform(mshLower.begin(), mshLower.end(), mshLower.begin(), ::tolower);
                                for (const auto& me : state.meshBrowser.allMeshes) {
                                    std::string dbLower = me.mshFile;
                                    std::transform(dbLower.begin(), dbLower.end(), dbLower.begin(), ::tolower);
                                    if (dbLower == mshLower) {
                                        state.currentModelAnimations = me.animations;
                                        break;
                                    }
                                }
                                if (loadModelFromEntry(state, entry)) {
                                    state.statusMessage = "Loaded: " + ce.name;
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
                                            state.pendingBodyMsh = ce.name;
                                            state.pendingBodyEntry = ce;
                                            state.selectedHeadIndex = 0;
                                            state.showHeadSelector = true;
                                        }
                                    }
                                    if (!eyes.empty()) {
                                        loadAndMergeHead(state, eyes[0].first);
                                        state.statusMessage += " + " + eyes[0].second;
                                    }
                                }
                                else state.statusMessage = "Failed to parse: " + ce.name;
                                state.showRenderSettings = true;
                            } else if (isMao) {
                                auto data = erf.readEntry(entry);
                                if (!data.empty()) {
                                    state.maoContent = std::string(data.begin(), data.end());
                                    state.maoFileName = ce.name;
                                    state.showMaoViewer = true;
                                }
                            } else if (isTexture) {
                                auto data = erf.readEntry(entry);
                                if (!data.empty()) {
                                    std::string nameLower = ce.name;
                                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                                    state.textureCache[nameLower] = data;
                                    state.previewTextureId = loadDDSTexture(data);
                                    state.previewTextureName = ce.name;
                                    state.showTexturePreview = true;
                                    state.previewMeshIndex = -1;
                                    state.statusMessage = "Previewing: " + ce.name;
                                }
                            } else if (isGda) {
                                auto data = erf.readEntry(entry);
                                if (!data.empty()) {
                                    delete state.gdaEditor.editor;
                                    state.gdaEditor.editor = new GDAFile();
                                    if (state.gdaEditor.editor->load(data, ce.name)) {
                                        state.gdaEditor.currentFile = state.erfFiles[ce.erfIdx] + ":" + ce.name;
                                        state.gdaEditor.selectedRow = -1;
                                        state.gdaEditor.statusMessage = "Loaded: " + ce.name;
                                        state.gdaEditor.showWindow = true;
                                        state.statusMessage = "Opened GDA: " + ce.name;
                                    } else {
                                        state.gdaEditor.statusMessage = "Failed to parse GDA";
                                        delete state.gdaEditor.editor;
                                        state.gdaEditor.editor = nullptr;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            bool isAudio = (state.selectedErfName == "[Audio]" || state.selectedErfName == "[VoiceOver]") &&
                           (ce.name.size() > 4 && (ce.name.substr(ce.name.size() - 4) == ".fsb" ));

            if (isAudio && ImGui::IsMouseDoubleClicked(0) && idx == state.selectedEntryIndex) {
                std::string fullPath;
                if (state.selectedErfName == "[Audio]" && ce.erfIdx < state.audioFiles.size()) {
                    fullPath = state.audioFiles[ce.erfIdx];
                } else if (state.selectedErfName == "[VoiceOver]" && ce.erfIdx < state.voiceOverFiles.size()) {
                    fullPath = state.voiceOverFiles[ce.erfIdx];
                }
                if (!fullPath.empty()) {
                    auto samples = parseFSB4Samples(fullPath);
                    state.statusMessage = "Parsed FSB: " + std::to_string(samples.size()) + " samples";
                    if (samples.size() >= 1) {
                        state.currentFSBPath = fullPath;
                        state.currentFSBSamples = samples;
                        state.selectedFSBSample = -1;
                        state.fsbSampleFilter[0] = '\0';
                        state.showFSBBrowser = true;
                        state.statusMessage = "Sound bank: " + std::to_string(samples.size()) + " samples";

                         if (samples.size() == 1) {
                            stopAudio();
                            state.audioPlaying = false;
                            auto mp3Data = extractFSB4toMP3Data(fullPath);
                            if (!mp3Data.empty()) {
                                state.currentAudioName = ce.name;
                                if (playAudioFromMemory(mp3Data)) {
                                    state.audioPlaying = true;
                                    state.showAudioPlayer = true;
                                    state.statusMessage = "Playing: " + ce.name;
                                }
                            } else {
                                auto wavData = extractFSB4SampleToWav(fullPath, 0);
                                if (!wavData.empty()) {
                                    state.currentAudioName = ce.name;
                                    if (playWavFromMemory(wavData)) {
                                        state.audioPlaying = true;
                                        state.showAudioPlayer = true;
                                        state.statusMessage = "Playing: " + ce.name;
                                    }
                                }
                            }
                         }
                    } else {
                        state.statusMessage = "Failed to parse FSB file";
                    }
                }
            }
            if (isAudio && ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Convert to MP3...")) {
                    IGFD::FileDialogConfig config;
                    #ifdef _WIN32
                    char* userProfile = getenv("USERPROFILE");
                    if (userProfile) config.path = std::string(userProfile) + "\\Documents";
                    else config.path = ".";
                    #else
                    char* home = getenv("HOME");
                    if (home) config.path = std::string(home) + "/Documents";
                    else config.path = ".";
                    #endif
                    std::string defaultName = ce.name;
                    size_t dotPos = defaultName.rfind('.');
                    if (dotPos != std::string::npos) defaultName = defaultName.substr(0, dotPos);
                    defaultName += ".mp3";
                    config.fileName = defaultName;
                    ImGuiFileDialog::Instance()->OpenDialog("ConvertSelectedAudio", "Save MP3", ".mp3", config);
                }
                ImGui::EndPopup();
            }
            if (isModel && ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Export as GLB...")) {
                    state.pendingExportEntry = ce;
                    state.pendingExport = true;
                    IGFD::FileDialogConfig config;
                    #ifdef _WIN32
                    char* userProfile = getenv("USERPROFILE");
                    if (userProfile) config.path = std::string(userProfile) + "\\Documents";
                    else config.path = ".";
                    #else
                    char* home = getenv("HOME");
                    if (home) config.path = std::string(home) + "/Documents";
                    else config.path = ".";
                    #endif
                    std::string defaultName = ce.name;
                    size_t dotPos = defaultName.rfind('.');
                    if (dotPos != std::string::npos) defaultName = defaultName.substr(0, dotPos);
                    defaultName += ".glb";
                    config.fileName = defaultName;
                    ImGuiFileDialog::Instance()->OpenDialog("ExportGLB", "Export as GLB", ".glb", config);
                }
                // Only show delete option for imported models
                if (isImportedModel(ce.name)) {
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                    if (ImGui::MenuItem("Delete Imported Model...")) {
                        s_deleteModelName = ce.name;
                        s_deleteEntry = ce;
                        s_showDeleteConfirm = true;
                    }
                    ImGui::PopStyleColor();
                }
                ImGui::EndPopup();
            }
            if (isTexture && ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Export as DDS...")) {
                    state.pendingTextureExport = ce;
                    state.pendingTexExportDds = true;
                    IGFD::FileDialogConfig config;
                    #ifdef _WIN32
                    char* userProfile = getenv("USERPROFILE");
                    if (userProfile) config.path = std::string(userProfile) + "\\Documents";
                    else config.path = ".";
                    #else
                    char* home = getenv("HOME");
                    if (home) config.path = std::string(home) + "/Documents";
                    else config.path = ".";
                    #endif
                    config.fileName = ce.name;
                    ImGuiFileDialog::Instance()->OpenDialog("ExportTexDDS", "Export as DDS", ".dds", config);
                }
                if (ImGui::MenuItem("Export as PNG...")) {
                    state.pendingTextureExport = ce;
                    state.pendingTexExportPng = true;
                    IGFD::FileDialogConfig config;
                    #ifdef _WIN32
                    char* userProfile = getenv("USERPROFILE");
                    if (userProfile) config.path = std::string(userProfile) + "\\Documents";
                    else config.path = ".";
                    #else
                    char* home = getenv("HOME");
                    if (home) config.path = std::string(home) + "/Documents";
                    else config.path = ".";
                    #endif
                    std::string defaultName = ce.name;
                    size_t dotPos = defaultName.rfind('.');
                    if (dotPos != std::string::npos) defaultName = defaultName.substr(0, dotPos);
                    defaultName += ".png";
                    config.fileName = defaultName;
                    ImGuiFileDialog::Instance()->OpenDialog("ExportTexPNG", "Export as PNG", ".png", config);
                }
                ImGui::EndPopup();
            }
            if (isModel || isMao || isPhy || isTexture || isAudioFile || isGda) ImGui::PopStyleColor();
        });

        ImGui::EndChild();
    } else ImGui::Text("Select an ERF file");
    ImGui::Columns(1);
    ImGui::End();

    if (ImGuiFileDialog::Instance()->Display("DumpAllFiles", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string outDir = ImGuiFileDialog::Instance()->GetCurrentPath();
            int count = 0;
            std::map<size_t, std::vector<size_t>> entriesByErf;
            for(const auto& ce : state.mergedEntries) {
                 if (ce.name.find("__HEADER__") == 0) continue;
                 entriesByErf[ce.erfIdx].push_back(ce.entryIdx);
            }
            for(const auto& [erfIdx, entryIndices] : entriesByErf) {
                if(erfIdx >= state.erfFiles.size()) continue;
                ERFFile erf;
                if(erf.open(state.erfFiles[erfIdx])) {
                    for(size_t entryIdx : entryIndices) {
                        if(entryIdx < erf.entries().size()) {
                            const auto& entry = erf.entries()[entryIdx];
                            std::string outPath = outDir + "/" + entry.name;
                            if(erf.extractEntry(entry, outPath)) {
                                count++;
                            }
                        }
                    }
                    erf.close();
                }
            }
            state.statusMessage = "Dumped " + std::to_string(count) + " files.";
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // Delete confirmation popup
    if (s_showDeleteConfirm) {
        ImGui::OpenPopup("Delete Imported Model?");
        s_showDeleteConfirm = false;
    }

    if (ImGui::BeginPopupModal("Delete Imported Model?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to delete:");
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "%s", s_deleteModelName.c_str());
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "This cannot be undone!");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
            // Get base name without extension
            std::string baseName = s_deleteModelName;
            size_t dotPos = baseName.rfind('.');
            if (dotPos != std::string::npos) baseName = baseName.substr(0, dotPos);

            int deletedCount = 0;

            // Delete from modelmeshdata.erf
            for (const auto& erfPath : state.erfFiles) {
                std::string erfLower = erfPath;
                std::transform(erfLower.begin(), erfLower.end(), erfLower.begin(), ::tolower);
                if (erfLower.find("modelmeshdata.erf") != std::string::npos) {
                    if (deleteFromERF(erfPath, {baseName + ".msh"})) {
                        deletedCount++;
                    }
                    break;
                }
            }

            // Delete from modelhierarchies.erf
            for (const auto& erfPath : state.erfFiles) {
                std::string erfLower = erfPath;
                std::transform(erfLower.begin(), erfLower.end(), erfLower.begin(), ::tolower);
                if (erfLower.find("modelhierarchies.erf") != std::string::npos) {
                    if (deleteFromERF(erfPath, {baseName + ".mmh"})) {
                        deletedCount++;
                    }
                    break;
                }
            }

            // Remove from imported models list
            unmarkModelAsImported(s_deleteModelName);

            // Clear current model if it matches the deleted one
            std::string currentModelLower = state.currentModel.name;
            std::transform(currentModelLower.begin(), currentModelLower.end(), currentModelLower.begin(), ::tolower);
            std::string deletedModelLower = s_deleteModelName;
            std::transform(deletedModelLower.begin(), deletedModelLower.end(), deletedModelLower.begin(), ::tolower);
            if (currentModelLower == deletedModelLower || currentModelLower == baseName + ".msh") {
                state.currentModel = Model();
                state.hasModel = false;
            }

            // Clear browser cache to force refresh
            state.mergedEntries.clear();
            state.filteredEntryIndices.clear();
            state.lastContentFilter.clear();

            state.statusMessage = "Deleted " + baseName + " (" + std::to_string(deletedCount) + " ERF files updated)";
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("No, Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}