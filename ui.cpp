#include "ui.h"
#include "types.h"
#include "mmh_loader.h"
#include "animation.h"
#include "erf.h"
#include "export.h"
#include "dds_loader.h"
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "ImGuiFileDialog.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <set>
#include <map>

#include "model_names_csv.h"

namespace fs = std::filesystem;

static const char* SETTINGS_FILE = "haventools_settings.ini";

static void saveSettings(const AppState& state) {
    std::ofstream f(SETTINGS_FILE);
    if (f.is_open()) {
        f << "lastDialogPath=" << state.lastDialogPath << "\n";
        f << "selectedFolder=" << state.selectedFolder << "\n";
    }
}

static void loadSettings(AppState& state) {
    std::ifstream f(SETTINGS_FILE);
    if (f.is_open()) {
        std::string line;
        while (std::getline(f, line)) {
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                std::string key = line.substr(0, eq);
                std::string val = line.substr(eq + 1);
                if (key == "lastDialogPath") state.lastDialogPath = val;
                else if (key == "selectedFolder") state.selectedFolder = val;
            }
        }
    }
}

static void loadMeshDatabase(AppState& state) {
    if (state.meshBrowser.loaded) return;

    std::string csvData(reinterpret_cast<const char*>(model_names_csv), model_names_csv_len);
    std::istringstream f(csvData);

    std::set<std::string> catSet;
    catSet.insert("All");

    std::string line;
    std::getline(f, line);

    while (std::getline(f, line)) {
        if (line.empty()) continue;

        MeshEntry entry;
        size_t p1 = line.find(',');
        if (p1 == std::string::npos) continue;
        entry.mshFile = line.substr(0, p1);

        size_t p2 = line.find(',', p1 + 1);
        if (p2 == std::string::npos) continue;
        entry.mshName = line.substr(p1 + 1, p2 - p1 - 1);

        size_t p3 = line.find(',', p2 + 1);
        if (p3 == std::string::npos) continue;
        std::string lodStr = line.substr(p2 + 1, p3 - p2 - 1);
        entry.lod = lodStr.empty() ? 0 : std::stoi(lodStr);

        size_t p4 = line.find(',', p3 + 1);
        if (p4 == std::string::npos) {
            entry.category = line.substr(p3 + 1);
        } else {
            entry.category = line.substr(p3 + 1, p4 - p3 - 1);
            std::string animStr = line.substr(p4 + 1);
            while (!animStr.empty() && (animStr.back() == '\r' || animStr.back() == '\n'))
                animStr.pop_back();
            if (!animStr.empty()) {
                std::istringstream animStream(animStr);
                std::string anim;
                while (animStream >> anim) {
                    entry.animations.push_back(anim);
                }
            }
        }

        while (!entry.category.empty() && (entry.category.back() == '\r' || entry.category.back() == '\n'))
            entry.category.pop_back();
        if (entry.category.empty()) entry.category = "UNK";

        catSet.insert(entry.category);
        state.meshBrowser.allMeshes.push_back(entry);
    }

    state.meshBrowser.categories.assign(catSet.begin(), catSet.end());
    std::sort(state.meshBrowser.categories.begin(), state.meshBrowser.categories.end());

    auto it = std::find(state.meshBrowser.categories.begin(), state.meshBrowser.categories.end(), "All");
    if (it != state.meshBrowser.categories.end()) {
        state.meshBrowser.categories.erase(it);
        state.meshBrowser.categories.insert(state.meshBrowser.categories.begin(), "All");
    }

    state.meshBrowser.loaded = true;
}

void handleInput(AppState& state, GLFWwindow* window, ImGuiIO& io) {
    static bool settingsLoaded = false;
    if (!settingsLoaded) {
        loadSettings(state);
        settingsLoaded = true;
    }

    if (!io.WantCaptureMouse) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            ImGui::SetWindowFocus(nullptr);
            if (state.isPanning) {
                float dx = static_cast<float>(mx - state.lastMouseX);
                float dy = static_cast<float>(my - state.lastMouseY);
                state.camera.rotate(-dx * state.camera.lookSensitivity, -dy * state.camera.lookSensitivity);
            }
            state.isPanning = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            float scroll = io.MouseWheel;
            if (scroll != 0.0f) {
                state.camera.moveSpeed *= (scroll > 0) ? 1.2f : 0.8f;
                if (state.camera.moveSpeed < 0.1f) state.camera.moveSpeed = 0.1f;
                if (state.camera.moveSpeed > 100.0f) state.camera.moveSpeed = 100.0f;
            }
        } else {
            if (state.isPanning) glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            state.isPanning = false;
        }
        state.lastMouseX = mx;
        state.lastMouseY = my;
    }
    if (!io.WantCaptureKeyboard) {
        float deltaTime = io.DeltaTime;
        float speed = state.camera.moveSpeed * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) speed *= 3.0f;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) state.camera.moveForward(speed);
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) state.camera.moveForward(-speed);
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) state.camera.moveRight(-speed);
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) state.camera.moveRight(speed);
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) state.camera.moveUp(speed);
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) state.camera.moveUp(-speed);
    }
}

