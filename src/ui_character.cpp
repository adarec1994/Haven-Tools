#include "ui_internal.h"
#include "tnt_loader.h"
#include <set>
#include <map>

static std::set<std::string> s_materialCache;
static bool s_materialCacheBuilt = false;

void buildMaterialCache(AppState& state, float startProgress, float endProgress) {
    if (s_materialCacheBuilt) {
        state.preloadProgress = endProgress;
        return;
    }

    int totalMats = 0;
    size_t total = state.erfFiles.size();
    size_t processed = 0;

    for (const auto& erfPath : state.erfFiles) {
        processed++;
        float ratio = (float)processed / (float)total;
        state.preloadProgress = startProgress + ratio * (endProgress - startProgress);

        std::string filename = fs::path(erfPath).filename().string();
        std::string filenameLower = filename;
        std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);

        if (filenameLower == "materialobjects.erf") {
            ERFFile erf;
            if (!erf.open(erfPath)) continue;


            for (const auto& entry : erf.entries()) {
                std::string entryLower = entry.name;
                std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);

                if (entryLower.size() > 4 && entryLower.substr(entryLower.size() - 4) == ".mao") {
                    std::string matName = entryLower.substr(0, entryLower.size() - 4);
                    s_materialCache.insert(matName);
                    totalMats++;
                }
            }
        }
    }

    s_materialCacheBuilt = true;
    state.preloadProgress = endProgress;
}

static bool materialExists(AppState& state, const std::string& matName) {
    buildMaterialCache(state);

    std::string matLower = matName;
    std::transform(matLower.begin(), matLower.end(), matLower.begin(), ::tolower);

    return s_materialCache.find(matLower) != s_materialCache.end();
}

static int getMaxStyleVariant(AppState& state, const std::string& baseName) {
    std::string baseLower = baseName;
    std::transform(baseLower.begin(), baseLower.end(), baseLower.begin(), ::tolower);

    int maxStyle = 0;
    for (char c = 'a'; c <= 'z'; c++) {
        std::string testName = baseLower + c;
        if (materialExists(state, testName)) {
            maxStyle = c - 'a';
        }
    }
    return maxStyle;
}

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

static void loadTintCache(AppState& state, float startProgress = 0.0f, float endProgress = 1.0f) {
    if (state.tintCacheLoaded && state.tintCache.getTintNames().size() > 0) {
        state.preloadProgress = endProgress;
        return;
    }

    state.tintCache.clear();
    int totalLoaded = 0;
    size_t total = state.erfFiles.size();
    size_t processed = 0;


    for (const auto& erfPath : state.erfFiles) {
        processed++;
        float ratio = (float)processed / (float)total;
        state.preloadProgress = startProgress + ratio * (endProgress - startProgress);

        ERFFile erf;
        if (!erf.open(erfPath)) continue;

        for (const auto& entry : erf.entries()) {
            std::string entryLower = entry.name;
            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);

            if (entryLower.size() > 4 && entryLower.substr(entryLower.size() - 4) == ".tnt") {
                std::string name = entryLower.substr(0, entryLower.size() - 4);
                if (state.tintCache.hasTint(name)) continue;

                std::vector<uint8_t> tntData = erf.readEntry(entry);
                if (tntData.empty()) continue;

                TintData tint;
                if (loadTNT(tntData, tint)) {
                    state.tintCache.addTint(name, tint);
                    totalLoaded++;

                    if (name.find("skn") != std::string::npos) {
                        TintColor c = tint.getPrimaryColor();
                    }
                }
            }
        }
    }

    state.tintCacheLoaded = true;
    state.preloadProgress = endProgress;
}

void preloadCharacterData(AppState& state) {
    buildMaterialCache(state, 0.0f, 0.4f);

    loadTintCache(state, 0.4f, 0.8f);
    state.preloadProgress = 0.8f;
    buildCharacterLists(state);

    state.preloadProgress = 1.0f;
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
                    cd.faceMorphAmount = 1.0f;

                    debugPrintMorph(cd.morphData);

                    if (!cd.morphData.hairModel.empty()) {
                        std::string hairLower = cd.morphData.hairModel;
                        std::transform(hairLower.begin(), hairLower.end(), hairLower.begin(), ::tolower);

                        std::string hairCode;
                        size_t harPos = hairLower.find("_har_");
                        if (harPos != std::string::npos && harPos + 8 < hairLower.size()) {
                            hairCode = hairLower.substr(harPos + 5, 3);
                        }

                        for (int i = 0; i < (int)cd.hairs.size(); i++) {
                            std::string hLower = cd.hairs[i].first;
                            std::transform(hLower.begin(), hLower.end(), hLower.begin(), ::tolower);

                            if (!hairCode.empty() && hLower.find("_" + hairCode) != std::string::npos) {
                                cd.selectedHair = i;
                                break;
                            }
                        }
                    }

                    if (!cd.morphData.beardModel.empty() && cd.isMale) {
                        std::string beardLower = cd.morphData.beardModel;
                        std::transform(beardLower.begin(), beardLower.end(), beardLower.begin(), ::tolower);

                        std::string beardCode;
                        size_t brdPos = beardLower.find("_brd_");
                        if (brdPos != std::string::npos && brdPos + 8 < beardLower.size()) {
                            beardCode = beardLower.substr(brdPos + 5, 3);
                        }

                        for (int i = 0; i < (int)cd.beards.size(); i++) {
                            std::string bLower = cd.beards[i].first;
                            std::transform(bLower.begin(), bLower.end(), bLower.begin(), ::tolower);

                            if (!beardCode.empty() && bLower.find("_" + beardCode) != std::string::npos) {
                                cd.selectedBeard = i;
                                break;
                            }
                        }
                    } else {
                        cd.selectedBeard = -1;
                    }

                    loadTintCache(state);

                    if (!cd.morphData.skinTexture.empty()) {
                        std::string skinTintName = cd.morphData.skinTexture;
                        std::transform(skinTintName.begin(), skinTintName.end(), skinTintName.begin(), ::tolower);

                        const TintData* skinTint = state.tintCache.getTint(skinTintName);
                        if (skinTint) {
                            TintColor color = skinTint->getPrimaryColor();
                            state.renderSettings.skinColor[0] = color.r;
                            state.renderSettings.skinColor[1] = color.g;
                            state.renderSettings.skinColor[2] = color.b;
                        }
                    }

                    if (!cd.morphData.hairTexture.empty()) {
                        std::string hairTintName = cd.morphData.hairTexture;
                        std::transform(hairTintName.begin(), hairTintName.end(), hairTintName.begin(), ::tolower);

                        const TintData* hairTint = state.tintCache.getTint(hairTintName);
                        if (hairTint) {
                            TintColor color = hairTint->getPrimaryColor();
                            state.renderSettings.hairColor[0] = color.r;
                            state.renderSettings.hairColor[1] = color.g;
                            state.renderSettings.hairColor[2] = color.b;
                        }
                    }

                    if (!cd.morphData.eyeTexture.empty()) {
                        std::string eyeTintName = cd.morphData.eyeTexture;
                        std::transform(eyeTintName.begin(), eyeTintName.end(), eyeTintName.begin(), ::tolower);

                        const TintData* eyeTint = state.tintCache.getTint(eyeTintName);
                        if (eyeTint) {
                            TintColor color = eyeTint->getPrimaryColor();
                            cd.eyeColor[0] = color.r;
                            cd.eyeColor[1] = color.g;
                            cd.eyeColor[2] = color.b;
                        }
                    }

                    if (!cd.morphData.lipsTint.empty()) {
                        std::string lipsTintName = cd.morphData.lipsTint;
                        std::transform(lipsTintName.begin(), lipsTintName.end(), lipsTintName.begin(), ::tolower);

                        const TintData* lipsTint = state.tintCache.getTint(lipsTintName);
                        if (lipsTint) {
                            TintColor color = lipsTint->getPrimaryColor();
                            cd.headTintZone1[0] = color.r;
                            cd.headTintZone1[1] = color.g;
                            cd.headTintZone1[2] = color.b;
                        }
                    } else {
                        cd.headTintZone1[0] = 1.0f;
                        cd.headTintZone1[1] = 1.0f;
                        cd.headTintZone1[2] = 1.0f;
                    }

                    if (!cd.morphData.eyeshadowTint.empty()) {
                        std::string eyesTintName = cd.morphData.eyeshadowTint;
                        std::transform(eyesTintName.begin(), eyesTintName.end(), eyesTintName.begin(), ::tolower);

                        const TintData* eyesTint = state.tintCache.getTint(eyesTintName);
                        if (eyesTint) {
                            TintColor color = eyesTint->getPrimaryColor();
                            cd.headTintZone2[0] = color.r;
                            cd.headTintZone2[1] = color.g;
                            cd.headTintZone2[2] = color.b;
                        }
                    } else {
                        cd.headTintZone2[0] = 1.0f;
                        cd.headTintZone2[1] = 1.0f;
                        cd.headTintZone2[2] = 1.0f;
                    }

                    if (!cd.morphData.blushTint.empty()) {
                        std::string blushTintName = cd.morphData.blushTint;
                        std::transform(blushTintName.begin(), blushTintName.end(), blushTintName.begin(), ::tolower);

                        const TintData* blushTint = state.tintCache.getTint(blushTintName);
                        if (blushTint) {
                            TintColor color = blushTint->getPrimaryColor();
                            cd.headTintZone3[0] = color.r;
                            cd.headTintZone3[1] = color.g;
                            cd.headTintZone3[2] = color.b;
                        }
                    } else {
                        cd.headTintZone3[0] = 1.0f;
                        cd.headTintZone3[1] = 1.0f;
                        cd.headTintZone3[2] = 1.0f;
                    }

                    return;
                }
            }
        }
    }
}

