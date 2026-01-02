#include "ui_internal.h"
#include <iostream>

void filterEncryptedErfs(AppState& state) {
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

static void buildMorphPresetList(AppState& state) {
    auto& cd = state.charDesigner;
    cd.availableMorphPresets.clear();
    cd.selectedMorphPreset = 0;
    cd.morphLoaded = false;
    cd.morphData = MorphData();

    std::string prefix;
    switch (cd.race) {
        case 0: prefix = cd.isMale ? "hm_" : "hf_"; break;
        case 1: prefix = cd.isMale ? "em_" : "ef_"; break;
        case 2: prefix = cd.isMale ? "dm_" : "df_"; break;
        default: return;
    }

    std::cout << "[MORPH] Scanning for ALL morph files with prefix: " << prefix << std::endl;


    for (const auto& erfPath : state.erfFiles) {
        std::string erfName = fs::path(erfPath).filename().string();
        std::string erfNameLower = erfName;
        std::transform(erfNameLower.begin(), erfNameLower.end(), erfNameLower.begin(), ::tolower);

        bool isFaceErf = (erfNameLower.find("face") != std::string::npos);
        bool isMorphErf = (erfNameLower.find("morph") != std::string::npos);
        bool isModuleErf = (erfNameLower.find("module") != std::string::npos);

        ERFFile erf;
        if (!erf.open(erfPath)) continue;

        for (const auto& entry : erf.entries()) {
            std::string entryLower = entry.name;
            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);

            if (entryLower.find(prefix) == 0 && entryLower.size() > 4 &&
                entryLower.substr(entryLower.size() - 4) == ".mor") {

                bool found = false;
                for (const auto& existing : cd.availableMorphPresets) {
                    if (existing.filename == entryLower) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    MorphPresetEntry preset;
                    preset.filename = entryLower;

                    std::string baseName = entryLower.substr(prefix.size());
                    size_t dotPos = baseName.rfind('.');
                    if (dotPos != std::string::npos) {
                        baseName = baseName.substr(0, dotPos);
                    }
                    preset.displayName = baseName;

                    if (baseName.find("pcc_b") == 0 && baseName.size() > 5) {
                        preset.presetNumber = std::atoi(baseName.c_str() + 5);
                    } else {
                        preset.presetNumber = 1000;
                    }

                    cd.availableMorphPresets.push_back(preset);
                }
            }
        }
    }

    std::sort(cd.availableMorphPresets.begin(), cd.availableMorphPresets.end(),
        [](const MorphPresetEntry& a, const MorphPresetEntry& b) {
            if (a.presetNumber < 1000 && b.presetNumber < 1000) {
                return a.presetNumber < b.presetNumber;
            }
            if (a.presetNumber < 1000) return true;
            if (b.presetNumber < 1000) return false;
            return a.displayName < b.displayName;
        });

    std::cout << "[MORPH] Found " << cd.availableMorphPresets.size() << " morph files for " << prefix << std::endl;
    for (const auto& p : cd.availableMorphPresets) {
        std::cout << "[MORPH]   - " << p.displayName << " (" << p.filename << ")" << std::endl;
    }

    for (size_t i = 0; i < cd.availableMorphPresets.size(); i++) {
        if (cd.availableMorphPresets[i].presetNumber >= 2 && cd.availableMorphPresets[i].presetNumber < 1000) {
            cd.selectedMorphPreset = (int)i;
            break;
        }
    }
}

static void loadSelectedMorphPreset(AppState& state) {
    auto& cd = state.charDesigner;
    cd.morphLoaded = false;
    cd.morphData = MorphData();

    if (cd.availableMorphPresets.empty() || cd.selectedMorphPreset < 0 ||
        cd.selectedMorphPreset >= (int)cd.availableMorphPresets.size()) {
        return;
    }

    const std::string& targetFile = cd.availableMorphPresets[cd.selectedMorphPreset].filename;
    std::cout << "[MORPH] Loading: " << targetFile << std::endl;

    for (const auto& erfPath : state.erfFiles) {
        ERFFile erf;
        if (!erf.open(erfPath)) continue;

        for (const auto& entry : erf.entries()) {
            std::string entryLower = entry.name;
            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);

            if (entryLower == targetFile) {
                std::vector<uint8_t> morphFileData = erf.readEntry(entry);

                if (!morphFileData.empty() && loadMOR(morphFileData, cd.morphData)) {
                    cd.morphLoaded = true;
                    cd.morphData.name = targetFile;
                    cd.morphData.displayName = cd.availableMorphPresets[cd.selectedMorphPreset].displayName;

                    debugPrintMorph(cd.morphData);
                    return;
                }
            }
        }
    }

    std::cout << "[MORPH] Failed to load: " << targetFile << std::endl;
}

