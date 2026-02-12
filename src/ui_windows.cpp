#include "ui_internal.h"

void drawRenderSettingsWindow(AppState& state) {
    ImGui::SetNextWindowPos(ImVec2(20, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(300, 100), ImVec2(500, 800));

    // Global ESC to deselect chunk
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        state.selectedLevelChunk = -1;
        state.selectedBoneIndex = -1;
    }

    ImGui::Begin("Render Settings", &state.showRenderSettings, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Checkbox("Wireframe", &state.renderSettings.wireframe);
    ImGui::Checkbox("Show Axes", &state.renderSettings.showAxes);
    ImGui::Checkbox("Show Grid", &state.renderSettings.showGrid);
    ImGui::Checkbox("Show Collision", &state.renderSettings.showCollision);
    if (state.renderSettings.showCollision) { ImGui::SameLine(); ImGui::Checkbox("Wireframe##coll", &state.renderSettings.collisionWireframe); }
    ImGui::Checkbox("Show Skeleton", &state.renderSettings.showSkeleton);
    ImGui::Checkbox("Show Textures", &state.renderSettings.showTextures);
    if (state.renderSettings.showTextures) {
        ImGui::Indent();
        ImGui::Checkbox("Normal Maps", &state.renderSettings.useNormalMaps);
        ImGui::Checkbox("Specular Maps", &state.renderSettings.useSpecularMaps);
        ImGui::Checkbox("Tint Maps", &state.renderSettings.useTintMaps);
        ImGui::Unindent();
    }
    ImGui::Separator();
    ImGui::Text("Camera Speed: %.1f", state.camera.moveSpeed);
    ImGui::SliderFloat("##speed", &state.camera.moveSpeed, 0.1f, 10000.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
    if (state.hasModel) {
        ImGui::Separator();
        size_t totalVerts = 0, totalTris = 0;
        for (const auto& m : state.currentModel.meshes) { totalVerts += m.vertices.size(); totalTris += m.indices.size() / 3; }
        ImGui::Text("Total: %zu meshes, %zu verts, %zu tris", state.currentModel.meshes.size(), totalVerts, totalTris);
        if (state.renderSettings.meshVisible.size() != state.currentModel.meshes.size())
            state.renderSettings.initMeshVisibility(state.currentModel.meshes.size());
        if (state.currentModel.meshes.size() >= 1) {
            ImGui::Separator(); ImGui::Text("Meshes:");
            bool isLevel = state.currentModel.meshes.size() > 20;

            if (state.selectedLevelChunk >= 0 && state.selectedLevelChunk < (int)state.currentModel.meshes.size()) {
                const auto& selMesh = state.currentModel.meshes[state.selectedLevelChunk];
                std::string selName = selMesh.name.empty() ? ("Mesh " + std::to_string(state.selectedLevelChunk)) : selMesh.name;
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Selected: %s", selName.c_str());
                ImGui::Indent();
                ImGui::TextDisabled("%zu verts, %zu tris", selMesh.vertices.size(), selMesh.indices.size() / 3);
                if (!selMesh.materialName.empty()) {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Material: %s", selMesh.materialName.c_str());
                    if (selMesh.materialIndex >= 0 && selMesh.materialIndex < (int)state.currentModel.materials.size()) {
                        const auto& mat = state.currentModel.materials[selMesh.materialIndex];
                        if (mat.diffuseTexId != 0) {
                            ImGui::SameLine();
                            if (ImGui::SmallButton("Texture")) {
                                state.previewTextureId = mat.diffuseTexId;
                                state.previewTextureName = mat.diffuseMap;
                                state.previewMeshIndex = state.selectedLevelChunk;
                                state.showTexturePreview = true;
                            }
                        }
                        if (!mat.maoContent.empty()) {
                            ImGui::SameLine();
                            if (ImGui::SmallButton("MAO")) {
                                state.maoContent = mat.maoContent;
                                state.maoFileName = mat.name + ".mao";
                                state.showMaoViewer = true;
                            }
                        }
                    }
                }
                if (ImGui::SmallButton("View UVs")) {
                    state.selectedMeshForUv = state.selectedLevelChunk;
                    state.showUvViewer = true;
                }
                ImGui::SameLine();
                bool vis = state.renderSettings.meshVisible[state.selectedLevelChunk] != 0;
                if (ImGui::Checkbox("Visible", &vis))
                    state.renderSettings.meshVisible[state.selectedLevelChunk] = vis ? 1 : 0;
                ImGui::Unindent();
                ImGui::TextDisabled("ESC to deselect");
            } else {
                ImGui::TextDisabled("Click a mesh in the viewport to select it");
            }

            // For individual models (few meshes), show the full list
            if (!isLevel) {
                float listHeight = std::min(300.0f, state.currentModel.meshes.size() * 50.0f + 20.0f);
                ImGui::BeginChild("MeshList", ImVec2(0, listHeight), true);
                for (size_t i = 0; i < state.currentModel.meshes.size(); i++) {
                    const auto& mesh = state.currentModel.meshes[i];
                    ImGui::PushID(static_cast<int>(i));
                    bool visible = state.renderSettings.meshVisible[i] != 0;
                    if (ImGui::Checkbox("##vis", &visible)) state.renderSettings.meshVisible[i] = visible ? 1 : 0;
                    ImGui::SameLine();
                    bool isSelected = (state.selectedLevelChunk == (int)i);
                    if (isSelected) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 1.0f, 0.5f, 1.0f));
                    std::string label = mesh.name.empty() ? ("Mesh " + std::to_string(i)) : mesh.name;
                    if (ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                        state.selectedLevelChunk = isSelected ? -1 : (int)i;
                    }
                    if (isSelected) {
                        ImGui::PopStyleColor();
                        if (ImGui::IsWindowAppearing() || !ImGui::IsItemVisible())
                            ImGui::SetScrollHereY(0.5f);
                    }
                    ImGui::Indent();
                    ImGui::TextDisabled("%zu verts, %zu tris", mesh.vertices.size(), mesh.indices.size() / 3);
                    if (!mesh.materialName.empty()) {
                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Material: %s", mesh.materialName.c_str());
                        if (mesh.materialIndex >= 0 && mesh.materialIndex < (int)state.currentModel.materials.size()) {
                            const auto& mat = state.currentModel.materials[mesh.materialIndex];
                            if (mat.diffuseTexId != 0) {
                                ImGui::SameLine();
                                if (ImGui::SmallButton("Texture")) {
                                    state.previewTextureId = mat.diffuseTexId;
                                    state.previewTextureName = mat.diffuseMap;
                                    state.previewMeshIndex = static_cast<int>(i);
                                    state.showTexturePreview = true;
                                }
                            }
                        }
                    }
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
        }
        bool isLevelModel = state.currentModel.meshes.size() > 20;
        if (!isLevelModel && !state.currentModel.materials.empty()) {
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
                if (ImGui::IsKeyPressed(state.keybinds.deselectBone)) {
                    state.selectedBoneIndex = -1;
                    state.selectedLevelChunk = -1;
                }
                if (state.selectedBoneIndex >= 0) {
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Selected: %s",
                        state.currentModel.skeleton.bones[state.selectedBoneIndex].name.c_str());
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

void drawMaoViewer(AppState& state) {
    ImGui::SetNextWindowPos(ImVec2(600, 200), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    ImGui::Begin(("MAO Viewer - " + state.maoFileName).c_str(), &state.showMaoViewer);
    if (ImGui::Button("Copy to Clipboard")) ImGui::SetClipboardText(state.maoContent.c_str());
    ImGui::Separator();
    ImGui::BeginChild("MaoContent", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::TextUnformatted(state.maoContent.c_str());
    ImGui::EndChild();
    ImGui::End();
}

void drawAudioPlayer(AppState& state) {
    int length = getAudioLength();
    int pos = getAudioPosition();
    bool playing = isAudioPlaying();

    if (!playing && state.audioPlaying && pos >= length - 100) {
        state.audioPlaying = false;
    }

    float progress = (length > 0) ? (float)pos / (float)length : 0.0f;
    int totalSec = length / 1000;
    int curSec = pos / 1000;

    float panelW = 420.0f;
    float panelH = 72.0f;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 panelPos(vp->Pos.x + (vp->Size.x - panelW) * 0.5f, vp->Pos.y + vp->Size.y - panelH - 16.0f);

    ImGui::SetNextWindowPos(panelPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH));

    ImGui::Begin("##AudioPlayer", &state.showAudioPlayer,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    ImVec2 ws = ImGui::GetWindowSize();

    std::string displayName = state.currentAudioName;
    float nameMaxW = panelW - 24.0f;
    ImVec2 nameSize = ImGui::CalcTextSize(displayName.c_str());
    if (nameSize.x > nameMaxW) {
        while (displayName.size() > 4 && ImGui::CalcTextSize((displayName + "...").c_str()).x > nameMaxW)
            displayName.pop_back();
        displayName += "...";
        nameSize = ImGui::CalcTextSize(displayName.c_str());
    }
    ImGui::SetCursorPosX((panelW - nameSize.x) * 0.5f);
    ImGui::Text("%s", displayName.c_str());

    float barX = wp.x + 12.0f;
    float barW = ws.x - 24.0f;
    float barY = wp.y + 32.0f;
    float barH = 4.0f;

    ImU32 colFrameBg = ImGui::GetColorU32(ImGuiCol_FrameBg);
    ImU32 colCheckMark = ImGui::GetColorU32(ImGuiCol_CheckMark);
    ImU32 colText = ImGui::GetColorU32(ImGuiCol_Text);

    dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), colFrameBg);
    if (progress > 0.0f) {
        dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * progress, barY + barH), colCheckMark);
    }

    float handleX = barX + barW * progress;
    dl->AddCircleFilled(ImVec2(handleX, barY + barH * 0.5f), 5.0f, colText);

    ImGui::SetCursorScreenPos(ImVec2(barX, barY - 4.0f));
    ImGui::InvisibleButton("##seekbar", ImVec2(barW, barH + 8.0f));
    if (ImGui::IsItemActive()) {
        float mx = ImGui::GetMousePos().x;
        float newProgress = (mx - barX) / barW;
        if (newProgress < 0.0f) newProgress = 0.0f;
        if (newProgress > 1.0f) newProgress = 1.0f;
        setAudioPosition((int)(newProgress * length));
    }
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    float rowY = barY + barH + 6.0f;
    char timeCur[16], timeTotal[16];
    snprintf(timeCur, sizeof(timeCur), "%d:%02d", curSec / 60, curSec % 60);
    snprintf(timeTotal, sizeof(timeTotal), "%d:%02d", totalSec / 60, totalSec % 60);

    ImVec4 dimText = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    ImGui::SetCursorScreenPos(ImVec2(barX, rowY));
    ImGui::TextColored(dimText, "%s", timeCur);

    ImVec2 totalSize = ImGui::CalcTextSize(timeTotal);
    ImGui::SetCursorScreenPos(ImVec2(barX + barW - totalSize.x, rowY));
    ImGui::TextColored(dimText, "%s", timeTotal);

    float btnSize = 22.0f;
    float btnGap = 6.0f;
    float totalBtnW = btnSize * 3 + btnGap * 2;
    float btnStartX = (panelW - totalBtnW) * 0.5f;

    auto iconButton = [&](const char* icon, const char* id, ImVec2 pos) -> bool {
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton(id, ImVec2(btnSize, btnSize));
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();
        bool clicked = ImGui::IsItemClicked();

        ImU32 bg;
        if (active) bg = ImGui::GetColorU32(ImGuiCol_ButtonActive);
        else if (hovered) bg = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
        else bg = ImGui::GetColorU32(ImGuiCol_Button);
        dl->AddRectFilled(pos, ImVec2(pos.x + btnSize, pos.y + btnSize), bg);

        ImVec2 iconSize = ImGui::CalcTextSize(icon);
        ImVec2 textPos(pos.x + (btnSize - iconSize.x) * 0.5f, pos.y + (btnSize - iconSize.y) * 0.5f);
        dl->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), icon);

        return clicked;
    };

    if (iconButton(ICON_FA_BACKWARD_STEP, "##restart", ImVec2(wp.x + btnStartX, rowY - 2.0f))) {
        setAudioPosition(0);
        if (!state.audioPlaying) { resumeAudio(); state.audioPlaying = true; }
    }

    if (state.audioPlaying && playing) {
        if (iconButton(ICON_FA_PAUSE, "##pause", ImVec2(wp.x + btnStartX + btnSize + btnGap, rowY - 2.0f))) {
            pauseAudio();
            state.audioPlaying = false;
        }
    } else {
        if (iconButton(ICON_FA_PLAY, "##play", ImVec2(wp.x + btnStartX + btnSize + btnGap, rowY - 2.0f))) {
            if (pos >= length - 100) setAudioPosition(0);
            else resumeAudio();
            state.audioPlaying = true;
        }
    }

    if (iconButton(ICON_FA_STOP, "##stop", ImVec2(wp.x + btnStartX + (btnSize + btnGap) * 2, rowY - 2.0f))) {
        stopAudio();
        state.audioPlaying = false;
        state.showAudioPlayer = false;
    }

    ImGui::End();

    if (!state.showAudioPlayer) {
        stopAudio();
        state.audioPlaying = false;
    }
}

void drawTexturePreview(AppState& state) {
    ImGui::SetNextWindowPos(ImVec2(550, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(520, 580), ImGuiCond_FirstUseEver);

    std::string title = "Texture Preview - " + state.previewTextureName;
    ImGui::Begin(title.c_str(), &state.showTexturePreview);
    if (ImGui::Button("Extract DDS")) {
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
        std::string defaultName = state.previewTextureName;
        config.fileName = defaultName;
        ImGuiFileDialog::Instance()->OpenDialog("ExtractTexture", "Extract Texture", ".dds", config);
    }
    ImGui::SameLine();
    if (ImGui::Button("Extract PNG")) {
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
        std::string defaultName = state.previewTextureName;
        size_t dotPos = defaultName.rfind('.');
        if (dotPos != std::string::npos) defaultName = defaultName.substr(0, dotPos);
        defaultName += ".png";
        config.fileName = defaultName;
        ImGuiFileDialog::Instance()->OpenDialog("ExtractTexturePNG", "Extract Texture as PNG", ".png", config);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Show UV Overlay", &state.showUvOverlay);
    ImGui::Separator();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float size = std::min(avail.x, avail.y - 20);
    if (size < 100) size = 100;
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + size, canvasPos.y + size), IM_COL32(40, 40, 40, 255));
    if (state.previewTextureId != 0) {
        auto* srv = getTextureSRV(state.previewTextureId);
        if (srv) {
            drawList->AddImage(
                (ImTextureID)srv,
                canvasPos,
                ImVec2(canvasPos.x + size, canvasPos.y + size),
                ImVec2(0, 0), ImVec2(1, 1)
            );
        }
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

void drawUvViewer(AppState& state) {
    ImGui::SetNextWindowPos(ImVec2(550, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);

    const auto& mesh = state.currentModel.meshes[state.selectedMeshForUv];
    std::string title = "UV Viewer - " + (mesh.name.empty() ? "Mesh " + std::to_string(state.selectedMeshForUv) : mesh.name);

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

void drawAnimWindow(AppState& state, ImGuiIO& io) {
    ImGui::SetNextWindowPos(ImVec2(1000, 40), ImGuiCond_FirstUseEver);
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
                                    auto normalize = [](const std::string& s) {
                                        std::string result;
                                        for (char c : s) {
                                            if (c != '_') result += std::tolower(c);
                                        }
                                        return result;
                                    };
                                    int matched = 0;
                                    for (auto& track : state.currentAnim.tracks) {
                                        track.boneIndex = state.currentModel.skeleton.findBone(track.boneName);
                                        if (track.boneIndex >= 0) {
                                            matched++;
                                            continue;
                                        }
                                        std::string trackNorm = normalize(track.boneName);
                                        for (size_t bi = 0; bi < state.currentModel.skeleton.bones.size(); bi++) {
                                            std::string boneNorm = normalize(state.currentModel.skeleton.bones[bi].name);
                                            if (trackNorm == boneNorm) {
                                                track.boneIndex = (int)bi;
                                                matched++;
                                                break;
                                            }
                                        }
                                    }
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

void drawHeightmapViewer(AppState& state) {
    ImGui::SetNextWindowPos(ImVec2(550, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(540, 580), ImGuiCond_FirstUseEver);

    ImGui::Begin("Heightmap", &state.showHeightmap);
    if (state.heightmapTexId && state.heightmapW > 0 && state.heightmapH > 0) {
        ImGui::Text("%dx%d", state.heightmapW, state.heightmapH);
        ImGui::Separator();
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float aspect = (float)state.heightmapW / (float)state.heightmapH;
        float drawW, drawH;
        if (avail.x / aspect <= avail.y) {
            drawW = avail.x;
            drawH = avail.x / aspect;
        } else {
            drawH = avail.y;
            drawW = avail.y * aspect;
        }
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(pos, ImVec2(pos.x + drawW, pos.y + drawH), IM_COL32(20, 20, 25, 255));
        auto* srv = getTextureSRV(state.heightmapTexId);
        if (srv) {
            dl->AddImage(
                (ImTextureID)srv,
                pos, ImVec2(pos.x + drawW, pos.y + drawH));
        }
        ImGui::Dummy(ImVec2(drawW, drawH));
    }
    ImGui::End();

    if (!state.showHeightmap && state.heightmapTexId) {
        destroyTexture(state.heightmapTexId);
        state.heightmapTexId = 0;
    }
}