static bool applyMorphToMesh(Mesh& mesh, const MorphMeshTarget* target, float amount,
                             const std::vector<Vertex>& baseVertices) {
    if (!target || target->vertices.empty()) return false;
    if (mesh.vertices.size() != target->vertices.size()) {
        return false;
    }
    if (baseVertices.size() != mesh.vertices.size()) {
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
    cd.clothes.clear();
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
        else if (type == "cth") cd.clothes.push_back(item);
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

    cd.swords.clear();
    cd.greatswords.clear();
    cd.daggers.clear();
    cd.staves.clear();
    cd.shields.clear();
    cd.axes.clear();
    cd.greataxes.clear();
    cd.maces.clear();
    cd.mauls.clear();

    for (const auto& mesh : state.meshBrowser.allMeshes) {
        if (mesh.lod != 0) continue;
        std::string fileLower = mesh.mshFile;
        std::transform(fileLower.begin(), fileLower.end(), fileLower.begin(), ::tolower);
        if (fileLower.find("w_") != 0) continue;
        std::string displayName = mesh.mshName.empty() ? mesh.mshFile : mesh.mshName;
        if (fileLower.find("w_lsw_") == 0) {
            cd.swords.push_back({mesh.mshFile, displayName});
        } else if (fileLower.find("w_gsw_") == 0) {
            cd.greatswords.push_back({mesh.mshFile, displayName});
        } else if (fileLower.find("w_dgr_") == 0) {
            cd.daggers.push_back({mesh.mshFile, displayName});
        } else if (fileLower.find("w_stf_") == 0) {
            cd.staves.push_back({mesh.mshFile, displayName});
        } else if (fileLower.find("w_shd_") == 0) {
            cd.shields.push_back({mesh.mshFile, displayName});
        } else if (fileLower.find("w_axe_") == 0) {
            cd.axes.push_back({mesh.mshFile, displayName});
        } else if (fileLower.find("w_gax_") == 0) {
            cd.greataxes.push_back({mesh.mshFile, displayName});
        } else if (fileLower.find("w_mce_") == 0) {
            cd.maces.push_back({mesh.mshFile, displayName});
        } else if (fileLower.find("w_mal_") == 0) {
            cd.mauls.push_back({mesh.mshFile, displayName});
        }
    }

    cd.listsBuilt = true;
}

// Face morph category definitions
struct FaceMorphDef {
    const char* code;
    const char* name;
};

// Forward declaration
static std::vector<Vertex> loadFaceMorphMesh(AppState& state, const std::string& mshFile);

static void buildFaceMorphCategories(AppState& state) {
    auto& cd = state.charDesigner;
    if (cd.faceMorphsBuilt) return;

    // Define universal categories (present for all races)
    std::vector<FaceMorphDef> catDefs = {
        {"nos", "Nose"}, {"jaw", "Jaw"}, {"mth", "Mouth"}, {"ear", "Ears"},
        {"eye", "Eyes"}, {"brw", "Brow"}, {"chk", "Cheeks"}, {"nec", "Neck"},
        {"shp", "Head Shape"}, {"tee", "Teeth"}
    };

    // Add race/gender-specific chin code
    std::string chinCode;
    if (cd.race == 2) chinCode = cd.isMale ? "cdm" : "cdf";       // dwarf
    else if (cd.race == 1) chinCode = cd.isMale ? "cem" : "cef";   // elf
    else chinCode = cd.isMale ? "cdm" : "cdf";                      // human fallback
    catDefs.push_back({chinCode.c_str(), "Chin"});

    // Add gender-specific cheek variants
    std::string cheekCode = cd.isMale ? "chm" : "chf";
    catDefs.push_back({cheekCode.c_str(), "Cheeks 2"});

    // Scan ERFs for uh_/uhm_/uem_/ulm_ mesh entries
    // key = "prefix_code_number" (e.g., "uh_nos_01"), value = full filename
    std::map<std::string, std::string> uhMap, uhmMap, uemMap, ulmMap;

    auto scanEntries = [](const std::vector<std::unique_ptr<ERFFile>>& erfs,
                          std::map<std::string, std::string>& uhMap,
                          std::map<std::string, std::string>& uhmMap,
                          std::map<std::string, std::string>& uemMap,
                          std::map<std::string, std::string>& ulmMap) {
        for (auto& erf : erfs) {
            for (const auto& entry : erf->entries()) {
                std::string name = entry.name;
                std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                if (name.size() < 12) continue;
                if (name.substr(name.size() - 4) != ".msh") continue;

                // Parse: {prefix}_{code}_{num}_0.msh
                std::string prefix, code, num;
                if (name.substr(0, 3) == "uh_") {
                    prefix = "uh";
                    size_t p1 = name.find('_', 3);
                    if (p1 == std::string::npos) continue;
                    code = name.substr(3, p1 - 3);
                    size_t p2 = name.find('_', p1 + 1);
                    if (p2 == std::string::npos) continue;
                    num = name.substr(p1 + 1, p2 - p1 - 1);
                    // Skip base meshes
                    if (code == "uhm" || code == "uem" || code == "ulm") continue;
                    uhMap[code + "_" + num] = name;
                } else if (name.substr(0, 4) == "uhm_") {
                    size_t p1 = name.find('_', 4);
                    if (p1 == std::string::npos) continue;
                    code = name.substr(4, p1 - 4);
                    size_t p2 = name.find('_', p1 + 1);
                    if (p2 == std::string::npos) continue;
                    num = name.substr(p1 + 1, p2 - p1 - 1);
                    uhmMap[code + "_" + num] = name;
                } else if (name.substr(0, 4) == "uem_") {
                    size_t p1 = name.find('_', 4);
                    if (p1 == std::string::npos) continue;
                    code = name.substr(4, p1 - 4);
                    size_t p2 = name.find('_', p1 + 1);
                    if (p2 == std::string::npos) continue;
                    num = name.substr(p1 + 1, p2 - p1 - 1);
                    uemMap[code + "_" + num] = name;
                } else if (name.substr(0, 4) == "ulm_") {
                    size_t p1 = name.find('_', 4);
                    if (p1 == std::string::npos) continue;
                    code = name.substr(4, p1 - 4);
                    size_t p2 = name.find('_', p1 + 1);
                    if (p2 == std::string::npos) continue;
                    num = name.substr(p1 + 1, p2 - p1 - 1);
                    ulmMap[code + "_" + num] = name;
                }
            }
        }
    };

    scanEntries(state.modelErfs, uhMap, uhmMap, uemMap, ulmMap);

    // Load and scan chargen.rim (contains face morph component meshes)
    if (!cd.chargenRim) {
        fs::path chargenPath = fs::path(state.selectedFolder) / "packages" / "core" / "data" / "chargen.rim";
        if (fs::exists(chargenPath)) {
            cd.chargenRim = std::make_unique<ERFFile>();
            if (!cd.chargenRim->open(chargenPath.string())) {
                cd.chargenRim.reset();
                printf("[FACE] Failed to open chargen.rim\n");
            } else {
                printf("[FACE] Loaded chargen.rim: %zu entries\n", cd.chargenRim->entries().size());
            }
        } else {
            printf("[FACE] chargen.rim not found at: %s\n", chargenPath.string().c_str());
        }
    }
    if (cd.chargenRim) {
        // Wrap in a temp vector so we can reuse scanEntries
        std::vector<std::unique_ptr<ERFFile>> tempVec;
        tempVec.push_back(std::move(cd.chargenRim));
        scanEntries(tempVec, uhMap, uhmMap, uemMap, ulmMap);
        cd.chargenRim = std::move(tempVec[0]);
    }

    // Build categories
    cd.faceCategories.clear();
    for (const auto& def : catDefs) {
        std::string code = def.code;

        // Collect all number keys for this code from uh_ entries
        std::set<std::string> numbers;
        for (const auto& [key, val] : uhMap) {
            if (key.substr(0, code.size() + 1) == code + "_") {
                numbers.insert(key.substr(code.size() + 1));
            }
        }
        // Also from uhm_ entries (some categories only exist in uhm_)
        for (const auto& [key, val] : uhmMap) {
            if (key.substr(0, code.size() + 1) == code + "_") {
                numbers.insert(key.substr(code.size() + 1));
            }
        }

        if (numbers.empty()) continue;

        AppState::CharacterDesigner::FaceMorphCategory cat;
        cat.name = def.name;
        cat.code = code;

        for (const auto& num : numbers) {
            std::string key = code + "_" + num;
            AppState::CharacterDesigner::FaceMorphCategory::Variant v;
            auto uhIt = uhMap.find(key);
            if (uhIt != uhMap.end()) v.uhMsh = uhIt->second;
            auto uhmIt = uhmMap.find(key);
            if (uhmIt != uhmMap.end()) v.uhmMsh = uhmIt->second;
            auto uemIt = uemMap.find(key);
            if (uemIt != uemMap.end()) v.uemMsh = uemIt->second;
            auto ulmIt = ulmMap.find(key);
            if (ulmIt != ulmMap.end()) v.ulmMsh = ulmIt->second;
            v.label = num;
            cat.variants.push_back(v);
        }

        // Sort by label number
        std::sort(cat.variants.begin(), cat.variants.end(),
            [](const auto& a, const auto& b) {
                int na = 0, nb = 0;
                try { na = std::stoi(a.label); } catch (...) {}
                try { nb = std::stoi(b.label); } catch (...) {}
                return na < nb;
            });

        cd.faceCategories.push_back(std::move(cat));
    }

    cd.faceMorphsBuilt = true;

    // Load universal morph base meshes from chargen.rim
    if (!cd.morphBasesLoaded && cd.chargenRim) {
        cd.morphBaseHead = loadFaceMorphMesh(state, "uh_uhm_bas_0.msh");
        cd.morphBaseEyes = loadFaceMorphMesh(state, "uh_uem_bas_0.msh");
        cd.morphBaseLashes = loadFaceMorphMesh(state, "uh_ulm_bas_0.msh");
        cd.morphBasesLoaded = true;
        printf("[FACE] Morph bases: head=%zu eyes=%zu lashes=%zu\n",
               cd.morphBaseHead.size(), cd.morphBaseEyes.size(), cd.morphBaseLashes.size());
    }
    printf("[FACE] Built %zu face morph categories\n", cd.faceCategories.size());
    for (const auto& cat : cd.faceCategories) {
        printf("[FACE]   %s (%s): %zu variants\n", cat.name.c_str(), cat.code.c_str(), cat.variants.size());
    }
}

// Load a face morph component mesh and return its first mesh's vertices
static std::vector<Vertex> loadFaceMorphMesh(AppState& state, const std::string& mshFile) {
    if (mshFile.empty()) return {};
    std::vector<uint8_t> data = readFromCache(state, mshFile, ".msh");
    if (data.empty()) {
        data = readFromErfs(state.modelErfs, mshFile);
    }
    // Try chargen.rim (face morph component meshes live here)
    if (data.empty() && state.charDesigner.chargenRim) {
        std::string nameLower = mshFile;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        for (const auto& entry : state.charDesigner.chargenRim->entries()) {
            std::string entryLower = entry.name;
            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
            if (entryLower == nameLower) {
                data = state.charDesigner.chargenRim->readEntry(entry);
                break;
            }
        }
    }
    if (data.empty()) return {};
    Model tempModel;
    if (!loadMSH(data, tempModel) || tempModel.meshes.empty()) return {};
    return tempModel.meshes[0].vertices;
}

// Load morph targets for a category's current selection
static void loadFaceMorphTargets(AppState& state, AppState::CharacterDesigner::FaceMorphCategory& cat) {
    cat.headTarget.clear();
    cat.eyesTarget.clear();
    cat.lashesTarget.clear();
    if (cat.selected <= 0 || cat.selected > (int)cat.variants.size()) return;

    const auto& v = cat.variants[cat.selected - 1];
    printf("[FMORPH] Loading targets for %s variant %d: uh='%s' uhm='%s' uem='%s' ulm='%s'\n",
           cat.name.c_str(), cat.selected, v.uhMsh.c_str(), v.uhmMsh.c_str(), v.uemMsh.c_str(), v.ulmMsh.c_str());

    // Load uh_ mesh (primary head morph)
    if (!v.uhMsh.empty()) {
        cat.headTarget = loadFaceMorphMesh(state, v.uhMsh);
        printf("[FMORPH]   uh_ head: %zu verts\n", cat.headTarget.size());
    }
    // If no uh_ but uhm_ exists, use that for head
    if (cat.headTarget.empty() && !v.uhmMsh.empty()) {
        cat.headTarget = loadFaceMorphMesh(state, v.uhmMsh);
        printf("[FMORPH]   uhm_ head fallback: %zu verts\n", cat.headTarget.size());
    }
    // Load uem_ for eyes
    if (!v.uemMsh.empty()) {
        cat.eyesTarget = loadFaceMorphMesh(state, v.uemMsh);
        printf("[FMORPH]   uem_ eyes: %zu verts\n", cat.eyesTarget.size());
    }
    // Load ulm_ for lashes
    if (!v.ulmMsh.empty()) {
        cat.lashesTarget = loadFaceMorphMesh(state, v.ulmMsh);
        printf("[FMORPH]   ulm_ lashes: %zu verts\n", cat.lashesTarget.size());
    }
}

static int getMaxMaterialStyle(AppState& state, const std::string& baseName) {
    buildMaterialCache(state);

    int maxStyle = 0;
    for (char c = 'a'; c <= 'z'; c++) {
        std::string testName = baseName + c;
        if (materialExists(state, testName)) {
            maxStyle = c - 'a';
        }
    }
    return maxStyle;
}

static void applyMaterialStyle(AppState& state, Model& model, int styleOffset, const std::string& partType) {
    if (styleOffset == 0) return;

    for (auto& mat : model.materials) {
        if (mat.name.empty()) continue;

        std::string matLower = mat.name;
        std::transform(matLower.begin(), matLower.end(), matLower.begin(), ::tolower);

        bool isMatch = false;
        if (partType == "arm") {
            if (matLower.find("_arm_") != std::string::npos ||
                matLower.find("_mas") != std::string::npos ||
                matLower.find("_med") != std::string::npos ||
                matLower.find("_hvy") != std::string::npos ||
                matLower.find("_lgt") != std::string::npos) {
                isMatch = true;
            }
        } else if (partType == "cth") {
            if (matLower.find("_cth_") != std::string::npos || matLower.find("_clo") != std::string::npos) {
                isMatch = true;
            }
        } else if (partType == "boo") {
            if (matLower.find("_boo_") != std::string::npos || matLower.find("_boot") != std::string::npos) {
                isMatch = true;
            }
        } else if (partType == "glv") {
            if (matLower.find("_glv_") != std::string::npos || matLower.find("_glove") != std::string::npos) {
                isMatch = true;
            }
        }

        if (!isMatch) continue;

        if (matLower.back() != 'a') continue;

        std::string baseName = matLower.substr(0, matLower.size() - 1);
        char newStyle = 'a' + styleOffset;
        std::string newMatName = baseName + newStyle;

        if (materialExists(state, newMatName)) {
            std::vector<uint8_t> maoData = readFromCache(state, newMatName + ".mao", ".mao");
            if (maoData.empty()) {
                maoData = readFromErfs(state.materialErfs, newMatName + ".mao");
            }
            if (!maoData.empty()) {
                std::string maoContent(maoData.begin(), maoData.end());
                Material newMat = parseMAO(maoContent, newMatName);
                newMat.maoSource = newMatName + ".mao";
                newMat.maoContent = maoContent;

                std::string oldName = mat.name;
                mat = newMat;
                mat.name = oldName;

                if (!mat.diffuseMap.empty()) {
                    mat.diffuseTexId = loadTexByName(state, mat.diffuseMap, &mat.diffuseData, &mat.diffuseWidth, &mat.diffuseHeight);
                }
                if (!mat.normalMap.empty()) {
                    mat.normalTexId = loadTexByName(state, mat.normalMap);
                }
                if (!mat.specularMap.empty()) {
                    mat.specularTexId = loadTexByName(state, mat.specularMap);
                }
                if (!mat.tintMap.empty()) {
                    mat.tintTexId = loadTexByName(state, mat.tintMap);
                }
            }
        }
    }
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
        std::vector<uint8_t> mmhData = readFromCache(state, candidate, ".mmh");
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
        std::vector<uint8_t> maoData = readFromCache(state, matName + ".mao", ".mao");
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
                    mat.diffuseTexId = createTextureFromDDSHair(texData);
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
        if (!mat.browStubbleMap.empty() && mat.browStubbleTexId == 0) {
            mat.browStubbleTexId = loadTexByName(state, mat.browStubbleMap);
        }
        if (!mat.browStubbleNormalMap.empty() && mat.browStubbleNormalTexId == 0) {
            mat.browStubbleNormalTexId = loadTexByName(state, mat.browStubbleNormalMap);
        }
        if (!mat.tattooMap.empty() && mat.tattooTexId == 0) {
            mat.tattooTexId = loadTexByName(state, mat.tattooMap);
        }
    }
    auto result = cd.partCache.emplace(partLower, std::move(partModel));
    return &result.first->second;
}

