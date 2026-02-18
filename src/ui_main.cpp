#include "ui_internal.h"
#include "update/update.h"
#include "animation.h"
#include <thread>
#include "import.h"
#include "export.h"
#include "terrain_export.h"
#include "update/about_text.h"
#include "update/changelog_text.h"
#include "blender_addon_embedded.h"

static bool exportBlenderAddon(const unsigned char* data, unsigned int size, const std::string& destDir) {
    namespace fs = std::filesystem;
    fs::path outPath = fs::path(destDir) / "havenarea_importer.zip";
    std::ofstream out(outPath, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(data), size);
    return out.good();
}
static const char* CURRENT_APP_VERSION = "2.1";

static void boneEditQuatMul(float q1x, float q1y, float q1z, float q1w,
                            float q2x, float q2y, float q2z, float q2w,
                            float& rx, float& ry, float& rz, float& rw) {
    rw = q1w*q2w - q1x*q2x - q1y*q2y - q1z*q2z;
    rx = q1w*q2x + q1x*q2w + q1y*q2z - q1z*q2y;
    ry = q1w*q2y - q1x*q2z + q1y*q2w + q1z*q2x;
    rz = q1w*q2z + q1x*q2y - q1y*q2x + q1z*q2w;
}

static void boneEditAxisAngleToQuat(float ax, float ay, float az, float angle,
                                    float& qx, float& qy, float& qz, float& qw) {
    float ha = angle * 0.5f;
    float s = sinf(ha);
    qx = ax * s; qy = ay * s; qz = az * s; qw = cosf(ha);
}

static void boneEditApply(AppState& state, float mouseDX, float mouseDY) {
    if (state.boneEditMode == 0 || state.selectedBoneIndex < 0) return;
    if (state.selectedBoneIndex >= (int)state.currentModel.skeleton.bones.size()) return;
    Bone& bone = state.currentModel.skeleton.bones[state.selectedBoneIndex];
    float sensitivity;

    if (state.boneEditMode == 1) {
        sensitivity = 0.005f;
        if (state.boneEditAxis >= 0) {
            float angle = mouseDX * sensitivity;
            float ax = 0, ay = 0, az = 0;
            if (state.boneEditAxis == 0) ax = 1;
            else if (state.boneEditAxis == 1) ay = 1;
            else if (state.boneEditAxis == 2) az = 1;
            float dqx, dqy, dqz, dqw;
            boneEditAxisAngleToQuat(ax, ay, az, angle, dqx, dqy, dqz, dqw);
            boneEditQuatMul(state.boneEditSavedRot[0], state.boneEditSavedRot[1],
                            state.boneEditSavedRot[2], state.boneEditSavedRot[3],
                            dqx, dqy, dqz, dqw,
                            bone.rotX, bone.rotY, bone.rotZ, bone.rotW);
        } else {
            float angleY = mouseDX * sensitivity;
            float angleX = -mouseDY * sensitivity;
            float dqx1, dqy1, dqz1, dqw1;
            boneEditAxisAngleToQuat(0, 1, 0, angleY, dqx1, dqy1, dqz1, dqw1);
            float dqx2, dqy2, dqz2, dqw2;
            boneEditAxisAngleToQuat(1, 0, 0, angleX, dqx2, dqy2, dqz2, dqw2);
            float cx, cy, cz, cw;
            boneEditQuatMul(dqx1, dqy1, dqz1, dqw1, dqx2, dqy2, dqz2, dqw2, cx, cy, cz, cw);
            boneEditQuatMul(state.boneEditSavedRot[0], state.boneEditSavedRot[1],
                            state.boneEditSavedRot[2], state.boneEditSavedRot[3],
                            cx, cy, cz, cw,
                            bone.rotX, bone.rotY, bone.rotZ, bone.rotW);
        }
    } else if (state.boneEditMode == 2) {
        sensitivity = 0.0005f;
        bone.posX = state.boneEditSavedPos[0];
        bone.posY = state.boneEditSavedPos[1];
        bone.posZ = state.boneEditSavedPos[2];
        if (state.boneEditAxis == 0) bone.posX += mouseDX * sensitivity;
        else if (state.boneEditAxis == 1) bone.posY += mouseDX * sensitivity;
        else if (state.boneEditAxis == 2) bone.posZ += mouseDX * sensitivity;
        else {
            bone.posX += mouseDX * sensitivity;
            bone.posY += -mouseDY * sensitivity;
        }
    }
    computeBoneWorldTransforms(state.currentModel);
    state.bonePoseMode = true;
}

static void boneEditCancel(AppState& state) {
    if (state.boneEditMode == 0 || state.selectedBoneIndex < 0) return;
    if (state.selectedBoneIndex >= (int)state.currentModel.skeleton.bones.size()) {
        state.boneEditMode = 0; state.boneEditAxis = -1; return;
    }
    Bone& bone = state.currentModel.skeleton.bones[state.selectedBoneIndex];
    bone.rotX = state.boneEditSavedRot[0]; bone.rotY = state.boneEditSavedRot[1];
    bone.rotZ = state.boneEditSavedRot[2]; bone.rotW = state.boneEditSavedRot[3];
    bone.posX = state.boneEditSavedPos[0]; bone.posY = state.boneEditSavedPos[1];
    bone.posZ = state.boneEditSavedPos[2];
    computeBoneWorldTransforms(state.currentModel);
    state.boneEditMode = 0;
    state.boneEditAxis = -1;
}

