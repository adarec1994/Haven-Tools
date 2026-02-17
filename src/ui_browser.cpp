#include "ui_internal.h"
#include "renderer.h"
#include "terrain_loader.h"
#include "rml_loader.h"
#include "spt.h"
#include "Gff.h"
#include "GffViewer.h"
#include "LevelDatabase.h"
#include "blender_addon_embedded.h"
#include <cstring>
#include <fstream>
#include <set>

static bool exportBlenderAddon(const unsigned char* data, unsigned int size, const std::string& destDir) {
    namespace fs = std::filesystem;
    fs::path outPath = fs::path(destDir) / "havenarea_importer.zip";
    std::ofstream out(outPath, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(data), size);
    return out.good();
}

static const std::vector<LevelGame>& getLevelDB() {
    static std::vector<LevelGame> db = buildLevelDatabase();
    return db;
}

static void startLevelLoad(AppState& state, const std::string& rimPath, const std::string& displayName) {
    if (state.levelLoad.stage != 0) return;
    state.showTerrain = false;
    g_terrainLoader.clear();
    state.currentErf = std::make_unique<ERFFile>();
    if (!state.currentErf->open(rimPath)) {
        state.statusMessage = "Failed to open: " + rimPath;
        return;
    }

    state.currentRIMPath = rimPath;
    state.rimEntries.clear();
    for (size_t ei = 0; ei < state.currentErf->entries().size(); ei++) {
        CachedEntry re;
        re.name = state.currentErf->entries()[ei].name;
        re.erfIdx = 0;
        re.entryIdx = ei;
        state.rimEntries.push_back(re);
    }

    state.textureErfsLoaded = false;
    state.modelErfsLoaded = false;
    state.materialErfsLoaded = false;
    state.textureErfs.clear();
    state.modelErfs.clear();
    state.materialErfs.clear();
    clearPropCache();
    clearErfIndices();
    state.modelErfIndex.clear();
    state.materialErfIndex.clear();
    state.textureErfIndex.clear();
    ensureBaseErfsLoaded(state);

    auto rimForModel = std::make_unique<ERFFile>();
    auto rimForMat = std::make_unique<ERFFile>();
    rimForModel->open(rimPath);
    rimForMat->open(rimPath);
    state.modelErfs.push_back(std::move(rimForModel));
    state.materialErfs.push_back(std::move(rimForMat));

    std::string rimDir = fs::path(rimPath).parent_path().string();
    for (const auto& dirEntry : fs::directory_iterator(rimDir)) {
        if (!dirEntry.is_regular_file()) continue;
        std::string fnameLower = dirEntry.path().filename().string();
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
        if (dpath == rimPath) continue;
        std::string fnameLower = dirEntry.path().filename().string();
        std::transform(fnameLower.begin(), fnameLower.end(), fnameLower.begin(), ::tolower);
        if (fnameLower.size() > 4 && fnameLower.substr(fnameLower.size() - 4) == ".rim" &&
            fnameLower.find(".gpu.rim") == std::string::npos) {
            auto sibRim = std::make_unique<ERFFile>();
            if (sibRim->open(dpath))
                state.materialErfs.push_back(std::move(sibRim));
        }
    }

    for (const auto& mat : state.currentModel.materials) {
        if (mat.diffuseTexId != 0) destroyTexture(mat.diffuseTexId);
        if (mat.normalTexId != 0) destroyTexture(mat.normalTexId);
        if (mat.specularTexId != 0) destroyTexture(mat.specularTexId);
        if (mat.tintTexId != 0) destroyTexture(mat.tintTexId);
        if (mat.paletteTexId != 0) destroyTexture(mat.paletteTexId);
        if (mat.palNormalTexId != 0) destroyTexture(mat.palNormalTexId);
        if (mat.maskVTexId != 0) destroyTexture(mat.maskVTexId);
        if (mat.maskATexId != 0) destroyTexture(mat.maskATexId);
    }
    destroyLevelBuffers();
    state.envSettings = EnvironmentSettings();
    state.skyboxModel = Model();
    state.skyboxLoaded = false;
    state.currentModel = Model();
    state.currentModel.name = displayName + " (" + fs::path(rimPath).stem().string() + ")";
    state.hasModel = true;
    state.selectedLevelChunk = -1;
    state.currentModelAnimations.clear();
    state.showRIMBrowser = false;

    state.levelLoad = {};
    state.levelLoad.stage = 1;
    state.levelLoad.stageLabel = "Scanning level data...";
    state.statusMessage = "Loading: " + displayName;
}

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
        ".plt", ".ptm", ".ptt", ".qst", ".stg", ".cre", ".bic", ".cam", ".caf", ".cut", ".ldf",
        ".arl", ".opf", ".mmh", ".mao", ".phy", ".mop", ".fxa", ".tnt"
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

static void classifyCachedEntry(CachedEntry& ce) {
    ce.flags = 0;
    if (ce.name.find("__HEADER__") == 0) return;
    if (isModelFile(ce.name))  ce.flags |= CachedEntry::FLAG_MODEL;
    if (isMaoFile(ce.name))    ce.flags |= CachedEntry::FLAG_MAO;
    if (isPhyFile(ce.name))    ce.flags |= CachedEntry::FLAG_PHY;
    if (isTerrain(ce.name))    ce.flags |= CachedEntry::FLAG_TERRAIN;
    if (isGffFile(ce.name))    ce.flags |= CachedEntry::FLAG_GFF;
    if (ce.name.size() > 4) {
        std::string ext = ce.name.substr(ce.name.size() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".dds") ce.flags |= CachedEntry::FLAG_TEXTURE;
        if (ext == ".tga") ce.flags |= CachedEntry::FLAG_TEXTURE;
        if (ext == ".fsb") ce.flags |= CachedEntry::FLAG_AUDIO;
        if (ext == ".gda") ce.flags |= CachedEntry::FLAG_GDA;
    }
}

static void classifyMergedEntries(AppState& state) {
    state.contentHasTextures = false;
    state.contentHasModels = false;
    state.contentHasTerrain = false;
    for (auto& ce : state.mergedEntries) {
        classifyCachedEntry(ce);
        if (ce.flags & CachedEntry::FLAG_TEXTURE) state.contentHasTextures = true;
        if (ce.flags & CachedEntry::FLAG_MODEL)   state.contentHasModels = true;
        if (ce.flags & CachedEntry::FLAG_TERRAIN)  state.contentHasTerrain = true;
    }
    state.contentFlagsDirty = false;
}