static void attachWeaponToBone(AppState& state, Model& model, const std::string& weaponFile, const std::string& boneName) {
    int boneIdx = model.skeleton.findBone(boneName);

    if (boneIdx < 0) {
        std::string boneNameLower = boneName;
        std::transform(boneNameLower.begin(), boneNameLower.end(), boneNameLower.begin(), ::tolower);
        for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
            std::string bn = model.skeleton.bones[i].name;
            std::transform(bn.begin(), bn.end(), bn.begin(), ::tolower);
            if (bn == boneNameLower) {
                boneIdx = (int)i;
                break;
            }
        }
    }

    if (boneIdx < 0) {
        return;
    }

    Model* weaponModel = getOrLoadPart(state, weaponFile);
    if (!weaponModel) {
        return;
    }

    const Bone& propBone = model.skeleton.bones[boneIdx];

    int gobIdx = weaponModel->skeleton.findBone("GOB");
    if (gobIdx < 0) gobIdx = weaponModel->skeleton.findBone("gob");

    float gobX = 0, gobY = 0, gobZ = 0;
    if (gobIdx >= 0) {
        const Bone& gob = weaponModel->skeleton.bones[gobIdx];
        gobX = gob.worldPosX;
        gobY = gob.worldPosY;
        gobZ = gob.worldPosZ;
    }

    bool isShield = (weaponFile.find("shd") != std::string::npos || weaponFile.find("SHD") != std::string::npos);

    auto quatRotate = [](float qx, float qy, float qz, float qw, float vx, float vy, float vz, float& ox, float& oy, float& oz) {
        float tx = 2.0f * (qy * vz - qz * vy);
        float ty = 2.0f * (qz * vx - qx * vz);
        float tz = 2.0f * (qx * vy - qy * vx);
        ox = vx + qw * tx + (qy * tz - qz * ty);
        oy = vy + qw * ty + (qz * tx - qx * tz);
        oz = vz + qw * tz + (qx * ty - qy * tx);
    };

    auto quatMul = [](float ax, float ay, float az, float aw,
                      float bx, float by, float bz, float bw,
                      float& ox, float& oy, float& oz, float& ow) {
        ox = aw * bx + ax * bw + ay * bz - az * by;
        oy = aw * by - ax * bz + ay * bw + az * bx;
        oz = aw * bz + ax * by - ay * bx + az * bw;
        ow = aw * bw - ax * bx - ay * by - az * bz;
    };

    bool isLeftHand = (boneName == "Prop17" || boneName == "Prop02");

    float finalRotX = propBone.worldRotX;
    float finalRotY = propBone.worldRotY;
    float finalRotZ = propBone.worldRotZ;
    float finalRotW = propBone.worldRotW;

    if (isLeftHand && !isShield) {
        float flipX, flipY, flipZ, flipW;
        quatMul(propBone.worldRotX, propBone.worldRotY, propBone.worldRotZ, propBone.worldRotW,
                0.0f, 0.0f, 1.0f, 0.0f,
                flipX, flipY, flipZ, flipW);
        finalRotX = flipX;
        finalRotY = flipY;
        finalRotZ = flipZ;
        finalRotW = flipW;
    }

    for (const auto& mesh : weaponModel->meshes) {
        Mesh meshCopy = mesh;

        for (auto& v : meshCopy.vertices) {
            v.x -= gobX;
            v.y -= gobY;
            v.z -= gobZ;

            v.boneIndices[0] = 0;
            v.boneWeights[0] = 1.0f;
            v.boneIndices[1] = v.boneIndices[2] = v.boneIndices[3] = 0;
            v.boneWeights[1] = v.boneWeights[2] = v.boneWeights[3] = 0.0f;
        }

        meshCopy.hasSkinning = true;
        meshCopy.skipInvBind = true;
        meshCopy.bonesUsed.clear();
        meshCopy.bonesUsed.push_back(boneIdx);
        meshCopy.skinningBoneMap.clear();
        meshCopy.skinningBoneMap.push_back(boneIdx);
        meshCopy.skinningCacheBuilt = true;

        model.meshes.push_back(std::move(meshCopy));
    }

    for (const auto& mat : weaponModel->materials) {
        bool found = false;
        for (const auto& existing : model.materials) {
            if (existing.name == mat.name) { found = true; break; }
        }
        if (!found) {
            model.materials.push_back(mat);
        }
    }
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
    buildFaceMorphCategories(state);
    Animation savedAnim = state.currentAnim;
    bool wasPlaying = state.animPlaying;
    float savedTime = state.animTime;
    int savedAnimIdx = state.selectedAnimIndex;
    state.currentModel = Model();
    state.hasModel = false;
    state.basePoseBones.clear();
    cd.baseHeadVertices.clear();
    cd.baseEyesVertices.clear();
    cd.baseLashesVertices.clear();
    cd.headMeshIndex = -1;
    cd.eyesMeshIndex = -1;
    cd.lashesMeshIndex = -1;

    std::string prefix = cd.currentPrefix;
    std::vector<std::string> partsToLoad;
    if (cd.selectedRobe >= 0 && cd.selectedRobe < (int)cd.robes.size()) {
        partsToLoad.push_back(cd.robes[cd.selectedRobe].first);
    } else if (cd.selectedClothes >= 0 && cd.selectedClothes < (int)cd.clothes.size()) {
        partsToLoad.push_back(cd.clothes[cd.selectedClothes].first);
    } else if (cd.selectedArmor >= 0 && cd.selectedArmor < (int)cd.armors.size()) {
        partsToLoad.push_back(cd.armors[cd.selectedArmor].first);
    }
    if (cd.selectedClothes < 0) {
        if (cd.selectedBoots >= 0 && cd.selectedBoots < (int)cd.boots.size()) {
            partsToLoad.push_back(cd.boots[cd.selectedBoots].first);
        }
        if (cd.selectedGloves >= 0 && cd.selectedGloves < (int)cd.gloves.size()) {
            partsToLoad.push_back(cd.gloves[cd.selectedGloves].first);
        }
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
        bool isEyesMesh = (partLower.find("uem_bas") != std::string::npos);
        bool isLashesMesh = (partLower.find("ulm_bas") != std::string::npos);

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
                if (isEyesMesh && cd.eyesMeshIndex < 0) {
                    cd.eyesMeshIndex = (int)state.currentModel.meshes.size();
                    cd.baseEyesVertices = mesh.vertices;
                }
                if (isLashesMesh && cd.lashesMeshIndex < 0) {
                    cd.lashesMeshIndex = (int)state.currentModel.meshes.size();
                    cd.baseLashesVertices = mesh.vertices;
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
                if (isEyesMesh && cd.eyesMeshIndex < 0) {
                    cd.eyesMeshIndex = (int)state.currentModel.meshes.size();
                    cd.baseEyesVertices = mesh.vertices;
                }
                if (isLashesMesh && cd.lashesMeshIndex < 0) {
                    cd.lashesMeshIndex = (int)state.currentModel.meshes.size();
                    cd.baseLashesVertices = mesh.vertices;
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

    if (cd.armorStyle > 0 && cd.selectedArmor >= 0 && cd.selectedRobe < 0 && cd.selectedClothes < 0) {
        applyMaterialStyle(state, state.currentModel, cd.armorStyle, "arm");
    }
    if (cd.clothesStyle > 0 && cd.selectedClothes >= 0) {
        applyMaterialStyle(state, state.currentModel, cd.clothesStyle, "cth");
    }
    if (cd.bootsStyle > 0 && cd.selectedBoots >= 0) {
        applyMaterialStyle(state, state.currentModel, cd.bootsStyle, "boo");
    }
    if (cd.glovesStyle > 0 && cd.selectedGloves >= 0) {
        applyMaterialStyle(state, state.currentModel, cd.glovesStyle, "glv");
    }

    if (cd.weaponStyle > 0) {
        std::string mainHandPart, offHandPart;
        switch (cd.weaponStyle) {
            case 1:
                if (cd.selectedMainHandWeapon >= 0 && cd.selectedMainHandWeapon < (int)cd.swords.size()) {
                    mainHandPart = cd.swords[cd.selectedMainHandWeapon].first;
                }
                if (cd.selectedOffHandWeapon >= 0 && cd.selectedOffHandWeapon < (int)cd.swords.size()) {
                    offHandPart = cd.swords[cd.selectedOffHandWeapon].first;
                }
                break;
            case 2:
                if (cd.selectedMainHandWeapon >= 0 && cd.selectedMainHandWeapon < (int)cd.daggers.size()) {
                    mainHandPart = cd.daggers[cd.selectedMainHandWeapon].first;
                }
                if (cd.selectedOffHandWeapon >= 0 && cd.selectedOffHandWeapon < (int)cd.daggers.size()) {
                    offHandPart = cd.daggers[cd.selectedOffHandWeapon].first;
                }
                break;
            case 3:
                if (cd.selectedMainHandWeapon >= 0 && cd.selectedMainHandWeapon < (int)cd.swords.size()) {
                    mainHandPart = cd.swords[cd.selectedMainHandWeapon].first;
                }
                if (cd.selectedOffHandWeapon >= 0 && cd.selectedOffHandWeapon < (int)cd.shields.size()) {
                    offHandPart = cd.shields[cd.selectedOffHandWeapon].first;
                }
                break;
            case 4:
                if (cd.selectedMainHandWeapon >= 0 && cd.selectedMainHandWeapon < (int)cd.daggers.size()) {
                    mainHandPart = cd.daggers[cd.selectedMainHandWeapon].first;
                }
                if (cd.selectedOffHandWeapon >= 0 && cd.selectedOffHandWeapon < (int)cd.shields.size()) {
                    offHandPart = cd.shields[cd.selectedOffHandWeapon].first;
                }
                break;
            case 5:
                if (cd.selectedMainHandWeapon >= 0 && cd.selectedMainHandWeapon < (int)cd.staves.size()) {
                    mainHandPart = cd.staves[cd.selectedMainHandWeapon].first;
                }
                break;
            case 6:
                if (cd.selectedMainHandWeapon >= 0 && cd.selectedMainHandWeapon < (int)cd.greatswords.size()) {
                    mainHandPart = cd.greatswords[cd.selectedMainHandWeapon].first;
                }
                break;
            case 7:
                if (cd.selectedMainHandWeapon >= 0 && cd.selectedMainHandWeapon < (int)cd.greataxes.size()) {
                    mainHandPart = cd.greataxes[cd.selectedMainHandWeapon].first;
                }
                break;
            case 8:
                if (cd.selectedMainHandWeapon >= 0 && cd.selectedMainHandWeapon < (int)cd.mauls.size()) {
                    mainHandPart = cd.mauls[cd.selectedMainHandWeapon].first;
                }
                break;
        }
        if (!mainHandPart.empty()) {
            attachWeaponToBone(state, state.currentModel, mainHandPart, "Prop15");
        }
        if (!offHandPart.empty()) {
            attachWeaponToBone(state, state.currentModel, offHandPart, "Prop17");
        }
    }

    if (cd.selectedTattoo > 0 && cd.selectedTattoo < (int)cd.tattoos.size()) {
        std::string tattooTexName = cd.tattoos[cd.selectedTattoo].first;
        if (!tattooTexName.empty()) {
            uint32_t tattooTexId = loadTexByName(state, tattooTexName);
            if (tattooTexId != 0) {
                for (auto& mat : state.currentModel.materials) {
                    std::string matLower = mat.name;
                    std::transform(matLower.begin(), matLower.end(), matLower.begin(), ::tolower);
                    if (matLower.find("_hed_") != std::string::npos ||
                        matLower.find("hed_fem") != std::string::npos ||
                        matLower.find("hed_mal") != std::string::npos) {
                        mat.tattooTexId = tattooTexId;
                        mat.tattooMap = tattooTexName;
                    }
                }
            }
        }
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
            applyMorphToMesh(headMesh, faceTarget, cd.faceMorphAmount, cd.baseHeadVertices);
        }
    }

    if (cd.morphLoaded && cd.eyesMeshIndex >= 0 && !cd.baseEyesVertices.empty()) {
        const MorphMeshTarget* eyesTarget = cd.morphData.getEyesTarget();
        if (eyesTarget) {
            Mesh& eyesMesh = state.currentModel.meshes[cd.eyesMeshIndex];
            applyMorphToMesh(eyesMesh, eyesTarget, cd.faceMorphAmount, cd.baseEyesVertices);
        }
    }

    if (cd.morphLoaded && cd.lashesMeshIndex >= 0 && !cd.baseLashesVertices.empty()) {
        const MorphMeshTarget* lashesTarget = cd.morphData.getLashesTarget();
        if (lashesTarget) {
            Mesh& lashesMesh = state.currentModel.meshes[cd.lashesMeshIndex];
            applyMorphToMesh(lashesMesh, lashesTarget, cd.faceMorphAmount, cd.baseLashesVertices);
        }
    }

    // Apply face morph components additively using spatial nearest-vertex matching
    auto applyFaceMorphSpatial = [&](const std::vector<Vertex>& target,
                                      const std::vector<Vertex>& base,
                                      Mesh& mesh,
                                      const std::vector<Vertex>& meshBaseVerts) {
        if (target.empty() || base.empty() || meshBaseVerts.empty()) return;
        // Build base→target mapping (target is similar to base but may have ±few verts)
        // Compute deltas for each base vertex from nearest target vertex
        std::vector<float> dx(base.size(), 0), dy(base.size(), 0), dz(base.size(), 0);
        for (size_t j = 0; j < base.size(); j++) {
            float bestDist = 1e30f;
            int bestIdx = 0;
            for (size_t t = 0; t < target.size(); t++) {
                float ddx = base[j].x - target[t].x;
                float ddy = base[j].y - target[t].y;
                float ddz = base[j].z - target[t].z;
                float d = ddx*ddx + ddy*ddy + ddz*ddz;
                if (d < bestDist) { bestDist = d; bestIdx = (int)t; }
            }
            dx[j] = target[bestIdx].x - base[j].x;
            dy[j] = target[bestIdx].y - base[j].y;
            dz[j] = target[bestIdx].z - base[j].z;
        }
        // Map mesh vertices to nearest base vertex and apply deltas
        for (size_t i = 0; i < mesh.vertices.size() && i < meshBaseVerts.size(); i++) {
            float bestDist = 1e30f;
            int bestIdx = 0;
            for (size_t j = 0; j < base.size(); j++) {
                float ddx = meshBaseVerts[i].x - base[j].x;
                float ddy = meshBaseVerts[i].y - base[j].y;
                float ddz = meshBaseVerts[i].z - base[j].z;
                float d = ddx*ddx + ddy*ddy + ddz*ddz;
                if (d < bestDist) { bestDist = d; bestIdx = (int)j; }
            }
            // Only apply if mesh vertex is close to a base vertex (within ~threshold)
            if (bestDist < 0.01f) {
                mesh.vertices[i].x += dx[bestIdx];
                mesh.vertices[i].y += dy[bestIdx];
                mesh.vertices[i].z += dz[bestIdx];
            }
        }
    };

    for (auto& cat : cd.faceCategories) {
        if (cat.selected <= 0) continue;

        if (!cat.headTarget.empty() && cd.headMeshIndex >= 0 && !cd.morphBaseHead.empty()) {
            Mesh& headMesh = state.currentModel.meshes[cd.headMeshIndex];
            applyFaceMorphSpatial(cat.headTarget, cd.morphBaseHead, headMesh, cd.baseHeadVertices);
        }
        if (!cat.eyesTarget.empty() && cd.eyesMeshIndex >= 0 && !cd.morphBaseEyes.empty()) {
            Mesh& eyesMesh = state.currentModel.meshes[cd.eyesMeshIndex];
            applyFaceMorphSpatial(cat.eyesTarget, cd.morphBaseEyes, eyesMesh, cd.baseEyesVertices);
        }
        if (!cat.lashesTarget.empty() && cd.lashesMeshIndex >= 0 && !cd.morphBaseLashes.empty()) {
            Mesh& lashesMesh = state.currentModel.meshes[cd.lashesMeshIndex];
            applyFaceMorphSpatial(cat.lashesTarget, cd.morphBaseLashes, lashesMesh, cd.baseLashesVertices);
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
                                resolveX360AnimHashes(state.currentAnim, state.currentModel.skeleton);
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
    if (!s_materialCacheBuilt) {
        buildMaterialCache(state);
    }

    auto& cd = state.charDesigner;

    memcpy(state.renderSettings.eyeColor, cd.eyeColor, sizeof(float) * 3);
    state.renderSettings.ageAmount = cd.ageAmount;
    memcpy(state.renderSettings.stubbleAmount, cd.stubbleAmount, sizeof(float) * 4);
    memcpy(state.renderSettings.tattooAmount, cd.tattooAmount, sizeof(float) * 3);
    memcpy(state.renderSettings.tattooColor1, cd.tattooColor1, sizeof(float) * 3);
    memcpy(state.renderSettings.tattooColor2, cd.tattooColor2, sizeof(float) * 3);
    memcpy(state.renderSettings.tattooColor3, cd.tattooColor3, sizeof(float) * 3);
    memcpy(state.renderSettings.headZone1, cd.headTintZone1, sizeof(float) * 3);
    memcpy(state.renderSettings.headZone2, cd.headTintZone2, sizeof(float) * 3);
    memcpy(state.renderSettings.headZone3, cd.headTintZone3, sizeof(float) * 3);
    memcpy(state.renderSettings.armorZone1, cd.armorTintZone1, sizeof(float) * 3);
    memcpy(state.renderSettings.armorZone2, cd.armorTintZone2, sizeof(float) * 3);
    memcpy(state.renderSettings.armorZone3, cd.armorTintZone3, sizeof(float) * 3);
    memcpy(state.renderSettings.clothesZone1, cd.clothesTintZone1, sizeof(float) * 3);
    memcpy(state.renderSettings.clothesZone2, cd.clothesTintZone2, sizeof(float) * 3);
    memcpy(state.renderSettings.clothesZone3, cd.clothesTintZone3, sizeof(float) * 3);
    memcpy(state.renderSettings.bootsZone1, cd.bootsTintZone1, sizeof(float) * 3);
    memcpy(state.renderSettings.bootsZone2, cd.bootsTintZone2, sizeof(float) * 3);
    memcpy(state.renderSettings.bootsZone3, cd.bootsTintZone3, sizeof(float) * 3);
    memcpy(state.renderSettings.glovesZone1, cd.glovesTintZone1, sizeof(float) * 3);
    memcpy(state.renderSettings.glovesZone2, cd.glovesTintZone2, sizeof(float) * 3);
    memcpy(state.renderSettings.glovesZone3, cd.glovesTintZone3, sizeof(float) * 3);
    memcpy(state.renderSettings.helmetZone1, cd.helmetTintZone1, sizeof(float) * 3);
    memcpy(state.renderSettings.helmetZone2, cd.helmetTintZone2, sizeof(float) * 3);
    memcpy(state.renderSettings.helmetZone3, cd.helmetTintZone3, sizeof(float) * 3);

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
        cd.selectedMorphPreset = -1;
        cd.baseHeadVertices.clear();
        cd.baseEyesVertices.clear();
        cd.baseLashesVertices.clear();
        cd.headMeshIndex = -1;
        cd.eyesMeshIndex = -1;
        cd.lashesMeshIndex = -1;
        cd.selectedHead = 0;
        cd.selectedHair = 0;
        cd.selectedBeard = -1;
        cd.selectedArmor = 0;
        cd.selectedBoots = 0;
        cd.selectedGloves = 0;
        cd.selectedHelmet = -1;
        cd.faceMorphsBuilt = false;
        for (auto& cat : cd.faceCategories) {
            cat.selected = 0;
            cat.headTarget.clear();
            cat.eyesTarget.clear();
            cat.lashesTarget.clear();
        }
    }

    buildCharacterLists(state);
    ImGui::Separator();

    if (ImGui::BeginTabBar("EquipTabs")) {
        if (ImGui::BeginTabItem("Head")) {
            if (!cd.availableMorphPresets.empty()) {
                ImGui::Text("Face Presets:");

                std::string currentPreset = (cd.selectedMorphPreset < 0) ? "Default" :
                    (cd.selectedMorphPreset < (int)cd.availableMorphPresets.size()
                        ? cd.availableMorphPresets[cd.selectedMorphPreset].displayName : "Default");

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 30);
                if (ImGui::BeginCombo("##morphpreset", currentPreset.c_str())) {
                    bool defaultSelected = (cd.selectedMorphPreset < 0);
                    if (ImGui::Selectable("Default", defaultSelected)) {
                        cd.selectedMorphPreset = -1;
                        cd.morphLoaded = false;
                        cd.morphData = MorphData();
                        cd.baseHeadVertices.clear();
                        cd.baseEyesVertices.clear();
                        cd.baseLashesVertices.clear();
                        cd.needsRebuild = true;
                    }
                    if (defaultSelected) ImGui::SetItemDefaultFocus();

                    ImGui::Separator();

                    for (int i = 0; i < (int)cd.availableMorphPresets.size(); i++) {
                        bool selected = (cd.selectedMorphPreset == i);
                        if (ImGui::Selectable(cd.availableMorphPresets[i].displayName.c_str(), selected)) {
                            cd.selectedMorphPreset = i;
                            loadSelectedMorphPreset(state);
                            cd.needsRebuild = true;
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                ImGui::SameLine();
                if (ImGui::Button("X##resetpreset")) {
                    cd.selectedMorphPreset = -1;
                    cd.morphLoaded = false;
                    cd.morphData = MorphData();
                    cd.baseHeadVertices.clear();
                    cd.baseEyesVertices.clear();
                    cd.baseLashesVertices.clear();
                    cd.needsRebuild = true;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Reset to Default");
                }

                ImGui::Separator();
            }

            ImGui::ColorEdit3("Skin Color", state.renderSettings.skinColor, ImGuiColorEditFlags_NoInputs);
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
            ImGui::ColorEdit3("Hair Color", state.renderSettings.hairColor, ImGuiColorEditFlags_NoInputs);

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
                ImGui::Text("Stubble:");
                ImGui::SliderFloat("Style 1##stubble", &cd.stubbleAmount[0], 0.0f, 1.0f);
                ImGui::SliderFloat("Style 2##stubble", &cd.stubbleAmount[1], 0.0f, 1.0f);
                ImGui::SliderFloat("Style 3##stubble", &cd.stubbleAmount[2], 0.0f, 1.0f);
                ImGui::SliderFloat("Style 4##stubble", &cd.stubbleAmount[3], 0.0f, 1.0f);
            }
            ImGui::Separator();

            ImGui::ColorEdit3("Eye Color", cd.eyeColor, ImGuiColorEditFlags_NoInputs);
            ImGui::Separator();

            ImGui::SliderFloat("Age", &cd.ageAmount, 0.0f, 1.0f);
            ImGui::Separator();

            ImGui::Text("Tattoo:");
            if (!cd.tattoos.empty()) {
                std::string currentTattoo = (cd.selectedTattoo < 0) ? "None" :
                    (cd.selectedTattoo < (int)cd.tattoos.size() ? cd.tattoos[cd.selectedTattoo].second : "None");
                if (ImGui::BeginCombo("##tattooselect", currentTattoo.c_str())) {
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
            ImGui::SliderFloat("Style 1##tattoo", &cd.tattooAmount[0], 0.0f, 1.0f);
            ImGui::ColorEdit3("Color 1##tattoo", cd.tattooColor1, ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Style 2##tattoo", &cd.tattooAmount[1], 0.0f, 1.0f);
            ImGui::ColorEdit3("Color 2##tattoo", cd.tattooColor2, ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Style 3##tattoo", &cd.tattooAmount[2], 0.0f, 1.0f);
            ImGui::ColorEdit3("Color 3##tattoo", cd.tattooColor3, ImGuiColorEditFlags_NoInputs);
            ImGui::Separator();

            ImGui::Text("Makeup:");
            ImGui::ColorEdit3("Lips", cd.headTintZone1, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit3("Eyeshadow", cd.headTintZone2, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit3("Blush", cd.headTintZone3, ImGuiColorEditFlags_NoInputs);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Face")) {
            if (cd.faceCategories.empty()) {
                ImGui::TextDisabled("No face morphs available");
            } else {
                bool anyChanged = false;
                for (int ci = 0; ci < (int)cd.faceCategories.size(); ci++) {
                    auto& cat = cd.faceCategories[ci];
                    ImGui::Text("%s:", cat.name.c_str());

                    std::string currentLabel = (cat.selected <= 0) ? "Default" :
                        ((cat.selected <= (int)cat.variants.size())
                            ? cat.variants[cat.selected - 1].label : "Default");

                    int val = cat.selected;
                    int maxVal = (int)cat.variants.size();
                    std::string sliderLabel = (val == 0) ? "Default" : currentLabel;
                    std::string id = "##face_" + cat.code;
                    if (ImGui::SliderInt(id.c_str(), &val, 0, maxVal, sliderLabel.c_str())) {
                        if (val != cat.selected) {
                            cat.selected = val;
                            loadFaceMorphTargets(state, cat);
                            anyChanged = true;
                        }
                    }
                }
                if (anyChanged) {
                    cd.needsRebuild = true;
                }

                ImGui::Separator();
                if (ImGui::Button("Reset All Face Morphs")) {
                    for (auto& cat : cd.faceCategories) {
                        cat.selected = 0;
                        cat.headTarget.clear();
                        cat.eyesTarget.clear();
                        cat.lashesTarget.clear();
                    }
                    cd.needsRebuild = true;
                }
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Armor")) {
            ImGui::TextDisabled("Body Armor:");
            for (int i = 0; i < (int)cd.armors.size(); i++) {
                bool selected = (cd.selectedArmor == i && cd.selectedRobe < 0 && cd.selectedClothes < 0);
                if (ImGui::Selectable(cd.armors[i].second.c_str(), selected)) {
                    cd.selectedArmor = i;
                    cd.selectedRobe = -1;
                    cd.selectedClothes = -1;
                    cd.armorStyle = 0;
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
                        cd.selectedClothes = -1;
                        cd.selectedArmor = -1;
                        cd.armorStyle = 0;
                        cd.needsRebuild = true;
                    }
                }
            }

            if (state.hasModel && cd.selectedArmor >= 0 && cd.selectedRobe < 0 && cd.selectedClothes < 0) {
                for (const auto& mat : state.currentModel.materials) {
                    std::string matLower = mat.name;
                    std::transform(matLower.begin(), matLower.end(), matLower.begin(), ::tolower);

                    if (matLower.find("_arm_") == std::string::npos &&
                        matLower.find("_mas") == std::string::npos &&
                        matLower.find("_med") == std::string::npos &&
                        matLower.find("_hvy") == std::string::npos &&
                        matLower.find("_lgt") == std::string::npos) {
                        continue;
                    }

                    if (matLower.back() == 'a') {
                        std::string baseName = matLower.substr(0, matLower.size() - 1);
                        if (materialExists(state, baseName + "b")) {
                            int maxStyle = getMaxMaterialStyle(state, baseName);
                            if (maxStyle > 0) {
                                ImGui::Separator();
                                char styleChar = 'A' + cd.armorStyle;
                                std::string styleLabel = std::string(1, styleChar);
                                ImGui::Text("Style:");
                                ImGui::SameLine();
                                if (ImGui::SliderInt("##armorstyle", &cd.armorStyle, 0, maxStyle, styleLabel.c_str())) {
                                    cd.needsRebuild = true;
                                }
                                break;
                            }
                        }
                    }
                }
            }

            ImGui::Separator();
            ImGui::Text("Colors:");
            ImGui::ColorEdit3("Color 1##armor", cd.armorTintZone1, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit3("Color 2##armor", cd.armorTintZone2, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit3("Color 3##armor", cd.armorTintZone3, ImGuiColorEditFlags_NoInputs);

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Clothes")) {
            if (cd.clothes.empty()) {
                ImGui::TextDisabled("No clothes available");
            } else {
                for (int i = 0; i < (int)cd.clothes.size(); i++) {
                    bool selected = (cd.selectedClothes == i);
                    if (ImGui::Selectable(cd.clothes[i].second.c_str(), selected)) {
                        cd.selectedClothes = i;
                        cd.selectedArmor = -1;
                        cd.selectedRobe = -1;
                        cd.selectedBoots = -1;
                        cd.selectedGloves = -1;
                        cd.clothesStyle = 0;
                        cd.needsRebuild = true;
                    }
                }

                if (state.hasModel && cd.selectedClothes >= 0) {
                    for (const auto& mat : state.currentModel.materials) {
                        std::string matName = mat.name;
                        std::string matLower = matName;
                        std::transform(matLower.begin(), matLower.end(), matLower.begin(), ::tolower);

                        if (matLower.find("_cth_") != std::string::npos || matLower.find("_clo") != std::string::npos) {
                            if (!matName.empty() && matName.back() >= 'a' && matName.back() <= 'z') {
                                std::string baseName = matName.substr(0, matName.size() - 1);
                                int maxStyle = getMaxMaterialStyle(state, baseName);

                                if (maxStyle > 0) {
                                    ImGui::Separator();
                                    char styleChar = 'A' + cd.clothesStyle;
                                    std::string styleLabel = std::string(1, styleChar);
                                    ImGui::Text("Style:");
                                    ImGui::SameLine();
                                    if (ImGui::SliderInt("##clothesstyle", &cd.clothesStyle, 0, maxStyle, styleLabel.c_str())) {
                                        cd.needsRebuild = true;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }

                ImGui::Separator();
                ImGui::Text("Colors:");
                ImGui::ColorEdit3("Color 1##clothes", cd.clothesTintZone1, ImGuiColorEditFlags_NoInputs);
                ImGui::ColorEdit3("Color 2##clothes", cd.clothesTintZone2, ImGuiColorEditFlags_NoInputs);
                ImGui::ColorEdit3("Color 3##clothes", cd.clothesTintZone3, ImGuiColorEditFlags_NoInputs);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Boots")) {
            bool noneSelected = (cd.selectedBoots < 0);
            if (ImGui::Selectable("None", noneSelected)) {
                cd.selectedBoots = -1;
                cd.needsRebuild = true;
            }
            for (int i = 0; i < (int)cd.boots.size(); i++) {
                bool selected = (cd.selectedBoots == i);
                if (ImGui::Selectable(cd.boots[i].second.c_str(), selected)) {
                    cd.selectedBoots = i;
                    cd.bootsStyle = 0;
                    cd.needsRebuild = true;
                }
            }

            if (state.hasModel && cd.selectedBoots >= 0) {
                for (const auto& mat : state.currentModel.materials) {
                    std::string matName = mat.name;
                    std::string matLower = matName;
                    std::transform(matLower.begin(), matLower.end(), matLower.begin(), ::tolower);

                    if (matLower.find("_boo_") != std::string::npos || matLower.find("_boot") != std::string::npos) {
                        if (!matName.empty() && matName.back() >= 'a' && matName.back() <= 'z') {
                            std::string baseName = matName.substr(0, matName.size() - 1);
                            int maxStyle = getMaxMaterialStyle(state, baseName);

                            if (maxStyle > 0) {
                                ImGui::Separator();
                                char styleChar = 'A' + cd.bootsStyle;
                                std::string styleLabel = std::string(1, styleChar);
                                ImGui::Text("Style:");
                                ImGui::SameLine();
                                if (ImGui::SliderInt("##bootsstyle", &cd.bootsStyle, 0, maxStyle, styleLabel.c_str())) {
                                    cd.needsRebuild = true;
                                }
                                break;
                            }
                        }
                    }
                }
            }

            ImGui::Separator();
            ImGui::Text("Colors:");
            ImGui::ColorEdit3("Color 1##boots", cd.bootsTintZone1, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit3("Color 2##boots", cd.bootsTintZone2, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit3("Color 3##boots", cd.bootsTintZone3, ImGuiColorEditFlags_NoInputs);

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Gloves")) {
            bool noneSelected = (cd.selectedGloves < 0);
            if (ImGui::Selectable("None", noneSelected)) {
                cd.selectedGloves = -1;
                cd.needsRebuild = true;
            }
            for (int i = 0; i < (int)cd.gloves.size(); i++) {
                bool selected = (cd.selectedGloves == i);
                if (ImGui::Selectable(cd.gloves[i].second.c_str(), selected)) {
                    cd.selectedGloves = i;
                    cd.glovesStyle = 0;
                    cd.needsRebuild = true;
                }
            }

            if (state.hasModel && cd.selectedGloves >= 0) {
                for (const auto& mat : state.currentModel.materials) {
                    std::string matName = mat.name;
                    std::string matLower = matName;
                    std::transform(matLower.begin(), matLower.end(), matLower.begin(), ::tolower);

                    if (matLower.find("_glv_") != std::string::npos || matLower.find("_glove") != std::string::npos) {
                        if (!matName.empty() && matName.back() >= 'a' && matName.back() <= 'z') {
                            std::string baseName = matName.substr(0, matName.size() - 1);
                            int maxStyle = getMaxMaterialStyle(state, baseName);

                            if (maxStyle > 0) {
                                ImGui::Separator();
                                char styleChar = 'A' + cd.glovesStyle;
                                std::string styleLabel = std::string(1, styleChar);
                                ImGui::Text("Style:");
                                ImGui::SameLine();
                                if (ImGui::SliderInt("##glovesstyle", &cd.glovesStyle, 0, maxStyle, styleLabel.c_str())) {
                                    cd.needsRebuild = true;
                                }
                                break;
                            }
                        }
                    }
                }
            }

            ImGui::Separator();
            ImGui::Text("Colors:");
            ImGui::ColorEdit3("Color 1##gloves", cd.glovesTintZone1, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit3("Color 2##gloves", cd.glovesTintZone2, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit3("Color 3##gloves", cd.glovesTintZone3, ImGuiColorEditFlags_NoInputs);

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

            ImGui::Separator();
            ImGui::Text("Colors:");
            ImGui::ColorEdit3("Color 1##helmet", cd.helmetTintZone1, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit3("Color 2##helmet", cd.helmetTintZone2, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit3("Color 3##helmet", cd.helmetTintZone3, ImGuiColorEditFlags_NoInputs);

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Weapons")) {
            const char* weaponStyles[] = {
                "None",
                "Dual Swords",
                "Dual Daggers",
                "Sword + Shield",
                "Dagger + Shield",
                "Staff",
                "Greatsword",
                "Greataxe",
                "Maul"
            };

            if (ImGui::Combo("Style", &cd.weaponStyle, weaponStyles, IM_ARRAYSIZE(weaponStyles))) {
                cd.selectedMainHandWeapon = -1;
                cd.selectedOffHandWeapon = -1;
                cd.needsRebuild = true;
            }

            ImGui::Separator();

            if (cd.weaponStyle == 1) {
                ImGui::Text("Main Hand:");
                ImGui::BeginChild("MainSwords", ImVec2(0, 150), true);
                for (int i = 0; i < (int)cd.swords.size(); i++) {
                    bool selected = (cd.selectedMainHandWeapon == i);
                    if (ImGui::Selectable((cd.swords[i].second + "##main").c_str(), selected)) {
                        cd.selectedMainHandWeapon = i;
                        cd.needsRebuild = true;
                    }
                }
                ImGui::EndChild();
                ImGui::Text("Off Hand:");
                ImGui::BeginChild("OffSwords", ImVec2(0, 150), true);
                for (int i = 0; i < (int)cd.swords.size(); i++) {
                    bool selected = (cd.selectedOffHandWeapon == i);
                    if (ImGui::Selectable((cd.swords[i].second + "##off").c_str(), selected)) {
                        cd.selectedOffHandWeapon = i;
                        cd.needsRebuild = true;
                    }
                }
                ImGui::EndChild();
            }
            else if (cd.weaponStyle == 2) {
                ImGui::Text("Main Hand:");
                ImGui::BeginChild("MainDaggers", ImVec2(0, 150), true);
                for (int i = 0; i < (int)cd.daggers.size(); i++) {
                    bool selected = (cd.selectedMainHandWeapon == i);
                    if (ImGui::Selectable((cd.daggers[i].second + "##main").c_str(), selected)) {
                        cd.selectedMainHandWeapon = i;
                        cd.needsRebuild = true;
                    }
                }
                ImGui::EndChild();
                ImGui::Text("Off Hand:");
                ImGui::BeginChild("OffDaggers", ImVec2(0, 150), true);
                for (int i = 0; i < (int)cd.daggers.size(); i++) {
                    bool selected = (cd.selectedOffHandWeapon == i);
                    if (ImGui::Selectable((cd.daggers[i].second + "##off").c_str(), selected)) {
                        cd.selectedOffHandWeapon = i;
                        cd.needsRebuild = true;
                    }
                }
                ImGui::EndChild();
            }
            else if (cd.weaponStyle == 3) {
                ImGui::Text("Sword:");
                ImGui::BeginChild("MainSword", ImVec2(0, 150), true);
                for (int i = 0; i < (int)cd.swords.size(); i++) {
                    bool selected = (cd.selectedMainHandWeapon == i);
                    if (ImGui::Selectable((cd.swords[i].second + "##main").c_str(), selected)) {
                        cd.selectedMainHandWeapon = i;
                        cd.needsRebuild = true;
                    }
                }
                ImGui::EndChild();
                ImGui::Text("Shield:");
                ImGui::BeginChild("OffShield", ImVec2(0, 150), true);
                for (int i = 0; i < (int)cd.shields.size(); i++) {
                    bool selected = (cd.selectedOffHandWeapon == i);
                    if (ImGui::Selectable((cd.shields[i].second + "##off").c_str(), selected)) {
                        cd.selectedOffHandWeapon = i;
                        cd.needsRebuild = true;
                    }
                }
                ImGui::EndChild();
            }
            else if (cd.weaponStyle == 4) {
                ImGui::Text("Dagger:");
                ImGui::BeginChild("MainDagger", ImVec2(0, 150), true);
                for (int i = 0; i < (int)cd.daggers.size(); i++) {
                    bool selected = (cd.selectedMainHandWeapon == i);
                    if (ImGui::Selectable((cd.daggers[i].second + "##main").c_str(), selected)) {
                        cd.selectedMainHandWeapon = i;
                        cd.needsRebuild = true;
                    }
                }
                ImGui::EndChild();
                ImGui::Text("Shield:");
                ImGui::BeginChild("OffShield2", ImVec2(0, 150), true);
                for (int i = 0; i < (int)cd.shields.size(); i++) {
                    bool selected = (cd.selectedOffHandWeapon == i);
                    if (ImGui::Selectable((cd.shields[i].second + "##off").c_str(), selected)) {
                        cd.selectedOffHandWeapon = i;
                        cd.needsRebuild = true;
                    }
                }
                ImGui::EndChild();
            }
            else if (cd.weaponStyle == 5) {
                ImGui::BeginChild("Staves", ImVec2(0, 300), true);
                for (int i = 0; i < (int)cd.staves.size(); i++) {
                    bool selected = (cd.selectedMainHandWeapon == i);
                    if (ImGui::Selectable(cd.staves[i].second.c_str(), selected)) {
                        cd.selectedMainHandWeapon = i;
                        cd.needsRebuild = true;
                    }
                }
                ImGui::EndChild();
            }
            else if (cd.weaponStyle == 6) {
                ImGui::BeginChild("Greatswords", ImVec2(0, 300), true);
                for (int i = 0; i < (int)cd.greatswords.size(); i++) {
                    bool selected = (cd.selectedMainHandWeapon == i);
                    if (ImGui::Selectable(cd.greatswords[i].second.c_str(), selected)) {
                        cd.selectedMainHandWeapon = i;
                        cd.needsRebuild = true;
                    }
                }
                ImGui::EndChild();
            }
            else if (cd.weaponStyle == 7) {
                ImGui::BeginChild("Greataxes", ImVec2(0, 300), true);
                for (int i = 0; i < (int)cd.greataxes.size(); i++) {
                    bool selected = (cd.selectedMainHandWeapon == i);
                    if (ImGui::Selectable(cd.greataxes[i].second.c_str(), selected)) {
                        cd.selectedMainHandWeapon = i;
                        cd.needsRebuild = true;
                    }
                }
                ImGui::EndChild();
            }
            else if (cd.weaponStyle == 8) {
                ImGui::BeginChild("Mauls", ImVec2(0, 300), true);
                for (int i = 0; i < (int)cd.mauls.size(); i++) {
                    bool selected = (cd.selectedMainHandWeapon == i);
                    if (ImGui::Selectable(cd.mauls[i].second.c_str(), selected)) {
                        cd.selectedMainHandWeapon = i;
                        cd.needsRebuild = true;
                    }
                }
                ImGui::EndChild();
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