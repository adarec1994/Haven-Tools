#include "ui_internal.h"
#include "update/update.h"
#include <thread>
#include "import.h"
#include "export.h"
#include "update/about_text.h"
#include "update/changelog_text.h"
static const char* CURRENT_APP_VERSION = "1.14";
bool showSplash = true;
static bool t_active = false;
static float t_alpha = 0.0f;
static int t_targetTab = 0;
static int t_phase = 0;
static bool t_isLoadingContent = false;
static bool s_showAbout = false;
static bool s_showChangelog = false;
static bool s_scrollToBottom = false;
static std::string s_pendingImportGlbPath;
static std::string s_pendingExportPath;
static bool s_showExportOptions = false;
static std::map<std::string, bool> s_animSelection;
static bool s_selectAllAnims = true;
static bool s_isFbxExport = false;
static bool s_exportCollision = true;
static bool s_exportArmature = true;
static bool s_animListExpanded = false;
static int s_fbxScaleIndex = 0;
void runLoadingTask(AppState* statePtr) {
    AppState& state = *statePtr;
    state.preloadStatus = "Scanning game folders...";
    state.preloadProgress = 0.0f;
    state.erfFiles = scanForERFFiles(state.selectedFolder);
    state.preloadStatus = "Filtering encrypted files...";
    state.preloadProgress = 0.05f;
    filterEncryptedErfs(state);
    state.meshCache.clear();
    state.mmhCache.clear();
    state.maoCache.clear();
    state.textureCache.clear();
    state.modelErfs.clear();
    state.modelErfPaths.clear();
    state.materialErfs.clear();
    state.materialErfPaths.clear();
    state.textureErfs.clear();
    state.textureErfPaths.clear();
    std::vector<std::string> charPrefixes = {"df_", "dm_", "hf_", "hm_", "ef_", "em_", "cn_"};
    std::vector<std::string> erfPaths;
    for (size_t i : state.filteredErfIndices) {
        erfPaths.push_back(state.erfFiles[i]);
    }
    size_t totalErfs = erfPaths.size();
    size_t processed = 0;
    for (const auto& erfPath : erfPaths) {
        std::string filename = fs::path(erfPath).filename().string();
        state.preloadStatus = filename;
        ERFFile erf;
        if (!erf.open(erfPath)) {
            processed++;
            state.preloadProgress = 0.1f + ((float)processed / (float)totalErfs) * 0.8f;
            continue;
        }
        std::string pathLower = erfPath;
        std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);
        bool isModel = pathLower.find("model") != std::string::npos ||
                       pathLower.find("morph") != std::string::npos ||
                       pathLower.find("face") != std::string::npos ||
                       pathLower.find("chargen") != std::string::npos;
        bool isMaterial = pathLower.find("material") != std::string::npos;
        bool isTexture = pathLower.find("texture") != std::string::npos;
        for (const auto& entry : erf.entries()) {
            std::string nameLower = entry.name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            if (isModel) {
                bool isCharFile = false;
                for (const auto& prefix : charPrefixes) {
                    if (nameLower.find(prefix) == 0) {
                        isCharFile = true;
                        break;
                    }
                }
                if (isCharFile && nameLower.size() > 4) {
                    std::string ext = nameLower.substr(nameLower.size() - 4);
                    if (ext == ".msh" && state.meshCache.find(nameLower) == state.meshCache.end()) {
                        state.meshCache[nameLower] = erf.readEntry(entry);
                    } else if (ext == ".mmh" && state.mmhCache.find(nameLower) == state.mmhCache.end()) {
                        state.mmhCache[nameLower] = erf.readEntry(entry);
                    }
                }
            }
            if (isMaterial) {
                if (nameLower.size() > 4 && nameLower.substr(nameLower.size() - 4) == ".mao") {
                    if (state.maoCache.find(nameLower) == state.maoCache.end()) {
                        state.maoCache[nameLower] = erf.readEntry(entry);
                    }
                }
            }
            if (isTexture) {
                if (nameLower.size() > 4 && nameLower.substr(nameLower.size() - 4) == ".dds") {
                    if (state.textureCache.find(nameLower) == state.textureCache.end()) {
                        state.textureCache[nameLower] = erf.readEntry(entry);
                    }
                }
            }
        }
        if (isModel) {
            auto erfPtr = std::make_unique<ERFFile>();
            if (erfPtr->open(erfPath)) {
                state.modelErfs.push_back(std::move(erfPtr));
                state.modelErfPaths.push_back(erfPath);
            }
        }
        if (isMaterial) {
            auto erfPtr = std::make_unique<ERFFile>();
            if (erfPtr->open(erfPath)) {
                state.materialErfs.push_back(std::move(erfPtr));
                state.materialErfPaths.push_back(erfPath);
            }
        }
        if (isTexture) {
            auto erfPtr = std::make_unique<ERFFile>();
            if (erfPtr->open(erfPath)) {
                state.textureErfs.push_back(std::move(erfPtr));
                state.textureErfPaths.push_back(erfPath);
            }
        }
        processed++;
        state.preloadProgress = 0.1f + ((float)processed / (float)totalErfs) * 0.8f;
    }
    state.modelErfsLoaded = true;
    state.materialErfsLoaded = true;
    state.textureErfsLoaded = true;
    state.cacheBuilt = true;
    state.preloadStatus = "Scanning audio files...";
    state.preloadProgress = 0.95f;
    scanAudioFiles(state);
    state.preloadProgress = 1.0f;
    state.statusMessage = "Ready";
    saveSettings(state);
    state.isPreloading = false;
    showSplash = false;
}
void runCharDesignerLoading(AppState* statePtr) {
    t_isLoadingContent = true;
    statePtr->preloadProgress = 0.0f;
    preloadCharacterData(*statePtr);
    statePtr->preloadProgress = 1.0f;
    t_isLoadingContent = false;
}
void runImportTask(AppState* statePtr) {
    AppState& state = *statePtr;
    state.preloadStatus = "Initializing import...";
    state.preloadProgress = 0.0f;
    DAOImporter importer;
    importer.SetProgressCallback([&](float progress, const std::string& status) {
        state.preloadProgress = progress * 0.9f;
        state.preloadStatus = status;
    });
    bool success = importer.ImportToDirectory(s_pendingImportGlbPath, state.selectedFolder);
    if (success) {
        std::string modelName = fs::path(s_pendingImportGlbPath).stem().string() + ".msh";
        markModelAsImported(modelName);
        state.preloadStatus = "Refreshing modified ERFs...";
        state.preloadProgress = 0.92f;
        fs::path baseDir(state.selectedFolder);
        fs::path corePath = baseDir / "packages" / "core" / "data";
        fs::path texturePath = baseDir / "packages" / "core" / "textures" / "high";
        std::vector<std::string> modifiedErfs = {
            (corePath / "modelmeshdata.erf").string(),
            (corePath / "modelhierarchies.erf").string(),
            (corePath / "materialobjects.erf").string(),
            (texturePath / "texturepack.erf").string()
        };
        for (const auto& erfPath : modifiedErfs) {
            if (!fs::exists(erfPath)) continue;
            std::string pathLower = erfPath;
            std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);
            auto removeAndReload = [&](std::vector<std::unique_ptr<ERFFile>>& erfs,
                                       std::vector<std::string>& paths) {
                for (size_t i = 0; i < paths.size(); ++i) {
                    std::string existingLower = paths[i];
                    std::transform(existingLower.begin(), existingLower.end(), existingLower.begin(), ::tolower);
                    if (existingLower == pathLower) {
                        erfs.erase(erfs.begin() + i);
                        paths.erase(paths.begin() + i);
                        break;
                    }
                }
                auto erfPtr = std::make_unique<ERFFile>();
                if (erfPtr->open(erfPath)) {
                    erfs.push_back(std::move(erfPtr));
                    paths.push_back(erfPath);
                }
            };
            if (pathLower.find("modelmesh") != std::string::npos ||
                pathLower.find("modelhierarch") != std::string::npos) {
                removeAndReload(state.modelErfs, state.modelErfPaths);
            }
            if (pathLower.find("material") != std::string::npos) {
                removeAndReload(state.materialErfs, state.materialErfPaths);
            }
            if (pathLower.find("texture") != std::string::npos) {
                removeAndReload(state.textureErfs, state.textureErfPaths);
            }
        }
        state.preloadProgress = 1.0f;
        state.preloadStatus = "Import complete!";
        state.statusMessage = "Model imported successfully!";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        t_isLoadingContent = false;
        t_active = false;
        t_phase = 0;
        t_alpha = 0.0f;
    } else {
        state.preloadStatus = "Import Failed!";
        std::this_thread::sleep_for(std::chrono::seconds(2));
        t_isLoadingContent = false;
        t_active = false;
        t_phase = 0;
        t_alpha = 0.0f;
    }
}
void runExportTask(AppState* statePtr) {
    AppState& state = *statePtr;
    state.preloadStatus = "Initializing export...";
    state.preloadProgress = 0.0f;
    std::vector<Animation> exportAnims;
    size_t totalSelected = 0;
    for (const auto& pair : s_animSelection) {
        if (pair.second) totalSelected++;
    }
    size_t processed = 0;
    for (const auto& animFile : state.availableAnimFiles) {
        if (!s_animSelection[animFile.first]) continue;
        ERFFile animErf;
        if (animErf.open(animFile.second)) {
            for (const auto& animEntry : animErf.entries()) {
                if (animEntry.name == animFile.first) {
                    auto aniData = animErf.readEntry(animEntry);
                    if (!aniData.empty()) {
                        Animation anim = loadANI(aniData, animEntry.name);
                        auto normalize = [](const std::string& s) {
                            std::string result;
                            for (char c : s) {
                                if (c != '_') result += std::tolower(c);
                            }
                            return result;
                        };
                        for (auto& track : anim.tracks) {
                            track.boneIndex = state.currentModel.skeleton.findBone(track.boneName);
                            if (track.boneIndex < 0) {
                                std::string trackNorm = normalize(track.boneName);
                                for (size_t bi = 0; bi < state.currentModel.skeleton.bones.size(); bi++) {
                                    if (trackNorm == normalize(state.currentModel.skeleton.bones[bi].name)) {
                                        track.boneIndex = (int)bi;
                                        break;
                                    }
                                }
                            }
                        }
                        exportAnims.push_back(anim);
                    }
                    break;
                }
            }
        }
        processed++;
        if (totalSelected > 0) {
            state.preloadProgress = (float)processed / (float)totalSelected * 0.9f;
        }
        state.preloadStatus = "Processing: " + animFile.first;
    }
    state.preloadStatus = "Writing File...";
    state.preloadProgress = 0.95f;
    ExportOptions exportOpts;
    exportOpts.includeCollision = s_exportCollision;
    exportOpts.includeArmature = s_exportArmature;
    exportOpts.includeAnimations = true;
    float scaleValues[] = { 1.0f, 10.0f, 100.0f, 1000.0f };
    exportOpts.fbxScale = scaleValues[s_fbxScaleIndex];
    bool success = false;
    if (s_isFbxExport) {
        success = exportToFBX(state.currentModel, exportAnims, s_pendingExportPath, exportOpts);
    } else {
        success = exportToGLB(state.currentModel, exportAnims, s_pendingExportPath, exportOpts);
    }
    std::string exportInfo = std::to_string(exportAnims.size()) + " anims";
    if (exportOpts.includeCollision && !state.currentModel.collisionShapes.empty()) {
        exportInfo += ", " + std::to_string(state.currentModel.collisionShapes.size()) + " collision";
    }
    if (success) {
        state.statusMessage = "Exported: " + s_pendingExportPath + " (" + exportInfo + ")";
    } else {
        state.statusMessage = "Export failed!";
    }
    state.preloadProgress = 1.0f;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    t_isLoadingContent = false;
    t_active = false;
    t_phase = 0;
    t_alpha = 0.0f;
}
void handleInput(AppState& state, GLFWwindow* window, ImGuiIO& io) {
    static bool settingsLoaded = false;
    if (!settingsLoaded) {
        loadSettings(state);
        if (state.lastRunVersion.empty()) {
            s_showAbout = true;
        }
        else if (state.lastRunVersion != CURRENT_APP_VERSION) {
            s_showChangelog = true;
            s_scrollToBottom = true;
        }
        if (state.lastRunVersion != CURRENT_APP_VERSION) {
            state.lastRunVersion = CURRENT_APP_VERSION;
            saveSettings(state);
        }
        settingsLoaded = true;
    }
    if (!io.WantCaptureMouse) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        static bool wasLeftPressed = false;
        bool leftPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (leftPressed && !wasLeftPressed && state.hasModel && state.renderSettings.showSkeleton) {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            float aspect = (float)width / (float)height;
            float fov = 45.0f * 3.14159f / 180.0f;
            float nearPlane = 0.1f;
            float top = nearPlane * std::tan(fov / 2.0f);
            float right = top * aspect;
            float ndcX = (2.0f * (float)mx / width) - 1.0f;
            float ndcY = 1.0f - (2.0f * (float)my / height);
            float rayX = ndcX * right / nearPlane;
            float rayY = ndcY * top / nearPlane;
            float rayZ = -1.0f;
            float cp = std::cos(-state.camera.pitch);
            float sp = std::sin(-state.camera.pitch);
            float cy = std::cos(-state.camera.yaw);
            float sy = std::sin(-state.camera.yaw);
            float rx1 = rayX;
            float ry1 = rayY * cp - rayZ * sp;
            float rz1 = rayY * sp + rayZ * cp;
            float dirX = rx1 * cy + rz1 * sy;
            float dirY = ry1;
            float dirZ = -rx1 * sy + rz1 * cy;
            float len = std::sqrt(dirX*dirX + dirY*dirY + dirZ*dirZ);
            dirX /= len; dirY /= len; dirZ /= len;
            float origX = state.camera.x;
            float origY = state.camera.y;
            float origZ = state.camera.z;
            int closestBone = -1;
            float closestDist = 999999.0f;
            float threshold = 0.15f;
            for (size_t i = 0; i < state.currentModel.skeleton.bones.size(); i++) {
                const auto& bone = state.currentModel.skeleton.bones[i];
                float bx = bone.worldPosY;
                float by = bone.worldPosZ;
                float bz = -bone.worldPosX;
                float toX = bx - origX;
                float toY = by - origY;
                float toZ = bz - origZ;
                float t = toX*dirX + toY*dirY + toZ*dirZ;
                if (t < 0) continue;
                float closestX = origX + dirX * t;
                float closestY = origY + dirY * t;
                float closestZ = origZ + dirZ * t;
                float dx = closestX - bx;
                float dy = closestY - by;
                float dz = closestZ - bz;
                float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
                if (dist < threshold && t < closestDist) {
                    closestDist = t;
                    closestBone = (int)i;
                }
            }
            if (closestBone >= 0) {
                state.selectedBoneIndex = closestBone;
            }
        }
        wasLeftPressed = leftPressed;
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
        if (ImGui::IsKeyDown(state.keybinds.moveForward)) state.camera.moveForward(speed);
        if (ImGui::IsKeyDown(state.keybinds.moveBackward)) state.camera.moveForward(-speed);
        if (ImGui::IsKeyDown(state.keybinds.moveLeft)) state.camera.moveRight(-speed);
        if (ImGui::IsKeyDown(state.keybinds.moveRight)) state.camera.moveRight(speed);
        if (ImGui::IsKeyDown(state.keybinds.panUp)) state.camera.moveUp(speed);
        if (ImGui::IsKeyDown(state.keybinds.panDown)) state.camera.moveUp(-speed);
    }
}
void drawSplashScreen(AppState& state, int displayW, int displayH) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)displayW, (float)displayH));
    ImGui::Begin("##Splash", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
    float centerX = displayW * 0.5f;
    float centerY = displayH * 0.5f;
    if (!state.isPreloading) {
        ImVec2 buttonSize(250, 40);
        ImGui::SetCursorPos(ImVec2(centerX - buttonSize.x * 0.5f, centerY));
        if (ImGui::Button("Browse to DAOriginsLauncher.exe", buttonSize)) {
            IGFD::FileDialogConfig config;
            config.path = state.lastDialogPath.empty() ? "." : state.lastDialogPath;
            ImGuiFileDialog::Instance()->OpenDialog("ChooseLauncher", "Select DAOriginsLauncher.exe", ".exe", config);
        }
    } else {
        ImGui::SetCursorPos(ImVec2(centerX - 150, centerY));
        ImGui::ProgressBar(state.preloadProgress, ImVec2(300, 20));
        ImGui::SetCursorPos(ImVec2(centerX - 150, centerY + 25));
        ImVec2 txtSize = ImGui::CalcTextSize(state.preloadStatus.c_str());
        ImGui::SetCursorPosX(centerX - txtSize.x * 0.5f);
        ImGui::TextWrapped("%s", state.preloadStatus.c_str());
    }
    ImGui::End();
}
void preloadErfs(AppState& state) {
    runLoadingTask(&state);
}
static int s_listeningBind = -1;