static void drawMeshBrowserWindow(AppState& state) {
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
                    std::string mshLower = entry->mshFile;
                    std::transform(mshLower.begin(), mshLower.end(), mshLower.begin(), ::tolower);

                    for (const auto& erfPath : state.erfFiles) {
                        ERFFile erf;
                        if (erf.open(erfPath)) {
                            for (const auto& erfEntry : erf.entries()) {
                                std::string entryLower = erfEntry.name;
                                std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);

                                if (entryLower == mshLower) {
                                    state.currentErf = std::make_unique<ERFFile>();
                                    state.currentErf->open(erfPath);
                                    if (loadModelFromEntry(state, erfEntry)) {
                                        state.statusMessage = "Loaded: " + displayName;
                                        state.showRenderSettings = true;
                                        state.currentModelAnimations = entry->animations;
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

static void drawBrowserWindow(AppState& state) {
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
    ImGui::Text("ERF Files (%zu)", state.erfsByName.size());
    ImGui::Separator();
    ImGui::BeginChild("ERFList", ImVec2(0, 0), true);

    for (const auto& [filename, indices] : state.erfsByName) {
        bool isSelected = (state.selectedErfName == filename);
        if (ImGui::Selectable(filename.c_str(), isSelected)) {
            if (!isSelected) {
                state.selectedErfName = filename;
                state.selectedEntryIndex = -1;
                state.mergedEntries.clear();

                std::set<std::string> seenNames;
                for (size_t erfIdx : indices) {
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
                                state.mergedEntries.push_back(ce);
                            }
                        }
                    }
                }
                state.statusMessage = std::to_string(state.mergedEntries.size()) + " entries from " + std::to_string(indices.size()) + " ERF(s)";
            }
        }
    }

    ImGui::EndChild();
    ImGui::NextColumn();
    if (!state.selectedErfName.empty() && !state.mergedEntries.empty()) {
        bool hasTextures = false, hasModels = false;
        for (const auto& ce : state.mergedEntries) {
            if (ce.name.size() > 4 && ce.name.substr(ce.name.size() - 4) == ".dds") hasTextures = true;
            if (isModelFile(ce.name)) hasModels = true;
            if (hasTextures && hasModels) break;
        }

        ImGui::Text("Contents (%zu)", state.mergedEntries.size());
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::InputText("##contentSearch", state.contentFilter, sizeof(state.contentFilter));
        if (hasTextures && ImGui::Button("Dump Textures")) {
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
        if (hasModels) {
            if (hasTextures) ImGui::SameLine();
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
        ImGui::Separator();

        std::string filterLower = state.contentFilter;
        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);

        std::vector<int> filteredIndices;
        for (int i = 0; i < (int)state.mergedEntries.size(); i++) {
            if (filterLower.empty()) {
                filteredIndices.push_back(i);
            } else {
                std::string nameLower = state.mergedEntries[i].name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                if (nameLower.find(filterLower) != std::string::npos) {
                    filteredIndices.push_back(i);
                }
            }
        }

        ImGui::BeginChild("EntryList", ImVec2(0, 0), true);
        ImGuiListClipper entryClipper;
        entryClipper.Begin(static_cast<int>(filteredIndices.size()));
        while (entryClipper.Step()) {
            for (int fi = entryClipper.DisplayStart; fi < entryClipper.DisplayEnd; fi++) {
                int i = filteredIndices[fi];
                const CachedEntry& ce = state.mergedEntries[i];

                bool isModel = isModelFile(ce.name), isMao = isMaoFile(ce.name), isPhy = isPhyFile(ce.name);
                bool isTexture = ce.name.size() > 4 && ce.name.substr(ce.name.size() - 4) == ".dds";
                if (isModel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
                else if (isMao) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.4f, 1.0f));
                else if (isPhy) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 1.0f, 1.0f));
                else if (isTexture) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
                char label[256]; snprintf(label, sizeof(label), "%s##%d", ce.name.c_str(), i);
                if (ImGui::Selectable(label, i == state.selectedEntryIndex, ImGuiSelectableFlags_AllowDoubleClick)) {
                    state.selectedEntryIndex = i;
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        ERFFile erf;
                        if (erf.open(state.erfFiles[ce.erfIdx])) {
                            if (ce.entryIdx < erf.entries().size()) {
                                const auto& entry = erf.entries()[ce.entryIdx];
                                if (isModel) {
                                    state.currentErf = std::make_unique<ERFFile>();
                                    state.currentErf->open(state.erfFiles[ce.erfIdx]);
                                    if (loadModelFromEntry(state, entry)) {
                                        state.statusMessage = "Loaded: " + ce.name;
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
                                }
                            }
                        }
                    }
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
                if (isModel || isMao || isPhy || isTexture) ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();
    } else ImGui::Text("Select an ERF file");
    ImGui::Columns(1);
    ImGui::End();
}

static void drawRenderSettingsWindow(AppState& state) {
    ImGui::SetNextWindowSizeConstraints(ImVec2(300, 100), ImVec2(500, 800));
    ImGui::Begin("Render Settings", &state.showRenderSettings, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Checkbox("Wireframe", &state.renderSettings.wireframe);
    ImGui::Checkbox("Show Axes", &state.renderSettings.showAxes);
    ImGui::Checkbox("Show Grid", &state.renderSettings.showGrid);
    ImGui::Checkbox("Show Collision", &state.renderSettings.showCollision);
    if (state.renderSettings.showCollision) { ImGui::SameLine(); ImGui::Checkbox("Wireframe##coll", &state.renderSettings.collisionWireframe); }
    ImGui::Checkbox("Show Skeleton", &state.renderSettings.showSkeleton);
    ImGui::Checkbox("Show Textures", &state.renderSettings.showTextures);
    ImGui::Separator();
    ImGui::Text("Camera Speed: %.1f", state.camera.moveSpeed);
    ImGui::SliderFloat("##speed", &state.camera.moveSpeed, 0.1f, 100.0f, "%.1f");
    if (state.hasModel) {
        ImGui::Separator();
        size_t totalVerts = 0, totalTris = 0;
        for (const auto& m : state.currentModel.meshes) { totalVerts += m.vertices.size(); totalTris += m.indices.size() / 3; }
        ImGui::Text("Total: %zu meshes, %zu verts, %zu tris", state.currentModel.meshes.size(), totalVerts, totalTris);
        if (state.renderSettings.meshVisible.size() != state.currentModel.meshes.size())
            state.renderSettings.initMeshVisibility(state.currentModel.meshes.size());
        if (state.currentModel.meshes.size() >= 1) {
            ImGui::Separator(); ImGui::Text("Meshes:");
            float listHeight = std::min(300.0f, state.currentModel.meshes.size() * 50.0f + 20.0f);
            ImGui::BeginChild("MeshList", ImVec2(0, listHeight), true);
            for (size_t i = 0; i < state.currentModel.meshes.size(); i++) {
                const auto& mesh = state.currentModel.meshes[i];
                ImGui::PushID(static_cast<int>(i));
                bool visible = state.renderSettings.meshVisible[i] != 0;
                if (ImGui::Checkbox("##vis", &visible)) state.renderSettings.meshVisible[i] = visible ? 1 : 0;
                ImGui::SameLine();
                ImGui::Text("%s", mesh.name.empty() ? ("Mesh " + std::to_string(i)).c_str() : mesh.name.c_str());
                ImGui::Indent();
                ImGui::TextDisabled("%zu verts, %zu tris", mesh.vertices.size(), mesh.indices.size() / 3);
                if (!mesh.materialName.empty()) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Material: %s", mesh.materialName.c_str());
                if (ImGui::SmallButton("View UVs")) { state.selectedMeshForUv = static_cast<int>(i); state.showUvViewer = true; }
                ImGui::Unindent();
                ImGui::PopID();
            }
            ImGui::EndChild();
            if (state.currentModel.meshes.size() > 1) {
                if (ImGui::Button("Show All")) for (auto& v : state.renderSettings.meshVisible) v = 1;
                ImGui::SameLine();
                if (ImGui::Button("Hide All")) for (auto& v : state.renderSettings.meshVisible) v = 0;
            }
        }
        if (!state.currentModel.materials.empty()) {
            ImGui::Separator();
            if (ImGui::TreeNode("Materials", "Materials (%zu)", state.currentModel.materials.size())) {
                for (size_t i = 0; i < state.currentModel.materials.size(); i++) {
                    const auto& mat = state.currentModel.materials[i];
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "%s", mat.name.c_str());
                    ImGui::Indent();
                    if (!mat.maoContent.empty()) {
                        if (ImGui::SmallButton("View MAO")) {
                            state.maoContent = mat.maoContent;
                            state.maoFileName = mat.name + ".mao";
                            state.showMaoViewer = true;
                        }
                    }

                    int meshForMat = -1;
                    for (size_t mi = 0; mi < state.currentModel.meshes.size(); mi++) {
                        if (state.currentModel.meshes[mi].materialIndex == (int)i) { meshForMat = (int)mi; break; }
                    }

                    if (!mat.diffuseMap.empty()) {
                        ImGui::Text("Diffuse: %s", mat.diffuseMap.c_str());
                        if (mat.diffuseTexId != 0) {
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Preview##diff")) {
                                state.previewTextureId = mat.diffuseTexId;
                                state.previewTextureName = mat.diffuseMap;
                                state.previewMeshIndex = meshForMat;
                                state.showTexturePreview = true;
                            }
                        }
                    }
                    if (!mat.normalMap.empty()) {
                        ImGui::Text("Normal: %s", mat.normalMap.c_str());
                        if (mat.normalTexId != 0) {
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Preview##norm")) {
                                state.previewTextureId = mat.normalTexId;
                                state.previewTextureName = mat.normalMap;
                                state.previewMeshIndex = meshForMat;
                                state.showTexturePreview = true;
                            }
                        }
                    }
                    if (!mat.specularMap.empty()) {
                        ImGui::Text("Specular: %s", mat.specularMap.c_str());
                        if (mat.specularTexId != 0) {
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Preview##spec")) {
                                state.previewTextureId = mat.specularTexId;
                                state.previewTextureName = mat.specularMap;
                                state.previewMeshIndex = meshForMat;
                                state.showTexturePreview = true;
                            }
                        }
                    }
                    if (!mat.tintMap.empty()) {
                        ImGui::Text("Tint: %s", mat.tintMap.c_str());
                        if (mat.tintTexId != 0) {
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Preview##tint")) {
                                state.previewTextureId = mat.tintTexId;
                                state.previewTextureName = mat.tintMap;
                                state.previewMeshIndex = meshForMat;
                                state.showTexturePreview = true;
                            }
                        }
                    }
                    ImGui::Unindent();
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
        }
        if (!state.currentModel.skeleton.bones.empty()) {
            ImGui::Separator();
            if (ImGui::TreeNode("Skeleton", "Skeleton (%zu bones)", state.currentModel.skeleton.bones.size())) {
                if (state.selectedBoneIndex >= 0) {
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Selected: %s",
                        state.currentModel.skeleton.bones[state.selectedBoneIndex].name.c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Clear")) state.selectedBoneIndex = -1;
                } else {
                    ImGui::TextDisabled("Click a bone to highlight it");
                }
                ImGui::BeginChild("BoneList", ImVec2(0, 200), true);
                for (size_t i = 0; i < state.currentModel.skeleton.bones.size(); i++) {
                    const auto& bone = state.currentModel.skeleton.bones[i];
                    bool isSelected = (state.selectedBoneIndex == (int)i);

                    ImGui::PushID(static_cast<int>(i));

                    ImVec4 color;
                    if (isSelected) {
                        color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
                    } else if (bone.parentIndex < 0) {
                        color = ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
                    } else {
                        color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                    }

                    ImGui::PushStyleColor(ImGuiCol_Text, color);

                    char label[256];
                    if (bone.parentIndex < 0) {
                        snprintf(label, sizeof(label), "[%zu] %s (root)", i, bone.name.c_str());
                    } else {
                        snprintf(label, sizeof(label), "[%zu] %s -> %s", i, bone.name.c_str(), bone.parentName.c_str());
                    }

                    if (ImGui::Selectable(label, isSelected)) {
                        state.selectedBoneIndex = isSelected ? -1 : (int)i;
                    }

                    ImGui::PopStyleColor();
                    ImGui::PopID();
                }
                ImGui::EndChild();
                ImGui::TreePop();
            }
        }
    }
    ImGui::End();
}

static void drawMaoViewer(AppState& state) {
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin(("MAO Viewer - " + state.maoFileName).c_str(), &state.showMaoViewer);
    if (ImGui::Button("Copy to Clipboard")) ImGui::SetClipboardText(state.maoContent.c_str());
    ImGui::Separator();
    ImGui::BeginChild("MaoContent", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::TextUnformatted(state.maoContent.c_str());
    ImGui::EndChild();
    ImGui::End();
}

static void drawTexturePreview(AppState& state) {
    std::string title = "Texture Preview - " + state.previewTextureName;
    ImGui::SetNextWindowSize(ImVec2(520, 580), ImGuiCond_FirstUseEver);
    ImGui::Begin(title.c_str(), &state.showTexturePreview);

    ImGui::Checkbox("Show UV Overlay", &state.showUvOverlay);
    ImGui::Separator();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float size = std::min(avail.x, avail.y - 20);
    if (size < 100) size = 100;

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + size, canvasPos.y + size), IM_COL32(40, 40, 40, 255));

    if (state.previewTextureId != 0) {
        drawList->AddImage(
            (ImTextureID)(intptr_t)state.previewTextureId,
            canvasPos,
            ImVec2(canvasPos.x + size, canvasPos.y + size),
            ImVec2(0, 0), ImVec2(1, 1)
        );
    }

    if (state.showUvOverlay && state.previewMeshIndex >= 0 &&
        state.previewMeshIndex < (int)state.currentModel.meshes.size()) {
        const auto& mesh = state.currentModel.meshes[state.previewMeshIndex];
        for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const auto& v0 = mesh.vertices[mesh.indices[i]];
            const auto& v1 = mesh.vertices[mesh.indices[i + 1]];
            const auto& v2 = mesh.vertices[mesh.indices[i + 2]];
            ImVec2 p0(canvasPos.x + v0.u * size, canvasPos.y + (1.0f - v0.v) * size);
            ImVec2 p1(canvasPos.x + v1.u * size, canvasPos.y + (1.0f - v1.v) * size);
            ImVec2 p2(canvasPos.x + v2.u * size, canvasPos.y + (1.0f - v2.v) * size);
            drawList->AddTriangle(p0, p1, p2, IM_COL32(255, 255, 0, 200), 1.0f);
        }
    }

    ImGui::Dummy(ImVec2(size, size));
    ImGui::End();
}