static bool applyMorphToMesh(Mesh& mesh, const MorphMeshTarget* target, float amount,
                             const std::vector<Vertex>& baseVertices) {
    if (!target || target->vertices.empty()) return false;
    if (mesh.vertices.size() != target->vertices.size()) {
        std::cout << "[MORPH] Vertex count mismatch: mesh=" << mesh.vertices.size()
                  << " morph=" << target->vertices.size() << std::endl;
        return false;
    }
    if (baseVertices.size() != mesh.vertices.size()) {
        std::cout << "[MORPH] Base vertex count mismatch" << std::endl;
        return false;
    }

    float invAmount = 1.0f - amount;

    for (size_t i = 0; i < mesh.vertices.size(); i++) {
        mesh.vertices[i].x = baseVertices[i].x * invAmount + target->vertices[i].x * amount;
        mesh.vertices[i].y = baseVertices[i].y * invAmount + target->vertices[i].y * amount;
        mesh.vertices[i].z = baseVertices[i].z * invAmount + target->vertices[i].z * amount;
    }

    return true;
}

void buildCharacterLists(AppState& state) {
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
    cd.beards.clear();
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
        else if (type == "brd") cd.beards.push_back(item);
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

    buildMorphPresetList(state);
    if (!cd.availableMorphPresets.empty()) {
        loadSelectedMorphPreset(state);
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
    bool isHairPart = (partLower.find("_har_") != std::string::npos ||
                       partLower.find("_brd_") != std::string::npos);
    bool isBaldPart = (partLower.find("_bld") != std::string::npos ||
                       partLower.find("bld_") != std::string::npos);

    for (auto& mat : partModel.materials) {
        std::string matNameLower = mat.name;
        std::transform(matNameLower.begin(), matNameLower.end(), matNameLower.begin(), ::tolower);
        bool isBaldMat = (matNameLower.find("bld") != std::string::npos);
        bool isHairMat = !isBaldMat && !isBaldPart &&
                         (isHairPart ||
                          matNameLower.find("har") != std::string::npos ||
                          matNameLower.find("brd") != std::string::npos ||
                          matNameLower.find("beard") != std::string::npos);
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

void loadCharacterModel(AppState& state) {
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
    cd.baseHeadVertices.clear();
    cd.headMeshIndex = -1;

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
    if (!hasHelmet && cd.isMale && cd.selectedBeard >= 0 && cd.selectedBeard < (int)cd.beards.size()) {
        partsToLoad.push_back(cd.beards[cd.selectedBeard].first);
    }
    if (hasHelmet) {
        partsToLoad.push_back(cd.helmets[cd.selectedHelmet].first);
    }
    bool firstPart = true;
    for (const auto& partFile : partsToLoad) {
        Model* partModel = getOrLoadPart(state, partFile);
        if (!partModel) continue;

        std::string partLower = partFile;
        std::transform(partLower.begin(), partLower.end(), partLower.begin(), ::tolower);
        bool isHeadMesh = (partLower.find("uhm_bas") != std::string::npos);

        if (firstPart) {
            state.currentModel.skeleton = partModel->skeleton;
            state.currentModel.boneIndexArray = partModel->boneIndexArray;
            state.currentModel.name = "Character";
            state.hasModel = true;
            firstPart = false;
            for (const auto& mesh : partModel->meshes) {
                Mesh meshCopy = mesh;
                meshCopy.skinningCacheBuilt = false;

                if (isHeadMesh && cd.headMeshIndex < 0) {
                    cd.headMeshIndex = (int)state.currentModel.meshes.size();
                    cd.baseHeadVertices = mesh.vertices;
                }

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

                if (isHeadMesh && cd.headMeshIndex < 0) {
                    cd.headMeshIndex = (int)state.currentModel.meshes.size();
                    cd.baseHeadVertices = mesh.vertices;
                }

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

    if (cd.morphLoaded && cd.headMeshIndex >= 0 && !cd.baseHeadVertices.empty()) {
        const MorphMeshTarget* faceTarget = cd.morphData.getFaceTarget();
        if (faceTarget) {
            Mesh& headMesh = state.currentModel.meshes[cd.headMeshIndex];
            if (applyMorphToMesh(headMesh, faceTarget, cd.faceMorphAmount, cd.baseHeadVertices)) {
                std::cout << "[MORPH] Applied FaceM1 morph to head mesh ("
                          << faceTarget->vertices.size() << " vertices, amount="
                          << cd.faceMorphAmount << ")" << std::endl;
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
    state.statusMessage = "Character: " + std::to_string(state.currentModel.meshes.size()) + " meshes";
    if (cd.morphLoaded) {
        state.statusMessage += " | Morph: " + cd.morphData.name;
        const MorphMeshTarget* face = cd.morphData.getFaceTarget();
        if (face) {
            state.statusMessage += " (" + std::to_string(face->vertices.size()) + " verts)";
        }
    }
}

void drawCharacterDesigner(AppState& state, ImGuiIO& io) {
    auto& cd = state.charDesigner;
    ImGui::SetNextWindowSize(ImVec2(350, 550), ImGuiCond_FirstUseEver);
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
        cd.morphLoaded = false;
        cd.availableMorphPresets.clear();
        cd.selectedMorphPreset = 0;
        cd.baseHeadVertices.clear();
        cd.headMeshIndex = -1;
        cd.selectedHead = 0;
        cd.selectedHair = 0;
        cd.selectedBeard = -1;
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
            if (cd.isMale && !cd.beards.empty()) {
                ImGui::Separator();
                ImGui::Text("Beard:");
                std::string currentBeard = (cd.selectedBeard < 0) ? "None" :
                    (cd.selectedBeard < (int)cd.beards.size() ? cd.beards[cd.selectedBeard].second : "None");
                int beardIdx = cd.selectedBeard + 1;
                int maxBeard = (int)cd.beards.size();
                std::string beardLabel = (beardIdx == 0) ? "None" : currentBeard;
                if (ImGui::SliderInt("##beard", &beardIdx, 0, maxBeard, beardLabel.c_str())) {
                    cd.selectedBeard = beardIdx - 1;
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

        if (ImGui::BeginTabItem("Morph")) {
            if (cd.availableMorphPresets.empty()) {
                ImGui::TextWrapped("No morph presets found.");
                if (!cd.currentPrefix.empty()) {
                    ImGui::TextDisabled("Looking for: %s_pcc_b*.mor in face.erf", cd.currentPrefix.substr(0, 2).c_str());
                }
            } else {
                ImGui::Text("Face Preset:");
                std::string currentPreset = (cd.selectedMorphPreset >= 0 && cd.selectedMorphPreset < (int)cd.availableMorphPresets.size())
                    ? cd.availableMorphPresets[cd.selectedMorphPreset].displayName : "None";

                if (ImGui::BeginCombo("##morphpreset", currentPreset.c_str())) {
                    for (int i = 0; i < (int)cd.availableMorphPresets.size(); i++) {
                        bool selected = (cd.selectedMorphPreset == i);
                        std::string label = cd.availableMorphPresets[i].displayName + " (" + cd.availableMorphPresets[i].filename + ")";
                        if (ImGui::Selectable(label.c_str(), selected)) {
                            cd.selectedMorphPreset = i;
                            loadSelectedMorphPreset(state);
                            cd.needsRebuild = true;
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                ImGui::TextDisabled("%d presets available", (int)cd.availableMorphPresets.size());
                ImGui::Separator();

                if (cd.morphLoaded) {
                    ImGui::Text("Loaded: %s", cd.morphData.name.c_str());

                    if (!cd.morphData.meshTargets.empty()) {
                        for (const auto& target : cd.morphData.meshTargets) {
                            ImGui::BulletText("%s: %d vertices", target.name.c_str(), (int)target.vertices.size());
                        }
                    } else {
                        ImGui::TextDisabled("(No vertex data - parameters only)");
                    }

                    ImGui::Separator();

                    ImGui::Text("Morph Amount:");
                    if (ImGui::SliderFloat("##facemorph", &cd.faceMorphAmount, 0.0f, 1.0f, "%.2f")) {
                        cd.needsRebuild = true;
                    }
                    ImGui::TextDisabled("0 = base mesh, 1 = fully morphed");

                    if (cd.headMeshIndex >= 0 && !cd.baseHeadVertices.empty()) {
                        const MorphMeshTarget* face = cd.morphData.getFaceTarget();
                        if (face) {
                            bool match = (cd.baseHeadVertices.size() == face->vertices.size());
                            if (match) {
                                ImGui::TextColored(ImVec4(0,1,0,1), "Vertices: %d (match)", (int)face->vertices.size());
                            } else {
                                ImGui::TextColored(ImVec4(1,0.5f,0,1), "Mismatch: mesh=%d morph=%d",
                                    (int)cd.baseHeadVertices.size(), (int)face->vertices.size());
                            }
                        }
                    }
                } else {
                    ImGui::TextDisabled("Select a preset to load");
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

        ImGui::EndTabBar();
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