static void drawKeybindRow(const char* label, ImGuiKey& key, int id) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
    ImGui::PushID(id);
    if (s_listeningBind == id) {
        ImGui::Button("Press a key...", ImVec2(140, 0));
        for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; k++) {
            if (ImGui::IsKeyPressed((ImGuiKey)k)) {
                key = (ImGuiKey)k;
                s_listeningBind = -1;
                break;
            }
        }
    } else {
        if (ImGui::Button(ImGui::GetKeyName(key), ImVec2(140, 0))) {
            s_listeningBind = id;
        }
    }
    ImGui::PopID();
}

void drawKeybindsWindow(AppState& state) {
    ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("Keybinds", &state.showKeybinds, ImGuiWindowFlags_AlwaysAutoResize);

    if (s_listeningBind >= 0 && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        s_listeningBind = -1;
    }

    if (ImGui::BeginTable("##keybinds", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Movement");
        ImGui::TableNextColumn();

        drawKeybindRow("Forward",     state.keybinds.moveForward,  0);
        drawKeybindRow("Backward",    state.keybinds.moveBackward, 1);
        drawKeybindRow("Left",        state.keybinds.moveLeft,     2);
        drawKeybindRow("Right",       state.keybinds.moveRight,    3);
        drawKeybindRow("Pan Up",      state.keybinds.panUp,        4);
        drawKeybindRow("Pan Down",    state.keybinds.panDown,      5);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "General");
        ImGui::TableNextColumn();

        drawKeybindRow("Deselect", state.keybinds.deselectBone, 6);

        ImGui::EndTable();
    }

    ImGui::Spacing();
    if (ImGui::Button("Reset to Defaults")) {
        state.keybinds = Keybinds();
        saveSettings(state);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        saveSettings(state);
    }

    ImGui::End();
}

