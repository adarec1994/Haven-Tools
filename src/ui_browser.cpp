#include "ui_internal.h"
#include "terrain_loader.h"
#include "rml_loader.h"
#include "GffViewer.h"
#include <cstring>
#include <fstream>

static bool isGffData(const std::vector<uint8_t>& data) {
    if (data.size() < 12) return false;
    if (memcmp(data.data(), "GFF ", 4) == 0) return true;
    if (data.size() >= 8 && memcmp(data.data() + 4, "V3.2", 4) == 0) return true;
    return false;
}

static bool isGffFile(const std::string& name) {
    if (name.size() < 4) return false;
    std::string ext = name.substr(name.size() - 4);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    static const std::vector<std::string> gffExtensions = {
        ".utc", ".uti", ".utp", ".utd", ".uts", ".utm", ".utt", ".utw", ".ute",
        ".dlg", ".jrl", ".fac", ".ifo", ".are", ".git", ".gic", ".gui",
        ".plt", ".ptm", ".ptt", ".qst", ".stg", ".cre", ".bic", ".cam", ".caf", ".cut", ".ldf"
    };
    for (const auto& gffExt : gffExtensions) {
        if (ext == gffExt) return true;
    }
    return false;
}
static std::string GetErfSource(const std::string& erfPath) {
    std::string pathLower = erfPath;
    std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);
    if (pathLower.find("packages/core_ep1") != std::string::npos ||
        pathLower.find("packages\\core_ep1") != std::string::npos) {
        return "Awakening";
    }
    return "Core";
}
static int s_meshDataSourceFilter = 0;
static int s_hierDataSourceFilter = 0;
static std::set<std::string> s_importedModels;
static bool s_importedModelsLoaded = false;
static std::string getImportedModelsPath() {
    return (fs::path(getExeDir()) / "imported_models.txt").string();
}
static void loadImportedModels() {
    if (s_importedModelsLoaded) return;
    s_importedModelsLoaded = true;
    std::ifstream file(getImportedModelsPath());
    if (!file) return;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::string lower = line;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        s_importedModels.insert(lower);
    }
}
static void saveImportedModels() {
    std::ofstream file(getImportedModelsPath());
    if (!file) {
        std::cerr << "[Browser] Failed to save imported models list" << std::endl;
        return;
    }
    for (const auto& name : s_importedModels) {
        file << name << "\n";
    }
}
void markModelAsImported(const std::string& modelName) {
    loadImportedModels();
    std::string nameLower = modelName;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
    s_importedModels.insert(nameLower);
    saveImportedModels();
}
static bool isImportedModel(const std::string& modelName) {
    loadImportedModels();
    std::string nameLower = modelName;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
    return s_importedModels.find(nameLower) != s_importedModels.end();
}
static void unmarkModelAsImported(const std::string& modelName) {
    loadImportedModels();
    std::string nameLower = modelName;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
    s_importedModels.erase(nameLower);
    saveImportedModels();
}
static bool s_showDeleteConfirm = false;
static std::string s_deleteModelName;
static CachedEntry s_deleteEntry;
static bool deleteFromERF(const std::string& erfPath, const std::vector<std::string>& namesToDelete) {
    if (!fs::exists(erfPath)) return false;
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
    std::set<std::string> deleteSet;
    for (const auto& name : namesToDelete) {
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        deleteSet.insert(lower);
    }
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
        }
    }
    if (keepEntries.size() == fileCount) {
        return false;
    }
    std::vector<uint8_t> newErf;
    newErf.reserve(erfData.size());
    for (int i = 0; i < 32; ++i) newErf.push_back(erfData[i]);
    uint32_t newCount = static_cast<uint32_t>(keepEntries.size());
    *reinterpret_cast<uint32_t*>(&newErf[16]) = newCount;
    size_t dataStart = 32 + keepEntries.size() * 72;
    uint32_t currentOffset = static_cast<uint32_t>(dataStart);
    for (auto& e : keepEntries) {
        for (size_t c = 0; c < 32; ++c) {
            if (c < e.name.size()) {
                newErf.push_back(static_cast<uint8_t>(e.name[c]));
                newErf.push_back(0);
            } else {
                newErf.push_back(0);
                newErf.push_back(0);
            }
        }
        uint32_t newOffset = currentOffset;
        for (int b = 0; b < 4; ++b) newErf.push_back((newOffset >> (b * 8)) & 0xFF);
        for (int b = 0; b < 4; ++b) newErf.push_back((e.size >> (b * 8)) & 0xFF);
        currentOffset += e.size;
    }
    for (const auto& e : keepEntries) {
        if (e.offset + e.size <= erfData.size()) {
            for (uint32_t i = 0; i < e.size; ++i) {
                newErf.push_back(erfData[e.offset + i]);
            }
        }
    }
    std::ofstream out(erfPath, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(newErf.data()), newErf.size());
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
    if (state.levelLoad.stage > 0) {
        auto& ll = state.levelLoad;
        const int BATCH_SIZE = 8;

        if (ll.stage == 1) {
            buildErfIndex(state);
            ll.propQueue.clear();

            auto collectProps = [&](ERFFile& rim) {
                for (const auto& entry : rim.entries()) {
                    std::string eLower = entry.name;
                    std::transform(eLower.begin(), eLower.end(), eLower.begin(), ::tolower);
                    if (eLower.size() < 4 || eLower.substr(eLower.size() - 4) != ".rml") continue;
                    std::vector<uint8_t> rmlData = rim.readEntry(entry);
                    if (rmlData.empty()) continue;
                    RMLData rml;
                    if (!parseRML(rmlData, rml)) continue;
                    for (const auto& prop : rml.props) {
                        std::string name = prop.modelFile.empty() ? prop.modelName : prop.modelFile;
                        if (name.empty()) continue;
                        AppState::PropWork pw;
                        pw.modelName = name;
                        pw.px = rml.roomPosX + prop.posX;
                        pw.py = rml.roomPosY + prop.posY;
                        pw.pz = rml.roomPosZ + prop.posZ;
                        pw.qx = prop.orientX; pw.qy = prop.orientY;
                        pw.qz = prop.orientZ; pw.qw = prop.orientW;
                        pw.scale = prop.scale;
                        ll.propQueue.push_back(pw);
                    }
                }
            };

            collectProps(*state.currentErf);

            std::string rimDir = fs::path(state.currentRIMPath).parent_path().string();
            for (const auto& dirEntry : fs::directory_iterator(rimDir)) {
                if (!dirEntry.is_regular_file()) continue;
                std::string fnameLower = dirEntry.path().filename().string();
                std::transform(fnameLower.begin(), fnameLower.end(), fnameLower.begin(), ::tolower);
                if (fnameLower.size() > 4 && fnameLower.substr(fnameLower.size() - 4) == ".rim" &&
                    fnameLower.find(".gpu.") == std::string::npos &&
                    dirEntry.path().string() != state.currentRIMPath) {
                    ERFFile siblingRim;
                    if (siblingRim.open(dirEntry.path().string()))
                        collectProps(siblingRim);
                }
            }

            ll.totalProps = (int)ll.propQueue.size();
            ll.itemIndex = 0;
            ll.stage = 2;
            ll.stageLabel = "Loading props...";
        }
        else if (ll.stage == 2) {
            int processed = 0;
            for (int i = ll.itemIndex; i < ll.totalProps && processed < BATCH_SIZE; i++) {
                const auto& pw = ll.propQueue[i];
                if (mergeModelByName(state, pw.modelName, pw.px, pw.py, pw.pz,
                                     pw.qx, pw.qy, pw.qz, pw.qw, pw.scale))
                    ll.propsLoaded++;
                ll.itemIndex = i + 1;
                processed++;
            }
            if (ll.itemIndex >= ll.totalProps) {
                ll.stage = 3;
                ll.stageLabel = "Loading materials & textures...";
            }
        }
        else if (ll.stage == 3) {
            finalizeLevelMaterials(state);

            if (!state.currentModel.meshes.empty()) {
                float minX = 1e30f, maxX = -1e30f;
                float minY = 1e30f, maxY = -1e30f;
                float minZ = 1e30f, maxZ = -1e30f;
                for (const auto& mesh : state.currentModel.meshes) {
                    if (mesh.minX < minX) minX = mesh.minX;
                    if (mesh.maxX > maxX) maxX = mesh.maxX;
                    if (mesh.minY < minY) minY = mesh.minY;
                    if (mesh.maxY > maxY) maxY = mesh.maxY;
                    if (mesh.minZ < minZ) minZ = mesh.minZ;
                    if (mesh.maxZ > maxZ) maxZ = mesh.maxZ;
                }
                float cx = (minX + maxX) / 2.0f, cy = (minY + maxY) / 2.0f, cz = (minZ + maxZ) / 2.0f;
                float dx = maxX - minX, dy = maxY - minY, dz = maxZ - minZ;
                float radius = std::sqrt(dx*dx + dy*dy + dz*dz) / 2.0f;
                state.camera.lookAt(cx, cy, cz, radius * 2.5f);
                state.camera.moveSpeed = radius * 0.1f;
            }

            state.statusMessage = "Loaded level: " +
                std::to_string(ll.propsLoaded) + " props, " +
                std::to_string(state.currentModel.materials.size()) + " materials";
            state.showRenderSettings = true;
            ll.stage = 0;
        }

        if (ll.stage > 0) {
            float progress = 0.0f;
            std::string detail;
            if (ll.stage == 1) {
                progress = 0.0f;
                detail = "Scanning RML files...";
            } else if (ll.stage == 2) {
                progress = ll.totalProps > 0 ? (float)ll.itemIndex / ll.totalProps : 0.0f;
                detail = std::to_string(ll.itemIndex) + " / " + std::to_string(ll.totalProps) + " props";
            } else if (ll.stage == 3) {
                progress = 1.0f;
                detail = "Finalizing...";
            }

            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(400, 0));
            ImGui::Begin("##LevelLoading", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("%s", ll.stageLabel.c_str());
            ImGui::ProgressBar(progress, ImVec2(-1, 0), detail.c_str());
            ImGui::End();
        }
    }

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
    float totalW = ImGui::GetContentRegionAvail().x;
    float totalH = ImGui::GetContentRegionAvail().y;
    if (state.leftPaneWidth < 100.0f) state.leftPaneWidth = 100.0f;
    if (state.leftPaneWidth > totalW - 100.0f) state.leftPaneWidth = totalW - 100.0f;
    float leftW = state.leftPaneWidth;
    ImGui::BeginChild("LeftPane", ImVec2(leftW, totalH), false);
    ImGui::Text("Files");
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
            state.showRIMBrowser = false;
            state.rimEntries.clear();
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
            state.showRIMBrowser = false;
            state.rimEntries.clear();
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

    auto isExtraFile = [](const std::string& name) {
        if (name.size() < 4) return false;
        std::string ext = name.substr(name.size() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".lvl";
    };

    auto drawErfEntry = [&](const std::string& filename, const std::vector<size_t>& indices) {
        bool isSelected = (state.selectedErfName == filename);
        std::string filenameLower = filename;
        std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);
        bool isModelMeshData = (filenameLower == "modelmeshdata.erf");
        bool isModelHierarchies = (filenameLower == "modelhierarchies.erf");
        if (ImGui::Selectable(filename.c_str(), isSelected)) {
            if (!isSelected) {
                state.selectedErfName = filename;
                state.selectedEntryIndex = -1;
                state.mergedEntries.clear();
                state.filteredEntryIndices.clear();
                state.lastContentFilter.clear();
                s_meshDataSourceFilter = 0;
                s_hierDataSourceFilter = 0;
                state.showRIMBrowser = false;
                state.rimEntries.clear();
                state.showFSBBrowser = false;
                state.currentFSBSamples.clear();
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
        if (isSelected && isModelMeshData && !state.mergedEntries.empty()) {
            ImGui::Indent();
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
        if (isSelected && isModelHierarchies && !state.mergedEntries.empty()) {
            ImGui::Indent();
            int coreCount = 0, awakCount = 0, modsCount = 0;
            for (const auto& ce : state.mergedEntries) {
                if (ce.source == "Core") coreCount++;
                else if (ce.source == "Awakening") awakCount++;
                else modsCount++;
            }
            char label[64];
            snprintf(label, sizeof(label), "All (%zu)", state.mergedEntries.size());
            if (ImGui::RadioButton(label, s_hierDataSourceFilter == 0)) {
                s_hierDataSourceFilter = 0;
                state.filteredEntryIndices.clear();
                state.lastContentFilter.clear();
            }
            if (coreCount > 0) {
                snprintf(label, sizeof(label), "Core (%d)", coreCount);
                if (ImGui::RadioButton(label, s_hierDataSourceFilter == 1)) {
                    s_hierDataSourceFilter = 1;
                    state.filteredEntryIndices.clear();
                    state.lastContentFilter.clear();
                }
            }
            if (awakCount > 0) {
                snprintf(label, sizeof(label), "Awakening (%d)", awakCount);
                if (ImGui::RadioButton(label, s_hierDataSourceFilter == 2)) {
                    s_hierDataSourceFilter = 2;
                    state.filteredEntryIndices.clear();
                    state.lastContentFilter.clear();
                }
            }
            if (modsCount > 0) {
                snprintf(label, sizeof(label), "Mods (%d)", modsCount);
                if (ImGui::RadioButton(label, s_hierDataSourceFilter == 3)) {
                    s_hierDataSourceFilter = 3;
                    state.filteredEntryIndices.clear();
                    state.lastContentFilter.clear();
                }
            }
            ImGui::Unindent();
        }
    };

    for (const auto& [filename, indices] : state.erfsByName) {
        if (!isExtraFile(filename)) drawErfEntry(filename, indices);
    }

    bool hasExtra = false;
    for (const auto& [filename, indices] : state.erfsByName) {
        if (isExtraFile(filename)) { hasExtra = true; break; }
    }
    if (hasExtra) {
        ImGui::Separator();
        for (const auto& [filename, indices] : state.erfsByName) {
            if (isExtraFile(filename)) drawErfEntry(filename, indices);
        }
    }

    if (!state.rimFiles.empty()) {
        ImGui::Separator();
        bool rimSelected = (state.selectedErfName == "[RIM]");
        char rimLabel[64];
        snprintf(rimLabel, sizeof(rimLabel), "RIM Files (%zu)", state.rimFiles.size());
        if (ImGui::Selectable(rimLabel, rimSelected)) {
            if (!rimSelected) {
                state.selectedErfName = "[RIM]";
                state.selectedEntryIndex = -1;
                state.mergedEntries.clear();
                state.filteredEntryIndices.clear();
                state.lastContentFilter.clear();
                state.showRIMBrowser = false;
                state.rimEntries.clear();
                state.showFSBBrowser = false;
                state.currentFSBSamples.clear();
                for (size_t i = 0; i < state.rimFiles.size(); i++) {
                    CachedEntry ce;
                    size_t lastSlash = state.rimFiles[i].find_last_of("/\\");
                    ce.name = (lastSlash != std::string::npos) ? state.rimFiles[i].substr(lastSlash + 1) : state.rimFiles[i];
                    ce.erfIdx = i;
                    ce.entryIdx = 0;
                    state.mergedEntries.push_back(ce);
                }
                state.statusMessage = std::to_string(state.rimFiles.size()) + " RIM files";
            }
        }
    }

    ImGui::EndChild();
    ImGui::EndChild();

    ImGui::SameLine(0, 0);
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorActive));
    ImGui::Button("##splitter", ImVec2(4.0f, totalH));
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemActive()) {
        state.leftPaneWidth += ImGui::GetIO().MouseDelta.x;
        if (state.leftPaneWidth < 100.0f) state.leftPaneWidth = 100.0f;
        if (state.leftPaneWidth > totalW - 100.0f) state.leftPaneWidth = totalW - 100.0f;
    }
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

    ImGui::SameLine(0, 0);
    ImGui::BeginChild("RightPane", ImVec2(0, totalH), false);
    if (!state.selectedErfName.empty() && !state.mergedEntries.empty()) {
        bool hasTextures = false, hasModels = false, hasTerrain = false;
        bool isAudioCategory = (state.selectedErfName == "[Audio]" || state.selectedErfName == "[VoiceOver]");
        for (const auto& ce : state.mergedEntries) {
            if (ce.name.find("__HEADER__") == 0) continue;
            if (ce.name.size() > 4 && ce.name.substr(ce.name.size() - 4) == ".dds") hasTextures = true;
            if (isModelFile(ce.name)) hasModels = true;
            if (isTerrain(ce.name)) hasTerrain = true;
            if (hasTextures && hasModels && hasTerrain) break;
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
            std::string selNameLower = state.selectedErfName;
            std::transform(selNameLower.begin(), selNameLower.end(), selNameLower.begin(), ::tolower);
            bool filterByMeshSource = (selNameLower == "modelmeshdata.erf" && s_meshDataSourceFilter > 0);
            bool filterByHierSource = (selNameLower == "modelhierarchies.erf" && s_hierDataSourceFilter > 0);
            for (int i = 0; i < (int)state.mergedEntries.size(); i++) {
                const auto& ce = state.mergedEntries[i];
                if (ce.name.find("__HEADER__") == 0) {
                    state.filteredEntryIndices.push_back(i);
                    continue;
                }
                if (filterByMeshSource) {
                    if (s_meshDataSourceFilter == 1 && ce.source != "Core") continue;
                    if (s_meshDataSourceFilter == 2 && ce.source != "Awakening") continue;
                    if (s_meshDataSourceFilter == 3 && ce.source != "Mods") continue;
                }
                if (filterByHierSource) {
                    if (s_hierDataSourceFilter == 1 && ce.source != "Core") continue;
                    if (s_hierDataSourceFilter == 2 && ce.source != "Awakening") continue;
                    if (s_hierDataSourceFilter == 3 && ce.source != "Mods") continue;
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
        bool showFSBPanel = state.showFSBBrowser && !state.currentFSBSamples.empty();
        bool showRIMPanel = state.showRIMBrowser && !state.rimEntries.empty();
        bool showSubPanel = showFSBPanel || showRIMPanel;
        float availW = ImGui::GetContentRegionAvail().x;
        float entryListW = showSubPanel ? availW * 0.5f : 0.0f;

        ImGui::BeginChild("EntryList", ImVec2(entryListW, 0), true);
        if (hasTerrain) {
            std::string loadLabel = ">> Load " + state.selectedErfName;
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.6f, 1.0f));
            if (ImGui::Selectable(loadLabel.c_str(), state.showTerrain)) {
                g_terrainLoader.clear();
                for (size_t erfIdx : state.erfsByName[state.selectedErfName]) {
                    ERFFile erf;
                    if (erf.open(state.erfFiles[erfIdx])) {
                        if (g_terrainLoader.loadFromERF(erf, "")) {
                            state.showTerrain = true;
                            state.hasModel = false;
                            state.currentModel = Model();
                            auto& t = g_terrainLoader.getTerrain();
                            float cx = (t.minX + t.maxX) * 0.5f;
                            float cy = (t.minY + t.maxY) * 0.5f;
                            float cz = (t.minZ + t.maxZ) * 0.5f;
                            float span = std::max(t.maxX - t.minX, t.maxY - t.minY);
                            state.camera.lookAt(cx, cy, cz, span * 0.8f);
                            state.camera.moveSpeed = span * 0.1f;
                            state.statusMessage = "Loaded terrain: " +
                                std::to_string(t.sectors.size()) + " sectors, " +
                                std::to_string(t.water.size()) + " water";
                        }
                    }
                }
            }
            if (ImGui::BeginPopupContextItem("##terrainCtx")) {
                if (ImGui::MenuItem("View Heightmap")) {
                    if (!g_terrainLoader.isLoaded()) {
                        for (size_t erfIdx : state.erfsByName[state.selectedErfName]) {
                            ERFFile erf;
                            if (erf.open(state.erfFiles[erfIdx]))
                                g_terrainLoader.loadFromERF(erf, "");
                        }
                    }
                    if (g_terrainLoader.isLoaded()) {
                        if (state.heightmapTexId) destroyTexture(state.heightmapTexId);
                        int w, h;
                        auto rgba = g_terrainLoader.generateHeightmap(w, h, 1024);
                        if (!rgba.empty()) {
                            state.heightmapTexId = createTexture2D(rgba.data(), w, h);
                            state.heightmapW = w;
                            state.heightmapH = h;
                            state.showHeightmap = true;
                        }
                    }
                }
                ImGui::EndPopup();
            }
            ImGui::PopStyleColor();
            ImGui::Separator();
        }
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
            bool isTerrainFile = isTerrain(ce.name);
            bool isTexture = ce.name.size() > 4 && ce.name.substr(ce.name.size() - 4) == ".dds";
            bool isAudioFile = ce.name.size() > 4 && (ce.name.substr(ce.name.size() - 4) == ".fsb" );
            bool isGda = ce.name.size() > 4 && ce.name.substr(ce.name.size() - 4) == ".gda";
            bool isGff = isGffFile(ce.name);
            if (isModel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
            else if (isTerrainFile) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.4f, 0.2f, 1.0f));
            else if (isMao) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.4f, 1.0f));
            else if (isPhy) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 1.0f, 1.0f));
            else if (isTexture) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
            else if (isAudioFile) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
            else if (isGda) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 1.0f, 1.0f));
            else if (isGff) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.8f, 1.0f));
            char label[256]; snprintf(label, sizeof(label), "%s##%d", ce.name.c_str(), idx);
            if (ImGui::Selectable(label, idx == state.selectedEntryIndex, ImGuiSelectableFlags_AllowDoubleClick)) {
                state.selectedEntryIndex = idx;

                bool isRimClick = (state.selectedErfName == "[RIM]");
                if (isRimClick) {
                    if (ce.erfIdx < state.rimFiles.size()) {
                        std::string rimPath = state.rimFiles[ce.erfIdx];
                        ERFFile rimErf;
                        if (rimErf.open(rimPath)) {
                            state.currentRIMPath = rimPath;
                            state.rimEntries.clear();
                            state.selectedRIMEntry = -1;
                            state.rimEntryFilter[0] = '\0';
                            for (size_t ei = 0; ei < rimErf.entries().size(); ei++) {
                                CachedEntry re;
                                re.name = rimErf.entries()[ei].name;
                                re.erfIdx = (size_t)0;
                                re.entryIdx = ei;
                                state.rimEntries.push_back(re);
                            }
                            state.showRIMBrowser = true;
                            state.statusMessage = ce.name + ": " + std::to_string(state.rimEntries.size()) + " entries";
                        }
                    }
                }

                bool isAudio = (state.selectedErfName == "[Audio]" || state.selectedErfName == "[VoiceOver]") &&
                               (ce.name.size() > 4 && (ce.name.substr(ce.name.size() - 4) == ".fsb" ));
                if (isAudio) {
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
                             if (samples.size() == 1) {
                                stopAudio();
                                state.audioPlaying = false;
                                std::string sampleName = samples[0].name.empty() ? ce.name : samples[0].name;
                                auto mp3Data = extractFSB4toMP3Data(fullPath);
                                if (!mp3Data.empty()) {
                                    state.currentAudioName = sampleName;
                                    if (playAudioFromMemory(mp3Data)) {
                                        state.audioPlaying = true;
                                        state.showAudioPlayer = true;
                                        state.statusMessage = "Playing: " + sampleName;
                                    }
                                } else {
                                    auto wavData = extractFSB4SampleToWav(fullPath, 0);
                                    if (!wavData.empty()) {
                                        state.currentAudioName = sampleName;
                                        if (playWavFromMemory(wavData)) {
                                            state.audioPlaying = true;
                                            state.showAudioPlayer = true;
                                            state.statusMessage = "Playing: " + sampleName;
                                        }
                                    }
                                }
                             } else {
                                state.showFSBBrowser = true;
                                state.statusMessage = "Sound bank: " + std::to_string(samples.size()) + " samples";
                             }
                        } else {
                            state.statusMessage = "Failed to parse FSB file";
                        }
                    }
                } else if (ImGui::IsMouseDoubleClicked(0)) {
                    ERFFile erf;
                    if (erf.open(state.erfFiles[ce.erfIdx])) {
                        if (ce.entryIdx < erf.entries().size()) {
                            const auto& entry = erf.entries()[ce.entryIdx];
                            if (isTerrainFile) {
                                auto terrainData = erf.readEntry(entry);
                                if (!terrainData.empty() && isGffData(terrainData)) {
                                    if (loadGffData(state.gffViewer, terrainData, ce.name, state.erfFiles[ce.erfIdx])) {
                                        state.gffViewer.showWindow = true;
                                        state.statusMessage = "Opened TMSH: " + ce.name;
                                    }
                                }
                            } else if (isModel) {
                                    state.showTerrain = false;
                                    g_terrainLoader.clear();
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
                                        state.previewTextureId = createTextureFromDDS(data);
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
                                } else if (isGff) {
                                    auto data = erf.readEntry(entry);
                                    if (!data.empty()) {
                                        if (loadGffData(state.gffViewer, data, ce.name, state.erfFiles[ce.erfIdx])) {
                                            state.gffViewer.showWindow = true;
                                            state.statusMessage = "Opened GFF: " + ce.name;
                                        } else {
                                            state.statusMessage = "Failed to parse GFF: " + ce.name;
                                        }
                                    }
                                } else {
                                    auto data = erf.readEntry(entry);
                                    if (!data.empty() && isGffData(data)) {
                                        if (loadGffData(state.gffViewer, data, ce.name, state.erfFiles[ce.erfIdx])) {
                                            state.gffViewer.showWindow = true;
                                            state.statusMessage = "Opened GFF: " + ce.name;
                                        } else {
                                            state.statusMessage = "Failed to parse GFF: " + ce.name;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            bool isAudio = (state.selectedErfName == "[Audio]" || state.selectedErfName == "[VoiceOver]") &&
                           (ce.name.size() > 4 && (ce.name.substr(ce.name.size() - 4) == ".fsb" ));
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
                ImGui::Separator();
                if (ImGui::MenuItem("Open with GFF Viewer")) {
                    std::string fullPath;
                    if (state.selectedErfName == "[Audio]" && ce.erfIdx < state.audioFiles.size())
                        fullPath = state.audioFiles[ce.erfIdx];
                    else if (state.selectedErfName == "[VoiceOver]" && ce.erfIdx < state.voiceOverFiles.size())
                        fullPath = state.voiceOverFiles[ce.erfIdx];
                    if (!fullPath.empty()) {
                        std::ifstream ifs(fullPath, std::ios::binary);
                        if (ifs) {
                            std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                            if (!data.empty() && isGffData(data)) {
                                if (loadGffData(state.gffViewer, data, ce.name, fullPath)) {
                                    state.gffViewer.showWindow = true;
                                    state.statusMessage = "Opened: " + ce.name;
                                }
                            } else {
                                state.statusMessage = "Not a valid GFF file: " + ce.name;
                            }
                        }
                    }
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
                ImGui::Separator();
                if (ImGui::MenuItem("Open with GFF Viewer")) {
                    ERFFile erf;
                    if (ce.erfIdx < state.erfFiles.size() && erf.open(state.erfFiles[ce.erfIdx])) {
                        if (ce.entryIdx < erf.entries().size()) {
                            auto data = erf.readEntry(erf.entries()[ce.entryIdx]);
                            if (!data.empty() && isGffData(data)) {
                                if (loadGffData(state.gffViewer, data, ce.name, state.erfFiles[ce.erfIdx])) {
                                    state.gffViewer.showWindow = true;
                                    state.statusMessage = "Opened: " + ce.name;
                                }
                            } else {
                                state.statusMessage = "Not a valid GFF file: " + ce.name;
                            }
                        }
                    }
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
                ImGui::Separator();
                if (ImGui::MenuItem("Open with GFF Viewer")) {
                    ERFFile erf;
                    if (ce.erfIdx < state.erfFiles.size() && erf.open(state.erfFiles[ce.erfIdx])) {
                        if (ce.entryIdx < erf.entries().size()) {
                            auto data = erf.readEntry(erf.entries()[ce.entryIdx]);
                            if (!data.empty() && isGffData(data)) {
                                if (loadGffData(state.gffViewer, data, ce.name, state.erfFiles[ce.erfIdx])) {
                                    state.gffViewer.showWindow = true;
                                    state.statusMessage = "Opened: " + ce.name;
                                }
                            } else {
                                state.statusMessage = "Not a valid GFF file: " + ce.name;
                            }
                        }
                    }
                }
                ImGui::EndPopup();
            }
            if (!isAudio && !isModel && !isTexture && ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Open with GFF Viewer")) {
                    ERFFile erf;
                    size_t erfIdx = ce.erfIdx;
                    if (erfIdx < state.erfFiles.size() && erf.open(state.erfFiles[erfIdx])) {
                        if (ce.entryIdx < erf.entries().size()) {
                            auto data = erf.readEntry(erf.entries()[ce.entryIdx]);
                            if (!data.empty() && isGffData(data)) {
                                if (loadGffData(state.gffViewer, data, ce.name, state.erfFiles[erfIdx])) {
                                    state.gffViewer.showWindow = true;
                                    state.statusMessage = "Opened: " + ce.name;
                                }
                            } else {
                                state.statusMessage = "Not a valid GFF file: " + ce.name;
                            }
                        }
                    }
                }
                ImGui::EndPopup();
            }
            if (isModel || isTerrainFile || isMao || isPhy || isTexture || isAudioFile || isGda || isGff) ImGui::PopStyleColor();
        });
        ImGui::EndChild();

        if (showFSBPanel) {
            ImGui::SameLine();
            ImGui::BeginChild("FSBPanel", ImVec2(0, 0), true);

            std::string fsbFilename = fs::path(state.currentFSBPath).filename().string();
            ImGui::Text("%s (%zu samples)", fsbFilename.c_str(), state.currentFSBSamples.size());

            if (ImGui::Button("Close")) {
                state.showFSBBrowser = false;
                state.currentFSBSamples.clear();
                state.selectedFSBSample = -1;
            }
            ImGui::SameLine();
            if (ImGui::Button("Export All")) {
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
                ImGuiFileDialog::Instance()->OpenDialog("ExportAllFSBSamples", "Select Output Folder", nullptr, config);
            }
            if (state.selectedFSBSample >= 0) {
                ImGui::SameLine();
                if (ImGui::Button("Export Selected")) {
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
                    std::string defaultName = state.currentFSBSamples[state.selectedFSBSample].name;
                    size_t dotPos = defaultName.rfind('.');
                    if (dotPos != std::string::npos) defaultName = defaultName.substr(0, dotPos);
                    defaultName += ".wav";
                    config.fileName = defaultName;
                    ImGuiFileDialog::Instance()->OpenDialog("ExportFSBSample", "Save WAV", ".wav", config);
                }
            }

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputText("##fsbFilter", state.fsbSampleFilter, sizeof(state.fsbSampleFilter));

            ImGui::BeginChild("FSBSampleList", ImVec2(0, 0), true);
            std::string fsbFilterLower = state.fsbSampleFilter;
            std::transform(fsbFilterLower.begin(), fsbFilterLower.end(), fsbFilterLower.begin(), ::tolower);

            if (ImGui::BeginTable("##FSBTable", 3,
                ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Length", ImGuiTableColumnFlags_WidthFixed, 50.0f);
                ImGui::TableSetupColumn("Hz", ImGuiTableColumnFlags_WidthFixed, 55.0f);
                ImGui::TableHeadersRow();

                for (int i = 0; i < (int)state.currentFSBSamples.size(); i++) {
                    const auto& sample = state.currentFSBSamples[i];

                    if (!fsbFilterLower.empty()) {
                        std::string nameLower = sample.name;
                        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                        if (nameLower.find(fsbFilterLower) == std::string::npos) continue;
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    int mins = (int)(sample.duration / 60);
                    int secs = (int)(sample.duration) % 60;

                    char idLabel[64];
                    snprintf(idLabel, sizeof(idLabel), "%s##fsb%d", sample.name.c_str(), i);
                    bool selected = (state.selectedFSBSample == i);
                    if (ImGui::Selectable(idLabel, selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                        state.selectedFSBSample = i;
                        if (ImGui::IsMouseDoubleClicked(0)) {
                            auto wavData = extractFSB4SampleToWav(state.currentFSBPath, i);
                            if (!wavData.empty()) {
                                playWavFromMemory(wavData);
                                state.currentAudioName = sample.name;
                                state.audioPlaying = true;
                                state.showAudioPlayer = true;
                            }
                        }
                    }

                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Play")) {
                            auto wavData = extractFSB4SampleToWav(state.currentFSBPath, i);
                            if (!wavData.empty()) {
                                playWavFromMemory(wavData);
                                state.currentAudioName = sample.name;
                                state.audioPlaying = true;
                                state.showAudioPlayer = true;
                            }
                        }
                        if (ImGui::MenuItem("Export to WAV...")) {
                            state.selectedFSBSample = i;
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
                            std::string defaultName = sample.name;
                            size_t dotPos = defaultName.rfind('.');
                            if (dotPos != std::string::npos) defaultName = defaultName.substr(0, dotPos);
                            defaultName += ".wav";
                            config.fileName = defaultName;
                            ImGuiFileDialog::Instance()->OpenDialog("ExportFSBSample", "Save WAV", ".wav", config);
                        }
                        ImGui::EndPopup();
                    }

                    ImGui::TableNextColumn();
                    ImGui::Text("%d:%02d", mins, secs);
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", sample.sampleRate);
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
            ImGui::EndChild();
        }

        if (showRIMPanel) {
            ImGui::SameLine();
            ImGui::BeginChild("RIMPanel", ImVec2(0, 0), true);

            std::string rimFilename = fs::path(state.currentRIMPath).filename().string();
            ImGui::Text("%s (%zu entries)", rimFilename.c_str(), state.rimEntries.size());

            if (ImGui::Button("Close")) {
                state.showRIMBrowser = false;
                state.rimEntries.clear();
                state.selectedRIMEntry = -1;
            }
            ImGui::SameLine();
            if (ImGui::Button("Dump All")) {
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
                ImGuiFileDialog::Instance()->OpenDialog("DumpAllRIM", "Select Output Folder", nullptr, config);
            }

            int mshCount = 0;
            for (const auto& re : state.rimEntries) {
                std::string nameLower = re.name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                if (nameLower.size() > 4 && nameLower.substr(nameLower.size() - 4) == ".msh")
                    mshCount++;
            }
            if (mshCount > 0) {
                ImGui::SameLine();
                char loadLabel[64];
                snprintf(loadLabel, sizeof(loadLabel), "Load Level (%d)", mshCount);
                if (ImGui::Button(loadLabel) && state.levelLoad.stage == 0) {
                    state.showTerrain = false;
                    g_terrainLoader.clear();
                    state.currentErf = std::make_unique<ERFFile>();
                    if (state.currentErf->open(state.currentRIMPath)) {
                        state.textureErfsLoaded = false;
                        state.modelErfsLoaded = false;
                        state.materialErfsLoaded = false;
                        state.textureErfs.clear();
                        state.modelErfs.clear();
                        state.materialErfs.clear();
                        clearPropCache();
                        ensureBaseErfsLoaded(state);

                        auto rimForModel = std::make_unique<ERFFile>();
                        auto rimForMat = std::make_unique<ERFFile>();
                        rimForModel->open(state.currentRIMPath);
                        rimForMat->open(state.currentRIMPath);
                        state.modelErfs.push_back(std::move(rimForModel));
                        state.materialErfs.push_back(std::move(rimForMat));

                        std::string rimDir = fs::path(state.currentRIMPath).parent_path().string();
                        for (const auto& dirEntry : fs::directory_iterator(rimDir)) {
                            if (!dirEntry.is_regular_file()) continue;
                            std::string fname = dirEntry.path().filename().string();
                            std::string fnameLower = fname;
                            std::transform(fnameLower.begin(), fnameLower.end(), fnameLower.begin(), ::tolower);
                            if (fnameLower.size() > 8 && fnameLower.substr(fnameLower.size() - 8) == ".gpu.rim") {
                                auto g1 = std::make_unique<ERFFile>();
                                auto g2 = std::make_unique<ERFFile>();
                                if (g1->open(dirEntry.path().string()) && g2->open(dirEntry.path().string())) {
                                    state.textureErfs.push_back(std::move(g1));
                                    state.materialErfs.push_back(std::move(g2));
                                }
                            }
                        }

                        for (const auto& dirEntry : fs::directory_iterator(rimDir)) {
                            if (!dirEntry.is_regular_file()) continue;
                            std::string dpath = dirEntry.path().string();
                            if (dpath == state.currentRIMPath) continue;
                            std::string fname = dirEntry.path().filename().string();
                            std::string fnameLower = fname;
                            std::transform(fnameLower.begin(), fnameLower.end(), fnameLower.begin(), ::tolower);
                            if (fnameLower.size() > 4 && fnameLower.substr(fnameLower.size() - 4) == ".rim" &&
                                fnameLower.find(".gpu.rim") == std::string::npos) {
                                auto sibRim = std::make_unique<ERFFile>();
                                if (sibRim->open(dpath)) {
                                    state.materialErfs.push_back(std::move(sibRim));
                                }
                            }
                        }

                        for (const auto& mat : state.currentModel.materials) {
                            if (mat.diffuseTexId != 0)     destroyTexture(mat.diffuseTexId);
                            if (mat.normalTexId != 0)      destroyTexture(mat.normalTexId);
                            if (mat.specularTexId != 0)    destroyTexture(mat.specularTexId);
                            if (mat.tintTexId != 0)        destroyTexture(mat.tintTexId);
                        }
                        state.currentModel = Model();
                        state.currentModel.name = fs::path(state.currentRIMPath).stem().string() + " (level)";
                        state.hasModel = true;
                        state.selectedLevelChunk = -1;
                        state.currentModelAnimations.clear();

                        state.levelLoad = {};
                        state.levelLoad.stage = 1;
                        state.levelLoad.stageLabel = "Scanning level data...";
                    }
                }
            }

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::InputText("##rimFilter", state.rimEntryFilter, sizeof(state.rimEntryFilter));

            ImGui::BeginChild("RIMEntryList", ImVec2(0, 0), true);
            std::string rimFilterLower = state.rimEntryFilter;
            std::transform(rimFilterLower.begin(), rimFilterLower.end(), rimFilterLower.begin(), ::tolower);

            static std::vector<int> s_filteredRimIndices;
            static std::string s_lastRimFilter;
            static size_t s_lastRimCount = 0;
            if (rimFilterLower != s_lastRimFilter || state.rimEntries.size() != s_lastRimCount) {
                s_lastRimFilter = rimFilterLower;
                s_lastRimCount = state.rimEntries.size();
                s_filteredRimIndices.clear();
                s_filteredRimIndices.reserve(state.rimEntries.size());
                for (int i = 0; i < (int)state.rimEntries.size(); i++) {
                    if (!rimFilterLower.empty()) {
                        std::string nameLower = state.rimEntries[i].name;
                        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                        if (nameLower.find(rimFilterLower) == std::string::npos) continue;
                    }
                    s_filteredRimIndices.push_back(i);
                }
            }

            ImGuiListClipper clipper;
            clipper.Begin((int)s_filteredRimIndices.size());
            while (clipper.Step()) {
                for (int fi = clipper.DisplayStart; fi < clipper.DisplayEnd; fi++) {
                    int i = s_filteredRimIndices[fi];
                    const auto& re = state.rimEntries[i];

                    bool isGff = isGffFile(re.name);
                    bool isModel = isModelFile(re.name);
                    bool isMao = isMaoFile(re.name);
                    bool isPhy = isPhyFile(re.name);
                    bool isTexture = re.name.size() > 4 && re.name.substr(re.name.size() - 4) == ".dds";
                    if (isModel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
                    else if (isMao) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.4f, 1.0f));
                    else if (isPhy) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 1.0f, 1.0f));
                    else if (isTexture) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
                    else if (isGff) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.8f, 1.0f));

                    char label[256];
                    snprintf(label, sizeof(label), "%s##rim%d", re.name.c_str(), i);
                    bool selected = (state.selectedRIMEntry == i);
                    if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                        state.selectedRIMEntry = i;
                        if (ImGui::IsMouseDoubleClicked(0)) {
                            if (isModel) {

                                state.showTerrain = false;
                                g_terrainLoader.clear();
                                state.currentErf = std::make_unique<ERFFile>();
                                if (state.currentErf->open(state.currentRIMPath)) {

                                    ensureBaseErfsLoaded(state);

                                    auto rimForModel = std::make_unique<ERFFile>();
                                    auto rimForMat = std::make_unique<ERFFile>();
                                    rimForModel->open(state.currentRIMPath);
                                    rimForMat->open(state.currentRIMPath);
                                    state.modelErfs.push_back(std::move(rimForModel));
                                    state.materialErfs.push_back(std::move(rimForMat));


                                    std::string rimDir = fs::path(state.currentRIMPath).parent_path().string();
                                    int gpuPushed = 0;
                                    for (const auto& dirEntry : fs::directory_iterator(rimDir)) {
                                        if (!dirEntry.is_regular_file()) continue;
                                        std::string fname = dirEntry.path().filename().string();
                                        std::string fnameLower = fname;
                                        std::transform(fnameLower.begin(), fnameLower.end(), fnameLower.begin(), ::tolower);
                                        if (fnameLower.size() > 8 && fnameLower.substr(fnameLower.size() - 8) == ".gpu.rim") {
                                            std::string gpuPath = dirEntry.path().string();
                                            auto g1 = std::make_unique<ERFFile>();
                                            auto g2 = std::make_unique<ERFFile>();
                                            if (g1->open(gpuPath) && g2->open(gpuPath)) {
                                                state.textureErfs.push_back(std::move(g1));
                                                state.materialErfs.push_back(std::move(g2));
                                                gpuPushed += 2;
                                            }
                                        }
                                    }

                                    if (re.entryIdx < state.currentErf->entries().size()) {
                                        const auto& entry = state.currentErf->entries()[re.entryIdx];
                                        state.currentModelAnimations.clear();
                                        if (loadModelFromEntry(state, entry)) {
                                            state.statusMessage = "Loaded: " + re.name + " (from RIM)";
                                            if (gpuPushed) state.statusMessage += " + gpu.rim";
                                            state.showRenderSettings = true;
                                        } else {
                                            state.statusMessage = "Failed to parse: " + re.name;
                                        }
                                    }


                                    for (int p = 0; p < gpuPushed / 2; p++) {
                                        state.materialErfs.pop_back();
                                        state.textureErfs.pop_back();
                                    }
                                    state.modelErfs.pop_back();
                                    state.materialErfs.pop_back();
                                }
                            } else if (isTexture) {
                                ERFFile rimErf;
                                if (rimErf.open(state.currentRIMPath)) {
                                    if (re.entryIdx < rimErf.entries().size()) {
                                        auto data = rimErf.readEntry(rimErf.entries()[re.entryIdx]);
                                        if (!data.empty()) {
                                            std::string nameLower = re.name;
                                            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                                            state.textureCache[nameLower] = data;
                                            state.previewTextureId = createTextureFromDDS(data);
                                            state.previewTextureName = re.name;
                                            state.showTexturePreview = true;
                                            state.previewMeshIndex = -1;
                                            state.statusMessage = "Preview: " + re.name;
                                        }
                                    }
                                }
                            } else if (isMao) {
                                ERFFile rimErf;
                                if (rimErf.open(state.currentRIMPath)) {
                                    if (re.entryIdx < rimErf.entries().size()) {
                                        auto data = rimErf.readEntry(rimErf.entries()[re.entryIdx]);
                                        if (!data.empty()) {
                                            state.maoContent = std::string(data.begin(), data.end());
                                            state.maoFileName = re.name;
                                            state.showMaoViewer = true;
                                            state.statusMessage = "Opened MAO: " + re.name;
                                        }
                                    }
                                }
                            } else {
                                ERFFile rimErf;
                                if (rimErf.open(state.currentRIMPath)) {
                                    if (re.entryIdx < rimErf.entries().size()) {
                                        auto data = rimErf.readEntry(rimErf.entries()[re.entryIdx]);
                                        if (!data.empty() && isGffData(data)) {
                                            if (loadGffData(state.gffViewer, data, re.name, state.currentRIMPath)) {
                                                state.gffViewer.showWindow = true;
                                                state.statusMessage = "Opened: " + re.name;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (ImGui::BeginPopupContextItem()) {
                        if (isModel && ImGui::MenuItem("Load Model")) {
                            state.showTerrain = false;
                            g_terrainLoader.clear();
                            state.currentErf = std::make_unique<ERFFile>();
                            if (state.currentErf->open(state.currentRIMPath)) {
                                ensureBaseErfsLoaded(state);

                                auto rimForModel = std::make_unique<ERFFile>();
                                auto rimForMat = std::make_unique<ERFFile>();
                                rimForModel->open(state.currentRIMPath);
                                rimForMat->open(state.currentRIMPath);
                                state.modelErfs.push_back(std::move(rimForModel));
                                state.materialErfs.push_back(std::move(rimForMat));

                                std::string rimDir = fs::path(state.currentRIMPath).parent_path().string();
                                int gpuPushed = 0;
                                for (const auto& dirEntry : fs::directory_iterator(rimDir)) {
                                    if (!dirEntry.is_regular_file()) continue;
                                    std::string fname = dirEntry.path().filename().string();
                                    std::string fnameLower = fname;
                                    std::transform(fnameLower.begin(), fnameLower.end(), fnameLower.begin(), ::tolower);
                                    if (fnameLower.size() > 8 && fnameLower.substr(fnameLower.size() - 8) == ".gpu.rim") {
                                        std::string gpuPath = dirEntry.path().string();
                                        auto g1 = std::make_unique<ERFFile>();
                                        auto g2 = std::make_unique<ERFFile>();
                                        if (g1->open(gpuPath) && g2->open(gpuPath)) {
                                            state.textureErfs.push_back(std::move(g1));
                                            state.materialErfs.push_back(std::move(g2));
                                            gpuPushed += 2;
                                        }
                                    }
                                }

                                if (re.entryIdx < state.currentErf->entries().size()) {
                                    const auto& entry = state.currentErf->entries()[re.entryIdx];
                                    state.currentModelAnimations.clear();
                                    if (loadModelFromEntry(state, entry)) {
                                        state.statusMessage = "Loaded: " + re.name + " (from RIM)";
                                        if (gpuPushed) state.statusMessage += " + gpu.rim";
                                        state.showRenderSettings = true;
                                    } else {
                                        state.statusMessage = "Failed to parse: " + re.name;
                                    }
                                }
                                for (int p = 0; p < gpuPushed / 2; p++) {
                                    state.materialErfs.pop_back();
                                    state.textureErfs.pop_back();
                                }
                                state.modelErfs.pop_back();
                                state.materialErfs.pop_back();
                            }
                        }
                        if (ImGui::MenuItem("Open with GFF Viewer")) {
                            ERFFile rimErf;
                            if (rimErf.open(state.currentRIMPath)) {
                                if (re.entryIdx < rimErf.entries().size()) {
                                    auto data = rimErf.readEntry(rimErf.entries()[re.entryIdx]);
                                    if (!data.empty() && isGffData(data)) {
                                        if (loadGffData(state.gffViewer, data, re.name, state.currentRIMPath)) {
                                            state.gffViewer.showWindow = true;
                                            state.statusMessage = "Opened: " + re.name;
                                        }
                                    } else {
                                        state.statusMessage = "Not a valid GFF file: " + re.name;
                                    }
                                }
                            }
                        }
                        if (ImGui::MenuItem("Extract...")) {
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
                            config.fileName = re.name;
                            state.selectedRIMEntry = i;
                            ImGuiFileDialog::Instance()->OpenDialog("ExtractRIMEntry", "Save File", ".*", config);
                        }
                        ImGui::EndPopup();
                    }

                    if (isModel || isMao || isPhy || isTexture || isGff) ImGui::PopStyleColor();
                }
            }

            ImGui::EndChild();
            ImGui::EndChild();
        }
    } else ImGui::Text("Select an ERF file");
    ImGui::EndChild();
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
            std::string baseName = s_deleteModelName;
            size_t dotPos = baseName.rfind('.');
            if (dotPos != std::string::npos) baseName = baseName.substr(0, dotPos);
            int deletedCount = 0;
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
            unmarkModelAsImported(s_deleteModelName);
            std::string currentModelLower = state.currentModel.name;
            std::transform(currentModelLower.begin(), currentModelLower.end(), currentModelLower.begin(), ::tolower);
            std::string deletedModelLower = s_deleteModelName;
            std::transform(deletedModelLower.begin(), deletedModelLower.end(), deletedModelLower.begin(), ::tolower);
            if (currentModelLower == deletedModelLower || currentModelLower == baseName + ".msh") {
                state.currentModel = Model();
                state.hasModel = false;
            }
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

    if (ImGuiFileDialog::Instance()->Display("ExportFSBSample", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk() && state.selectedFSBSample >= 0) {
            std::string outPath = ImGuiFileDialog::Instance()->GetFilePathName();
            if (saveFSB4SampleToWav(state.currentFSBPath, state.selectedFSBSample, outPath)) {
                state.statusMessage = "Exported: " + state.currentFSBSamples[state.selectedFSBSample].name;
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (ImGuiFileDialog::Instance()->Display("ExportAllFSBSamples", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string outDir = ImGuiFileDialog::Instance()->GetCurrentPath();
            int exported = 0;
            for (int i = 0; i < (int)state.currentFSBSamples.size(); i++) {
                std::string name = state.currentFSBSamples[i].name;
                size_t dotPos = name.rfind('.');
                if (dotPos != std::string::npos) name = name.substr(0, dotPos);
                std::string outPath = outDir + "/" + name + ".wav";
                if (saveFSB4SampleToWav(state.currentFSBPath, i, outPath)) {
                    exported++;
                }
            }
            state.statusMessage = "Exported " + std::to_string(exported) + " samples";
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (ImGuiFileDialog::Instance()->Display("DumpAllRIM", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string outDir = ImGuiFileDialog::Instance()->GetCurrentPath();
            ERFFile rimErf;
            int count = 0;
            if (rimErf.open(state.currentRIMPath)) {
                for (const auto& entry : rimErf.entries()) {
                    std::string outPath = outDir + "/" + entry.name;
                    if (rimErf.extractEntry(entry, outPath)) count++;
                }
            }
            state.statusMessage = "Dumped " + std::to_string(count) + " files from RIM";
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (ImGuiFileDialog::Instance()->Display("ExtractRIMEntry", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk() && state.selectedRIMEntry >= 0 &&
            state.selectedRIMEntry < (int)state.rimEntries.size()) {
            std::string outPath = ImGuiFileDialog::Instance()->GetFilePathName();
            ERFFile rimErf;
            if (rimErf.open(state.currentRIMPath)) {
                size_t entryIdx = state.rimEntries[state.selectedRIMEntry].entryIdx;
                if (entryIdx < rimErf.entries().size()) {
                    if (rimErf.extractEntry(rimErf.entries()[entryIdx], outPath)) {
                        state.statusMessage = "Extracted: " + state.rimEntries[state.selectedRIMEntry].name;
                    }
                }
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }
}