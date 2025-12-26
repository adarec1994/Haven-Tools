#include "ui.h"
#include "types.h"
#include "mmh_loader.h"
#include "animation.h"
#include "erf.h"
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "ImGuiFileDialog.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <set>

namespace fs = std::filesystem;

static void dumpAllMshFileNames(const AppState& state) {
    std::cout << "\n=== ALL MSH FILES ===" << std::endl;
    std::set<std::string> allMsh;
    for (const auto& erfPath : state.erfFiles) {
        ERFFile erf;
        if (erf.open(erfPath)) {
            for (const auto& entry : erf.entries()) {
                if (isMshFile(entry.name)) {
                    allMsh.insert(entry.name);
                }
            }
        }
    }
    for (const auto& name : allMsh) {
        std::cout << name << std::endl;
    }
    std::cout << "=== TOTAL: " << allMsh.size() << " MSH files ===" << std::endl;
}

void handleInput(AppState& state, GLFWwindow* window, ImGuiIO& io) {
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

static void drawBrowserWindow(AppState& state) {
    ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("ERF Browser", &state.showBrowser, ImGuiWindowFlags_MenuBar);
    if (ImGui::BeginMenuBar()) {
        if (ImGui::Button("Open Folder")) {
            IGFD::FileDialogConfig config;
            config.path = state.selectedFolder.empty() ? "." : state.selectedFolder;
            ImGuiFileDialog::Instance()->OpenDialog("ChooseFolder", "Choose Folder", nullptr, config);
        }
        ImGui::SameLine();
        if (ImGui::Button("Dump MSH Names")) { dumpAllMshFileNames(state.erfFiles); }
        if (!state.statusMessage.empty()) { ImGui::SameLine(); ImGui::Text("%s", state.statusMessage.c_str()); }
        ImGui::EndMenuBar();
    }
    ImGui::Columns(2, "browser_columns");
    ImGui::Text("ERF Files (%zu)", state.erfFiles.size());
    ImGui::Separator();
    ImGui::BeginChild("ERFList", ImVec2(0, 0), true);
    ImGuiListClipper erfClipper;
    erfClipper.Begin(static_cast<int>(state.erfFiles.size()));
    while (erfClipper.Step()) {
        for (int i = erfClipper.DisplayStart; i < erfClipper.DisplayEnd; i++) {
            std::string displayName = fs::path(state.erfFiles[i]).filename().string();
            if (ImGui::Selectable(displayName.c_str(), i == state.selectedErfIndex)) {
                if (state.selectedErfIndex != i) {
                    state.selectedErfIndex = i;
                    state.selectedEntryIndex = -1;
                    state.currentErf = std::make_unique<ERFFile>();
                    if (!state.currentErf->open(state.erfFiles[i])) {
                        state.statusMessage = "Failed to open";
                        state.currentErf.reset();
                    } else state.statusMessage = versionToString(state.currentErf->version());
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::NextColumn();
    if (state.currentErf) {
        ImGui::Text("Contents (%zu)", state.currentErf->entries().size());
        if (state.currentErf->encryption() != 0) { ImGui::SameLine(); ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "[Enc]"); }
        if (state.currentErf->compression() != 0) { ImGui::SameLine(); ImGui::TextColored(ImVec4(0.5f, 0.5f, 1, 1), "[Comp]"); }
        ImGui::Separator();
        ImGui::BeginChild("EntryList", ImVec2(0, 0), true);
        ImGuiListClipper entryClipper;
        entryClipper.Begin(static_cast<int>(state.currentErf->entries().size()));
        while (entryClipper.Step()) {
            for (int i = entryClipper.DisplayStart; i < entryClipper.DisplayEnd; i++) {
                const auto& entry = state.currentErf->entries()[i];
                bool isModel = isModelFile(entry.name), isMao = isMaoFile(entry.name), isPhy = isPhyFile(entry.name);
                if (isModel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
                else if (isMao) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.4f, 1.0f));
                else if (isPhy) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 1.0f, 1.0f));
                char label[256]; snprintf(label, sizeof(label), "%s##%d", entry.name.c_str(), i);
                if (ImGui::Selectable(label, i == state.selectedEntryIndex, ImGuiSelectableFlags_AllowDoubleClick)) {
                    state.selectedEntryIndex = i;
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        if (isModel) {
                            if (loadModelFromEntry(state, entry)) state.statusMessage = "Loaded: " + entry.name;
                            else state.statusMessage = "Failed to parse: " + entry.name;
                            state.showRenderSettings = true;
                        } else if (isMao) {
                            auto data = state.currentErf->readEntry(entry);
                            if (!data.empty()) {
                                state.maoContent = std::string(data.begin(), data.end());
                                state.maoFileName = entry.name;
                                state.showMaoViewer = true;
                            }
                        }
                    }
                }
                if (isModel || isMao || isPhy) ImGui::PopStyleColor();
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

                    // Color based on selection and root status
                    ImVec4 color;
                    if (isSelected) {
                        color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);  // Yellow for selected
                    } else if (bone.parentIndex < 0) {
                        color = ImVec4(1.0f, 0.5f, 0.5f, 1.0f);  // Red for root
                    } else {
                        color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);  // White for normal
                    }

                    ImGui::PushStyleColor(ImGuiCol_Text, color);

                    char label[256];
                    if (bone.parentIndex < 0) {
                        snprintf(label, sizeof(label), "[%zu] %s (root)", i, bone.name.c_str());
                    } else {
                        snprintf(label, sizeof(label), "[%zu] %s -> %s", i, bone.name.c_str(), bone.parentName.c_str());
                    }

                    if (ImGui::Selectable(label, isSelected)) {
                        state.selectedBoneIndex = isSelected ? -1 : (int)i;  // Toggle selection
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
    if (state.availableAnimFiles.empty()) ImGui::TextDisabled("No animations found");
    else {
        if (state.animPlaying && state.currentAnim.duration > 0) {
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Playing: %s", state.currentAnim.name.c_str());
            ImGui::ProgressBar(state.animTime / state.currentAnim.duration);
            if (ImGui::Button("Stop")) { state.animPlaying = false; state.animTime = 0.0f; state.currentModel.skeleton.bones = state.basePoseBones; }
            ImGui::Separator();
        }
        ImGui::Text("%zu animations", state.availableAnimFiles.size());
        ImGui::InputText("Filter", state.animFilter, sizeof(state.animFilter));
        std::string filterLower = state.animFilter;
        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
        ImGui::BeginChild("AnimList", ImVec2(0, 0), true);
        for (size_t ii = 0; ii < state.availableAnimFiles.size(); ii++) {
            std::string nameLower = state.availableAnimFiles[ii].first;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            if (!filterLower.empty() && nameLower.find(filterLower) == std::string::npos) continue;
            if (ImGui::Selectable(state.availableAnimFiles[ii].first.c_str(), state.selectedAnimIndex == (int)ii, ImGuiSelectableFlags_AllowDoubleClick)) {
                state.selectedAnimIndex = (int)ii;
                if (ImGui::IsMouseDoubleClicked(0)) {
                    ERFFile erf;
                    if (erf.open(state.availableAnimFiles[ii].second)) {
                        for (const auto& entry : erf.entries()) {
                            if (entry.name == state.availableAnimFiles[ii].first) {
                                auto aniData = erf.readEntry(entry);
                                if (!aniData.empty()) {
                                    state.currentAnim = loadANI(aniData, entry.name);

                                    // Try exact matching first
                                    int matched = 0;
                                    for (auto& track : state.currentAnim.tracks) {
                                        track.boneIndex = state.currentModel.skeleton.findBone(track.boneName);
                                        if (track.boneIndex >= 0) {
                                            matched++;
                                            std::cout << "  EXACT MATCH: '" << track.boneName << "' -> bone " << track.boneIndex << std::endl;
                                        }
                                    }
                                    std::cout << "  Exact matches: " << matched << "/" << state.currentAnim.tracks.size() << std::endl;

                                    // If no exact matches, try case-insensitive matching
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

void drawUI(AppState& state, GLFWwindow* window, ImGuiIO& io) {
    if (state.showBrowser) drawBrowserWindow(state);
    if (ImGuiFileDialog::Instance()->Display("ChooseFolder", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            state.selectedFolder = ImGuiFileDialog::Instance()->GetCurrentPath();
            state.erfFiles = scanForERFFiles(state.selectedFolder);
            state.selectedErfIndex = -1;
            state.currentErf.reset();
            state.selectedEntryIndex = -1;
            state.statusMessage = "Found " + std::to_string(state.erfFiles.size()) + " ERF files";
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Browser", nullptr, &state.showBrowser);
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
    if (state.showUvViewer && state.hasModel && state.selectedMeshForUv >= 0 && state.selectedMeshForUv < (int)state.currentModel.meshes.size()) drawUvViewer(state);
    if (state.showAnimWindow && state.hasModel) drawAnimWindow(state, io);
}