static void boneEditStart(AppState& state, int mode, GLFWwindow* window) {
    if (state.selectedBoneIndex < 0 || !state.renderSettings.showSkeleton) return;
    if (!state.hasModel || state.currentModel.skeleton.bones.empty()) return;
    if (state.selectedBoneIndex >= (int)state.currentModel.skeleton.bones.size()) return;
    if (state.basePoseBones.empty())
        state.basePoseBones = state.currentModel.skeleton.bones;
    const Bone& bone = state.currentModel.skeleton.bones[state.selectedBoneIndex];
    state.boneEditSavedRot[0] = bone.rotX; state.boneEditSavedRot[1] = bone.rotY;
    state.boneEditSavedRot[2] = bone.rotZ; state.boneEditSavedRot[3] = bone.rotW;
    state.boneEditSavedPos[0] = bone.posX; state.boneEditSavedPos[1] = bone.posY;
    state.boneEditSavedPos[2] = bone.posZ;
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    state.boneEditStartX = (float)mx;
    state.boneEditStartY = (float)my;
    state.boneEditMode = mode;
    state.boneEditAxis = -1;
}
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
static bool s_showImportOptions = false;
static int s_importMode = 1;
static std::string s_pendingExportPath;
static bool s_showExportOptions = false;
static bool s_showLevelExportOptions = false;
static std::string s_levelExportDir;
static bool s_levelExportFbx = false;
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
    state.rimFiles.clear();
    state.arlFiles.clear();
    state.opfFiles.clear();
    state.rimMshCounts.clear();
    try {
        for (const auto& entry : fs::recursive_directory_iterator(state.selectedFolder,
                fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".lvl") {
                state.erfFiles.push_back(entry.path().string());
            } else if (ext == ".rim") {
                state.rimFiles.push_back(entry.path().string());
            } else if (ext == ".arl") {
                state.arlFiles.push_back(entry.path().string());
            } else if (ext == ".opf") {
                state.opfFiles.push_back(entry.path().string());
            }
        }
    } catch (...) {}
    std::sort(state.rimFiles.begin(), state.rimFiles.end());
    std::sort(state.arlFiles.begin(), state.arlFiles.end());
    std::sort(state.opfFiles.begin(), state.opfFiles.end());

    state.rimMshCounts.assign(state.rimFiles.size(), 0);
    state.rimScanDone = false;
    std::thread([&state]() {
        for (size_t i = 0; i < state.rimFiles.size(); i++) {
            ERFFile rim;
            if (rim.open(state.rimFiles[i])) {
                int mshCount = 0;
                for (const auto& e : rim.entries()) {
                    size_t len = e.name.size();
                    if (len > 4) {
                        std::string ext = e.name.substr(len - 4);
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if (ext == ".msh") mshCount++;
                    }
                }
                state.rimMshCounts[i] = mshCount;
            }
        }
        state.rimScanDone = true;
    }).detach();

    state.preloadStatus = "Filtering encrypted files...";
    state.preloadProgress = 0.05f;
    filterEncryptedErfs(state);

    state.preloadStatus = "Loading talk tables...";
    state.preloadProgress = 0.07f;
    GFF4TLK::clear();
    {
        int tlkCount = GFF4TLK::loadAllFromPath(state.selectedFolder);
        if (tlkCount > 0)
            state.gffViewer.tlkStatus = "Loaded " + std::to_string(GFF4TLK::count()) + " strings from " + std::to_string(tlkCount) + " TLK files";
    }

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

        std::string extLower = fs::path(erfPath).extension().string();
        std::transform(extLower.begin(), extLower.end(), extLower.begin(), ::tolower);
        if (extLower == ".lvl") {
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

        if (!isModel && !isMaterial && !isTexture) {
            processed++;
            state.preloadProgress = 0.1f + ((float)processed / (float)totalErfs) * 0.8f;
            continue;
        }

        ERFFile erf;
        if (!erf.open(erfPath)) {
            processed++;
            state.preloadProgress = 0.1f + ((float)processed / (float)totalErfs) * 0.8f;
            continue;
        }

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
    bool success;
    if (s_importMode == 1) {
        success = importer.ImportToOverride(s_pendingImportGlbPath, state.selectedFolder);
    } else {
        success = importer.ImportToDirectory(s_pendingImportGlbPath, state.selectedFolder);
    }
    if (success) {
        std::string modelName = fs::path(s_pendingImportGlbPath).stem().string() + ".msh";
        markModelAsImported(modelName);
        if (s_importMode == 0) {
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
        }
        state.preloadProgress = 1.0f;
        state.preloadStatus = "Import complete!";
        std::string dest = (s_importMode == 1) ? "override folder" : "ERF archives";
        state.statusMessage = "Model imported to " + dest + " successfully!";
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
        Model fbxModel = state.currentModel;
        for (auto& mesh : fbxModel.meshes) {
            for (auto& v : mesh.vertices) {
                float oy = v.y; v.y = v.z; v.z = -oy;
                float ony = v.ny; v.ny = v.nz; v.nz = -ony;
            }
        }
        success = exportToFBX(fbxModel, exportAnims, s_pendingExportPath, exportOpts);
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

static float s_scrollAccum = 0.0f;
static GLFWscrollfun s_prevScrollCb = nullptr;
static bool s_scrollHooked = false;

static void scrollCallbackWrapper(GLFWwindow* window, double x, double y) {
    s_scrollAccum += (float)y;
    if (s_prevScrollCb) s_prevScrollCb(window, x, y);
}

void handleInput(AppState& state, GLFWwindow* window, ImGuiIO& io) {
    if (!s_scrollHooked) {
        s_prevScrollCb = glfwSetScrollCallback(window, scrollCallbackWrapper);
        s_scrollHooked = true;
    }
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
        if (!state.selectedFolder.empty() && !state.isPreloading) {
            fs::path launcherPath = fs::path(state.selectedFolder) / "DAOriginsLauncher.exe";
            fs::path exePath = fs::path(state.selectedFolder) / "DAOrigins.exe";
            if (fs::exists(launcherPath) || fs::exists(exePath)) {
                state.isPreloading = true;
                std::thread(runLoadingTask, &state).detach();
            }
        }
    }
    if (!io.WantCaptureMouse) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        static bool wasLeftPressed = false;
        bool leftPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (leftPressed && !wasLeftPressed && state.boneEditMode != 0) {
            state.boneEditMode = 0;
            state.boneEditAxis = -1;
        }
        else if (leftPressed && !wasLeftPressed && state.hasModel && state.renderSettings.showSkeleton) {
            int width, height;
            glfwGetWindowSize(window, &width, &height);
            if (width <= 0 || height <= 0) { wasLeftPressed = leftPressed; return; }
            float aspect = (float)width / (float)height;
            float fov = 45.0f * 3.14159f / 180.0f;
            float tanHalfFov = std::tan(fov / 2.0f);

            float ndcX = (2.0f * (float)mx / width) - 1.0f;
            float ndcY = 1.0f - (2.0f * (float)my / height);

            auto identity = [](float* m) { for(int i=0;i<16;i++) m[i]=(i%5==0)?1.0f:0.0f; };
            auto mul = [](const float* a, const float* b, float* out) {
                for(int i=0;i<4;i++) for(int j=0;j<4;j++) {
                    out[i*4+j]=0;
                    for(int k=0;k<4;k++) out[i*4+j]+=a[i*4+k]*b[k*4+j];
                }
            };
            auto applyRX = [&](float* m, float angle) {
                float c=cosf(angle), s=sinf(angle);
                float r[16]; identity(r);
                r[5]=c; r[6]=s; r[9]=-s; r[10]=c;
                float tmp[16]; mul(m,r,tmp); memcpy(m,tmp,64);
            };
            auto applyRY = [&](float* m, float angle) {
                float c=cosf(angle), s=sinf(angle);
                float r[16]; identity(r);
                r[0]=c; r[2]=-s; r[8]=s; r[10]=c;
                float tmp[16]; mul(m,r,tmp); memcpy(m,tmp,64);
            };
            auto applyT = [&](float* m, float x, float y, float z) {
                float t[16]; identity(t);
                t[12]=x; t[13]=y; t[14]=z;
                float tmp[16]; mul(m,t,tmp); memcpy(m,tmp,64);
            };

            float view[16];
            identity(view);
            applyRX(view, -90.0f * 3.14159f / 180.0f);
            applyT(view, -state.camera.x, -state.camera.y, -state.camera.z);
            applyRY(view, -state.camera.yaw);
            applyRX(view, -state.camera.pitch);

            float inv[16], det;
            inv[0] = view[5]*view[10]*view[15]-view[5]*view[11]*view[14]-view[9]*view[6]*view[15]+view[9]*view[7]*view[14]+view[13]*view[6]*view[11]-view[13]*view[7]*view[10];
            inv[4] = -view[4]*view[10]*view[15]+view[4]*view[11]*view[14]+view[8]*view[6]*view[15]-view[8]*view[7]*view[14]-view[12]*view[6]*view[11]+view[12]*view[7]*view[10];
            inv[8] = view[4]*view[9]*view[15]-view[4]*view[11]*view[13]-view[8]*view[5]*view[15]+view[8]*view[7]*view[13]+view[12]*view[5]*view[11]-view[12]*view[7]*view[9];
            inv[12] = -view[4]*view[9]*view[14]+view[4]*view[10]*view[13]+view[8]*view[5]*view[14]-view[8]*view[6]*view[13]-view[12]*view[5]*view[10]+view[12]*view[6]*view[9];
            inv[1] = -view[1]*view[10]*view[15]+view[1]*view[11]*view[14]+view[9]*view[2]*view[15]-view[9]*view[3]*view[14]-view[13]*view[2]*view[11]+view[13]*view[3]*view[10];
            inv[5] = view[0]*view[10]*view[15]-view[0]*view[11]*view[14]-view[8]*view[2]*view[15]+view[8]*view[3]*view[14]+view[12]*view[2]*view[11]-view[12]*view[3]*view[10];
            inv[9] = -view[0]*view[9]*view[15]+view[0]*view[11]*view[13]+view[8]*view[1]*view[15]-view[8]*view[3]*view[13]-view[12]*view[1]*view[11]+view[12]*view[3]*view[9];
            inv[13] = view[0]*view[9]*view[14]-view[0]*view[10]*view[13]-view[8]*view[1]*view[14]+view[8]*view[2]*view[13]+view[12]*view[1]*view[10]-view[12]*view[2]*view[9];
            inv[2] = view[1]*view[6]*view[15]-view[1]*view[7]*view[14]-view[5]*view[2]*view[15]+view[5]*view[3]*view[14]+view[13]*view[2]*view[7]-view[13]*view[3]*view[6];
            inv[6] = -view[0]*view[6]*view[15]+view[0]*view[7]*view[14]+view[4]*view[2]*view[15]-view[4]*view[3]*view[14]-view[12]*view[2]*view[7]+view[12]*view[3]*view[6];
            inv[10] = view[0]*view[5]*view[15]-view[0]*view[7]*view[13]-view[4]*view[1]*view[15]+view[4]*view[3]*view[13]+view[12]*view[1]*view[7]-view[12]*view[3]*view[5];
            inv[14] = -view[0]*view[5]*view[14]+view[0]*view[6]*view[13]+view[4]*view[1]*view[14]-view[4]*view[2]*view[13]-view[12]*view[1]*view[6]+view[12]*view[2]*view[5];
            inv[3] = -view[1]*view[6]*view[11]+view[1]*view[7]*view[10]+view[5]*view[2]*view[11]-view[5]*view[3]*view[10]-view[9]*view[2]*view[7]+view[9]*view[3]*view[6];
            inv[7] = view[0]*view[6]*view[11]-view[0]*view[7]*view[10]-view[4]*view[2]*view[11]+view[4]*view[3]*view[10]+view[8]*view[2]*view[7]-view[8]*view[3]*view[6];
            inv[11] = -view[0]*view[5]*view[11]+view[0]*view[7]*view[9]+view[4]*view[1]*view[11]-view[4]*view[3]*view[9]-view[8]*view[1]*view[7]+view[8]*view[3]*view[5];
            inv[15] = view[0]*view[5]*view[10]-view[0]*view[6]*view[9]-view[4]*view[1]*view[10]+view[4]*view[2]*view[9]+view[8]*view[1]*view[6]-view[8]*view[2]*view[5];
            det = view[0]*inv[0]+view[1]*inv[4]+view[2]*inv[8]+view[3]*inv[12];
            if (std::abs(det) > 1e-12f) {
                float invDet = 1.0f / det;
                for(int i=0;i<16;i++) inv[i]*=invDet;
            }

            float vx = ndcX * tanHalfFov * aspect;
            float vy = ndcY * tanHalfFov;
            float vz = -1.0f;

            float dirX = inv[0]*vx + inv[4]*vy + inv[8]*vz;
            float dirY = inv[1]*vx + inv[5]*vy + inv[9]*vz;
            float dirZ = inv[2]*vx + inv[6]*vy + inv[10]*vz;
            float len = std::sqrt(dirX*dirX + dirY*dirY + dirZ*dirZ);
            dirX /= len; dirY /= len; dirZ /= len;

            float origX = inv[12];
            float origY = inv[13];
            float origZ = inv[14];

            int closestBone = -1;
            float closestDist = 999999.0f;
            for (size_t i = 0; i < state.currentModel.skeleton.bones.size(); i++) {
                const auto& bone = state.currentModel.skeleton.bones[i];
                float bx = bone.worldPosX;
                float by = bone.worldPosY;
                float bz = bone.worldPosZ;
                float toX = bx - origX;
                float toY = by - origY;
                float toZ = bz - origZ;
                float t = toX*dirX + toY*dirY + toZ*dirZ;
                if (t < 0) continue;
                float cx = origX + dirX * t - bx;
                float cy = origY + dirY * t - by;
                float cz = origZ + dirZ * t - bz;
                float dist = std::sqrt(cx*cx + cy*cy + cz*cz);
                float threshold = std::max(0.05f, t * 0.02f);
                if (dist < threshold && t < closestDist) {
                    closestDist = t;
                    closestBone = (int)i;
                }
            }
            if (closestBone >= 0) {
                state.selectedBoneIndex = closestBone;
            }
        }
        if (leftPressed && !wasLeftPressed && state.hasModel && !state.renderSettings.showSkeleton
            && state.currentModel.meshes.size() > 1) {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            float aspect = (float)width / (float)height;
            float fov = 45.0f * 3.14159f / 180.0f;
            float tanHalfFov = std::tan(fov / 2.0f);

            float ndcX = (2.0f * (float)mx / width) - 1.0f;
            float ndcY = 1.0f - (2.0f * (float)my / height);

            auto identity = [](float* m) { for(int i=0;i<16;i++) m[i]=(i%5==0)?1.0f:0.0f; };
            auto mul = [](const float* a, const float* b, float* out) {
                for(int i=0;i<4;i++) for(int j=0;j<4;j++) {
                    out[i*4+j]=0;
                    for(int k=0;k<4;k++) out[i*4+j]+=a[i*4+k]*b[k*4+j];
                }
            };
            auto applyRX = [&](float* m, float angle) {
                float c=cosf(angle), s=sinf(angle);
                float r[16]; identity(r);
                r[5]=c; r[6]=s; r[9]=-s; r[10]=c;
                float tmp[16]; mul(m,r,tmp); memcpy(m,tmp,64);
            };
            auto applyRY = [&](float* m, float angle) {
                float c=cosf(angle), s=sinf(angle);
                float r[16]; identity(r);
                r[0]=c; r[2]=-s; r[8]=s; r[10]=c;
                float tmp[16]; mul(m,r,tmp); memcpy(m,tmp,64);
            };
            auto applyT = [&](float* m, float x, float y, float z) {
                float t[16]; identity(t);
                t[12]=x; t[13]=y; t[14]=z;
                float tmp[16]; mul(m,t,tmp); memcpy(m,tmp,64);
            };

            float view[16];
            identity(view);
            applyRX(view, -90.0f * 3.14159f / 180.0f);
            applyT(view, -state.camera.x, -state.camera.y, -state.camera.z);
            applyRY(view, -state.camera.yaw);
            applyRX(view, -state.camera.pitch);

            float inv[16], det;
            inv[0] = view[5]*view[10]*view[15]-view[5]*view[11]*view[14]-view[9]*view[6]*view[15]+view[9]*view[7]*view[14]+view[13]*view[6]*view[11]-view[13]*view[7]*view[10];
            inv[4] = -view[4]*view[10]*view[15]+view[4]*view[11]*view[14]+view[8]*view[6]*view[15]-view[8]*view[7]*view[14]-view[12]*view[6]*view[11]+view[12]*view[7]*view[10];
            inv[8] = view[4]*view[9]*view[15]-view[4]*view[11]*view[13]-view[8]*view[5]*view[15]+view[8]*view[7]*view[13]+view[12]*view[5]*view[11]-view[12]*view[7]*view[9];
            inv[12] = -view[4]*view[9]*view[14]+view[4]*view[10]*view[13]+view[8]*view[5]*view[14]-view[8]*view[6]*view[13]-view[12]*view[5]*view[10]+view[12]*view[6]*view[9];
            inv[1] = -view[1]*view[10]*view[15]+view[1]*view[11]*view[14]+view[9]*view[2]*view[15]-view[9]*view[3]*view[14]-view[13]*view[2]*view[11]+view[13]*view[3]*view[10];
            inv[5] = view[0]*view[10]*view[15]-view[0]*view[11]*view[14]-view[8]*view[2]*view[15]+view[8]*view[3]*view[14]+view[12]*view[2]*view[11]-view[12]*view[3]*view[10];
            inv[9] = -view[0]*view[9]*view[15]+view[0]*view[11]*view[13]+view[8]*view[1]*view[15]-view[8]*view[3]*view[13]-view[12]*view[1]*view[11]+view[12]*view[3]*view[9];
            inv[13] = view[0]*view[9]*view[14]-view[0]*view[10]*view[13]-view[8]*view[1]*view[14]+view[8]*view[2]*view[13]+view[12]*view[1]*view[10]-view[12]*view[2]*view[9];
            inv[2] = view[1]*view[6]*view[15]-view[1]*view[7]*view[14]-view[5]*view[2]*view[15]+view[5]*view[3]*view[14]+view[13]*view[2]*view[7]-view[13]*view[3]*view[6];
            inv[6] = -view[0]*view[6]*view[15]+view[0]*view[7]*view[14]+view[4]*view[2]*view[15]-view[4]*view[3]*view[14]-view[12]*view[2]*view[7]+view[12]*view[3]*view[6];
            inv[10] = view[0]*view[5]*view[15]-view[0]*view[7]*view[13]-view[4]*view[1]*view[15]+view[4]*view[3]*view[13]+view[12]*view[1]*view[7]-view[12]*view[3]*view[5];
            inv[14] = -view[0]*view[5]*view[14]+view[0]*view[6]*view[13]+view[4]*view[1]*view[14]-view[4]*view[2]*view[13]-view[12]*view[1]*view[6]+view[12]*view[2]*view[5];
            inv[3] = -view[1]*view[6]*view[11]+view[1]*view[7]*view[10]+view[5]*view[2]*view[11]-view[5]*view[3]*view[10]-view[9]*view[2]*view[7]+view[9]*view[3]*view[6];
            inv[7] = view[0]*view[6]*view[11]-view[0]*view[7]*view[10]-view[4]*view[2]*view[11]+view[4]*view[3]*view[10]+view[8]*view[2]*view[7]-view[8]*view[3]*view[6];
            inv[11] = -view[0]*view[5]*view[11]+view[0]*view[7]*view[9]+view[4]*view[1]*view[11]-view[4]*view[3]*view[9]-view[8]*view[1]*view[7]+view[8]*view[3]*view[5];
            inv[15] = view[0]*view[5]*view[10]-view[0]*view[6]*view[9]-view[4]*view[1]*view[10]+view[4]*view[2]*view[9]+view[8]*view[1]*view[6]-view[8]*view[2]*view[5];
            det = view[0]*inv[0]+view[1]*inv[4]+view[2]*inv[8]+view[3]*inv[12];
            if (std::abs(det) > 1e-12f) {
                float invDet = 1.0f / det;
                for(int i=0;i<16;i++) inv[i]*=invDet;
            }

            float vx = ndcX * tanHalfFov * aspect;
            float vy = ndcY * tanHalfFov;
            float vz = -1.0f;

            float dirX = inv[0]*vx + inv[4]*vy + inv[8]*vz;
            float dirY = inv[1]*vx + inv[5]*vy + inv[9]*vz;
            float dirZ = inv[2]*vx + inv[6]*vy + inv[10]*vz;
            float len = std::sqrt(dirX*dirX + dirY*dirY + dirZ*dirZ);
            dirX /= len; dirY /= len; dirZ /= len;

            float origX = inv[12];
            float origY = inv[13];
            float origZ = inv[14];

            int closestChunk = -1;
            float closestT = 1e30f;

            for (size_t mi = 0; mi < state.currentModel.meshes.size(); mi++) {
                if (mi < state.renderSettings.meshVisible.size() && state.renderSettings.meshVisible[mi] == 0) continue;
                const auto& m = state.currentModel.meshes[mi];
                if (m.vertices.empty() || m.indices.empty()) continue;
                for (size_t ti = 0; ti + 2 < m.indices.size(); ti += 3) {
                    const auto& v0 = m.vertices[m.indices[ti]];
                    const auto& v1 = m.vertices[m.indices[ti+1]];
                    const auto& v2 = m.vertices[m.indices[ti+2]];
                    float ax = v0.x, ay = v0.y, az = v0.z;
                    float bx = v1.x, by = v1.y, bz = v1.z;
                    float cx = v2.x, cy = v2.y, cz = v2.z;
                    float e1x = bx-ax, e1y = by-ay, e1z = bz-az;
                    float e2x = cx-ax, e2y = cy-ay, e2z = cz-az;
                    float px = dirY*e2z - dirZ*e2y;
                    float py = dirZ*e2x - dirX*e2z;
                    float pz = dirX*e2y - dirY*e2x;
                    float det2 = e1x*px + e1y*py + e1z*pz;
                    if (std::abs(det2) < 1e-8f) continue;
                    float invDet2 = 1.0f / det2;
                    float tx = origX-ax, ty = origY-ay, tz = origZ-az;
                    float u = (tx*px + ty*py + tz*pz) * invDet2;
                    if (u < 0.0f || u > 1.0f) continue;
                    float qx = ty*e1z - tz*e1y;
                    float qy = tz*e1x - tx*e1z;
                    float qz = tx*e1y - ty*e1x;
                    float v = (dirX*qx + dirY*qy + dirZ*qz) * invDet2;
                    if (v < 0.0f || u + v > 1.0f) continue;
                    float tt = (e2x*qx + e2y*qy + e2z*qz) * invDet2;
                    if (tt > 0.0f && tt < closestT) {
                        closestT = tt;
                        closestChunk = (int)mi;
                    }
                }
            }
            state.selectedLevelChunk = closestChunk;
        }
        wasLeftPressed = leftPressed;
    }
    {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);

        if (state.boneEditMode != 0) {
            float dx = (float)mx - state.boneEditStartX;
            float dy = (float)my - state.boneEditStartY;
            boneEditApply(state, dx, dy);
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                boneEditCancel(state);
            }
        }
        else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            ImGui::SetWindowFocus(nullptr);
            if (state.isPanning) {
                float dx = static_cast<float>(mx - state.lastMouseX);
                float dy = static_cast<float>(my - state.lastMouseY);
                state.camera.rotate(-dx * state.camera.lookSensitivity, -dy * state.camera.lookSensitivity);
            }
            state.isPanning = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            float scroll = s_scrollAccum;
            s_scrollAccum = 0.0f;
            if (scroll != 0.0f) {
                state.camera.moveSpeed *= (scroll > 0) ? 1.5f : (1.0f / 1.5f);
                if (state.camera.moveSpeed < 0.1f) state.camera.moveSpeed = 0.1f;
                if (state.camera.moveSpeed > 10000.0f) state.camera.moveSpeed = 10000.0f;
            }
        } else {
            s_scrollAccum = 0.0f;
            if (state.isPanning) glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            state.isPanning = false;
        }
        state.lastMouseX = mx;
        state.lastMouseY = my;
    }
    if (!io.WantCaptureKeyboard) {
        if (state.boneEditMode != 0) {
            if (ImGui::IsKeyPressed(ImGuiKey_X)) state.boneEditAxis = 0;
            if (ImGui::IsKeyPressed(ImGuiKey_Y)) state.boneEditAxis = 1;
            if (ImGui::IsKeyPressed(ImGuiKey_Z)) state.boneEditAxis = 2;
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
                state.boneEditMode = 0;
                state.boneEditAxis = -1;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                boneEditCancel(state);
            }
        } else {
            if (state.selectedBoneIndex >= 0 && state.renderSettings.showSkeleton && state.hasModel) {
                if (ImGui::IsKeyPressed(state.keybinds.boneRotate))
                    boneEditStart(state, 1, window);
                if (ImGui::IsKeyPressed(state.keybinds.boneGrab))
                    boneEditStart(state, 2, window);
            }
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

    if (state.boneEditMode != 0 && state.selectedBoneIndex >= 0) {
        const char* modeName = (state.boneEditMode == 1) ? "ROTATE" : "GRAB";
        const char* axisName = "Free";
        if (state.boneEditAxis == 0) axisName = "X";
        else if (state.boneEditAxis == 1) axisName = "Y";
        else if (state.boneEditAxis == 2) axisName = "Z";
        const auto& boneName = state.currentModel.skeleton.bones[state.selectedBoneIndex].name;
        char buf[256];
        snprintf(buf, sizeof(buf), "%s [%s] - %s  |  X/Y/Z: axis  LMB/Enter: confirm  RMB/Esc: cancel",
                 modeName, axisName, boneName.c_str());
        ImVec2 textSize = ImGui::CalcTextSize(buf);
        ImVec2 pos(io.DisplaySize.x * 0.5f - textSize.x * 0.5f, 30.0f);
        ImGui::GetForegroundDrawList()->AddRectFilled(
            ImVec2(pos.x - 8, pos.y - 4), ImVec2(pos.x + textSize.x + 8, pos.y + textSize.y + 4),
            IM_COL32(0, 0, 0, 180), 4.0f);
        ImGui::GetForegroundDrawList()->AddText(pos, IM_COL32(255, 200, 50, 255), buf);
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
        if (ImGui::Button("Browse to Game Executable", buttonSize)) {
            IGFD::FileDialogConfig config;
            config.path = state.lastDialogPath.empty() ? "." : state.lastDialogPath;
            ImGuiFileDialog::Instance()->OpenDialog("ChooseLauncher", "Select DAOriginsLauncher.exe or DAOrigins.exe", ".exe", config);
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
        drawKeybindRow("Delete Object", state.keybinds.deleteObject, 7);
        drawKeybindRow("Bone Rotate", state.keybinds.boneRotate, 8);
        drawKeybindRow("Bone Grab", state.keybinds.boneGrab, 9);

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
                    state.gffViewer.gamePath = state.selectedFolder;
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
            // Clear render state on any tab switch
            state.hasModel = false;
            state.currentModel = Model();
            state.currentAnim = Animation();
            state.animPlaying = false;
            if (t_targetTab == 1 && state.mainTab != 1) {
                state.renderSettings.showSkeleton = false;
                state.renderSettings.showAxes = false;
                state.renderSettings.showGrid = false;
                state.charDesigner.needsRebuild = true;
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
            state.gffViewer.gamePath = state.selectedFolder;
            state.isPreloading = true;
            showSplash = true;
            std::thread(runLoadingTask, &state).detach();
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGuiFileDialog::Instance()->Display("ImportGLB", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            s_pendingImportGlbPath = ImGuiFileDialog::Instance()->GetFilePathName();
            s_showImportOptions = true;
            s_importMode = 1;
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (s_showImportOptions) {
        ImGui::OpenPopup("Import Options");
        s_showImportOptions = false;
    }
    if (ImGui::BeginPopupModal("Import Options", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Import: %s", fs::path(s_pendingImportGlbPath).filename().string().c_str());
        ImGui::Separator();
        ImGui::Text("Choose import destination:");
        ImGui::Spacing();
        ImGui::RadioButton("Override Folder", &s_importMode, 1);
        ImGui::SameLine(); ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Writes loose files to packages/core/override/.\nSafe and easy to revert - just delete the files.");
        ImGui::RadioButton("ERF Embedding", &s_importMode, 0);
        ImGui::SameLine(); ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Repacks game ERF archives directly.\nBackups are created automatically.");
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.3f, 1.0f));
        ImGui::TextWrapped("Note: ERF embedding is experimental!");
        ImGui::PopStyleColor();
        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Button("Import", ImVec2(120, 0))) {
            s_showImportOptions = false;
            t_active = true;
            t_targetTab = state.mainTab;
            t_phase = 1;
            t_alpha = 0.0f;
            t_isLoadingContent = true;
            std::thread(runImportTask, &state).detach();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            s_showImportOptions = false;
            s_pendingImportGlbPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
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
    if (ImGuiFileDialog::Instance()->Display("ExportLevelArea", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            s_levelExportDir = ImGuiFileDialog::Instance()->GetCurrentPath();
            s_showLevelExportOptions = true;
            ImGui::OpenPopup("Level Export Options");
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGui::BeginPopupModal("Level Export Options", &s_showLevelExportOptions, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Export level area: %s", fs::path(state.currentRIMPath).stem().string().c_str());
        ImGui::Separator();
        ImGui::Text("Model Format:");
        if (ImGui::RadioButton("GLB", !s_levelExportFbx)) s_levelExportFbx = false;
        ImGui::SameLine();
        if (ImGui::RadioButton("FBX", s_levelExportFbx)) s_levelExportFbx = true;
        ImGui::Separator();
        if (ImGui::Button("Export", ImVec2(120, 0))) {
            LevelExportOptions opts;
            opts.useFbx = s_levelExportFbx;
            startLevelExport(state, s_levelExportDir, opts);
            s_showLevelExportOptions = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            s_showLevelExportOptions = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
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
    if (ImGuiFileDialog::Instance()->Display("ExportBlenderAddon", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string outDir = ImGuiFileDialog::Instance()->GetCurrentPath();
            if (kBlenderAddonZipSize > 0 && exportBlenderAddon(kBlenderAddonZip, kBlenderAddonZipSize, outDir)) {
                state.statusMessage = "Exported havenarea_importer.zip to: " + outDir;
            } else {
                state.statusMessage = "Failed to export Blender importer";
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::BeginMenu("Import")) {
                if (ImGui::MenuItem("GLB...")) {
                    IGFD::FileDialogConfig cfg;
                    cfg.path = state.selectedFolder;
                    ImGuiFileDialog::Instance()->OpenDialog("ImportGLB", "Choose GLB File", ".glb", cfg);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Export")) {
                bool levelLoaded = state.hasModel && state.levelLoad.stage == 0 &&
                    state.levelExport.stage == 0 &&
                    (!state.levelLoad.propQueue.empty() || !state.levelLoad.sptQueue.empty());
                bool canExportModel = state.hasModel && !levelLoaded;
                if (ImGui::MenuItem("To GLB", nullptr, false, canExportModel)) {
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
                if (ImGui::MenuItem("To FBX", nullptr, false, canExportModel)) {
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
                ImGui::Separator();
                if (ImGui::MenuItem("Level Area...", nullptr, false, levelLoaded)) {
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
                    ImGuiFileDialog::Instance()->OpenDialog("ExportLevelArea", "Choose Export Folder", nullptr, config);
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
        if (ImGui::BeginMenu("Add Ons")) {
            if (ImGui::MenuItem("Export Blender Importer")) {
                IGFD::FileDialogConfig config;
                config.path = state.lastDialogPath.empty() ? "." : state.lastDialogPath;
                ImGuiFileDialog::Instance()->OpenDialog("ExportBlenderAddon", "Select Output Folder", nullptr, config);
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
        ImGui::SameLine();
        ImGui::Text(" | ");
        ImGui::SameLine();
        char speedBuf[32];
        snprintf(speedBuf, sizeof(speedBuf), "Speed: %.1f", state.camera.moveSpeed);
        if (ImGui::SmallButton(speedBuf)) {
            ImGui::OpenPopup("##SpeedPopup");
        }
        if (ImGui::BeginPopup("##SpeedPopup")) {
            ImGui::Text("Camera Speed");
            ImGui::SetNextItemWidth(200);
            ImGui::SliderFloat("##speedslider", &state.camera.moveSpeed, 0.1f, 10000.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
            ImGui::EndPopup();
        }
        const char* ver = Update::GetInstalledVersionText();
        float verW = ImGui::CalcTextSize(ver).x;
        float right = ImGui::GetWindowContentRegionMax().x;
        ImGui::SameLine();
        ImGui::SetCursorPosX(right - verW - ImGui::GetStyle().ItemSpacing.x);
        ImGui::TextUnformatted(ver);
        ImGui::EndMainMenuBar();
    }
    if (state.levelExport.stage > 0) {
        tickLevelExport(state);
        auto& ex = state.levelExport;
        float progress = 0.0f;
        std::string detail;
        if (ex.stage == 1) {
            progress = 0.0f;
            detail = "Terrain...";
        } else if (ex.stage == 2) {
            int total = std::max(ex.totalProps, 1);
            progress = 0.1f + 0.5f * ((float)ex.itemIndex / total);
            detail = std::to_string(ex.propsExported) + " / " + std::to_string(ex.totalProps) + " props";
        } else if (ex.stage == 3) {
            int total = std::max(ex.totalTrees, 1);
            progress = 0.6f + 0.35f * ((float)ex.itemIndex / total);
            detail = std::to_string(ex.treesExported) + " / " + std::to_string(ex.totalTrees) + " trees";
        } else if (ex.stage == 4) {
            progress = 0.95f;
            detail = "Writing .havenarea...";
        }
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400, 0));
        ImGui::Begin("##LevelExporting", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("%s", ex.stageLabel.c_str());
        ImGui::ProgressBar(progress, ImVec2(-1, 0), detail.c_str());
        ImGui::End();
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
    if (state.showHeightmap && state.heightmapTexId) drawHeightmapViewer(state);
    if (state.showAnimWindow && state.hasModel) drawAnimWindow(state, io);
    if (state.showAudioPlayer) drawAudioPlayer(state);
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
    drawGffLoadingOverlay(state.gffViewer);
}