static void drawUvViewer(AppState& state) {
    const auto& mesh = state.currentModel.meshes[state.selectedMeshForUv];
    std::string title = "UV Viewer - " + (mesh.name.empty() ? "Mesh " + std::to_string(state.selectedMeshForUv) : mesh.name);
    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin(title.c_str(), &state.showUvViewer);
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    float size = std::min(canvasSize.x, canvasSize.y - 20);
    if (size < 100) size = 100;
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + size, canvasPos.y + size), IM_COL32(40, 40, 40, 255));
    for (int ii = 0; ii <= 8; ii++) {
        float t = float(ii) / 8;
        ImU32 col = (ii == 0 || ii == 8) ? IM_COL32(100, 100, 100, 255) : IM_COL32(60, 60, 60, 255);
        drawList->AddLine(ImVec2(canvasPos.x + t * size, canvasPos.y), ImVec2(canvasPos.x + t * size, canvasPos.y + size), col);
        drawList->AddLine(ImVec2(canvasPos.x, canvasPos.y + t * size), ImVec2(canvasPos.x + size, canvasPos.y + t * size), col);
    }
    for (size_t ii = 0; ii + 2 < mesh.indices.size(); ii += 3) {
        const auto& v0 = mesh.vertices[mesh.indices[ii]];
        const auto& v1 = mesh.vertices[mesh.indices[ii + 1]];
        const auto& v2 = mesh.vertices[mesh.indices[ii + 2]];
        ImVec2 p0(canvasPos.x + v0.u * size, canvasPos.y + (1.0f - v0.v) * size);
        ImVec2 p1(canvasPos.x + v1.u * size, canvasPos.y + (1.0f - v1.v) * size);
        ImVec2 p2(canvasPos.x + v2.u * size, canvasPos.y + (1.0f - v2.v) * size);
        drawList->AddTriangle(p0, p1, p2, IM_COL32(0, 200, 255, 200), 1.0f);
    }
    ImGui::Dummy(ImVec2(size, size));
    ImGui::Text("Triangles: %zu", mesh.indices.size() / 3);
    ImGui::End();
}