void drawUI(AppState& state, GLFWwindow* window, ImGuiIO& io) {
    int displayW, displayH;
    glfwGetFramebufferSize(window, &displayW, &displayH);
    static bool s_startedUpdateCheck = false;
    static bool s_openUpdatePopup = false;
    static bool s_dismissedUpdatePopup = false;
    if (showSplash) {
        drawSplashScreen(state, displayW, displayH);
        if (!s_startedUpdateCheck) {
            s_startedUpdateCheck = true;
            Update::StartCheckForUpdates();
        }
        if (Update::IsCheckDone() && Update::IsUpdateAvailable() && !s_dismissedUpdatePopup && !s_openUpdatePopup && !Update::IsBusy()) {
            s_openUpdatePopup = true;
            ImGui::OpenPopup("Update Available");
        }
        if (ImGui::BeginPopupModal("Update Available", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            const char* latest = Update::GetLatestVersionText();
            if (!latest) latest = "?";
            ImGui::Text("An update to version %s is available.", latest);
            ImGui::TextUnformatted("Do you want to update?");
            ImGui::Spacing();
            if (ImGui::Button("Yes", ImVec2(120, 0))) {
                Update::DownloadAndApplyLatest();
                s_dismissedUpdatePopup = true;
                s_openUpdatePopup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(120, 0))) {
                s_dismissedUpdatePopup = true;
                s_openUpdatePopup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (!state.isPreloading) {
            if (ImGuiFileDialog::Instance()->Display("ChooseLauncher", ImGuiWindowFlags_NoCollapse, ImVec2(700, 450))) {
                if (ImGuiFileDialog::Instance()->IsOk()) {
                    std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
                    state.selectedFolder = fs::path(filePath).parent_path().string();
                    state.lastDialogPath = state.selectedFolder;
                    state.isPreloading = true;
                    std::thread(runLoadingTask, &state).detach();
                }
                ImGuiFileDialog::Instance()->Close();
            }
        }
        return;
    }
    if (t_active) {
        float dt = io.DeltaTime;
        if (t_phase == 1) {
            t_alpha += dt * 5.0f;
            if (t_alpha >= 1.0f) {
                t_alpha = 1.0f;
                if (!t_isLoadingContent) {
                    t_phase = 2;
                }
            }
        } else if (t_phase == 2) {
            if (t_targetTab == 1 && state.mainTab != 1) {
                state.renderSettings.showSkeleton = false;
                state.renderSettings.showAxes = false;
                state.renderSettings.showGrid = false;
                state.hasModel = false;
                state.currentModel = Model();
                state.currentAnim = Animation();
                state.animPlaying = false;
            }
            state.mainTab = t_targetTab;
            t_phase = 3;
        } else if (t_phase == 3) {
            t_alpha -= dt * 1.5f;
            if (t_alpha <= 0.0f) {
                t_alpha = 0.0f;
                t_active = false;
                t_phase = 0;
            }
        }
    }
    if (ImGuiFileDialog::Instance()->Display("ChooseFolder", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            state.selectedFolder = ImGuiFileDialog::Instance()->GetCurrentPath();
            state.lastDialogPath = state.selectedFolder;
            state.isPreloading = true;
            showSplash = true;
            std::thread(runLoadingTask, &state).detach();
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGuiFileDialog::Instance()->Display("ImportGLB", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            s_pendingImportGlbPath = ImGuiFileDialog::Instance()->GetFilePathName();
            t_active = true;
            t_targetTab = state.mainTab;
            t_phase = 1;
            t_alpha = 0.0f;
            t_isLoadingContent = true;
            std::thread(runImportTask, &state).detach();
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGuiFileDialog::Instance()->Display("ExportCurrentGLB", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk() && state.hasModel) {
            s_pendingExportPath = ImGuiFileDialog::Instance()->GetFilePathName();
            s_isFbxExport = false;
            s_animSelection.clear();
            for (const auto& animFile : state.availableAnimFiles) {
                std::string animName = animFile.first;
                size_t dotPos = animName.rfind('.');
                if (dotPos != std::string::npos) animName = animName.substr(0, dotPos);
                bool valid = state.currentModelAnimations.empty();
                if (!valid) {
                    for (const auto& va : state.currentModelAnimations) {
                        if (animName == va) { valid = true; break; }
                    }
                }
                if (valid) s_animSelection[animFile.first] = true;
            }
            s_selectAllAnims = true;
            s_showExportOptions = true;
            ImGui::OpenPopup("Export Options");
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGuiFileDialog::Instance()->Display("ExportCurrentFBX", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk() && state.hasModel) {
            s_pendingExportPath = ImGuiFileDialog::Instance()->GetFilePathName();
            s_isFbxExport = true;
            s_animSelection.clear();
            for (const auto& animFile : state.availableAnimFiles) {
                std::string animName = animFile.first;
                size_t dotPos = animName.rfind('.');
                if (dotPos != std::string::npos) animName = animName.substr(0, dotPos);
                bool valid = state.currentModelAnimations.empty();
                if (!valid) {
                    for (const auto& va : state.currentModelAnimations) {
                        if (animName == va) { valid = true; break; }
                    }
                }
                if (valid) s_animSelection[animFile.first] = true;
            }
            s_selectAllAnims = true;
            s_showExportOptions = true;
            ImGui::OpenPopup("Export Options");
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGui::BeginPopupModal("Export Options", &s_showExportOptions, ImGuiWindowFlags_AlwaysAutoResize)) {
        bool hasCollision = !state.currentModel.collisionShapes.empty();
        if (hasCollision) {
            ImGui::Checkbox("Include Collision Shapes", &s_exportCollision);
            ImGui::SameLine();
            ImGui::TextDisabled("(%zu shapes)", state.currentModel.collisionShapes.size());
        } else {
            ImGui::TextDisabled("No collision shapes in model");
        }
        if (s_isFbxExport) {
            ImGui::Separator();
            ImGui::Checkbox("Include Armature", &s_exportArmature);
            const char* scaleOptions[] = { "x1", "x10", "x100", "x1000" };
            ImGui::Text("Scale:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            ImGui::Combo("##FBXScale", &s_fbxScaleIndex, scaleOptions, 4);
        }
        ImGui::Separator();
        int selectedCount = 0;
        for (const auto& pair : s_animSelection) {
            if (pair.second) selectedCount++;
        }
        std::string animHeader = "Animations (" + std::to_string(selectedCount) + "/" + std::to_string(s_animSelection.size()) + " selected)";
        if (ImGui::CollapsingHeader(animHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Checkbox("Select All", &s_selectAllAnims)) {
                for (auto& pair : s_animSelection) {
                    pair.second = s_selectAllAnims;
                }
            }
            ImGui::BeginChild("AnimList", ImVec2(400, 200), true);
            for (auto& pair : s_animSelection) {
                ImGui::Checkbox(pair.first.c_str(), &pair.second);
            }
            ImGui::EndChild();
        }
        ImGui::Separator();
        if (ImGui::Button("Export", ImVec2(120, 0))) {
            s_showExportOptions = false;
            ImGui::CloseCurrentPopup();
            t_active = true;
            t_targetTab = state.mainTab;
            t_phase = 1;
            t_alpha = 0.0f;
            t_isLoadingContent = true;
            std::thread(runExportTask, &state).detach();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            s_showExportOptions = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
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
                                            auto normalize = [](const std::string& s) {
                                                std::string result;
                                                for (char c : s) {
                                                    if (c != '_') result += std::tolower(c);
                                                }
                                                return result;
                                            };
                                            for (auto& track : anim.tracks) {
                                                track.boneIndex = state.currentModel.skeleton.findBone(track.boneName);
                                                if (track.boneIndex < 0) {
                                                    std::string trackNorm = normalize(track.boneName);
                                                    for (size_t bi = 0; bi < state.currentModel.skeleton.bones.size(); bi++) {
                                                        if (trackNorm == normalize(state.currentModel.skeleton.bones[bi].name)) {
                                                            track.boneIndex = (int)bi;
                                                            break;
                                                        }
                                                    }
                                                }
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
    if (ImGuiFileDialog::Instance()->Display("ExtractTexture", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string exportPath = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string texName = state.previewTextureName;
            std::transform(texName.begin(), texName.end(), texName.begin(), ::tolower);
            auto it = state.textureCache.find(texName);
            if (it != state.textureCache.end()) {
                std::ofstream out(exportPath, std::ios::binary);
                out.write(reinterpret_cast<const char*>(it->second.data()), it->second.size());
                state.statusMessage = "Extracted: " + exportPath;
            } else {
                for (const auto& erfPath : state.erfFiles) {
                    ERFFile erf;
                    if (erf.open(erfPath)) {
                        for (const auto& entry : erf.entries()) {
                            std::string entryLower = entry.name;
                            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
                            if (entryLower == texName) {
                                auto data = erf.readEntry(entry);
                                if (!data.empty()) {
                                    std::ofstream out(exportPath, std::ios::binary);
                                    out.write(reinterpret_cast<const char*>(data.data()), data.size());
                                    state.statusMessage = "Extracted: " + exportPath;
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGuiFileDialog::Instance()->Display("ExtractTexturePNG", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string exportPath = ImGuiFileDialog::Instance()->GetFilePathName();
            std::string texName = state.previewTextureName;
            std::transform(texName.begin(), texName.end(), texName.begin(), ::tolower);
            std::vector<uint8_t> ddsData;
            auto it = state.textureCache.find(texName);
            if (it != state.textureCache.end()) {
                ddsData = it->second;
            } else {
                for (const auto& erfPath : state.erfFiles) {
                    ERFFile erf;
                    if (erf.open(erfPath)) {
                        for (const auto& entry : erf.entries()) {
                            std::string entryLower = entry.name;
                            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
                            if (entryLower == texName) {
                                ddsData = erf.readEntry(entry);
                                break;
                            }
                        }
                    }
                    if (!ddsData.empty()) break;
                }
            }
            if (!ddsData.empty()) {
                std::vector<uint8_t> rgba;
                int w, h;
                if (decodeDDSToRGBA(ddsData, rgba, w, h)) {
                    std::vector<uint8_t> png;
                    encodePNG(rgba, w, h, png);
                    std::ofstream out(exportPath, std::ios::binary);
                    out.write(reinterpret_cast<const char*>(png.data()), png.size());
                    state.statusMessage = "Extracted: " + exportPath;
                } else {
                    state.statusMessage = "Failed to decode texture";
                }
            }
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
                                                    auto normalize = [](const std::string& s) {
                                                        std::string result;
                                                        for (char c : s) {
                                                            if (c != '_') result += std::tolower(c);
                                                        }
                                                        return result;
                                                    };
                                                    for (auto& track : anim.tracks) {
                                                        track.boneIndex = state.currentModel.skeleton.findBone(track.boneName);
                                                        if (track.boneIndex < 0) {
                                                            std::string trackNorm = normalize(track.boneName);
                                                            for (size_t bi = 0; bi < state.currentModel.skeleton.bones.size(); bi++) {
                                                                if (trackNorm == normalize(state.currentModel.skeleton.bones[bi].name)) {
                                                                    track.boneIndex = (int)bi;
                                                                    break;
                                                                }
                                                            }
                                                        }
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
    if (ImGuiFileDialog::Instance()->Display("ConvertAllAudio", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string outDir = ImGuiFileDialog::Instance()->GetCurrentPath();
            int converted = 0;
            const std::vector<std::string>& files = (state.selectedErfName == "[Audio]") ? state.audioFiles : state.voiceOverFiles;
            for (const auto& fsbPath : files) {
                std::string ext = fsbPath.substr(fsbPath.size() - 4);
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext != ".fsb") continue;
                size_t lastSlash = fsbPath.find_last_of("/\\");
                std::string filename = (lastSlash != std::string::npos) ? fsbPath.substr(lastSlash + 1) : fsbPath;
                size_t dotPos = filename.rfind('.');
                if (dotPos != std::string::npos) filename = filename.substr(0, dotPos);
                filename += ".mp3";
                std::string outPath = outDir + "/" + filename;
                if (extractFSB4toMP3(fsbPath, outPath)) {
                    converted++;
                }
            }
            state.statusMessage = "Converted " + std::to_string(converted) + " audio files to " + outDir;
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGuiFileDialog::Instance()->Display("ConvertSelectedAudio", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk() && state.selectedEntryIndex >= 0) {
            std::string outPath = ImGuiFileDialog::Instance()->GetFilePathName();
            const CachedEntry& ce = state.mergedEntries[state.selectedEntryIndex];
            std::string fullPath;
            if (state.selectedErfName == "[Audio]" && ce.erfIdx < state.audioFiles.size()) {
                fullPath = state.audioFiles[ce.erfIdx];
            } else if (state.selectedErfName == "[VoiceOver]" && ce.erfIdx < state.voiceOverFiles.size()) {
                fullPath = state.voiceOverFiles[ce.erfIdx];
            }
            if (!fullPath.empty()) {
                if (extractFSB4toMP3(fullPath, outPath)) {
                    state.statusMessage = "Converted: " + outPath;
                } else {
                    state.statusMessage = "Failed to convert: " + ce.name;
                }
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::BeginMenu("Import")) {
                if (ImGui::MenuItem("GLB to ERF...")) {
                    IGFD::FileDialogConfig cfg;
                    cfg.path = state.selectedFolder;
                    ImGuiFileDialog::Instance()->OpenDialog("ImportGLB", "Choose GLB File", ".glb", cfg);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Export")) {
                if (ImGui::MenuItem("To GLB", nullptr, false, state.hasModel)) {
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
                    std::string defaultName = state.currentModel.name;
                    size_t dotPos = defaultName.rfind('.');
                    if (dotPos != std::string::npos) defaultName = defaultName.substr(0, dotPos);
                    defaultName += ".glb";
                    config.fileName = defaultName;
                    ImGuiFileDialog::Instance()->OpenDialog("ExportCurrentGLB", "Export Model as GLB", ".glb", config);
                }
                if (ImGui::MenuItem("To FBX", nullptr, false, state.hasModel)) {
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
                    std::string defaultName = state.currentModel.name;
                    size_t dotPos = defaultName.rfind('.');
                    if (dotPos != std::string::npos) defaultName = defaultName.substr(0, dotPos);
                    defaultName += ".fbx";
                    config.fileName = defaultName;
                    ImGuiFileDialog::Instance()->OpenDialog("ExportCurrentFBX", "Export Model as FBX", ".fbx", config);
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("About")) {
                s_showAbout = true;
            }
            if (ImGui::MenuItem("Changelog")) {
                s_showChangelog = true;
                s_scrollToBottom = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit", "Alt+F4")) {
                glfwSetWindowShouldClose(window, true);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window")) {
            if (state.mainTab == 0) {
                ImGui::MenuItem("ERF Browser", nullptr, &state.showBrowser);
                ImGui::MenuItem("Mesh Browser", nullptr, &state.showMeshBrowser);
            }
            ImGui::MenuItem("Render Settings", nullptr, &state.showRenderSettings);
            ImGui::MenuItem("Animation", nullptr, &state.showAnimWindow);
            ImGui::Separator();
            ImGui::MenuItem("2DA/GDA Editor", nullptr, &state.gdaEditor.showWindow);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::MenuItem("Keybinds")) {
                state.showKeybinds = true;
            }
            ImGui::EndMenu();
        }
        ImGui::Text(" | ");
        ImGui::Text("Mode:");
        ImGui::SameLine();
        bool browserSelected = (state.mainTab == 0);
        if (ImGui::RadioButton("Browser", browserSelected)) {
            if (state.mainTab != 0 && !t_active) {
                t_active = true;
                t_targetTab = 0;
                t_phase = 1;
                t_alpha = 0.0f;
            }
        }
        ImGui::SameLine();
        bool charSelected = (state.mainTab == 1);
        if (ImGui::RadioButton("Character Designer", charSelected)) {
            if (state.mainTab != 1 && !t_active) {
                t_active = true;
                t_targetTab = 1;
                t_phase = 1;
                t_alpha = 0.0f;
                std::thread(runCharDesignerLoading, &state).detach();
            }
        }
        ImGui::SameLine();
        ImGui::Text(" | ");
        ImGui::SameLine();
        if (state.hasModel) {
            ImGui::Text("| %s | RMB: Look | %s%s%s%s: Move | %s/%s: Pan",
                state.currentModel.name.c_str(),
                ImGui::GetKeyName(state.keybinds.moveForward),
                ImGui::GetKeyName(state.keybinds.moveLeft),
                ImGui::GetKeyName(state.keybinds.moveBackward),
                ImGui::GetKeyName(state.keybinds.moveRight),
                ImGui::GetKeyName(state.keybinds.panUp),
                ImGui::GetKeyName(state.keybinds.panDown));
        }
        const char* ver = Update::GetInstalledVersionText();
        float verW = ImGui::CalcTextSize(ver).x;
        float right = ImGui::GetWindowContentRegionMax().x;
        ImGui::SameLine();
        ImGui::SetCursorPosX(right - verW - ImGui::GetStyle().ItemSpacing.x);
        ImGui::TextUnformatted(ver);
        ImGui::EndMainMenuBar();
    }
    if (state.mainTab == 0) {
        if (state.showBrowser) drawBrowserWindow(state);
        if (state.showMeshBrowser) drawMeshBrowserWindow(state);
    } else {
        drawCharacterDesigner(state, io);
    }
    if (state.showRenderSettings) drawRenderSettingsWindow(state);
    if (state.showKeybinds) drawKeybindsWindow(state);
    if (state.showMaoViewer) drawMaoViewer(state);
    if (state.showTexturePreview && state.previewTextureId != 0) drawTexturePreview(state);
    if (state.showUvViewer && state.hasModel && state.selectedMeshForUv >= 0 && state.selectedMeshForUv < (int)state.currentModel.meshes.size()) drawUvViewer(state);
    if (state.showAnimWindow && state.hasModel) drawAnimWindow(state, io);
    if (state.showAudioPlayer) drawAudioPlayer(state);
    if (state.showFSBBrowser) drawFSBBrowserWindow(state);
    draw2DAEditorWindow(state);
    drawGffViewerWindow(state.gffViewer);
    if (state.showHeadSelector) {
        ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
        ImGui::Begin("Select Head", &state.showHeadSelector, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Body: %s", state.pendingBodyMsh.c_str());
        ImGui::TextDisabled("Double-click to switch heads");
        ImGui::Separator();
        for (int i = 0; i < (int)state.availableHeads.size(); i++) {
            bool selected = (state.selectedHeadIndex == i);
            if (ImGui::Selectable(state.availableHeadNames[i].c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::IsMouseDoubleClicked(0) && i != state.selectedHeadIndex) {
                    ERFFile erf;
                    if (erf.open(state.erfFiles[state.pendingBodyEntry.erfIdx])) {
                        if (state.pendingBodyEntry.entryIdx < erf.entries().size()) {
                            const auto& entry = erf.entries()[state.pendingBodyEntry.entryIdx];
                            state.currentModelAnimations.clear();
                            loadMeshDatabase(state);
                            std::string mshLower = state.pendingBodyMsh;
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
                                loadAndMergeHead(state, state.availableHeads[i]);
                                state.statusMessage = "Loaded: " + state.pendingBodyMsh + " + " + state.availableHeadNames[i];
                                auto eyes = findAssociatedEyes(state, state.pendingBodyMsh);
                                if (!eyes.empty()) {
                                    loadAndMergeHead(state, eyes[0].first);
                                    state.statusMessage += " + " + eyes[0].second;
                                }
                                state.selectedHeadIndex = i;
                            }
                        }
                    }
                }
            }
        }
        ImGui::End();
    }
    if (s_showAbout) {
        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
        ImGui::Begin("About", &s_showAbout);
        ImGui::BeginChild("AboutText", ImVec2(0, 0), true);
        ImGui::TextWrapped("%s", s_aboutText);
        ImGui::EndChild();
        ImGui::End();
    }
    if (s_showChangelog) {
        ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
        ImGui::Begin("Changelog", &s_showChangelog);
        ImGui::BeginChild("ChangeLogText", ImVec2(0, 0), true);
        ImGui::TextWrapped("%s", s_changelogHistory);
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
        ImGui::TextWrapped("%s", s_changelogLatest);
        ImGui::PopStyleColor();
        if (s_scrollToBottom) {
            ImGui::SetScrollHereY(1.0f);
            s_scrollToBottom = false;
        }
        ImGui::EndChild();
        ImGui::End();
    }
    bool showLoading = t_active || Update::IsBusy() || Update::HadError();
    if (showLoading) {
        if (!ImGui::IsPopupOpen("##GlobalLoadingModal")) {
            ImGui::OpenPopup("##GlobalLoadingModal");
        }
        float alpha = Update::IsBusy() ? 1.0f : t_alpha;
        ImVec2 center(displayW * 0.5f, displayH * 0.5f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.0f, 0.0f, 0.0f, 0.8f * alpha));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        if (ImGui::BeginPopupModal("##GlobalLoadingModal", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize))
        {
            float p = 0.0f;
            std::string statusText;
            if (Update::IsBusy()) {
                statusText = Update::GetStatusText();
                p = Update::GetProgress();
            } else if (Update::HadError()) {
                statusText = "Update failed";
                p = 0.0f;
            } else {
                if (t_isLoadingContent) {
                    statusText = state.preloadStatus;
                    p = state.preloadProgress;
                } else {
                    statusText = "Loading...";
                    p = (t_phase == 1) ? t_alpha * 0.5f : 1.0f;
                }
            }
            float winWidth = ImGui::GetWindowSize().x;
            float textWidth = ImGui::CalcTextSize(statusText.c_str()).x;
            ImGui::SetCursorPosX((winWidth - textWidth) * 0.5f);
            ImGui::TextUnformatted(statusText.c_str());
            ImGui::Spacing();
            ImGui::ProgressBar(p, ImVec2(300, 25));
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor(1);
    }
}