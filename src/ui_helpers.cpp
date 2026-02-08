#include "ui_internal.h"
#include "model_names_csv.h"

static size_t lastMeshCacheSize = 0;

void loadMeshDatabase(AppState& state) {
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

std::vector<std::pair<std::string, std::string>> findAssociatedHeads(AppState& state, const std::string& bodyMsh) {
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

std::vector<std::pair<std::string, std::string>> findAssociatedEyes(AppState& state, const std::string& bodyMsh) {
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

std::vector<uint8_t> readFromCache(AppState& state, const std::string& name, const std::string& ext) {
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

std::vector<uint8_t> readFromErfs(const std::vector<std::unique_ptr<ERFFile>>& erfs, const std::string& name) {
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

uint32_t loadTexByNameCached(AppState& state, const std::string& texName,
                             std::vector<uint8_t>* rgbaOut, int* wOut, int* hOut) {
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
        return createTextureFromDDS(it->second);
    }
    it = state.textureCache.find(texNameLower);
    if (it != state.textureCache.end() && !it->second.empty()) {
        if (rgbaOut && wOut && hOut) {
            decodeDDSToRGBA(it->second, *rgbaOut, *wOut, *hOut);
        }
        return createTextureFromDDS(it->second);
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
                    return createTextureFromDDS(texData);
                }
            }
        }
    }
    return 0;
}

uint32_t loadTexByName(AppState& state, const std::string& texName,
                       std::vector<uint8_t>* rgbaOut, int* wOut, int* hOut) {
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
                    return createTextureFromDDS(texData);
                }
            }
        }
    }
    return 0;
}

std::vector<uint8_t> loadTextureData(AppState& state, const std::string& texName) {
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

void drawVirtualList(int itemCount, std::function<void(int)> renderItem) {
    ImGuiListClipper clipper;
    clipper.Begin(itemCount);
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            renderItem(i);
        }
    }
}

void loadAndMergeHead(AppState& state, const std::string& headMshFile) {
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