static void drawAnimWindow(AppState& state, ImGuiIO& io) {
    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("Animations", &state.showAnimWindow);

    std::vector<size_t> filteredAnims;
    for (size_t ii = 0; ii < state.availableAnimFiles.size(); ii++) {
        if (state.currentModelAnimations.empty()) {
            filteredAnims.push_back(ii);
        } else {
            std::string animName = state.availableAnimFiles[ii].first;
            size_t dotPos = animName.rfind('.');
            if (dotPos != std::string::npos) animName = animName.substr(0, dotPos);
            for (const auto& validAnim : state.currentModelAnimations) {
                if (animName == validAnim) {
                    filteredAnims.push_back(ii);
                    break;
                }
            }
        }
    }

    if (filteredAnims.empty()) ImGui::TextDisabled("No animations for this model");
    else {
        if (state.animPlaying && state.currentAnim.duration > 0) {
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Playing: %s", state.currentAnim.name.c_str());
            ImGui::ProgressBar(state.animTime / state.currentAnim.duration);
            if (ImGui::Button("Stop")) { state.animPlaying = false; state.animTime = 0.0f; state.currentModel.skeleton.bones = state.basePoseBones; }
            ImGui::Separator();
        }
        ImGui::Text("%zu animations", filteredAnims.size());
        ImGui::InputText("Filter", state.animFilter, sizeof(state.animFilter));
        std::string filterLower = state.animFilter;
        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
        ImGui::BeginChild("AnimList", ImVec2(0, 0), true);
        for (size_t idx : filteredAnims) {
            std::string nameLower = state.availableAnimFiles[idx].first;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            if (!filterLower.empty() && nameLower.find(filterLower) == std::string::npos) continue;
            if (ImGui::Selectable(state.availableAnimFiles[idx].first.c_str(), state.selectedAnimIndex == (int)idx, ImGuiSelectableFlags_AllowDoubleClick)) {
                state.selectedAnimIndex = (int)idx;
                if (ImGui::IsMouseDoubleClicked(0)) {
                    ERFFile erf;
                    if (erf.open(state.availableAnimFiles[idx].second)) {
                        for (const auto& entry : erf.entries()) {
                            if (entry.name == state.availableAnimFiles[idx].first) {
                                auto aniData = erf.readEntry(entry);
                                if (!aniData.empty()) {
                                    state.currentAnim = loadANI(aniData, entry.name);

                                    int matched = 0;
                                    for (auto& track : state.currentAnim.tracks) {
                                        track.boneIndex = state.currentModel.skeleton.findBone(track.boneName);
                                        if (track.boneIndex >= 0) {
                                            matched++;
                                            std::cout << "  EXACT MATCH: '" << track.boneName << "' -> bone " << track.boneIndex << std::endl;
                                        }
                                    }
                                    std::cout << "  Exact matches: " << matched << "/" << state.currentAnim.tracks.size() << std::endl;

                                    if (matched == 0) {
                                        std::cout << "  Trying case-insensitive matching..." << std::endl;
                                        for (auto& track : state.currentAnim.tracks) {
                                            std::string trackNameLower = track.boneName;
                                            std::transform(trackNameLower.begin(), trackNameLower.end(), trackNameLower.begin(), ::tolower);

                                            for (size_t bi = 0; bi < state.currentModel.skeleton.bones.size(); bi++) {
                                                std::string boneNameLower = state.currentModel.skeleton.bones[bi].name;
                                                std::transform(boneNameLower.begin(), boneNameLower.end(), boneNameLower.begin(), ::tolower);

                                                if (trackNameLower == boneNameLower) {
                                                    track.boneIndex = (int)bi;
                                                    matched++;
                                                    std::cout << "  CASE-INSENSITIVE MATCH: '" << track.boneName << "' -> '" << state.currentModel.skeleton.bones[bi].name << "' (bone " << bi << ")" << std::endl;
                                                    break;
                                                }
                                            }
                                        }
                                    }

                                    std::cout << "  Final matched: " << matched << "/" << state.currentAnim.tracks.size() << " bones" << std::endl;

                                    if (matched > 0) {
                                        state.animPlaying = true;
                                        state.animTime = 0.0f;
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();
    if (state.animPlaying && state.currentAnim.duration > 0) {
        state.animTime += io.DeltaTime * state.animSpeed;
        if (state.animTime > state.currentAnim.duration) state.animTime = 0.0f;
    }
}

static bool showSplash = true;

static void filterEncryptedErfs(AppState& state) {
    state.filteredErfIndices.clear();
    state.erfsByName.clear();
    for (size_t i = 0; i < state.erfFiles.size(); i++) {
        ERFFile testErf;
        if (testErf.open(state.erfFiles[i]) && testErf.encryption() == 0) {
            state.filteredErfIndices.push_back(i);
            std::string filename = fs::path(state.erfFiles[i]).filename().string();
            state.erfsByName[filename].push_back(i);
        }
    }
}

static void drawSplashScreen(AppState& state, int displayW, int displayH) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)displayW, (float)displayH));
    ImGui::Begin("##Splash", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);

    float centerX = displayW * 0.5f;
    float centerY = displayH * 0.5f;

    const char* title = "Haven Tools";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    ImGui::SetCursorPos(ImVec2(centerX - titleSize.x * 0.5f, centerY - 60));
    ImGui::Text("%s", title);

    const char* subtitle = "Dragon Age: Origins Model Browser";
    ImVec2 subSize = ImGui::CalcTextSize(subtitle);
    ImGui::SetCursorPos(ImVec2(centerX - subSize.x * 0.5f, centerY - 30));
    ImGui::TextDisabled("%s", subtitle);

    ImVec2 buttonSize(250, 40);
    ImGui::SetCursorPos(ImVec2(centerX - buttonSize.x * 0.5f, centerY + 10));
    if (ImGui::Button("Browse to DAOriginsLauncher.exe", buttonSize)) {
        IGFD::FileDialogConfig config;
        config.path = state.lastDialogPath.empty() ? "." : state.lastDialogPath;
        ImGuiFileDialog::Instance()->OpenDialog("ChooseLauncher", "Select DAOriginsLauncher.exe", ".exe", config);
    }

    ImGui::End();
}

void drawUI(AppState& state, GLFWwindow* window, ImGuiIO& io) {
    int displayW, displayH;
    glfwGetFramebufferSize(window, &displayW, &displayH);

    if (showSplash) {
        drawSplashScreen(state, displayW, displayH);

        if (ImGuiFileDialog::Instance()->Display("ChooseLauncher", ImGuiWindowFlags_NoCollapse, ImVec2(700, 450))) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
                state.selectedFolder = fs::path(filePath).parent_path().string();
                state.lastDialogPath = state.selectedFolder;
                state.erfFiles = scanForERFFiles(state.selectedFolder);
                filterEncryptedErfs(state);
                state.textureErfsLoaded = false;
                state.textureErfs.clear();
                state.modelErfsLoaded = false;
                state.modelErfs.clear();
                state.materialErfsLoaded = false;
                state.materialErfs.clear();
                state.statusMessage = "Found " + std::to_string(state.filteredErfIndices.size()) + " ERF files";
                saveSettings(state);
                showSplash = false;
            }
            ImGuiFileDialog::Instance()->Close();
        }
        return;
    }

    if (state.showBrowser) drawBrowserWindow(state);
    if (state.showMeshBrowser) drawMeshBrowserWindow(state);
    if (ImGuiFileDialog::Instance()->Display("ChooseFolder", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            state.selectedFolder = ImGuiFileDialog::Instance()->GetCurrentPath();
            state.lastDialogPath = state.selectedFolder;
            state.erfFiles = scanForERFFiles(state.selectedFolder);
            filterEncryptedErfs(state);
            state.textureErfsLoaded = false;
            state.textureErfs.clear();
            state.modelErfsLoaded = false;
            state.modelErfs.clear();
            state.materialErfsLoaded = false;
            state.materialErfs.clear();
            state.selectedErfName.clear();
            state.mergedEntries.clear();
            state.selectedEntryIndex = -1;
            state.statusMessage = "Found " + std::to_string(state.erfsByName.size()) + " ERF files";
            saveSettings(state);
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGuiFileDialog::Instance()->Display("ExportGLB", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk() && state.pendingExport) {
            std::string exportPath = ImGuiFileDialog::Instance()->GetFilePathName();

            ERFFile erf;
            if (erf.open(state.erfFiles[state.pendingExportEntry.erfIdx])) {
                if (state.pendingExportEntry.entryIdx < erf.entries().size()) {
                    const auto& entry = erf.entries()[state.pendingExportEntry.entryIdx];
                    state.currentErf = std::make_unique<ERFFile>();
                    state.currentErf->open(state.erfFiles[state.pendingExportEntry.erfIdx]);
                    if (loadModelFromEntry(state, entry)) {
                        std::vector<Animation> exportAnims;
                        for (const auto& animFile : state.availableAnimFiles) {
                            std::string animName = animFile.first;
                            size_t dotPos = animName.rfind('.');
                            if (dotPos != std::string::npos) animName = animName.substr(0, dotPos);
                            bool found = false;
                            for (const auto& validAnim : state.currentModelAnimations) {
                                if (animName == validAnim) { found = true; break; }
                            }
                            if (!found) continue;

                            ERFFile animErf;
                            if (animErf.open(animFile.second)) {
                                for (const auto& animEntry : animErf.entries()) {
                                    if (animEntry.name == animFile.first) {
                                        auto aniData = animErf.readEntry(animEntry);
                                        if (!aniData.empty()) {
                                            Animation anim = loadANI(aniData, animEntry.name);
                                            for (auto& track : anim.tracks) {
                                                track.boneIndex = state.currentModel.skeleton.findBone(track.boneName);
                                            }
                                            exportAnims.push_back(anim);
                                        }
                                        break;
                                    }
                                }
                            }
                        }

                        if (exportToGLB(state.currentModel, exportAnims, exportPath)) {
                            state.statusMessage = "Exported: " + exportPath + " (" + std::to_string(exportAnims.size()) + " anims)";
                        } else {
                            state.statusMessage = "Export failed!";
                        }
                    }
                }
            }
            state.pendingExport = false;
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGuiFileDialog::Instance()->Display("ExportTexDDS", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk() && state.pendingTexExportDds) {
            std::string exportPath = ImGuiFileDialog::Instance()->GetFilePathName();
            ERFFile erf;
            if (erf.open(state.erfFiles[state.pendingTextureExport.erfIdx])) {
                if (state.pendingTextureExport.entryIdx < erf.entries().size()) {
                    auto data = erf.readEntry(erf.entries()[state.pendingTextureExport.entryIdx]);
                    if (!data.empty()) {
                        std::ofstream out(exportPath, std::ios::binary);
                        out.write(reinterpret_cast<const char*>(data.data()), data.size());
                        state.statusMessage = "Exported: " + exportPath;
                    }
                }
            }
            state.pendingTexExportDds = false;
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGuiFileDialog::Instance()->Display("ExportTexPNG", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk() && state.pendingTexExportPng) {
            std::string exportPath = ImGuiFileDialog::Instance()->GetFilePathName();
            ERFFile erf;
            if (erf.open(state.erfFiles[state.pendingTextureExport.erfIdx])) {
                if (state.pendingTextureExport.entryIdx < erf.entries().size()) {
                    auto data = erf.readEntry(erf.entries()[state.pendingTextureExport.entryIdx]);
                    if (!data.empty()) {
                        std::vector<uint8_t> rgba;
                        int w, h;
                        if (decodeDDSToRGBA(data, rgba, w, h)) {
                            std::vector<uint8_t> png;
                            encodePNG(rgba, w, h, png);
                            std::ofstream out(exportPath, std::ios::binary);
                            out.write(reinterpret_cast<const char*>(png.data()), png.size());
                            state.statusMessage = "Exported: " + exportPath;
                        } else {
                            state.statusMessage = "Failed to decode texture";
                        }
                    }
                }
            }
            state.pendingTexExportPng = false;
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGuiFileDialog::Instance()->Display("DumpTextures", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string outDir = ImGuiFileDialog::Instance()->GetCurrentPath();
            int exported = 0;
            for (const auto& ce : state.mergedEntries) {
                if (ce.name.size() > 4 && ce.name.substr(ce.name.size() - 4) == ".dds") {
                    ERFFile erf;
                    if (erf.open(state.erfFiles[ce.erfIdx])) {
                        if (ce.entryIdx < erf.entries().size()) {
                            auto data = erf.readEntry(erf.entries()[ce.entryIdx]);
                            if (!data.empty()) {
                                std::string outPath = outDir + "/" + ce.name;
                                std::ofstream out(outPath, std::ios::binary);
                                out.write(reinterpret_cast<const char*>(data.data()), data.size());
                                exported++;
                            }
                        }
                    }
                }
            }
            state.statusMessage = "Dumped " + std::to_string(exported) + " textures to " + outDir;
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGuiFileDialog::Instance()->Display("DumpModels", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string outDir = ImGuiFileDialog::Instance()->GetCurrentPath();
            int exported = 0;
            loadMeshDatabase(state);
            for (const auto& ce : state.mergedEntries) {
                if (isModelFile(ce.name)) {
                    ERFFile erf;
                    if (erf.open(state.erfFiles[ce.erfIdx])) {
                        if (ce.entryIdx < erf.entries().size()) {
                            const auto& entry = erf.entries()[ce.entryIdx];
                            state.currentErf = std::make_unique<ERFFile>();
                            state.currentErf->open(state.erfFiles[ce.erfIdx]);
                            if (loadModelFromEntry(state, entry)) {
                                state.currentModelAnimations.clear();
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

                                std::vector<Animation> exportAnims;
                                for (const auto& animFile : state.availableAnimFiles) {
                                    std::string animName = animFile.first;
                                    size_t dotPos = animName.rfind('.');
                                    if (dotPos != std::string::npos) animName = animName.substr(0, dotPos);
                                    bool found = false;
                                    for (const auto& validAnim : state.currentModelAnimations) {
                                        if (animName == validAnim) { found = true; break; }
                                    }
                                    if (!found) continue;

                                    ERFFile animErf;
                                    if (animErf.open(animFile.second)) {
                                        for (const auto& animEntry : animErf.entries()) {
                                            if (animEntry.name == animFile.first) {
                                                auto aniData = animErf.readEntry(animEntry);
                                                if (!aniData.empty()) {
                                                    Animation anim = loadANI(aniData, animEntry.name);
                                                    for (auto& track : anim.tracks) {
                                                        track.boneIndex = state.currentModel.skeleton.findBone(track.boneName);
                                                    }
                                                    exportAnims.push_back(anim);
                                                }
                                                break;
                                            }
                                        }
                                    }
                                }

                                std::string outName = ce.name;
                                size_t dotPos = outName.rfind('.');
                                if (dotPos != std::string::npos) outName = outName.substr(0, dotPos);
                                outName += ".glb";
                                std::string outPath = outDir + "/" + outName;
                                if (exportToGLB(state.currentModel, exportAnims, outPath)) {
                                    exported++;
                                }
                            }
                        }
                    }
                }
            }
            state.statusMessage = "Dumped " + std::to_string(exported) + " models to " + outDir;
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Browser", nullptr, &state.showBrowser);
            ImGui::MenuItem("Mesh Browser", nullptr, &state.showMeshBrowser);
            ImGui::MenuItem("Render Settings", nullptr, &state.showRenderSettings);
            ImGui::MenuItem("Animation", nullptr, &state.showAnimWindow);
            ImGui::EndMenu();
        }
        if (state.hasModel) {
            ImGui::SameLine(ImGui::GetWindowWidth() - 500);
            ImGui::Text("Model: %s | RMB: Look | WASD: Move", state.currentModel.name.c_str());
        }
        ImGui::EndMainMenuBar();
    }
    if (state.showRenderSettings) drawRenderSettingsWindow(state);
    if (state.showMaoViewer) drawMaoViewer(state);
    if (state.showTexturePreview && state.previewTextureId != 0) drawTexturePreview(state);
    if (state.showUvViewer && state.hasModel && state.selectedMeshForUv >= 0 && state.selectedMeshForUv < (int)state.currentModel.meshes.size()) drawUvViewer(state);
    if (state.showAnimWindow && state.hasModel) drawAnimWindow(state, io);
}