#include "ui.h"
#include "types.h"
#include "mmh_loader.h"
#include "animation.h"
#include "erf.h"
#include "export.h"
#include "dds_loader.h"
#include "Gff.h"
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
#include <functional>
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
static size_t lastMeshCacheSize = 0;
static void loadMeshDatabase(AppState& state) {
    bool needsCacheUpdate = (state.meshCache.size() != lastMeshCacheSize);
    if (state.meshBrowser.loaded && !needsCacheUpdate) return;
    if (!state.meshBrowser.loaded) {
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
            if (!lodStr.empty()) {
                entry.lod = std::stoi(lodStr);
            } else {
                entry.lod = 0;
                size_t dotPos = entry.mshFile.rfind('.');
                if (dotPos != std::string::npos && dotPos >= 2) {
                    char lodChar = entry.mshFile[dotPos - 1];
                    char underscore = entry.mshFile[dotPos - 2];
                    if (underscore == '_' && lodChar >= '0' && lodChar <= '9') {
                        entry.lod = lodChar - '0';
                    }
                }
            }
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
    if (needsCacheUpdate && !state.meshCache.empty()) {
        std::set<std::string> knownMeshes;
        for (const auto& entry : state.meshBrowser.allMeshes) {
            std::string lower = entry.mshFile;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            knownMeshes.insert(lower);
        }
        for (const auto& [meshName, meshData] : state.meshCache) {
            if (knownMeshes.find(meshName) == knownMeshes.end()) {
                MeshEntry entry;
                entry.mshFile = meshName;
                entry.mshName = "";
                entry.category = "UNK";
                entry.lod = 0;
                size_t dotPos = meshName.rfind('.');
                if (dotPos != std::string::npos && dotPos >= 2) {
                    char lodChar = meshName[dotPos - 1];
                    char underscore = meshName[dotPos - 2];
                    if (underscore == '_' && lodChar >= '0' && lodChar <= '9') {
                        entry.lod = lodChar - '0';
                    }
                }
                state.meshBrowser.allMeshes.push_back(entry);
            }
        }
        lastMeshCacheSize = state.meshCache.size();
        state.charDesigner.listsBuilt = false;
    }
}
static std::vector<std::pair<std::string, std::string>> findAssociatedHeads(AppState& state, const std::string& bodyMsh) {
    std::vector<std::pair<std::string, std::string>> heads;
    std::string bodyLower = bodyMsh;
    std::transform(bodyLower.begin(), bodyLower.end(), bodyLower.begin(), ::tolower);
    size_t bdyIdx = bodyLower.find("cn_bdy_");
    if (bdyIdx == std::string::npos) return heads;
    size_t bdyPos = bdyIdx + 7;
    size_t lodPos = bodyLower.rfind('_');
    if (lodPos == std::string::npos || lodPos <= bdyPos) return heads;
    std::string baseName = bodyLower.substr(bdyPos, lodPos - bdyPos);
    std::string lodSuffix = bodyLower.substr(lodPos);
    loadMeshDatabase(state);
    for (const auto& mesh : state.meshBrowser.allMeshes) {
        std::string mshLower = mesh.mshFile;
        std::transform(mshLower.begin(), mshLower.end(), mshLower.begin(), ::tolower);
        size_t hedIdx = mshLower.find("cn_hed_");
        if (hedIdx == std::string::npos) continue;
        size_t hedPos = hedIdx + 7;
        size_t hedLodPos = mshLower.rfind('_');
        if (hedLodPos == std::string::npos || hedLodPos <= hedPos) continue;
        std::string headBase = mshLower.substr(hedPos, hedLodPos - hedPos);
        std::string headLodSuffix = mshLower.substr(hedLodPos);
        if (headLodSuffix != lodSuffix) continue;
        if (headBase.find(baseName) == 0) {
            std::string remainder = headBase.substr(baseName.length());
            bool isMatch = remainder.empty();
            if (!isMatch && !remainder.empty()) {
                isMatch = std::all_of(remainder.begin(), remainder.end(), ::isdigit);
            }
            if (isMatch) {
                heads.push_back({mesh.mshFile, mesh.mshName.empty() ? mesh.mshFile : mesh.mshName});
            }
        }
    }
    return heads;
}
static std::vector<std::pair<std::string, std::string>> findAssociatedEyes(AppState& state, const std::string& bodyMsh) {
    std::vector<std::pair<std::string, std::string>> eyes;
    std::string bodyLower = bodyMsh;
    std::transform(bodyLower.begin(), bodyLower.end(), bodyLower.begin(), ::tolower);
    size_t bdyIdx = bodyLower.find("cn_bdy_");
    if (bdyIdx == std::string::npos) return eyes;
    size_t bdyPos = bdyIdx + 7;
    size_t lodPos = bodyLower.rfind('_');
    if (lodPos == std::string::npos || lodPos <= bdyPos) return eyes;
    std::string baseName = bodyLower.substr(bdyPos, lodPos - bdyPos);
    std::string lodSuffix = bodyLower.substr(lodPos);
    loadMeshDatabase(state);
    for (const auto& mesh : state.meshBrowser.allMeshes) {
        std::string mshLower = mesh.mshFile;
        std::transform(mshLower.begin(), mshLower.end(), mshLower.begin(), ::tolower);
        size_t eyeIdx = mshLower.find("cn_eye_");
        if (eyeIdx == std::string::npos) continue;
        size_t eyePos = eyeIdx + 7;
        size_t eyeLodPos = mshLower.rfind('_');
        if (eyeLodPos == std::string::npos || eyeLodPos <= eyePos) continue;
        std::string eyeBase = mshLower.substr(eyePos, eyeLodPos - eyePos);
        std::string eyeLodSuffix = mshLower.substr(eyeLodPos);
        if (eyeLodSuffix != lodSuffix) continue;
        if (eyeBase.find(baseName) == 0) {
            std::string remainder = eyeBase.substr(baseName.length());
            bool isMatch = remainder.empty();
            if (!isMatch && !remainder.empty()) {
                isMatch = std::all_of(remainder.begin(), remainder.end(), ::isdigit);
            }
            if (isMatch) {
                eyes.push_back({mesh.mshFile, mesh.mshName.empty() ? mesh.mshFile : mesh.mshName});
            }
        }
    }
    return eyes;
}
static std::vector<uint8_t> readFromCache(AppState& state, const std::string& name, const std::string& ext) {
    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
    if (ext == ".msh") {
        auto it = state.meshCache.find(nameLower);
        if (it != state.meshCache.end()) return it->second;
    } else if (ext == ".mmh") {
        auto it = state.mmhCache.find(nameLower);
        if (it != state.mmhCache.end()) return it->second;
    } else if (ext == ".mao") {
        auto it = state.maoCache.find(nameLower);
        if (it != state.maoCache.end()) return it->second;
    } else if (ext == ".dds") {
        auto it = state.textureCache.find(nameLower);
        if (it != state.textureCache.end()) return it->second;
    }
    const auto& erfs = (ext == ".msh" || ext == ".mmh") ? state.modelErfs :
                       (ext == ".mao") ? state.materialErfs : state.textureErfs;
    for (const auto& erf : erfs) {
        for (const auto& entry : erf->entries()) {
            std::string entryLower = entry.name;
            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
            if (entryLower == nameLower) {
                return erf->readEntry(entry);
            }
        }
    }
    return {};
}
static std::vector<uint8_t> readFromErfs(const std::vector<std::unique_ptr<ERFFile>>& erfs, const std::string& name) {
    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
    for (const auto& erf : erfs) {
        for (const auto& entry : erf->entries()) {
            std::string entryLower = entry.name;
            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
            if (entryLower == nameLower) {
                return erf->readEntry(entry);
            }
        }
    }
    return {};
}
static uint32_t loadTexByNameCached(AppState& state, const std::string& texName,
                              std::vector<uint8_t>* rgbaOut = nullptr, int* wOut = nullptr, int* hOut = nullptr) {
    if (texName.empty()) return 0;
    std::string texNameLower = texName;
    std::transform(texNameLower.begin(), texNameLower.end(), texNameLower.begin(), ::tolower);
    std::string texKey = texNameLower;
    if (texKey.size() < 4 || texKey.substr(texKey.size() - 4) != ".dds") {
        texKey += ".dds";
    }
    auto it = state.textureCache.find(texKey);
    if (it != state.textureCache.end() && !it->second.empty()) {
        if (rgbaOut && wOut && hOut) {
            decodeDDSToRGBA(it->second, *rgbaOut, *wOut, *hOut);
        }
        return loadDDSTexture(it->second);
    }
    it = state.textureCache.find(texNameLower);
    if (it != state.textureCache.end() && !it->second.empty()) {
        if (rgbaOut && wOut && hOut) {
            decodeDDSToRGBA(it->second, *rgbaOut, *wOut, *hOut);
        }
        return loadDDSTexture(it->second);
    }
    for (const auto& erf : state.textureErfs) {
        for (const auto& entry : erf->entries()) {
            std::string entryLower = entry.name;
            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
            if (entryLower == texKey || entryLower == texNameLower) {
                std::vector<uint8_t> texData = erf->readEntry(entry);
                if (!texData.empty()) {
                    if (rgbaOut && wOut && hOut) {
                        decodeDDSToRGBA(texData, *rgbaOut, *wOut, *hOut);
                    }
                    return loadDDSTexture(texData);
                }
            }
        }
    }
    return 0;
}
static uint32_t loadTexByName(AppState& state, const std::string& texName,
                              std::vector<uint8_t>* rgbaOut = nullptr, int* wOut = nullptr, int* hOut = nullptr) {
    if (texName.empty()) return 0;
    std::string texNameLower = texName;
    std::transform(texNameLower.begin(), texNameLower.end(), texNameLower.begin(), ::tolower);
    for (const auto& erf : state.textureErfs) {
        for (const auto& entry : erf->entries()) {
            std::string entryLower = entry.name;
            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
            if (entryLower == texNameLower) {
                std::vector<uint8_t> texData = erf->readEntry(entry);
                if (!texData.empty()) {
                    if (rgbaOut && wOut && hOut) {
                        decodeDDSToRGBA(texData, *rgbaOut, *wOut, *hOut);
                    }
                    return loadDDSTexture(texData);
                }
            }
        }
    }
    return 0;
}
static std::vector<uint8_t> loadTextureData(AppState& state, const std::string& texName) {
    if (texName.empty()) return {};
    std::string texNameLower = texName;
    std::transform(texNameLower.begin(), texNameLower.end(), texNameLower.begin(), ::tolower);
    std::string texKey = texNameLower;
    if (texKey.size() < 4 || texKey.substr(texKey.size() - 4) != ".dds") {
        texKey += ".dds";
    }
    auto it = state.textureCache.find(texKey);
    if (it != state.textureCache.end() && !it->second.empty()) {
        return it->second;
    }
    it = state.textureCache.find(texNameLower);
    if (it != state.textureCache.end() && !it->second.empty()) {
        return it->second;
    }
    for (const auto& erf : state.textureErfs) {
        for (const auto& entry : erf->entries()) {
            std::string entryLower = entry.name;
            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
            if (entryLower == texKey || entryLower == texNameLower) {
                return erf->readEntry(entry);
            }
        }
    }
    return {};
}
static void loadAndMergeHead(AppState& state, const std::string& headMshFile) {
    if (!state.hasModel) {
        return;
    }
    std::string headLower = headMshFile;
    std::transform(headLower.begin(), headLower.end(), headLower.begin(), ::tolower);
    std::vector<uint8_t> mshData = readFromErfs(state.modelErfs, headMshFile);
    if (mshData.empty()) {
        for (const auto& erfPath : state.erfFiles) {
            ERFFile erf;
            if (erf.open(erfPath)) {
                for (const auto& entry : erf.entries()) {
                    std::string entryLower = entry.name;
                    std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
                    if (entryLower == headLower) {
                        mshData = erf.readEntry(entry);
                        break;
                    }
                }
            }
            if (!mshData.empty()) break;
        }
    }
    if (mshData.empty()) {
        return;
    }
    Model headModel;
    if (!loadMSH(mshData, headModel)) {
        return;
    }
    std::string baseName = headMshFile;
    size_t dotPos = baseName.rfind('.');
    if (dotPos != std::string::npos) baseName = baseName.substr(0, dotPos);
    std::vector<std::string> mmhCandidates = {baseName + ".mmh", baseName + "a.mmh"};
    size_t lastUnderscore = baseName.find_last_of('_');
    if (lastUnderscore != std::string::npos) {
        std::string variantA = baseName;
        variantA.insert(lastUnderscore, "a");
        mmhCandidates.push_back(variantA + ".mmh");
    }
    for (const auto& candidate : mmhCandidates) {
        std::vector<uint8_t> mmhData = readFromErfs(state.modelErfs, candidate);
        if (!mmhData.empty()) {
            loadMMH(mmhData, headModel);
            break;
        }
    }
    std::set<std::string> headMaterialNames;
    for (const auto& mesh : headModel.meshes) {
        if (!mesh.materialName.empty()) {
            headMaterialNames.insert(mesh.materialName);
        }
    }
    size_t matStartIdx = state.currentModel.materials.size();
    for (const std::string& matName : headMaterialNames) {
        int existingIdx = state.currentModel.findMaterial(matName);
        if (existingIdx >= 0) continue;
        std::vector<uint8_t> maoData = readFromErfs(state.materialErfs, matName + ".mao");
        if (!maoData.empty()) {
            std::string maoContent(maoData.begin(), maoData.end());
            Material mat = parseMAO(maoContent, matName);
            mat.maoSource = matName + ".mao";
            mat.maoContent = maoContent;
            state.currentModel.materials.push_back(mat);
        } else {
            Material mat;
            mat.name = matName;
            state.currentModel.materials.push_back(mat);
        }
    }
    for (size_t i = matStartIdx; i < state.currentModel.materials.size(); i++) {
        auto& mat = state.currentModel.materials[i];
        if (!mat.diffuseMap.empty() && mat.diffuseTexId == 0) {
            mat.diffuseTexId = loadTexByName(state, mat.diffuseMap, &mat.diffuseData, &mat.diffuseWidth, &mat.diffuseHeight);
        }
        if (!mat.normalMap.empty() && mat.normalTexId == 0) mat.normalTexId = loadTexByName(state, mat.normalMap);
        if (!mat.specularMap.empty() && mat.specularTexId == 0) mat.specularTexId = loadTexByName(state, mat.specularMap);
        if (!mat.tintMap.empty() && mat.tintTexId == 0) mat.tintTexId = loadTexByName(state, mat.tintMap);
    }
    for (auto& mesh : headModel.meshes) {
        if (!mesh.materialName.empty()) {
            mesh.materialIndex = state.currentModel.findMaterial(mesh.materialName);
        }
        std::vector<int> newBonesUsed;
        for (int headBoneIdx : mesh.bonesUsed) {
            if (headBoneIdx >= 0 && headBoneIdx < (int)headModel.skeleton.bones.size()) {
                const std::string& boneName = headModel.skeleton.bones[headBoneIdx].name;
                int bodyBoneIdx = state.currentModel.skeleton.findBone(boneName);
                if (bodyBoneIdx >= 0) {
                    newBonesUsed.push_back(bodyBoneIdx);
                } else {
                    newBonesUsed.push_back(0);
                }
            } else {
                newBonesUsed.push_back(0);
            }
        }
        mesh.bonesUsed = newBonesUsed;
        mesh.skinningCacheBuilt = false;
        state.currentModel.meshes.push_back(std::move(mesh));
    }
    state.renderSettings.initMeshVisibility(state.currentModel.meshes.size());
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
                state.filteredEntryIndices.clear();
                state.lastContentFilter.clear();
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
        std::string currentFilter = state.contentFilter;
        if (currentFilter != state.lastContentFilter || state.filteredEntryIndices.empty()) {
            state.lastContentFilter = currentFilter;
            state.filteredEntryIndices.clear();
            state.filteredEntryIndices.reserve(state.mergedEntries.size());
            std::string filterLower = currentFilter;
            std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
            for (int i = 0; i < (int)state.mergedEntries.size(); i++) {
                if (filterLower.empty()) {
                    state.filteredEntryIndices.push_back(i);
                } else {
                    std::string nameLower = state.mergedEntries[i].name;
                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                    if (nameLower.find(filterLower) != std::string::npos) {
                        state.filteredEntryIndices.push_back(i);
                    }
                }
            }
        }
        ImGui::BeginChild("EntryList", ImVec2(0, 0), true);
        ImGuiListClipper entryClipper;
        entryClipper.Begin(static_cast<int>(state.filteredEntryIndices.size()));
        while (entryClipper.Step()) {
            for (int fi = entryClipper.DisplayStart; fi < entryClipper.DisplayEnd; fi++) {
                int i = state.filteredEntryIndices[fi];
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
static void buildCharacterLists(AppState& state) {
    auto& cd = state.charDesigner;
    if (cd.listsBuilt && !cd.currentPrefix.empty()) return;
    loadMeshDatabase(state);
    std::string prefix;
    switch (cd.race) {
        case 0: prefix = cd.isMale ? "hm_" : "hf_"; break;
        case 1: prefix = cd.isMale ? "em_" : "ef_"; break;
        case 2: prefix = cd.isMale ? "dm_" : "df_"; break;
    }
    if (prefix == cd.currentPrefix && cd.listsBuilt) return;
    cd.currentPrefix = prefix;
    cd.heads.clear();
    cd.hairs.clear();
    cd.armors.clear();
    cd.boots.clear();
    cd.gloves.clear();
    cd.helmets.clear();
    cd.robes.clear();
    std::pair<std::string, std::string> baldHair;
    bool foundBald = false;
    for (const auto& mesh : state.meshBrowser.allMeshes) {
        if (mesh.lod != 0) continue;
        std::string mshLower = mesh.mshFile;
        std::transform(mshLower.begin(), mshLower.end(), mshLower.begin(), ::tolower);
        if (mshLower.find(prefix) != 0) continue;
        size_t pos1 = mshLower.find('_', prefix.length());
        if (pos1 == std::string::npos) continue;
        std::string type = mshLower.substr(prefix.length(), pos1 - prefix.length());
        std::string displayName = mesh.mshName.empty() ? mesh.mshFile : mesh.mshName;
        auto item = std::make_pair(mesh.mshFile, displayName);
        if (type == "uhm") cd.heads.push_back(item);
        else if (type == "har") {
            if (mshLower.find("_bld_") != std::string::npos) {
                baldHair = item;
                foundBald = true;
            } else {
                cd.hairs.push_back(item);
            }
        }
        else if (type == "arm") cd.armors.push_back(item);
        else if (type == "boo") cd.boots.push_back(item);
        else if (type == "glv") cd.gloves.push_back(item);
        else if (type == "hlf" || type == "hlh") cd.helmets.push_back(item);
        else if (type == "rob") cd.robes.push_back(item);
    }
    if (foundBald) {
        cd.hairs.insert(cd.hairs.begin(), baldHair);
    }
    cd.tattoos.clear();
    cd.tattoos.push_back(std::make_pair("", "None"));
    std::string tatPrefix;
    switch (cd.race) {
        case 0: tatPrefix = "uh_tat_"; break;
        case 1: tatPrefix = "ue_tat_"; break;
        case 2: tatPrefix = "ud_tat_"; break;
    }
    for (const auto& [texName, texData] : state.textureCache) {
        std::string nameLower = texName;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        if (nameLower.find("_tat_") != std::string::npos &&
            nameLower.find("_0t.dds") != std::string::npos) {
            std::string displayName = texName;
            size_t dotPos = displayName.rfind('.');
            if (dotPos != std::string::npos) displayName = displayName.substr(0, dotPos);
            cd.tattoos.push_back(std::make_pair(texName, displayName));
        }
    }
    cd.listsBuilt = true;
}
static Model* getOrLoadPart(AppState& state, const std::string& partFile) {
    auto& cd = state.charDesigner;
    std::string partLower = partFile;
    std::transform(partLower.begin(), partLower.end(), partLower.begin(), ::tolower);
    auto it = cd.partCache.find(partLower);
    if (it != cd.partCache.end()) {
        return &it->second;
    }
    std::vector<uint8_t> mshData = readFromCache(state, partLower, ".msh");
    if (mshData.empty()) {
        mshData = readFromErfs(state.modelErfs, partLower);
    }
    if (mshData.empty()) {
        return nullptr;
    }
    Model partModel;
    if (!loadMSH(mshData, partModel)) {
        return nullptr;
    }
    std::string baseName = partFile;
    size_t dotPos = baseName.rfind('.');
    if (dotPos != std::string::npos) baseName = baseName.substr(0, dotPos);
    std::vector<std::string> mmhCandidates = {baseName + ".mmh", baseName + "a.mmh"};
    size_t lastUnderscore = baseName.find_last_of('_');
    if (lastUnderscore != std::string::npos) {
        std::string variantA = baseName;
        variantA.insert(lastUnderscore, "a");
        mmhCandidates.push_back(variantA + ".mmh");
    }
    for (const auto& candidate : mmhCandidates) {
        std::vector<uint8_t> mmhData = readFromErfs(state.modelErfs, candidate);
        if (!mmhData.empty()) {
            loadMMH(mmhData, partModel);
            break;
        }
    }
    std::set<std::string> materialNames;
    for (const auto& mesh : partModel.meshes) {
        if (!mesh.materialName.empty()) {
            materialNames.insert(mesh.materialName);
        }
    }
    for (const std::string& matName : materialNames) {
        std::vector<uint8_t> maoData = readFromErfs(state.materialErfs, matName + ".mao");
        if (!maoData.empty()) {
            std::string maoContent(maoData.begin(), maoData.end());
            Material mat = parseMAO(maoContent, matName);
            mat.maoSource = matName + ".mao";
            mat.maoContent = maoContent;
            partModel.materials.push_back(mat);
        } else {
            Material mat;
            mat.name = matName;
            partModel.materials.push_back(mat);
        }
    }
    for (auto& mesh : partModel.meshes) {
        if (!mesh.materialName.empty()) {
            mesh.materialIndex = partModel.findMaterial(mesh.materialName);
        }
    }
    for (auto& mat : partModel.materials) {
        std::string matNameLower = mat.name;
        std::transform(matNameLower.begin(), matNameLower.end(), matNameLower.begin(), ::tolower);
        bool isHairMat = (matNameLower.find("har") != std::string::npos);
        if (!mat.diffuseMap.empty() && mat.diffuseTexId == 0) {
            if (isHairMat) {
                std::vector<uint8_t> texData = loadTextureData(state, mat.diffuseMap);
                if (!texData.empty()) {
                    mat.diffuseTexId = loadDDSTextureHair(texData);
                }
            } else {
                mat.diffuseTexId = loadTexByName(state, mat.diffuseMap, &mat.diffuseData, &mat.diffuseWidth, &mat.diffuseHeight);
            }
        }
        if (!mat.normalMap.empty() && mat.normalTexId == 0) {
            mat.normalTexId = loadTexByName(state, mat.normalMap);
        }
        if (!mat.specularMap.empty() && mat.specularTexId == 0) {
            mat.specularTexId = loadTexByName(state, mat.specularMap);
        }
        if (!mat.tintMap.empty() && mat.tintTexId == 0) {
            mat.tintTexId = loadTexByName(state, mat.tintMap);
        }
        if (!mat.ageDiffuseMap.empty() && mat.ageDiffuseTexId == 0) {
            mat.ageDiffuseTexId = loadTexByName(state, mat.ageDiffuseMap);
        }
        if (!mat.ageNormalMap.empty() && mat.ageNormalTexId == 0) {
            mat.ageNormalTexId = loadTexByName(state, mat.ageNormalMap);
        }
        if (!mat.tattooMap.empty() && mat.tattooTexId == 0) {
            mat.tattooTexId = loadTexByName(state, mat.tattooMap);
        }
    }
    auto result = cd.partCache.emplace(partLower, std::move(partModel));
    return &result.first->second;
}
static void loadCharacterModel(AppState& state) {
    auto& cd = state.charDesigner;
    if (!cd.needsRebuild) return;
    cd.needsRebuild = false;
    if (!state.modelErfsLoaded || !state.materialErfsLoaded || !state.textureErfsLoaded) {
        state.statusMessage = "ERFs not loaded - please select game folder first";
        return;
    }
    buildCharacterLists(state);
    Animation savedAnim = state.currentAnim;
    bool wasPlaying = state.animPlaying;
    float savedTime = state.animTime;
    int savedAnimIdx = state.selectedAnimIndex;
    state.currentModel = Model();
    state.hasModel = false;
    state.basePoseBones.clear();
    std::string prefix = cd.currentPrefix;
    std::vector<std::string> partsToLoad;
    if (cd.selectedRobe >= 0 && cd.selectedRobe < (int)cd.robes.size()) {
        partsToLoad.push_back(cd.robes[cd.selectedRobe].first);
    } else if (cd.selectedArmor >= 0 && cd.selectedArmor < (int)cd.armors.size()) {
        partsToLoad.push_back(cd.armors[cd.selectedArmor].first);
    }
    if (cd.selectedBoots >= 0 && cd.selectedBoots < (int)cd.boots.size()) {
        partsToLoad.push_back(cd.boots[cd.selectedBoots].first);
    }
    if (cd.selectedGloves >= 0 && cd.selectedGloves < (int)cd.gloves.size()) {
        partsToLoad.push_back(cd.gloves[cd.selectedGloves].first);
    }
    if (cd.selectedHead >= 0 && cd.selectedHead < (int)cd.heads.size()) {
        partsToLoad.push_back(cd.heads[cd.selectedHead].first);
    } else {
        partsToLoad.push_back(prefix + "uhm_bas_0.msh");
    }
    partsToLoad.push_back(prefix + "uem_bas_0.msh");
    partsToLoad.push_back(prefix + "ulm_bas_0.msh");
    bool hasHelmet = (cd.selectedHelmet >= 0 && cd.selectedHelmet < (int)cd.helmets.size());
    if (!hasHelmet && !cd.hairs.empty()) {
        std::string baldMesh = cd.hairs[0].first;
        partsToLoad.push_back(baldMesh);
        if (cd.selectedHair > 0 && cd.selectedHair < (int)cd.hairs.size()) {
            partsToLoad.push_back(cd.hairs[cd.selectedHair].first);
        }
    }
    if (hasHelmet) {
        partsToLoad.push_back(cd.helmets[cd.selectedHelmet].first);
    }
    bool firstPart = true;
    for (const auto& partFile : partsToLoad) {
        Model* partModel = getOrLoadPart(state, partFile);
        if (!partModel) continue;
        if (firstPart) {
            state.currentModel.skeleton = partModel->skeleton;
            state.currentModel.boneIndexArray = partModel->boneIndexArray;
            state.currentModel.name = "Character";
            state.hasModel = true;
            firstPart = false;
            for (const auto& mesh : partModel->meshes) {
                Mesh meshCopy = mesh;
                meshCopy.skinningCacheBuilt = false;
                state.currentModel.meshes.push_back(std::move(meshCopy));
            }
            for (const auto& mat : partModel->materials) {
                state.currentModel.materials.push_back(mat);
            }
        } else {
            for (const auto& mesh : partModel->meshes) {
                Mesh meshCopy = mesh;
                std::vector<int> newBonesUsed;
                for (int partBoneIdx : meshCopy.bonesUsed) {
                    if (partBoneIdx >= 0 && partBoneIdx < (int)partModel->skeleton.bones.size()) {
                        const std::string& boneName = partModel->skeleton.bones[partBoneIdx].name;
                        int mainBoneIdx = state.currentModel.skeleton.findBone(boneName);
                        newBonesUsed.push_back(mainBoneIdx >= 0 ? mainBoneIdx : 0);
                    } else {
                        newBonesUsed.push_back(0);
                    }
                }
                meshCopy.bonesUsed = newBonesUsed;
                meshCopy.skinningCacheBuilt = false;
                state.currentModel.meshes.push_back(std::move(meshCopy));
            }
            for (const auto& mat : partModel->materials) {
                bool found = false;
                for (const auto& existing : state.currentModel.materials) {
                    if (existing.name == mat.name) { found = true; break; }
                }
                if (!found) {
                    state.currentModel.materials.push_back(mat);
                }
            }
        }
    }
    if (!state.hasModel) {
        state.statusMessage = "Failed to load any character parts";
        return;
    }
    for (auto& mesh : state.currentModel.meshes) {
        if (!mesh.materialName.empty()) {
            mesh.materialIndex = state.currentModel.findMaterial(mesh.materialName);
        }
    }
    state.renderSettings.initMeshVisibility(state.currentModel.meshes.size());
    bool hasBaldSelected = (cd.selectedHair == 0);
    if (!hasBaldSelected) {
        for (size_t i = 0; i < state.currentModel.meshes.size(); i++) {
            std::string meshName = state.currentModel.meshes[i].name;
            std::transform(meshName.begin(), meshName.end(), meshName.begin(), ::tolower);
            if (meshName.find("hairm1") != std::string::npos && meshName.find("bld") == std::string::npos) {
                state.renderSettings.meshVisible[i] = 0;
            }
        }
    }
    state.basePoseBones = state.currentModel.skeleton.bones;
    if (cd.animsLoaded && savedAnimIdx >= 0 && !savedAnim.tracks.empty()) {
        state.currentAnim = savedAnim;
        state.animPlaying = wasPlaying;
        state.animTime = savedTime;
        state.selectedAnimIndex = savedAnimIdx;
        auto normalize = [](const std::string& s) {
            std::string result;
            for (char c : s) {
                if (c != '_') result += std::tolower(c);
            }
            return result;
        };
        for (auto& track : state.currentAnim.tracks) {
            track.boneIndex = state.currentModel.skeleton.findBone(track.boneName);
            if (track.boneIndex < 0) {
                std::string trackNorm = normalize(track.boneName);
                for (size_t bi = 0; bi < state.currentModel.skeleton.bones.size(); bi++) {
                    std::string boneNorm = normalize(state.currentModel.skeleton.bones[bi].name);
                    if (trackNorm == boneNorm) {
                        track.boneIndex = (int)bi;
                        break;
                    }
                }
            }
        }
    }
    loadMeshDatabase(state);
    if (!cd.animsLoaded) {
        state.availableAnimFiles.clear();
        state.currentModelAnimations.clear();
        state.selectedAnimIndex = -1;
        std::string animPrefix = cd.isMale ? "mh" : "fh";
        std::set<std::string> foundNames;
        for (const auto& erfPath : state.erfFiles) {
            ERFFile erf;
            if (erf.open(erfPath)) {
                for (const auto& entry : erf.entries()) {
                    std::string entryLower = entry.name;
                    std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
                    if (entryLower.size() > 4 && entryLower.substr(entryLower.size() - 4) == ".ani") {
                        if ((entryLower.find(animPrefix) == 0 || entryLower.find("mh") == 0) &&
                            foundNames.find(entryLower) == foundNames.end()) {
                            foundNames.insert(entryLower);
                            state.availableAnimFiles.push_back(std::make_pair(entry.name, erfPath));
                        }
                    }
                }
            }
        }
        if (!state.availableAnimFiles.empty()) {
            int defaultIdx = -1;
            std::string foundName;
            std::string defaultAnim = cd.isMale ? "mh_m.p.ani" : "fh_m.p.ani";
            for (int i = 0; i < (int)state.availableAnimFiles.size(); i++) {
                std::string nameLower = state.availableAnimFiles[i].first;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                if (nameLower == defaultAnim) {
                    defaultIdx = i;
                    foundName = state.availableAnimFiles[i].first;
                    break;
                }
            }
            if (defaultIdx < 0) {
                for (int i = 0; i < (int)state.availableAnimFiles.size(); i++) {
                    std::string nameLower = state.availableAnimFiles[i].first;
                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                    if (nameLower == "mh_m.p.ani") {
                        defaultIdx = i;
                        foundName = state.availableAnimFiles[i].first;
                        break;
                    }
                }
            }
            if (defaultIdx < 0) {
                for (int i = 0; i < (int)state.availableAnimFiles.size(); i++) {
                    std::string nameLower = state.availableAnimFiles[i].first;
                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                    if (nameLower.find("std_idle") != std::string::npos ||
                        nameLower.find("std.idle") != std::string::npos) {
                        defaultIdx = i;
                        foundName = state.availableAnimFiles[i].first;
                        break;
                    }
                }
            }
            if (defaultIdx < 0 && !state.availableAnimFiles.empty()) {
                defaultIdx = 0;
                foundName = state.availableAnimFiles[0].first;
            }
            if (defaultIdx >= 0) {
                ERFFile animErf;
                if (animErf.open(state.availableAnimFiles[defaultIdx].second)) {
                    for (const auto& entry : animErf.entries()) {
                        if (entry.name == state.availableAnimFiles[defaultIdx].first) {
                            std::vector<uint8_t> animData = animErf.readEntry(entry);
                            if (!animData.empty()) {
                                state.currentAnim = loadANI(animData, entry.name);
                                auto normalize = [](const std::string& s) {
                                    std::string result;
                                    for (char c : s) {
                                        if (c != '_') result += std::tolower(c);
                                    }
                                    return result;
                                };
                                for (auto& track : state.currentAnim.tracks) {
                                    track.boneIndex = state.currentModel.skeleton.findBone(track.boneName);
                                    if (track.boneIndex < 0) {
                                        std::string trackNorm = normalize(track.boneName);
                                        for (size_t bi = 0; bi < state.currentModel.skeleton.bones.size(); bi++) {
                                            std::string boneNorm = normalize(state.currentModel.skeleton.bones[bi].name);
                                            if (trackNorm == boneNorm) {
                                                track.boneIndex = (int)bi;
                                                break;
                                            }
                                        }
                                    }
                                }
                                state.selectedAnimIndex = defaultIdx;
                                state.animPlaying = true;
                                state.animLoop = true;
                                state.animTime = 0.0f;
                            }
                            break;
                        }
                    }
                }
            }
        }
        cd.animsLoaded = true;
    }
    static bool firstLoad = true;
    if (firstLoad) {
        float minZ = 1e9f, maxZ = -1e9f;
        for (const auto& mesh : state.currentModel.meshes) {
            for (const auto& v : mesh.vertices) {
                minZ = std::min(minZ, v.z);
                maxZ = std::max(maxZ, v.z);
            }
        }
        float height = maxZ - minZ;
        state.camera.lookAt(0, 0, (minZ + maxZ) * 0.5f, height * 1.5f);
        firstLoad = false;
    }
    state.statusMessage = "Character: " + std::to_string(state.currentModel.meshes.size()) + " meshes, " +
                          std::to_string(state.currentModel.materials.size()) + " materials";
}
static void drawCharacterDesigner(AppState& state, ImGuiIO& io) {
    auto& cd = state.charDesigner;
    ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_FirstUseEver);
    ImGui::Begin("Character Designer", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::Text("Race:");
    ImGui::SameLine();
    bool raceChanged = false;
    if (ImGui::RadioButton("Human", cd.race == 0)) { cd.race = 0; raceChanged = true; }
    ImGui::SameLine();
    if (ImGui::RadioButton("Elf", cd.race == 1)) { cd.race = 1; raceChanged = true; }
    ImGui::SameLine();
    if (ImGui::RadioButton("Dwarf", cd.race == 2)) { cd.race = 2; raceChanged = true; }
    ImGui::Text("Gender:");
    ImGui::SameLine();
    bool genderChanged = false;
    if (ImGui::RadioButton("Male", cd.isMale)) { cd.isMale = true; genderChanged = true; }
    ImGui::SameLine();
    if (ImGui::RadioButton("Female", !cd.isMale)) { cd.isMale = false; genderChanged = true; }
    if (raceChanged || genderChanged) {
        cd.listsBuilt = false;
        cd.needsRebuild = true;
        cd.animsLoaded = false;
        cd.partCache.clear();
        cd.selectedHead = 0;
        cd.selectedHair = 0;
        cd.selectedArmor = 0;
        cd.selectedBoots = 0;
        cd.selectedGloves = 0;
        cd.selectedHelmet = -1;
    }
    buildCharacterLists(state);
    ImGui::Separator();
    if (ImGui::BeginTabBar("EquipTabs")) {
        if (ImGui::BeginTabItem("Head")) {
            ImGui::Text("Face:");
            for (int i = 0; i < (int)cd.heads.size(); i++) {
                bool selected = (cd.selectedHead == i);
                if (ImGui::Selectable(cd.heads[i].second.c_str(), selected)) {
                    cd.selectedHead = i;
                    cd.needsRebuild = true;
                }
            }
            ImGui::Separator();
            ImGui::Text("Hair:");
            if (!cd.hairs.empty()) {
                std::string currentHair = (cd.selectedHair >= 0 && cd.selectedHair < (int)cd.hairs.size())
                    ? cd.hairs[cd.selectedHair].second : "None";
                int hairIdx = cd.selectedHair;
                if (ImGui::SliderInt("##hair", &hairIdx, 0, (int)cd.hairs.size() - 1, currentHair.c_str())) {
                    cd.selectedHair = hairIdx;
                    cd.selectedHelmet = -1;
                    cd.needsRebuild = true;
                }
            }
            ImGui::Separator();
            ImGui::ColorEdit3("Hair Color", state.renderSettings.hairColor, ImGuiColorEditFlags_NoInputs);
            ImGui::Separator();
            ImGui::Text("Age:");
            if (ImGui::SliderFloat("##age", &cd.ageAmount, 0.0f, 1.0f, "%.2f")) {
                state.renderSettings.ageAmount = cd.ageAmount;
            }
            state.renderSettings.ageAmount = cd.ageAmount;
            ImGui::Separator();
            if (!cd.tattoos.empty()) {
                ImGui::Text("Tattoo:");
                std::string currentTattoo = (cd.selectedTattoo < 0) ? "None" :
                    (cd.selectedTattoo < (int)cd.tattoos.size() ? cd.tattoos[cd.selectedTattoo].second : "None");
                if (ImGui::BeginCombo("##tattoo", currentTattoo.c_str())) {
                    for (int i = 0; i < (int)cd.tattoos.size(); i++) {
                        bool selected = (cd.selectedTattoo == i) || (i == 0 && cd.selectedTattoo < 0);
                        if (ImGui::Selectable(cd.tattoos[i].second.c_str(), selected)) {
                            cd.selectedTattoo = (i == 0) ? -1 : i;
                            state.renderSettings.selectedTattoo = cd.selectedTattoo;
                            cd.needsRebuild = true;
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Armor")) {
            ImGui::TextDisabled("Body Armor:");
            for (int i = 0; i < (int)cd.armors.size(); i++) {
                bool selected = (cd.selectedArmor == i && cd.selectedRobe < 0);
                if (ImGui::Selectable(cd.armors[i].second.c_str(), selected)) {
                    cd.selectedArmor = i;
                    cd.selectedRobe = -1;
                    cd.needsRebuild = true;
                }
            }
            if (!cd.robes.empty()) {
                ImGui::Separator();
                ImGui::TextDisabled("Robes:");
                for (int i = 0; i < (int)cd.robes.size(); i++) {
                    bool selected = (cd.selectedRobe == i);
                    if (ImGui::Selectable(cd.robes[i].second.c_str(), selected)) {
                        cd.selectedRobe = i;
                        cd.needsRebuild = true;
                    }
                }
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Boots")) {
            for (int i = 0; i < (int)cd.boots.size(); i++) {
                bool selected = (cd.selectedBoots == i);
                if (ImGui::Selectable(cd.boots[i].second.c_str(), selected)) {
                    cd.selectedBoots = i;
                    cd.needsRebuild = true;
                }
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Gloves")) {
            for (int i = 0; i < (int)cd.gloves.size(); i++) {
                bool selected = (cd.selectedGloves == i);
                if (ImGui::Selectable(cd.gloves[i].second.c_str(), selected)) {
                    cd.selectedGloves = i;
                    cd.needsRebuild = true;
                }
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Helmet")) {
            bool noHelmet = (cd.selectedHelmet == -1);
            if (ImGui::Selectable("Remove Helmet", noHelmet)) {
                if (cd.selectedHelmet >= 0) {
                    cd.selectedHair = cd.rememberedHair;
                }
                cd.selectedHelmet = -1;
                cd.needsRebuild = true;
            }
            if (!cd.helmets.empty()) {
                ImGui::Separator();
                for (int i = 0; i < (int)cd.helmets.size(); i++) {
                    bool selected = (cd.selectedHelmet == i);
                    if (ImGui::Selectable(cd.helmets[i].second.c_str(), selected)) {
                        if (cd.selectedHelmet < 0) {
                            cd.rememberedHair = cd.selectedHair;
                        }
                        cd.selectedHelmet = i;
                        cd.needsRebuild = true;
                    }
                }
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Animations")) {
            if (state.availableAnimFiles.empty()) {
                ImGui::TextDisabled("No animations found");
                ImGui::TextDisabled("(Load armor to populate list)");
            } else {
                if (state.animPlaying && state.currentAnim.duration > 0) {
                    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Playing: %s", state.currentAnim.name.c_str());
                    ImGui::ProgressBar(state.animTime / state.currentAnim.duration);
                    if (ImGui::Button("Stop")) {
                        state.animPlaying = false;
                        state.animTime = 0.0f;
                        state.currentModel.skeleton.bones = state.basePoseBones;
                    }
                    ImGui::SameLine();
                    ImGui::SliderFloat("Speed", &state.animSpeed, 0.1f, 3.0f);
                    ImGui::Separator();
                }
                ImGui::Text("%d animations", (int)state.availableAnimFiles.size());
                ImGui::InputText("Filter", state.animFilter, sizeof(state.animFilter));
                ImGui::BeginChild("AnimList", ImVec2(0, 180), true);
                std::string filterLower = state.animFilter;
                std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
                for (int i = 0; i < (int)state.availableAnimFiles.size(); i++) {
                    const auto& animFile = state.availableAnimFiles[i];
                    std::string nameLower = animFile.first;
                    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                    if (!filterLower.empty() && nameLower.find(filterLower) == std::string::npos) {
                        continue;
                    }
                    bool selected = (state.selectedAnimIndex == i);
                    if (ImGui::Selectable(animFile.first.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                        state.selectedAnimIndex = i;
                        if (ImGui::IsMouseDoubleClicked(0)) {
                            ERFFile animErf;
                            if (animErf.open(animFile.second)) {
                                for (const auto& entry : animErf.entries()) {
                                    if (entry.name == animFile.first) {
                                        std::vector<uint8_t> animData = animErf.readEntry(entry);
                                        if (!animData.empty()) {
                                            state.currentAnim = loadANI(animData, animFile.first);
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
                ImGui::TextDisabled("Double-click to play animation");
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::Separator();
    ImGui::Checkbox("Show Skeleton", &state.renderSettings.showSkeleton);
    ImGui::End();
    ImGui::SetNextWindowSize(ImVec2(320, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("Materials & Textures", nullptr, ImGuiWindowFlags_NoCollapse);
    if (state.currentModel.materials.empty()) {
        ImGui::TextDisabled("No materials loaded");
    } else {
        ImGui::Text("%d materials", (int)state.currentModel.materials.size());
        ImGui::Separator();
        for (int i = 0; i < (int)state.currentModel.materials.size(); i++) {
            auto& mat = state.currentModel.materials[i];
            bool open = ImGui::TreeNode((void*)(intptr_t)i, "%s", mat.name.c_str());
            if (open) {
                if (!mat.maoSource.empty()) {
                    ImGui::TextDisabled("MAO: %s", mat.maoSource.c_str());
                }
                if (!mat.diffuseMap.empty()) {
                    ImGui::Text("Diffuse: %s", mat.diffuseMap.c_str());
                    if (mat.diffuseTexId != 0) {
                        ImGui::SameLine();
                        if (ImGui::SmallButton(("View##diff" + std::to_string(i)).c_str())) {
                            state.previewTextureId = mat.diffuseTexId;
                            state.previewTextureName = mat.diffuseMap;
                            state.showTexturePreview = true;
                        }
                        ImGui::Image((ImTextureID)(intptr_t)mat.diffuseTexId, ImVec2(64, 64));
                    }
                }
                if (!mat.normalMap.empty()) {
                    ImGui::Text("Normal: %s", mat.normalMap.c_str());
                    if (mat.normalTexId != 0) {
                        ImGui::SameLine();
                        if (ImGui::SmallButton(("View##norm" + std::to_string(i)).c_str())) {
                            state.previewTextureId = mat.normalTexId;
                            state.previewTextureName = mat.normalMap;
                            state.showTexturePreview = true;
                        }
                    }
                }
                if (!mat.specularMap.empty()) {
                    ImGui::Text("Specular: %s", mat.specularMap.c_str());
                    if (mat.specularTexId != 0) {
                        ImGui::SameLine();
                        if (ImGui::SmallButton(("View##spec" + std::to_string(i)).c_str())) {
                            state.previewTextureId = mat.specularTexId;
                            state.previewTextureName = mat.specularMap;
                            state.showTexturePreview = true;
                        }
                    }
                }
                if (!mat.tintMap.empty()) {
                    ImGui::Text("Tint: %s", mat.tintMap.c_str());
                    if (mat.tintTexId != 0) {
                        ImGui::SameLine();
                        if (ImGui::SmallButton(("View##tint" + std::to_string(i)).c_str())) {
                            state.previewTextureId = mat.tintTexId;
                            state.previewTextureName = mat.tintMap;
                            state.showTexturePreview = true;
                        }
                    }
                }
                if (!mat.ageDiffuseMap.empty()) {
                    ImGui::Text("Age Diffuse: %s", mat.ageDiffuseMap.c_str());
                    if (mat.ageDiffuseTexId != 0) {
                        ImGui::SameLine();
                        if (ImGui::SmallButton(("View##aged" + std::to_string(i)).c_str())) {
                            state.previewTextureId = mat.ageDiffuseTexId;
                            state.previewTextureName = mat.ageDiffuseMap;
                            state.showTexturePreview = true;
                        }
                    }
                }
                if (!mat.ageNormalMap.empty()) {
                    ImGui::Text("Age Normal: %s", mat.ageNormalMap.c_str());
                    if (mat.ageNormalTexId != 0) {
                        ImGui::SameLine();
                        if (ImGui::SmallButton(("View##agen" + std::to_string(i)).c_str())) {
                            state.previewTextureId = mat.ageNormalTexId;
                            state.previewTextureName = mat.ageNormalMap;
                            state.showTexturePreview = true;
                        }
                    }
                }
                if (!mat.tattooMap.empty()) {
                    ImGui::Text("Tattoo: %s", mat.tattooMap.c_str());
                    if (mat.tattooTexId != 0) {
                        ImGui::SameLine();
                        if (ImGui::SmallButton(("View##tat" + std::to_string(i)).c_str())) {
                            state.previewTextureId = mat.tattooTexId;
                            state.previewTextureName = mat.tattooMap;
                            state.showTexturePreview = true;
                        }
                    }
                }
                ImGui::TreePop();
            }
        }
    }
    ImGui::End();
    if (state.animPlaying && state.currentAnim.duration > 0) {
        state.animTime += io.DeltaTime * state.animSpeed;
        if (state.animTime > state.currentAnim.duration) state.animTime = 0.0f;
    }
    if (cd.needsRebuild && state.modelErfsLoaded) {
        loadCharacterModel(state);
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
    if (state.isPreloading) {
        ImGui::SetCursorPos(ImVec2(centerX - 150, centerY + 70));
        ImGui::ProgressBar(state.preloadProgress, ImVec2(300, 20));
        ImGui::SetCursorPos(ImVec2(centerX - 150, centerY + 95));
        ImGui::TextWrapped("%s", state.preloadStatus.c_str());
    }
    ImGui::End();
}
static void preloadErfs(AppState& state) {
    state.isPreloading = true;
    state.preloadProgress = 0.0f;
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
        state.preloadStatus = "Caching: " + filename;
        ERFFile erf;
        if (!erf.open(erfPath)) {
            processed++;
            state.preloadProgress = (float)processed / (float)totalErfs;
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
        state.preloadProgress = (float)processed / (float)totalErfs;
    }
    state.modelErfsLoaded = true;
    state.materialErfsLoaded = true;
    state.textureErfsLoaded = true;
    state.cacheBuilt = true;
    state.preloadStatus = "Cached: " + std::to_string(state.meshCache.size()) + " meshes, " +
                          std::to_string(state.maoCache.size()) + " materials, " +
                          std::to_string(state.textureCache.size()) + " textures";
    state.isPreloading = false;
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
                preloadErfs(state);
                state.statusMessage = "Found " + std::to_string(state.filteredErfIndices.size()) + " ERF files";
                saveSettings(state);
                showSplash = false;
            }
            ImGuiFileDialog::Instance()->Close();
        }
        return;
    }
    if (ImGuiFileDialog::Instance()->Display("ChooseFolder", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            state.selectedFolder = ImGuiFileDialog::Instance()->GetCurrentPath();
            state.lastDialogPath = state.selectedFolder;
            state.erfFiles = scanForERFFiles(state.selectedFolder);
            filterEncryptedErfs(state);
            preloadErfs(state);
            state.selectedErfName.clear();
            state.mergedEntries.clear();
            state.filteredEntryIndices.clear();
            state.lastContentFilter.clear();
            state.selectedEntryIndex = -1;
            state.statusMessage = "Found " + std::to_string(state.erfsByName.size()) + " ERF files";
            saveSettings(state);
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGuiFileDialog::Instance()->Display("ExportCurrentGLB", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk() && state.hasModel) {
            std::string exportPath = ImGuiFileDialog::Instance()->GetFilePathName();
            std::vector<Animation> exportAnims;
            for (const auto& animFile : state.availableAnimFiles) {
                std::string animName = animFile.first;
                size_t dotPos = animName.rfind('.');
                if (dotPos != std::string::npos) animName = animName.substr(0, dotPos);
                bool found = state.currentModelAnimations.empty();
                if (!found) {
                    for (const auto& validAnim : state.currentModelAnimations) {
                        if (animName == validAnim) { found = true; break; }
                    }
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
                                auto normalize = [](const std::string& s) {
                                    std::string result;
                                    for (char c : s) {
                                        if (c != '_') result += std::tolower(c);
                                    }
                                    return result;
                                };
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
    if (ImGui::BeginMainMenuBar()) {
        ImGui::Text("Mode:");
        ImGui::SameLine();
        if (ImGui::RadioButton("Browser", state.mainTab == 0)) state.mainTab = 0;
        ImGui::SameLine();
        if (ImGui::RadioButton("Character Designer", state.mainTab == 1)) {
            if (state.mainTab != 1) {
                state.renderSettings.showSkeleton = false;
                state.hasModel = false;
                state.currentModel = Model();
                state.currentAnim = Animation();
                state.animPlaying = false;
            }
            state.mainTab = 1;
        }
        ImGui::SameLine();
        ImGui::Text(" | ");
        ImGui::SameLine();
        if (ImGui::BeginMenu("View")) {
            if (state.mainTab == 0) {
                ImGui::MenuItem("ERF Browser", nullptr, &state.showBrowser);
                ImGui::MenuItem("Mesh Browser", nullptr, &state.showMeshBrowser);
            }
            ImGui::MenuItem("Render Settings", nullptr, &state.showRenderSettings);
            ImGui::MenuItem("Animation", nullptr, &state.showAnimWindow);
            ImGui::EndMenu();
        }
        if (state.hasModel) {
            if (ImGui::Button("Export GLB")) {
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
            ImGui::SameLine();
            ImGui::Text("| %s | RMB: Look | WASD: Move", state.currentModel.name.c_str());
        }
        ImGui::EndMainMenuBar();
    }
    if (state.mainTab == 0) {
        if (state.showBrowser) drawBrowserWindow(state);
        if (state.showMeshBrowser) drawMeshBrowserWindow(state);
    } else {
        drawCharacterDesigner(state, io);
    }
    if (state.showRenderSettings) drawRenderSettingsWindow(state);
    if (state.showMaoViewer) drawMaoViewer(state);
    if (state.showTexturePreview && state.previewTextureId != 0) drawTexturePreview(state);
    if (state.showUvViewer && state.hasModel && state.selectedMeshForUv >= 0 && state.selectedMeshForUv < (int)state.currentModel.meshes.size()) drawUvViewer(state);
    if (state.showAnimWindow && state.hasModel) drawAnimWindow(state, io);
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
}