static std::vector<uint8_t> readCachedEntryData(AppState& state, const CachedEntry& ce) {
    if (ce.erfIdx == SIZE_MAX) {
        std::ifstream f(ce.source, std::ios::binary | std::ios::ate);
        if (!f) return {};
        size_t sz = static_cast<size_t>(f.tellg());
        f.seekg(0);
        std::vector<uint8_t> data(sz);
        f.read(reinterpret_cast<char*>(data.data()), sz);
        return data;
    }
    if (ce.erfIdx >= state.erfFiles.size()) return {};
    ERFFile erf;
    if (!erf.open(state.erfFiles[ce.erfIdx])) return {};
    if (ce.entryIdx >= erf.entries().size()) return {};
    return erf.readEntry(erf.entries()[ce.entryIdx]);
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
static CachedEntry s_pendingDumpEntry;
static bool s_pendingDump = false;
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

void drawBrowserWindow(AppState& state) {
    if (state.levelLoad.stage > 0) {
        auto& ll = state.levelLoad;
        const int BATCH_SIZE = 8;

        if (ll.stage == 1) {
            buildErfIndex(state);
            ll.terrainQueue.clear();
            ll.propQueue.clear();

            for (size_t i = 0; i < state.rimEntries.size(); i++) {
                std::string nameLower = state.rimEntries[i].name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                if (nameLower.size() < 4 || nameLower.substr(nameLower.size() - 4) != ".msh") continue;

                size_t lastUnderscore = nameLower.rfind('_');
                if (lastUnderscore != std::string::npos && lastUnderscore + 1 < nameLower.size() - 4) {
                    std::string lodStr = nameLower.substr(lastUnderscore + 1, nameLower.size() - 4 - lastUnderscore - 1);
                    if (lodStr == "1" || lodStr == "2" || lodStr == "3") continue;
                }
                ll.terrainQueue.push_back(i);
            }
            ll.totalTerrain = (int)ll.terrainQueue.size();

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
                        std::string nameLow = name;
                        std::transform(nameLow.begin(), nameLow.end(), nameLow.begin(), ::tolower);
                        size_t lastUs = nameLow.rfind('_');
                        if (lastUs != std::string::npos && lastUs + 1 < nameLow.size()) {
                            std::string suffix = nameLow.substr(lastUs + 1);
                            if (suffix == "1" || suffix == "2" || suffix == "3") continue;
                        }
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
                    for (const auto& si : rml.sptInstances) {
                        AppState::SptWork sw;
                        sw.treeId = si.treeId;
                        sw.px = rml.roomPosX + si.posX;
                        sw.py = rml.roomPosY + si.posY;
                        sw.pz = rml.roomPosZ + si.posZ;
                        sw.qx = si.orientX; sw.qy = si.orientY;
                        sw.qz = si.orientZ; sw.qw = si.orientW;
                        sw.scale = si.scale;
                        ll.sptQueue.push_back(sw);
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

            std::string speedTreeErfPath;
            for (const auto& erfPath : state.erfFiles) {
                std::string fname = fs::path(erfPath).filename().string();
                std::transform(fname.begin(), fname.end(), fname.begin(), ::tolower);
                if (fname == "speedtreetools.erf") { speedTreeErfPath = erfPath; break; }
            }

            std::map<std::string, std::string> sptLowerToActual;
            if (!speedTreeErfPath.empty()) {
                ERFFile stErf;
                if (stErf.open(speedTreeErfPath)) {
                    for (const auto& entry : stErf.entries()) {
                        std::string eLower = entry.name;
                        std::transform(eLower.begin(), eLower.end(), eLower.begin(), ::tolower);
                        if (eLower.size() > 4 && eLower.substr(eLower.size() - 4) == ".spt") {
                            ll.sptFileToErf[entry.name] = speedTreeErfPath;
                            std::string stem = eLower.substr(0, eLower.size() - 4);
                            sptLowerToActual[stem] = entry.name;
                        }
                    }
                }
            }

            {
                std::string rimStem = fs::path(state.currentRIMPath).stem().string();
                std::string rimStemLower = rimStem;
                std::transform(rimStemLower.begin(), rimStemLower.end(), rimStemLower.begin(), ::tolower);
                std::string rimPrefix = rimStemLower + "_";
                std::string arlPath;
                for (const auto& arl : state.arlFiles) {
                    std::string arlStem = fs::path(arl).stem().string();
                    std::transform(arlStem.begin(), arlStem.end(), arlStem.begin(), ::tolower);
                    if (arlStem == rimStemLower) { arlPath = arl; break; }
                }
                if (!arlPath.empty()) {
                    GFFFile::initLabelCache();
                    GFFFile arlGff;
                    std::ifstream arlIn(arlPath, std::ios::binary);
                    if (arlIn) {
                        std::vector<uint8_t> arlData((std::istreambuf_iterator<char>(arlIn)),
                                                      std::istreambuf_iterator<char>());
                        if (arlGff.load(arlData)) {
                            int areaIdx = -1;
                            for (size_t si = 0; si < arlGff.structs().size(); si++) {
                                if (std::string(arlGff.structs()[si].structType, 4) == "AREA") {
                                    areaIdx = (int)si;
                                    break;
                                }
                            }
                            if (areaIdx >= 0) {
                                auto treeList = arlGff.readStructList(areaIdx, 3355, 0);
                                for (int i = 0; i < (int)treeList.size(); i++) {
                                    std::string treeName = arlGff.readStringByLabel(
                                        treeList[i].structIndex, 3353, treeList[i].offset);
                                    if (treeName.empty()) continue;
                                    std::string treeNameLower = treeName;
                                    std::transform(treeNameLower.begin(), treeNameLower.end(), treeNameLower.begin(), ::tolower);
                                    std::string stripped = treeNameLower;
                                    if (stripped.size() > rimPrefix.size() &&
                                        stripped.substr(0, rimPrefix.size()) == rimPrefix) {
                                        stripped = stripped.substr(rimPrefix.size());
                                    }
                                    auto it = sptLowerToActual.find(stripped);
                                    if (it != sptLowerToActual.end()) {
                                        ll.sptIdToFile[i] = it->second;
                                    }
                                }

                                auto& env = state.envSettings;
                                env = EnvironmentSettings();

                                auto readVec3 = [&](uint32_t si, uint32_t label, uint32_t base, float* out) {
                                    const GFFField* f = arlGff.findField(si, label);
                                    if (!f) return;
                                    uint32_t pos = arlGff.dataOffset() + base + f->dataOffset;
                                    out[0] = arlGff.readFloatAt(pos);
                                    out[1] = arlGff.readFloatAt(pos + 4);
                                    out[2] = arlGff.readFloatAt(pos + 8);
                                };
                                auto readColor4 = [&](uint32_t si, uint32_t label, uint32_t base, float* out) {
                                    const GFFField* f = arlGff.findField(si, label);
                                    if (!f) return;
                                    uint32_t pos = arlGff.dataOffset() + base + f->dataOffset;
                                    out[0] = arlGff.readFloatAt(pos);
                                    out[1] = arlGff.readFloatAt(pos + 4);
                                    out[2] = arlGff.readFloatAt(pos + 8);
                                    out[3] = arlGff.readFloatAt(pos + 12);
                                };

                                env.skydomeModel = arlGff.readStringByLabel(areaIdx, 3025, 0);
                                readVec3(areaIdx, 3024, 0, env.areaCenter);
                                readVec3(areaIdx, 3150, 0, env.sunDirection);
                                float sunCol4[4] = {1,1,1,1};
                                readColor4(areaIdx, 3152, 0, sunCol4);
                                env.sunColor[0] = sunCol4[0]; env.sunColor[1] = sunCol4[1]; env.sunColor[2] = sunCol4[2];
                                readColor4(areaIdx, 3149, 0, env.sunColorChar);

                                const GFFField* atmoField = arlGff.findField(areaIdx, 22500);
                                if (atmoField && (atmoField->flags & 0x4000)) {
                                    uint32_t atmoSI = atmoField->typeId;
                                    uint32_t atmoBase = atmoField->dataOffset;
                                    readVec3(atmoSI, 22519, atmoBase, env.atmoSunColor);
                                    env.atmoSunIntensity = arlGff.readFloatByLabel(atmoSI, 22520, atmoBase);
                                    env.atmoDistanceMultiplier = arlGff.readFloatByLabel(atmoSI, 22526, atmoBase);
                                    env.atmoAlpha = arlGff.readFloatByLabel(atmoSI, 22528, atmoBase);
                                    readVec3(atmoSI, 22529, atmoBase, env.atmoFogColor);
                                    env.atmoFogIntensity = arlGff.readFloatByLabel(atmoSI, 22530, atmoBase);
                                    env.atmoFogCap = arlGff.readFloatByLabel(atmoSI, 22531, atmoBase);
                                    env.atmoFogZenith = arlGff.readFloatByLabel(atmoSI, 22532, atmoBase);
                                    env.moonScale = arlGff.readFloatByLabel(atmoSI, 22700, atmoBase);
                                    env.moonAlpha = arlGff.readFloatByLabel(atmoSI, 22701, atmoBase);
                                    env.moonRotation = arlGff.readFloatByLabel(atmoSI, 22703, atmoBase);
                                }

                                const GFFField* cldsField = arlGff.findField(areaIdx, 22600);
                                if (cldsField && (cldsField->flags & 0x4000)) {
                                    uint32_t cldsSI = cldsField->typeId;
                                    uint32_t cldsBase = cldsField->dataOffset;
                                    env.cloudDensity = arlGff.readFloatByLabel(cldsSI, 22620, cldsBase);
                                    env.cloudSharpness = arlGff.readFloatByLabel(cldsSI, 22621, cldsBase);
                                    env.cloudDepth = arlGff.readFloatByLabel(cldsSI, 22622, cldsBase);
                                    env.cloudRange1 = arlGff.readFloatByLabel(cldsSI, 22623, cldsBase);
                                    env.cloudRange2 = arlGff.readFloatByLabel(cldsSI, 22624, cldsBase);
                                    readVec3(cldsSI, 22625, cldsBase, env.cloudColor);
                                }

                                env.loaded = true;
                                std::cout << "[ARL] Environment: sky='" << env.skydomeModel
                                          << "' sunDir=(" << env.sunDirection[0] << "," << env.sunDirection[1] << "," << env.sunDirection[2]
                                          << ") atmoSun=(" << env.atmoSunColor[0] << "," << env.atmoSunColor[1] << "," << env.atmoSunColor[2]
                                          << ") fog=(" << env.atmoFogColor[0] << "," << env.atmoFogColor[1] << "," << env.atmoFogColor[2]
                                          << ") clouds=" << env.cloudDensity << std::endl;

                                if (!env.skydomeModel.empty()) {
                                    std::string skyMshName = env.skydomeModel + ".msh";
                                    std::string skyMshLower = skyMshName;
                                    std::transform(skyMshLower.begin(), skyMshLower.end(), skyMshLower.begin(), ::tolower);
                                    std::vector<uint8_t> skyData;
                                    if (state.currentErf) {
                                        for (const auto& e : state.currentErf->entries()) {
                                            std::string eLower = e.name;
                                            std::transform(eLower.begin(), eLower.end(), eLower.begin(), ::tolower);
                                            if (eLower == skyMshLower) { skyData = state.currentErf->readEntry(e); break; }
                                        }
                                    }
                                    if (skyData.empty()) {
                                        for (const auto& erf : state.modelErfs) {
                                            for (const auto& e : erf->entries()) {
                                                std::string eLower = e.name;
                                                std::transform(eLower.begin(), eLower.end(), eLower.begin(), ::tolower);
                                                if (eLower == skyMshLower) { skyData = erf->readEntry(e); break; }
                                            }
                                            if (!skyData.empty()) break;
                                        }
                                    }
                                    if (!skyData.empty()) {
                                        Model skyModel;
                                        if (loadMSH(skyData, skyModel)) {
                                            std::string skyMmhName = env.skydomeModel + ".mmh";
                                            std::string skyMmhLower = skyMmhName;
                                            std::transform(skyMmhLower.begin(), skyMmhLower.end(), skyMmhLower.begin(), ::tolower);
                                            std::vector<uint8_t> mmhData;
                                            if (state.currentErf) {
                                                for (const auto& e : state.currentErf->entries()) {
                                                    std::string eLower = e.name;
                                                    std::transform(eLower.begin(), eLower.end(), eLower.begin(), ::tolower);
                                                    if (eLower == skyMmhLower) { mmhData = state.currentErf->readEntry(e); break; }
                                                }
                                            }
                                            if (mmhData.empty()) {
                                                for (const auto& erf : state.modelErfs) {
                                                    for (const auto& e : erf->entries()) {
                                                        std::string eLower = e.name;
                                                        std::transform(eLower.begin(), eLower.end(), eLower.begin(), ::tolower);
                                                        if (eLower == skyMmhLower) { mmhData = erf->readEntry(e); break; }
                                                    }
                                                    if (!mmhData.empty()) break;
                                                }
                                            }
                                            if (!mmhData.empty()) {
                                                loadMMH(mmhData, skyModel);
                                                std::cout << "[ARL] Loaded skybox MMH, " << skyModel.meshes.size() << " meshes" << std::endl;
                                                for (size_t mi = 0; mi < skyModel.meshes.size(); mi++)
                                                    std::cout << "[ARL]   mesh[" << mi << "] mat='" << skyModel.meshes[mi].materialName << "'" << std::endl;
                                            } else {
                                                std::cout << "[ARL] Skybox MMH '" << skyMmhName << "' not found" << std::endl;
                                            }
                                            finalizeModelMaterials(state, skyModel);
                                            for (size_t mi = 0; mi < skyModel.materials.size(); mi++) {
                                                auto& mat = skyModel.materials[mi];
                                                std::cout << "[ARL] Skybox material[" << mi << "] '" << mat.name
                                                          << "' diffuse='" << mat.diffuseMap << "' texId=" << mat.diffuseTexId;
                                                if (mat.diffuseMap.empty())
                                                    std::cout << " [WARNING: no diffuse map from MAO - check MAO field names]";
                                                if (!mat.diffuseMap.empty() && mat.diffuseTexId == 0)
                                                    std::cout << " [WARNING: texture not found in ERFs]";
                                                std::cout << std::endl;
                                            }
                                            float skyScale = 50000.0f;
                                            for (auto& mesh : skyModel.meshes) {
                                                for (auto& v : mesh.vertices) {
                                                    v.x *= skyScale; v.y *= skyScale; v.z *= skyScale;
                                                }
                                                mesh.calculateBounds();
                                            }
                                            state.skyboxModel = skyModel;
                                            state.skyboxLoaded = true;
                                            std::cout << "[ARL] Loaded skybox '" << env.skydomeModel
                                                      << "' (" << skyModel.meshes.size() << " meshes, "
                                                      << skyModel.materials.size() << " materials)" << std::endl;
                                        } else {
                                            std::cout << "[ARL] Failed to parse skybox mesh '" << skyMshName << "'" << std::endl;
                                        }
                                    } else {
                                        std::cout << "[ARL] Skybox mesh '" << skyMshName << "' not found in ERFs" << std::endl;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (!speedTreeErfPath.empty()) {
                auto stTexErf = std::make_unique<ERFFile>();
                if (stTexErf->open(speedTreeErfPath))
                    state.textureErfs.push_back(std::move(stTexErf));
            }

            ll.totalProps = (int)ll.propQueue.size();

            {
                std::set<uint64_t> seen;
                std::vector<AppState::SptWork> deduped;
                deduped.reserve(ll.sptQueue.size());
                for (const auto& sw : ll.sptQueue) {
                    uint32_t hx = *reinterpret_cast<const uint32_t*>(&sw.px);
                    uint32_t hy = *reinterpret_cast<const uint32_t*>(&sw.py);
                    uint32_t hz = *reinterpret_cast<const uint32_t*>(&sw.pz);
                    uint64_t key = ((uint64_t)(hx ^ (hy << 16) ^ (hz >> 3)) << 32) | (uint32_t)sw.treeId;
                    if (seen.insert(key).second)
                        deduped.push_back(sw);
                }
                ll.sptQueue = std::move(deduped);
            }

            ll.totalSpt = (int)ll.sptQueue.size();

            state.modelErfIndex.build(state.modelErfs);
            state.materialErfIndex.build(state.materialErfs);
            state.textureErfIndex.build(state.textureErfs);
            registerErfIndex(&state.modelErfs, &state.modelErfIndex);
            registerErfIndex(&state.materialErfs, &state.materialErfIndex);
            registerErfIndex(&state.textureErfs, &state.textureErfIndex);

            std::sort(ll.propQueue.begin(), ll.propQueue.end(),
                [](const AppState::PropWork& a, const AppState::PropWork& b) {
                    return a.modelName < b.modelName;
                });

            ll.itemIndex = 0;
            ll.stage = 2;
            ll.stageLabel = "Loading terrain...";
        }
        else if (ll.stage == 2) {
            if (!state.currentErf) { ll.stage = 0; ll.stageLabel = ""; }
            else {
            int processed = 0;
            for (int i = ll.itemIndex; i < ll.totalTerrain && processed < BATCH_SIZE; i++) {
                size_t rimIdx = ll.terrainQueue[i];
                if (rimIdx < state.rimEntries.size() && state.rimEntries[rimIdx].entryIdx < state.currentErf->entries().size()) {
                    const auto& erfEntry = state.currentErf->entries()[state.rimEntries[rimIdx].entryIdx];
                    size_t meshCountBefore = state.currentModel.meshes.size();
                    if (mergeModelEntry(state, erfEntry)) {
                        ll.terrainLoaded++;
                        for (size_t mi = meshCountBefore; mi < state.currentModel.meshes.size(); mi++) {
                            if (state.currentModel.meshes[mi].name.empty()) {
                                std::string displayName = erfEntry.name;
                                auto dot = displayName.rfind('.');
                                if (dot != std::string::npos) displayName = displayName.substr(0, dot);
                                state.currentModel.meshes[mi].name = displayName;
                            }
                        }
                    }
                }
                ll.itemIndex = i + 1;
                processed++;
            }
            if (ll.itemIndex >= ll.totalTerrain) {
                ll.itemIndex = 0;
                ll.stage = 3;
                ll.stageLabel = "Loading props...";
            }
            }
        }
        else if (ll.stage == 3) {
            const int PROP_BATCH_SIZE = 32;
            int processed = 0;
            for (int i = ll.itemIndex; i < ll.totalProps && processed < PROP_BATCH_SIZE; i++) {
                const auto& pw = ll.propQueue[i];
                size_t meshCountBefore = state.currentModel.meshes.size();
                if (mergeModelByName(state, pw.modelName, pw.px, pw.py, pw.pz,
                                     pw.qx, pw.qy, pw.qz, pw.qw, pw.scale)) {
                    ll.propsLoaded++;
                    for (size_t mi = meshCountBefore; mi < state.currentModel.meshes.size(); mi++) {
                        if (state.currentModel.meshes[mi].name.empty()) {
                            std::string displayName = pw.modelName;
                            auto dot = displayName.rfind('.');
                            if (dot != std::string::npos) displayName = displayName.substr(0, dot);
                            state.currentModel.meshes[mi].name = displayName;
                        }
                    }
                } else {
                    std::cout << "[LEVEL LOAD] Missing MSH: " << pw.modelName << std::endl;
                }
                ll.itemIndex = i + 1;
                processed++;
            }
            if (ll.itemIndex >= ll.totalProps) {
                ll.itemIndex = 0;
                ll.stage = 4;
                ll.stageLabel = "Loading trees...";
            }
        }
        else if (ll.stage == 4) {
            if (!ll.sptFileToErf.empty() && !ll.sptIdToFile.empty() && !ll.sptSetupDone) {
                std::map<std::string, std::unique_ptr<ERFFile>> openErfs;
                auto getErf = [&](const std::string& sptFile) -> ERFFile* {
                    auto it = ll.sptFileToErf.find(sptFile);
                    if (it == ll.sptFileToErf.end()) return nullptr;
                    auto& ptr = openErfs[it->second];
                    if (!ptr) {
                        ptr = std::make_unique<ERFFile>();
                        if (!ptr->open(it->second)) { ptr.reset(); return nullptr; }
                    }
                    return ptr.get();
                };

                for (const auto& [treeId, fileName] : ll.sptIdToFile) {
                    ERFFile* erf = getErf(fileName);
                    if (!erf) continue;
                    for (const auto& entry : erf->entries()) {
                        if (entry.name == fileName) {
                            auto sptData = erf->readEntry(entry);
                            if (!sptData.empty()) {
                                std::string tempDir;
                                #ifdef _WIN32
                                char tmp[MAX_PATH]; GetTempPathA(MAX_PATH, tmp); tempDir = tmp;
                                #else
                                tempDir = "/tmp/";
                                #endif
                                std::string tempSpt = tempDir + "haven_level_temp.spt";
                                { std::ofstream f(tempSpt, std::ios::binary);
                                  f.write((char*)sptData.data(), sptData.size()); }
                                SptModel model;
                                if (loadSptModel(tempSpt, model)) {
                                    extractSptTextures(sptData, model);
                                    ll.sptCache[treeId] = std::move(model);
                                }
                                #ifdef _WIN32
                                DeleteFileA(tempSpt.c_str());
                                #else
                                remove(tempSpt.c_str());
                                #endif
                            }
                            break;
                        }
                    }
                }

                auto stripExt = [](const std::string& s) -> std::string {
                    size_t d = s.rfind('.');
                    return (d != std::string::npos) ? s.substr(0, d) : s;
                };

                for (const auto& [treeId, treeModel] : ll.sptCache) {
                    auto fit = ll.sptIdToFile.find(treeId);
                    if (fit == ll.sptIdToFile.end()) continue;
                    std::string baseName = stripExt(fit->second);
                    ll.sptBaseName[treeId] = baseName;

                    ERFFile* erf = getErf(fit->second);
                    std::string branchKey = treeModel.branchTexture.empty() ? baseName : stripExt(treeModel.branchTexture);
                    int branchMatIdx = -1;
                    for (int mi = 0; mi < (int)state.currentModel.materials.size(); mi++)
                        if (state.currentModel.materials[mi].diffuseMap == branchKey) { branchMatIdx = mi; break; }
                    if (branchMatIdx < 0) {
                        Material mat;
                        mat.name = branchKey;
                        mat.diffuseMap = branchKey;
                        mat.opacity = 1.0f;
                        if (erf) {
                            std::string keyLower = branchKey;
                            std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);
                            for (const auto& te : erf->entries()) {
                                std::string eLower = te.name;
                                std::transform(eLower.begin(), eLower.end(), eLower.begin(), ::tolower);
                                if (eLower == keyLower + ".tga") {
                                    auto tgaData = erf->readEntry(te);
                                    if (!tgaData.empty()) {
                                        std::vector<uint8_t> rgba; int w, h;
                                        if (decodeTGAToRGBA(tgaData, rgba, w, h)) {
                                            mat.diffuseTexId = createTexture2D(rgba.data(), w, h);
                                            mat.diffuseData = std::move(rgba);
                                            mat.diffuseWidth = w;
                                            mat.diffuseHeight = h;
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                        branchMatIdx = (int)state.currentModel.materials.size();
                        state.currentModel.materials.push_back(std::move(mat));
                    }
                    ll.sptBranchMatIdx[treeId] = branchMatIdx;

                    std::string diffuseKey = baseName + "_diffuse";
                    int ddsMatIdx = -1;
                    for (int mi = 0; mi < (int)state.currentModel.materials.size(); mi++)
                        if (state.currentModel.materials[mi].diffuseMap == diffuseKey) { ddsMatIdx = mi; break; }
                    if (ddsMatIdx < 0) {
                        Material mat;
                        mat.name = diffuseKey;
                        mat.diffuseMap = diffuseKey;
                        mat.opacity = 1.0f;
                        ddsMatIdx = (int)state.currentModel.materials.size();
                        state.currentModel.materials.push_back(std::move(mat));
                    }
                    ll.sptDdsMatIdx[treeId] = ddsMatIdx;
                }
                ll.sptSetupDone = true;
            }
            ll.itemIndex = 0;
            ll.sptLoaded = 0;

            if (!ll.sptCache.empty()) {
                size_t estimatedNewMeshes = 0;
                for (const auto& sw : ll.sptQueue) {
                    auto cit = ll.sptCache.find(sw.treeId);
                    if (cit != ll.sptCache.end())
                        estimatedNewMeshes += cit->second.submeshes.size();
                }
                state.currentModel.meshes.reserve(
                    state.currentModel.meshes.size() + estimatedNewMeshes);
            }
            ll.stage = 5;
            ll.stageLabel = "Placing trees...";
        }
        else if (ll.stage == 5) {

            const int SPT_BATCH_SIZE = 200;
            if (!ll.sptSetupDone || ll.sptCache.empty()) {
                ll.stage = 6;
                ll.stageLabel = "Loading materials & textures...";
            } else {
                const char* typeNames[] = {"Branch", "Frond", "LeafCard", "LeafMesh"};
                int processed = 0;
                for (int i = ll.itemIndex; i < (int)ll.sptQueue.size() && processed < SPT_BATCH_SIZE; i++) {
                    const auto& sw = ll.sptQueue[i];
                    auto cit = ll.sptCache.find(sw.treeId);
                    if (cit == ll.sptCache.end()) {
                        ll.itemIndex = i + 1;
                        processed++;
                        continue;
                    }

                    float qx = sw.qx, qy = sw.qy, qz = sw.qz, qw = sw.qw;
                    float qlen = std::sqrt(qx*qx + qy*qy + qz*qz + qw*qw);
                    if (qlen > 0.00001f) { qx/=qlen; qy/=qlen; qz/=qlen; qw/=qlen; }

                    const std::string& baseName = ll.sptBaseName[sw.treeId];
                    int branchMatIdx = ll.sptBranchMatIdx[sw.treeId];
                    int ddsMatIdx = ll.sptDdsMatIdx[sw.treeId];

                    for (int si = 0; si < (int)cit->second.submeshes.size(); si++) {
                        const auto& sm = cit->second.submeshes[si];
                        if (sm.vertexCount() == 0) continue;

                        Mesh mesh;
                        mesh.name = baseName + "_" + typeNames[(int)sm.type];
                        if (sm.type == SptSubmeshType::Branch) {
                            mesh.materialIndex = branchMatIdx;
                            mesh.materialName = state.currentModel.materials[branchMatIdx].name;
                            mesh.alphaTest = false;
                        } else {
                            mesh.materialIndex = ddsMatIdx;
                            mesh.materialName = state.currentModel.materials[ddsMatIdx].name;
                            mesh.alphaTest = true;
                        }

                        uint32_t nv = sm.vertexCount();
                        mesh.vertices.resize(nv);
                        for (uint32_t vi = 0; vi < nv; vi++) {
                            float lx = sm.positions[vi*3+0] * sw.scale;
                            float ly = sm.positions[vi*3+1] * sw.scale;
                            float lz = sm.positions[vi*3+2] * sw.scale;
                            float tx = 2.0f*(qy*lz - qz*ly);
                            float ty = 2.0f*(qz*lx - qx*lz);
                            float tz = 2.0f*(qx*ly - qy*lx);

                            auto& v = mesh.vertices[vi];
                            v.x = lx + qw*tx + (qy*tz - qz*ty) + sw.px;
                            v.y = ly + qw*ty + (qz*tx - qx*tz) + sw.py;
                            v.z = lz + qw*tz + (qx*ty - qy*tx) + sw.pz;

                            float nx = sm.normals[vi*3+0];
                            float ny = sm.normals[vi*3+1];
                            float nz = sm.normals[vi*3+2];
                            float tnx = 2.0f*(qy*nz - qz*ny);
                            float tny = 2.0f*(qz*nx - qx*nz);
                            float tnz = 2.0f*(qx*ny - qy*nx);
                            v.nx = nx + qw*tnx + (qy*tnz - qz*tny);
                            v.ny = ny + qw*tny + (qz*tnx - qx*tnz);
                            v.nz = nz + qw*tnz + (qx*tny - qy*tnx);
                            v.u = sm.texcoords[vi*2+0];
                            v.v = sm.texcoords[vi*2+1];
                        }
                        mesh.indices = sm.indices;
                        mesh.calculateBounds();
                        state.currentModel.meshes.push_back(std::move(mesh));
                    }
                    ll.sptLoaded++;
                    ll.itemIndex = i + 1;
                    processed++;
                }

                if (ll.itemIndex >= (int)ll.sptQueue.size()) {
                    ll.sptCache.clear();
                    ll.sptBranchMatIdx.clear();
                    ll.sptDdsMatIdx.clear();
                    ll.sptBaseName.clear();

                    ll.stage = 6;
                    ll.stageLabel = "Loading materials & textures...";
                }
            }
        }
        else if (ll.stage == 6) {
            finalizeLevelMaterials(state);
            bakeLevelBuffers(state.currentModel);

            if (!state.currentModel.meshes.empty()) {

                float minX = 1e30f, maxX = -1e30f;
                float minY = 1e30f, maxY = -1e30f;
                float minZ = 1e30f, maxZ = -1e30f;
                bool hasVerts = false;
                for (const auto& mesh : state.currentModel.meshes) {
                    if (mesh.vertices.empty()) continue;
                    hasVerts = true;
                    if (mesh.minX < minX) minX = mesh.minX;
                    if (mesh.maxX > maxX) maxX = mesh.maxX;
                    if (mesh.minY < minY) minY = mesh.minY;
                    if (mesh.maxY > maxY) maxY = mesh.maxY;
                    if (mesh.minZ < minZ) minZ = mesh.minZ;
                    if (mesh.maxZ > maxZ) maxZ = mesh.maxZ;
                }
                if (hasVerts) {
                    float cx = (minX + maxX) / 2.0f;
                    float cy = (minY + maxY) / 2.0f;
                    float cz = (minZ + maxZ) / 2.0f;
                    float dx = maxX - minX, dy = maxY - minY, dz = maxZ - minZ;
                    float radius = std::sqrt(dx*dx + dy*dy + dz*dz) / 2.0f;
                    if (radius < 1.0f) radius = 1.0f;
                    state.camera.lookAt(cx, cy, cz, radius * 1.5f);
                    state.camera.moveSpeed = radius * 0.05f;
                    if (state.camera.moveSpeed < 1.0f) state.camera.moveSpeed = 1.0f;
                }
            }

            state.statusMessage = "Loaded level: " + std::to_string(ll.terrainLoaded) + " terrain, " +
                std::to_string(ll.propsLoaded) + " props (" +
                std::to_string(ll.totalProps - ll.propsLoaded) + " missing), " +
                std::to_string(ll.sptLoaded) + " trees, " +
                std::to_string(state.currentModel.materials.size()) + " materials";
            state.showRenderSettings = true;
            clearErfIndices();
            ll.stage = 0;
        }

        if (ll.stage > 0) {
            float progress = 0.0f;
            std::string detail;
            if (ll.stage == 1) {
                progress = 0.0f;
                detail = "Scanning level data...";
            } else if (ll.stage == 2) {
                progress = ll.totalTerrain > 0 ? (float)ll.itemIndex / ll.totalTerrain : 0.0f;
                detail = std::to_string(ll.itemIndex) + " / " + std::to_string(ll.totalTerrain) + " terrain";
            } else if (ll.stage == 3) {
                progress = ll.totalProps > 0 ? (float)ll.itemIndex / ll.totalProps : 0.0f;
                detail = std::to_string(ll.itemIndex) + " / " + std::to_string(ll.totalProps) + " props";
            } else if (ll.stage == 4) {
                progress = 0.3f;
                detail = "Setting up " + std::to_string(ll.sptIdToFile.size()) + " tree types...";
            } else if (ll.stage == 5) {
                progress = ll.totalSpt > 0 ? (float)ll.itemIndex / ll.totalSpt : 0.5f;
                detail = std::to_string(ll.sptLoaded) + " / " + std::to_string(ll.totalSpt) + " trees";
            } else if (ll.stage == 6) {
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
        return;
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
        if (ImGui::Button("Add Ons")) {
            ImGui::OpenPopup("AddOnsPopup");
        }
        if (ImGui::BeginPopup("AddOnsPopup")) {
            if (ImGui::MenuItem("Export Blender Importer")) {
                IGFD::FileDialogConfig config;
                config.path = state.lastDialogPath.empty() ? "." : state.lastDialogPath;
                ImGuiFileDialog::Instance()->OpenDialog("ExportBlenderAddon", "Select Output Folder", nullptr, config);
            }
            ImGui::EndPopup();
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
    if (!state.selectedFolder.empty()) {
        bool levelsSelected = (state.selectedErfName == "[Levels]");
        if (ImGui::Selectable("Levels", levelsSelected)) {
            if (!levelsSelected) {
                state.selectedErfName = "[Levels]";
                state.selectedEntryIndex = -1;
                state.mergedEntries.clear();
                state.contentFlagsDirty = true;
                state.filteredEntryIndices.clear();
                state.lastContentFilter = "\x01_REBUILD";
                state.showRIMBrowser = false;
                state.rimEntries.clear();
                state.showFSBBrowser = false;
                state.currentFSBSamples.clear();
            }
        }
    }

    ImGui::Separator();

        if (!state.audioFilesLoaded && !state.selectedFolder.empty()) {
        scanAudioFiles(state);
    }
    bool audioSelected = (state.selectedErfName == "[Audio]");
    if (ImGui::Selectable("Audio - Sound Effects", audioSelected)) {
        if (!audioSelected) {
            state.selectedErfName = "[Audio]";
            state.selectedEntryIndex = -1;
            state.mergedEntries.clear();
                state.contentFlagsDirty = true;
            state.filteredEntryIndices.clear();
            state.lastContentFilter = "\x01_REBUILD";
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
                state.contentFlagsDirty = true;
            state.filteredEntryIndices.clear();
            state.lastContentFilter = "\x01_REBUILD";
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
                state.contentFlagsDirty = true;
                state.filteredEntryIndices.clear();
                state.lastContentFilter = "\x01_REBUILD";
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
                state.lastContentFilter = "\x01_REBUILD";
            }
            if (coreCount > 0) {
                snprintf(label, sizeof(label), "Core (%d)", coreCount);
                if (ImGui::RadioButton(label, s_meshDataSourceFilter == 1)) {
                    s_meshDataSourceFilter = 1;
                    state.filteredEntryIndices.clear();
                    state.lastContentFilter = "\x01_REBUILD";
                }
            }
            if (awakCount > 0) {
                snprintf(label, sizeof(label), "Awakening (%d)", awakCount);
                if (ImGui::RadioButton(label, s_meshDataSourceFilter == 2)) {
                    s_meshDataSourceFilter = 2;
                    state.filteredEntryIndices.clear();
                    state.lastContentFilter = "\x01_REBUILD";
                }
            }
            if (modsCount > 0) {
                snprintf(label, sizeof(label), "Mods (%d)", modsCount);
                if (ImGui::RadioButton(label, s_meshDataSourceFilter == 3)) {
                    s_meshDataSourceFilter = 3;
                    state.filteredEntryIndices.clear();
                    state.lastContentFilter = "\x01_REBUILD";
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
                state.lastContentFilter = "\x01_REBUILD";
            }
            if (coreCount > 0) {
                snprintf(label, sizeof(label), "Core (%d)", coreCount);
                if (ImGui::RadioButton(label, s_hierDataSourceFilter == 1)) {
                    s_hierDataSourceFilter = 1;
                    state.filteredEntryIndices.clear();
                    state.lastContentFilter = "\x01_REBUILD";
                }
            }
            if (awakCount > 0) {
                snprintf(label, sizeof(label), "Awakening (%d)", awakCount);
                if (ImGui::RadioButton(label, s_hierDataSourceFilter == 2)) {
                    s_hierDataSourceFilter = 2;
                    state.filteredEntryIndices.clear();
                    state.lastContentFilter = "\x01_REBUILD";
                }
            }
            if (modsCount > 0) {
                snprintf(label, sizeof(label), "Mods (%d)", modsCount);
                if (ImGui::RadioButton(label, s_hierDataSourceFilter == 3)) {
                    s_hierDataSourceFilter = 3;
                    state.filteredEntryIndices.clear();
                    state.lastContentFilter = "\x01_REBUILD";
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

    if (!state.rimFiles.empty() || !state.arlFiles.empty() || !state.opfFiles.empty()) {
        ImGui::Separator();
        bool rimSelected = (state.selectedErfName == "[Env]");
        size_t envTotal = state.rimFiles.size() + state.arlFiles.size() + state.opfFiles.size();
        char rimLabel[64];
        if (state.rimScanDone)
            snprintf(rimLabel, sizeof(rimLabel), "Env (%zu)", envTotal);
        else
            snprintf(rimLabel, sizeof(rimLabel), "Env (%zu) [scanning...]", envTotal);

        auto buildRimList = [&]() {
            state.mergedEntries.clear();
                state.contentFlagsDirty = true;
            state.filteredEntryIndices.clear();
            state.lastContentFilter = "\x01_REBUILD";

            for (size_t i = 0; i < state.rimFiles.size(); i++) {
                CachedEntry ce;
                size_t lastSlash = state.rimFiles[i].find_last_of("/\\");
                ce.name = (lastSlash != std::string::npos) ? state.rimFiles[i].substr(lastSlash + 1) : state.rimFiles[i];
                ce.erfIdx = i;
                ce.entryIdx = 0;
                state.mergedEntries.push_back(ce);
            }
            for (size_t i = 0; i < state.arlFiles.size(); i++) {
                CachedEntry ce;
                size_t lastSlash = state.arlFiles[i].find_last_of("/\\");
                ce.name = (lastSlash != std::string::npos) ? state.arlFiles[i].substr(lastSlash + 1) : state.arlFiles[i];
                ce.erfIdx = i;
                ce.entryIdx = 1;
                state.mergedEntries.push_back(ce);
            }
            for (size_t i = 0; i < state.opfFiles.size(); i++) {
                CachedEntry ce;
                size_t lastSlash = state.opfFiles[i].find_last_of("/\\");
                ce.name = (lastSlash != std::string::npos) ? state.opfFiles[i].substr(lastSlash + 1) : state.opfFiles[i];
                ce.erfIdx = i;
                ce.entryIdx = 2;
                state.mergedEntries.push_back(ce);
            }
            state.statusMessage = std::to_string(envTotal) + " env files";
        };

        if (ImGui::Selectable(rimLabel, rimSelected)) {
            if (!rimSelected) {
                state.selectedErfName = "[Env]";
                state.selectedEntryIndex = -1;
                state.showRIMBrowser = false;
                state.rimEntries.clear();
                state.showFSBBrowser = false;
                state.currentFSBSamples.clear();
                buildRimList();
            }
        }
    }

    if (!state.selectedFolder.empty()) {
        ImGui::Separator();
        bool overrideSelected = (state.selectedErfName == "[Override]");
        if (ImGui::Selectable("Override Folder", overrideSelected)) {
            if (!overrideSelected) {
                state.selectedErfName = "[Override]";
                state.selectedEntryIndex = -1;
                state.mergedEntries.clear();
                state.contentFlagsDirty = true;
                state.filteredEntryIndices.clear();
                state.lastContentFilter = "\x01_REBUILD";
                state.showRIMBrowser = false;
                state.rimEntries.clear();
                state.showFSBBrowser = false;
                state.currentFSBSamples.clear();
                fs::path overrideDir = fs::path(state.selectedFolder) / "packages" / "core" / "override";
                if (fs::exists(overrideDir) && fs::is_directory(overrideDir)) {
                    for (const auto& entry : fs::recursive_directory_iterator(overrideDir,
                             fs::directory_options::skip_permission_denied)) {
                        if (entry.is_regular_file()) {
                            CachedEntry ce;
                            fs::path relPath = fs::relative(entry.path(), overrideDir);
                            ce.name = relPath.string();
                            std::replace(ce.name.begin(), ce.name.end(), '\\', '/');
                            ce.erfIdx = SIZE_MAX;
                            ce.entryIdx = 0;
                            ce.source = entry.path().string();
                            state.mergedEntries.push_back(ce);
                        }
                    }
                    std::sort(state.mergedEntries.begin(), state.mergedEntries.end(),
                        [](const CachedEntry& a, const CachedEntry& b) { return a.name < b.name; });
                    state.statusMessage = std::to_string(state.mergedEntries.size()) + " files in override";
                } else {
                    state.statusMessage = "Override folder not found";
                }
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
    if (state.selectedErfName == "[Levels]") {
        ImGui::Text("Levels");
        ImGui::Separator();
        ImGui::BeginChild("LevelTree", ImVec2(0, 0), true);
        const auto& db = getLevelDB();

        static std::unordered_map<std::string, std::string> s_rimLookup;
        static size_t s_lastRimCount = 0;
        if (s_rimLookup.empty() || s_lastRimCount != state.rimFiles.size()) {
            s_rimLookup.clear();
            for (const auto& rimPath : state.rimFiles) {
                std::string stem = fs::path(rimPath).stem().string();
                std::transform(stem.begin(), stem.end(), stem.begin(), ::tolower);
                s_rimLookup[stem] = rimPath;
            }
            s_lastRimCount = state.rimFiles.size();
        }

        auto lookupRim = [&](const std::string& prefix) -> std::string {
            std::string key = prefix;
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            auto it = s_rimLookup.find(key);
            return (it != s_rimLookup.end()) ? it->second : "";
        };

        auto drawEntry = [&](const LevelEntry& entry, const char* idSuffix) {
            std::string rimPath = lookupRim(entry.rimPrefix);
            bool found = !rimPath.empty();
            if (!found) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            char label[256];
            snprintf(label, sizeof(label), "      %s%s", entry.displayName.c_str(), idSuffix);
            if (ImGui::Selectable(label, false, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::IsMouseDoubleClicked(0) && found) {
                    startLevelLoad(state, rimPath, entry.displayName);
                }
            }
            if (found) {
                char ctxId[64];
                snprintf(ctxId, sizeof(ctxId), "##lvlCtx%s", idSuffix);
                if (ImGui::BeginPopupContextItem(ctxId)) {
                    if (ImGui::MenuItem("Show Heightmap")) {
                        ERFFile rim;
                        if (rim.open(rimPath)) {
                            float minX = 1e30f, maxX = -1e30f;
                            float minY = 1e30f, maxY = -1e30f;
                            float minZ = 1e30f, maxZ = -1e30f;
                            struct HmVert { float x, y, z; };
                            std::vector<std::array<HmVert, 3>> tris;

                            for (const auto& re : rim.entries()) {
                                std::string eLower = re.name;
                                std::transform(eLower.begin(), eLower.end(), eLower.begin(), ::tolower);
                                size_t dotPos = eLower.rfind('.');
                                if (dotPos == std::string::npos) continue;
                                std::string ext = eLower.substr(dotPos);
                                if (ext != ".msh") continue;
                                auto data = rim.readEntry(re);
                                if (data.empty()) continue;
                                Model tmp;
                                if (!loadMSH(data, tmp)) continue;
                                for (const auto& m : tmp.meshes) {
                                    for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
                                        uint32_t i0 = m.indices[i], i1 = m.indices[i+1], i2 = m.indices[i+2];
                                        if (i0 >= m.vertices.size() || i1 >= m.vertices.size() || i2 >= m.vertices.size()) continue;
                                        const auto& v0 = m.vertices[i0];
                                        const auto& v1 = m.vertices[i1];
                                        const auto& v2 = m.vertices[i2];
                                        tris.push_back({HmVert{v0.x,v0.y,v0.z}, HmVert{v1.x,v1.y,v1.z}, HmVert{v2.x,v2.y,v2.z}});
                                        if (v0.x < minX) minX = v0.x; if (v0.x > maxX) maxX = v0.x;
                                        if (v0.y < minY) minY = v0.y; if (v0.y > maxY) maxY = v0.y;
                                        if (v0.z < minZ) minZ = v0.z; if (v0.z > maxZ) maxZ = v0.z;
                                        if (v1.x < minX) minX = v1.x; if (v1.x > maxX) maxX = v1.x;
                                        if (v1.y < minY) minY = v1.y; if (v1.y > maxY) maxY = v1.y;
                                        if (v1.z < minZ) minZ = v1.z; if (v1.z > maxZ) maxZ = v1.z;
                                        if (v2.x < minX) minX = v2.x; if (v2.x > maxX) maxX = v2.x;
                                        if (v2.y < minY) minY = v2.y; if (v2.y > maxY) maxY = v2.y;
                                        if (v2.z < minZ) minZ = v2.z; if (v2.z > maxZ) maxZ = v2.z;
                                    }
                                }
                            }

                            if (!tris.empty()) {
                                float spanX = maxX - minX, spanY = maxY - minY;
                                if (spanX < 1e-6f) spanX = 1.0f;
                                if (spanY < 1e-6f) spanY = 1.0f;
                                int hmW, hmH;
                                if (spanX >= spanY) { hmW = 1024; hmH = std::max(1, (int)(1024.0f * spanY / spanX)); }
                                else { hmH = 1024; hmW = std::max(1, (int)(1024.0f * spanX / spanY)); }

                                std::vector<float> zBuf(hmW * hmH, -1e30f);
                                for (const auto& tri : tris) {
                                    int px[3], py[3];
                                    for (int k = 0; k < 3; k++) {
                                        px[k] = (int)((tri[k].x - minX) / spanX * (hmW - 1));
                                        py[k] = (hmH - 1) - (int)((tri[k].y - minY) / spanY * (hmH - 1));
                                        if (px[k] < 0) px[k] = 0; if (px[k] >= hmW) px[k] = hmW - 1;
                                        if (py[k] < 0) py[k] = 0; if (py[k] >= hmH) py[k] = hmH - 1;
                                    }
                                    int bx0 = std::min({px[0],px[1],px[2]}), bx1 = std::max({px[0],px[1],px[2]});
                                    int by0 = std::min({py[0],py[1],py[2]}), by1 = std::max({py[0],py[1],py[2]});
                                    for (int ry = by0; ry <= by1; ry++) {
                                        for (int rx = bx0; rx <= bx1; rx++) {
                                            float dx0 = (float)(px[1]-px[0]), dy0 = (float)(py[1]-py[0]);
                                            float dx1 = (float)(px[2]-px[0]), dy1 = (float)(py[2]-py[0]);
                                            float dx2 = (float)(rx-px[0]),    dy2 = (float)(ry-py[0]);
                                            float d00 = dx0*dx0+dy0*dy0, d01 = dx0*dx1+dy0*dy1;
                                            float d11 = dx1*dx1+dy1*dy1, d20 = dx2*dx0+dy2*dy0, d21 = dx2*dx1+dy2*dy1;
                                            float den = d00*d11 - d01*d01;
                                            if (std::abs(den) < 1e-10f) continue;
                                            float u = (d11*d20 - d01*d21) / den;
                                            float v = (d00*d21 - d01*d20) / den;
                                            if (u >= -0.001f && v >= -0.001f && u + v <= 1.002f) {
                                                float z = tri[0].z*(1-u-v) + tri[1].z*u + tri[2].z*v;
                                                if (z > zBuf[ry*hmW+rx]) zBuf[ry*hmW+rx] = z;
                                            }
                                        }
                                    }
                                }

                                float rangeZ = maxZ - minZ;
                                if (rangeZ < 1e-6f) rangeZ = 1.0f;
                                std::vector<uint8_t> rgba(hmW * hmH * 4);
                                for (int i = 0; i < hmW * hmH; i++) {
                                    if (zBuf[i] < -1e20f) {
                                        rgba[i*4] = rgba[i*4+1] = rgba[i*4+2] = 0; rgba[i*4+3] = 255;
                                    } else {
                                        float t = (zBuf[i] - minZ) / rangeZ;
                                        if (t < 0) t = 0; if (t > 1) t = 1;
                                        uint8_t val = (uint8_t)(t * 255.0f);
                                        rgba[i*4] = rgba[i*4+1] = rgba[i*4+2] = val; rgba[i*4+3] = 255;
                                    }
                                }

                                if (state.heightmapTexId) destroyTexture(state.heightmapTexId);
                                state.heightmapTexId = createTexture2D(rgba.data(), hmW, hmH);
                                state.heightmapW = hmW;
                                state.heightmapH = hmH;
                                state.showHeightmap = true;
                            }
                        }
                    }
                    ImGui::EndPopup();
                }
            }
            if (!found) ImGui::PopStyleColor();
        };
        int entryId = 0;
        for (const auto& game : db) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));
            ImGui::Text("%s", game.name.c_str());
            ImGui::PopStyleColor();
            ImGui::Separator();
            for (const auto& sec : game.sections) {
                if (ImGui::TreeNode(sec.name.c_str())) {
                    for (const auto& entry : sec.entries) {
                        char suffix[32]; snprintf(suffix, sizeof(suffix), "##e%d", entryId++);
                        drawEntry(entry, suffix);
                    }
                    for (const auto& sf : sec.subFolders) {
                        if (ImGui::TreeNode(sf.name.c_str())) {
                            for (const auto& entry : sf.entries) {
                                char suffix[32]; snprintf(suffix, sizeof(suffix), "##e%d", entryId++);
                                drawEntry(entry, suffix);
                            }
                            ImGui::TreePop();
                        }
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::Spacing();
        }
        ImGui::EndChild();
    }
    else if (!state.selectedErfName.empty() && !state.mergedEntries.empty()) {
        if (state.contentFlagsDirty) classifyMergedEntries(state);
        bool hasTextures = state.contentHasTextures;
        bool hasModels = state.contentHasModels;
        bool hasTerrain = state.contentHasTerrain;
        bool isAudioCategory = (state.selectedErfName == "[Audio]" || state.selectedErfName == "[VoiceOver]");
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
        static int s_lastMeshSourceFilter = -1;
        static int s_lastHierSourceFilter = -1;
        bool sourceFilterChanged = (s_lastMeshSourceFilter != s_meshDataSourceFilter ||
                                    s_lastHierSourceFilter != s_hierDataSourceFilter);
        if (currentFilter != state.lastContentFilter || sourceFilterChanged) {
            s_lastMeshSourceFilter = s_meshDataSourceFilter;
            s_lastHierSourceFilter = s_hierDataSourceFilter;
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
                if (ce.name.find("__COLLAPSE_LEVELS__") == 0) continue;
                if (ce.name.find("__COLLAPSE_OTHER__") == 0) continue;
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
                            destroyLevelBuffers();
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
        bool isOverrideTree = (state.selectedErfName == "[Override]");
        bool hasSubfolders = false;
        if (isOverrideTree) {
            for (int i : state.filteredEntryIndices) {
                if (state.mergedEntries[i].name.find('/') != std::string::npos) {
                    hasSubfolders = true;
                    break;
                }
            }
        }
        if (isOverrideTree && hasSubfolders) {
            std::map<std::string, std::vector<int>> folderMap;
            std::vector<int> rootFiles;
            for (int i : state.filteredEntryIndices) {
                const auto& ce = state.mergedEntries[i];
                size_t slash = ce.name.find('/');
                if (slash != std::string::npos) {
                    std::string folder = ce.name.substr(0, slash);
                    folderMap[folder].push_back(i);
                } else {
                    rootFiles.push_back(i);
                }
            }
            auto renderOverrideEntry = [&](int idx) {
                const CachedEntry& ce = state.mergedEntries[idx];
                std::string displayName = ce.name;
                size_t lastSlash = displayName.rfind('/');
                if (lastSlash != std::string::npos) displayName = displayName.substr(lastSlash + 1);
                std::string nameLower = displayName;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                bool isModel = (nameLower.size() > 4 && nameLower.substr(nameLower.size() - 4) == ".msh");
                bool isMao = (nameLower.size() > 4 && nameLower.substr(nameLower.size() - 4) == ".mao");
                bool isPhy = (nameLower.size() > 4 && nameLower.substr(nameLower.size() - 4) == ".phy");
                bool isTexture = (nameLower.size() > 4 && nameLower.substr(nameLower.size() - 4) == ".dds");
                bool isMmh = (nameLower.size() > 4 && nameLower.substr(nameLower.size() - 4) == ".mmh");
                if (isModel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
                else if (isMao) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.4f, 1.0f));
                else if (isPhy) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 1.0f, 1.0f));
                else if (isTexture) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
                else if (isMmh) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.4f, 1.0f));
                else ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                char label[512]; snprintf(label, sizeof(label), "%s##ovr%d", displayName.c_str(), idx);
                if (ImGui::Selectable(label, idx == state.selectedEntryIndex, ImGuiSelectableFlags_AllowDoubleClick)) {
                    state.selectedEntryIndex = idx;
                    if (ImGui::IsMouseDoubleClicked(0) && isModel) {
                        state.showTerrain = false;
                        g_terrainLoader.clear();
                        state.currentModelAnimations.clear();
                        if (loadModelFromOverride(state, ce.source)) {
                            state.statusMessage = "Loaded (override): " + displayName;
                        } else {
                            state.statusMessage = "Failed to load: " + displayName;
                        }
                    } else if (ImGui::IsMouseDoubleClicked(0)) {
                        auto data = readCachedEntryData(state, ce);
                        if (!data.empty() && isGffData(data)) {
                            loadGffData(state.gffViewer, data, ce.name);
                        }
                    }
                }
                ImGui::PopStyleColor();
            };
            for (int idx : rootFiles) {
                renderOverrideEntry(idx);
            }
            for (auto& [folder, indices] : folderMap) {
                if (ImGui::TreeNode(folder.c_str())) {
                    for (int idx : indices) {
                        renderOverrideEntry(idx);
                    }
                    ImGui::TreePop();
                }
            }
        } else {
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

            bool isModel = (ce.flags & CachedEntry::FLAG_MODEL) != 0;
            bool isMao = (ce.flags & CachedEntry::FLAG_MAO) != 0;
            bool isPhy = (ce.flags & CachedEntry::FLAG_PHY) != 0;
            bool isTerrainFile = (ce.flags & CachedEntry::FLAG_TERRAIN) != 0;
            bool isTexture = (ce.flags & CachedEntry::FLAG_TEXTURE) != 0;
            bool isAudioFile = (ce.flags & CachedEntry::FLAG_AUDIO) != 0;
            bool isGda = (ce.flags & CachedEntry::FLAG_GDA) != 0;
            bool isGff = (ce.flags & CachedEntry::FLAG_GFF) != 0;
            bool isSpt = (ce.name.size() > 4 && ce.name.substr(ce.name.size() - 4) == ".spt");
            if (isModel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
            else if (isTerrainFile) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.4f, 0.2f, 1.0f));
            else if (isMao) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.4f, 1.0f));
            else if (isPhy) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 1.0f, 1.0f));
            else if (isTexture) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
            else if (isAudioFile) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.2f, 1.0f));
            else if (isGda) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 1.0f, 1.0f));
            else if (isGff) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.8f, 1.0f));
            else if (isSpt) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.9f, 0.5f, 1.0f));
            char label[256]; snprintf(label, sizeof(label), "%s##%d", ce.name.c_str(), idx);
            if (ImGui::Selectable(label, idx == state.selectedEntryIndex, ImGuiSelectableFlags_AllowDoubleClick)) {
                state.selectedEntryIndex = idx;

                bool isRimClick = (state.selectedErfName == "[Env]" && ce.entryIdx == 0);
                if (isRimClick) {
                    if (state.levelLoad.stage != 0) {
                        state.levelLoad.stage = 0;
                        state.levelLoad.stageLabel = "";
                    }
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
                }
                bool isArlOpfClick = (state.selectedErfName == "[Env]" && (ce.entryIdx == 1 || ce.entryIdx == 2));
                if (ImGui::IsMouseDoubleClicked(0) && isArlOpfClick) {
                    std::string fullPath;
                    if (ce.entryIdx == 1 && ce.erfIdx < state.arlFiles.size()) {
                        fullPath = state.arlFiles[ce.erfIdx];
                    } else if (ce.entryIdx == 2 && ce.erfIdx < state.opfFiles.size()) {
                        fullPath = state.opfFiles[ce.erfIdx];
                    }
                    if (!fullPath.empty()) {
                        std::ifstream ifs(fullPath, std::ios::binary);
                        if (ifs) {
                            std::vector<uint8_t> data((std::istreambuf_iterator<char>(ifs)),
                                                       std::istreambuf_iterator<char>());
                            if (loadGffData(state.gffViewer, data, ce.name, fullPath)) {
                                state.gffViewer.showWindow = true;
                                state.statusMessage = "GFF: " + ce.name;
                            } else {
                                state.statusMessage = "Failed to parse GFF: " + ce.name;
                            }
                        }
                    }
                }
                bool isEnvEntry = (state.selectedErfName == "[Env]" ||
                                   state.selectedErfName == "[Audio]" ||
                                   state.selectedErfName == "[VoiceOver]");
                if (ImGui::IsMouseDoubleClicked(0) && !isRimClick && !isAudio && !isEnvEntry) {
                    if (ce.erfIdx == SIZE_MAX) {
                        std::string ceLower = ce.name;
                        std::transform(ceLower.begin(), ceLower.end(), ceLower.begin(), ::tolower);
                        bool isMshOverride = (ceLower.size() > 4 && ceLower.substr(ceLower.size() - 4) == ".msh");
                        if (isMshOverride) {
                            state.showTerrain = false;
                            g_terrainLoader.clear();
                            state.currentModelAnimations.clear();
                            if (loadModelFromOverride(state, ce.source)) {
                                state.statusMessage = "Loaded (override): " + ce.name;
                            } else {
                                state.statusMessage = "Failed to load: " + ce.name;
                            }
                        } else {
                            auto data = readCachedEntryData(state, ce);
                            if (!data.empty() && isGffData(data)) {
                                if (loadGffData(state.gffViewer, data, ce.name)) {
                                    state.statusMessage = "Opened: " + ce.name;
                                }
                            } else {
                                state.statusMessage = "Not a valid GFF file: " + ce.name;
                            }
                        }
                    } else {
                    ERFFile erf;
                    if (erf.open(state.erfFiles[ce.erfIdx])) {
                        if (ce.entryIdx < erf.entries().size()) {
                            const auto& entry = erf.entries()[ce.entryIdx];
                            if (isTerrainFile) {
                                auto terrainData = erf.readEntry(entry);
                                if (!terrainData.empty() && isGffData(terrainData)) {
                                    if (loadGffData(state.gffViewer, terrainData, ce.name, state.erfFiles[ce.erfIdx], ce.entryIdx)) {
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
                                        bool isTga = nameLower.size() > 4 && nameLower.substr(nameLower.size() - 4) == ".tga";
                                        if (isTga) {
                                            std::vector<uint8_t> rgba; int w, h;
                                            if (decodeTGAToRGBA(data, rgba, w, h))
                                                state.previewTextureId = createTexture2D(rgba.data(), w, h);
                                        } else {
                                            state.previewTextureId = createTextureFromDDS(data);
                                        }
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
                                        if (loadGffData(state.gffViewer, data, ce.name, state.erfFiles[ce.erfIdx], ce.entryIdx)) {
                                            state.statusMessage = "Opened GFF: " + ce.name;
                                        } else {
                                            state.statusMessage = "Failed to parse GFF: " + ce.name;
                                        }
                                    }
                                } else if (isSpt) {
                                    auto data = erf.readEntry(entry);
                                    if (!data.empty()) {
                                        state.showTerrain = false;
                                        g_terrainLoader.clear();
                                        if (loadSptFromData(state, data, ce.name, state.erfFiles[ce.erfIdx])) {
                                            state.statusMessage = "Loaded SPT: " + ce.name;
                                            state.showRenderSettings = true;
                                        } else {
                                            state.statusMessage = "Failed to load SPT: " + ce.name;
                                        }
                                    }
                                } else {
                                    auto data = erf.readEntry(entry);
                                    if (!data.empty() && isGffData(data)) {
                                        if (loadGffData(state.gffViewer, data, ce.name, state.erfFiles[ce.erfIdx], ce.entryIdx)) {
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
                                    state.statusMessage = "Opened: " + ce.name;
                                }
                            } else {
                                state.statusMessage = "Not a valid GFF file: " + ce.name;
                            }
                        }
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Dump File...")) {
                    s_pendingDumpEntry = ce;
                    s_pendingDump = true;
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
                    ImGuiFileDialog::Instance()->OpenDialog("DumpSingleFile", "Save File", ".*", config);
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
                    auto data = readCachedEntryData(state, ce);
                    if (!data.empty() && isGffData(data)) {
                        std::string erfSrc = (ce.erfIdx == SIZE_MAX || ce.erfIdx >= state.erfFiles.size()) ? "" : state.erfFiles[ce.erfIdx];
                        if (loadGffData(state.gffViewer, data, ce.name, erfSrc, ce.entryIdx)) {
                            state.statusMessage = "Opened: " + ce.name;
                        }
                    } else {
                        state.statusMessage = "Not a valid GFF file: " + ce.name;
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Dump File...")) {
                    s_pendingDumpEntry = ce;
                    s_pendingDump = true;
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
                    ImGuiFileDialog::Instance()->OpenDialog("DumpSingleFile", "Save File", ".*", config);
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
                    auto data = readCachedEntryData(state, ce);
                    if (!data.empty() && isGffData(data)) {
                        std::string erfSrc = (ce.erfIdx == SIZE_MAX || ce.erfIdx >= state.erfFiles.size()) ? "" : state.erfFiles[ce.erfIdx];
                        if (loadGffData(state.gffViewer, data, ce.name, erfSrc, ce.entryIdx)) {
                            state.statusMessage = "Opened: " + ce.name;
                        }
                    } else {
                        state.statusMessage = "Not a valid GFF file: " + ce.name;
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Dump File...")) {
                    s_pendingDumpEntry = ce;
                    s_pendingDump = true;
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
                    ImGuiFileDialog::Instance()->OpenDialog("DumpSingleFile", "Save File", ".*", config);
                }
                ImGui::EndPopup();
            }
            if (!isAudio && !isModel && !isTexture && ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Open with GFF Viewer")) {
                    auto data = readCachedEntryData(state, ce);
                    if (!data.empty() && isGffData(data)) {
                        std::string erfSrc = (ce.erfIdx == SIZE_MAX || ce.erfIdx >= state.erfFiles.size()) ? "" : state.erfFiles[ce.erfIdx];
                        if (loadGffData(state.gffViewer, data, ce.name, erfSrc, ce.entryIdx)) {
                            state.statusMessage = "Opened: " + ce.name;
                        }
                    } else {
                        state.statusMessage = "Not a valid GFF file: " + ce.name;
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Dump File...")) {
                    s_pendingDumpEntry = ce;
                    s_pendingDump = true;
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
                    ImGuiFileDialog::Instance()->OpenDialog("DumpSingleFile", "Save File", ".*", config);
                }
                ImGui::EndPopup();
            }
            if (isModel || isTerrainFile || isMao || isPhy || isTexture || isAudioFile || isGda || isGff || isSpt) ImGui::PopStyleColor();
        });
        }
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

                static std::vector<int> s_filteredFsbIndices;
                static std::string s_lastFsbFilter;
                static size_t s_lastFsbCount = 0;
                if (fsbFilterLower != s_lastFsbFilter || state.currentFSBSamples.size() != s_lastFsbCount) {
                    s_lastFsbFilter = fsbFilterLower;
                    s_lastFsbCount = state.currentFSBSamples.size();
                    s_filteredFsbIndices.clear();
                    s_filteredFsbIndices.reserve(state.currentFSBSamples.size());
                    for (int i = 0; i < (int)state.currentFSBSamples.size(); i++) {
                        if (!fsbFilterLower.empty()) {
                            std::string nameLower = state.currentFSBSamples[i].name;
                            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                            if (nameLower.find(fsbFilterLower) == std::string::npos) continue;
                        }
                        s_filteredFsbIndices.push_back(i);
                    }
                }

                ImGuiListClipper clipper;
                clipper.Begin((int)s_filteredFsbIndices.size());
                while (clipper.Step()) {
                    for (int fi = clipper.DisplayStart; fi < clipper.DisplayEnd; fi++) {
                        int i = s_filteredFsbIndices[fi];
                        const auto& sample = state.currentFSBSamples[i];

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
                        clearErfIndices();
                        state.modelErfIndex.clear();
                        state.materialErfIndex.clear();
                        state.textureErfIndex.clear();
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
                            if (mat.paletteTexId != 0)     destroyTexture(mat.paletteTexId);
                            if (mat.palNormalTexId != 0)   destroyTexture(mat.palNormalTexId);
                            if (mat.maskVTexId != 0)       destroyTexture(mat.maskVTexId);
                            if (mat.maskATexId != 0)       destroyTexture(mat.maskATexId);
                        }
                        destroyLevelBuffers();
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
                                            bool isTga = nameLower.size() > 4 && nameLower.substr(nameLower.size() - 4) == ".tga";
                                            if (isTga) {
                                                std::vector<uint8_t> rgba; int w, h;
                                                if (decodeTGAToRGBA(data, rgba, w, h))
                                                    state.previewTextureId = createTexture2D(rgba.data(), w, h);
                                            } else {
                                                state.previewTextureId = createTextureFromDDS(data);
                                            }
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
                                            if (loadGffData(state.gffViewer, data, re.name, state.currentRIMPath, re.entryIdx)) {
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
                                        if (loadGffData(state.gffViewer, data, re.name, state.currentRIMPath, re.entryIdx)) {
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
    if (ImGuiFileDialog::Instance()->Display("DumpSingleFile", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string outPath = ImGuiFileDialog::Instance()->GetFilePathName();
            auto data = readCachedEntryData(state, s_pendingDumpEntry);
            if (!data.empty()) {
                std::ofstream f(outPath, std::ios::binary);
                if (f) {
                    f.write(reinterpret_cast<const char*>(data.data()), data.size());
                    state.statusMessage = "Dumped: " + s_pendingDumpEntry.name;
                } else {
                    state.statusMessage = "Failed to write: " + outPath;
                }
            } else {
                state.statusMessage = "Failed to read: " + s_pendingDumpEntry.name;
            }
        }
        s_pendingDump = false;
        ImGuiFileDialog::Instance()->Close();
    }
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
                destroyLevelBuffers();
                state.currentModel = Model();
                state.hasModel = false;
            }
            state.mergedEntries.clear();
                state.contentFlagsDirty = true;
            state.filteredEntryIndices.clear();
            state.lastContentFilter = "\x01_REBUILD";
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
}