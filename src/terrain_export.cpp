#include "terrain_export.h"
#include "ui_internal.h"
#include "export.h"
#include "spt.h"
#include "dds_loader.h"
#include <fstream>
#include <sstream>
#include <set>
#include <map>
#include <filesystem>
#include <algorithm>
#include <cmath>
namespace fs = std::filesystem;

static std::string sanitizeName(const std::string& name) {
    std::string out;
    for (char c : name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|' || c == ' ')
            out += '_';
        else
            out += c;
    }
    return out;
}

static std::string stripExt(const std::string& s) {
    size_t d = s.rfind('.');
    return (d != std::string::npos) ? s.substr(0, d) : s;
}

static std::string jsonStr(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    out += "\"";
    return out;
}

static std::string jsonVec3(float x, float y, float z) {
    char buf[128];
    snprintf(buf, sizeof(buf), "[%.6f, %.6f, %.6f]", x, y, z);
    return buf;
}

static std::string jsonVec4(float x, float y, float z, float w) {
    char buf[160];
    snprintf(buf, sizeof(buf), "[%.6f, %.6f, %.6f, %.6f]", x, y, z, w);
    return buf;
}

static void convertModelToYUp(Model& model) {
    for (auto& mesh : model.meshes) {
        for (auto& v : mesh.vertices) {
            float oy = v.y;
            v.y = v.z;
            v.z = -oy;
            float ony = v.ny;
            v.ny = v.nz;
            v.nz = -ony;
        }
        mesh.calculateBounds();
    }
}

struct TerrainMatExport {
    std::string matName;
    std::string palettePath;
    std::string maskAPath;
    std::string maskA2Path;
    float palDim[4];
    float palParam[4];
    float uvScales[8];
    int totalCells;
};

static std::vector<TerrainMatExport> exportTerrainTextures(
    AppState& state, Model& terrainModel, const std::string& modelsDir)
{
    state.textureErfIndex.build(state.textureErfs);

    std::vector<TerrainMatExport> result;
    std::set<std::string> exported;

    for (auto& mat : terrainModel.materials) {

        if (!mat.isTerrain) continue;

        std::string safeName = sanitizeName(mat.name);
        if (exported.count(safeName)) continue;
        exported.insert(safeName);

        TerrainMatExport info;
        info.matName = mat.name;
        memcpy(info.palDim, mat.palDim, sizeof(info.palDim));
        memcpy(info.palParam, mat.palParam, sizeof(info.palParam));
        memcpy(info.uvScales, mat.uvScales, sizeof(info.uvScales));
        int cols = (int)mat.palDim[2], rows = (int)mat.palDim[3];
        info.totalCells = cols * rows;
        if (info.totalCells < 1) info.totalCells = 1;
        if (info.totalCells > 8) info.totalCells = 8;

        if (!mat.diffuseData.empty() && mat.diffuseWidth > 0) {
            std::string fn = "terrain_" + safeName + "_palette.png";
            std::string fullPath = modelsDir + "/" + fn;
            bool ok = saveRGBAToPNG(fullPath, mat.diffuseData, mat.diffuseWidth, mat.diffuseHeight);
            if (ok) info.palettePath = "models/" + fn;
        } else {
        }

        auto saveMask = [&](const std::string& mapName, const std::string& suffix) -> std::string {
            if (mapName.empty()) {
                return {};
            }
            auto ddsData = loadTextureData(state, mapName);
            if (ddsData.empty()) {
                return {};
            }
            std::vector<uint8_t> rgba; int w, h;
            if (!decodeDDSToRGBA(ddsData, rgba, w, h)) {
                return {};
            }
            std::string fn = "terrain_" + safeName + "_" + suffix + ".png";
            bool ok = saveRGBAToPNG(modelsDir + "/" + fn, rgba, w, h);
            if (ok) return "models/" + fn;
            return {};
        };

        std::vector<uint8_t> mvRGBA, maRGBA, ma2RGBA;
        int mvW = 0, mvH = 0, maW = 0, maH = 0, ma2W = 0, ma2H = 0;

        if (!mat.maskVMap.empty()) {
            auto dds = loadTextureData(state, mat.maskVMap);
            if (!dds.empty()) decodeDDSToRGBA(dds, mvRGBA, mvW, mvH);
        }
        if (!mat.maskAMap.empty()) {
            auto dds = loadTextureData(state, mat.maskAMap);
            if (!dds.empty()) decodeDDSToRGBA(dds, maRGBA, maW, maH);
        }
        if (!mat.maskA2Map.empty()) {
            auto dds = loadTextureData(state, mat.maskA2Map);
            if (!dds.empty()) decodeDDSToRGBA(dds, ma2RGBA, ma2W, ma2H);
        }

        if (!mvRGBA.empty() && !maRGBA.empty() && mvW == maW && mvH == maH) {
            int tc = info.totalCells;
            int cleaned = 0;
            for (int i = 0; i < mvW * mvH; i++) {
                float r = mvRGBA[i * 4 + 0] / 255.0f;
                float g = mvRGBA[i * 4 + 1] / 255.0f;
                float b = mvRGBA[i * 4 + 2] / 255.0f;
                int ci0 = std::max(0, std::min(tc - 1, (int)(r * 7.5f + 0.5f)));
                int ci1 = std::max(0, std::min(tc - 1, (int)(g * 7.5f + 0.5f)));
                int ci2 = std::max(0, std::min(tc - 1, (int)(b * 7.5f + 0.5f)));

                bool active[8] = {};
                active[ci0] = true;
                active[ci1] = true;
                active[ci2] = true;

                uint8_t* pa = &maRGBA[i * 4];
                if (!active[0]) pa[0] = 0;
                if (!active[1]) pa[1] = 0;
                if (!active[2]) pa[2] = 0;
                if (!active[3]) pa[3] = 0;

                if (!ma2RGBA.empty() && mvW == ma2W && mvH == ma2H) {
                    uint8_t* pa2 = &ma2RGBA[i * 4];
                    if (!active[4]) pa2[0] = 0;
                    if (!active[5]) pa2[1] = 0;
                    if (!active[6]) pa2[2] = 0;
                    if (!active[7]) pa2[3] = 0;
                }

                if (cleaned < 3) {
                }
                cleaned++;
            }

            std::string fnA = "terrain_" + safeName + "_maskA.png";
            bool okA = saveRGBAToPNG(modelsDir + "/" + fnA, maRGBA, maW, maH);
            if (okA) info.maskAPath = "models/" + fnA;

            if (!ma2RGBA.empty() && ma2W > 0) {
                std::string fnA2 = "terrain_" + safeName + "_maskA2.png";
                bool okA2 = saveRGBAToPNG(modelsDir + "/" + fnA2, ma2RGBA, ma2W, ma2H);
                if (okA2) info.maskA2Path = "models/" + fnA2;
            }
        } else {
            info.maskAPath = saveMask(mat.maskAMap, "maskA");
            info.maskA2Path = saveMask(mat.maskA2Map, "maskA2");
        }

        if (!info.palettePath.empty() && !info.maskAPath.empty()) {
            result.push_back(std::move(info));
        } else {
        }
    }

    return result;
}

static Model buildModelFromSpt(const SptModel& spt, const std::string& baseName) {
    Model model;
    model.name = baseName;
    const char* typeNames[] = {"Branch", "Frond", "LeafCard", "LeafMesh"};

    std::string branchKey = spt.branchTexture.empty() ? baseName : stripExt(spt.branchTexture);
    Material branchMat;
    branchMat.name = branchKey;
    branchMat.diffuseMap = branchKey;
    branchMat.opacity = 1.0f;
    int branchMatIdx = (int)model.materials.size();
    model.materials.push_back(std::move(branchMat));

    std::string diffuseKey = baseName + "_diffuse";
    Material ddsMat;
    ddsMat.name = diffuseKey;
    ddsMat.diffuseMap = diffuseKey;
    ddsMat.opacity = 1.0f;
    int ddsMatIdx = (int)model.materials.size();
    model.materials.push_back(std::move(ddsMat));

    for (int si = 0; si < (int)spt.submeshes.size(); si++) {
        const auto& sm = spt.submeshes[si];
        if (sm.vertexCount() == 0) continue;
        Mesh mesh;
        mesh.name = baseName + "_" + typeNames[(int)sm.type];
        if (sm.type == SptSubmeshType::Branch) {
            mesh.materialIndex = branchMatIdx;
            mesh.materialName = model.materials[branchMatIdx].name;
        } else {
            mesh.materialIndex = ddsMatIdx;
            mesh.materialName = model.materials[ddsMatIdx].name;
            mesh.alphaTest = true;
        }
        uint32_t nv = sm.vertexCount();
        mesh.vertices.resize(nv);
        for (uint32_t vi = 0; vi < nv; vi++) {
            auto& v = mesh.vertices[vi];
            v.x = sm.positions[vi * 3 + 0];
            v.y = sm.positions[vi * 3 + 1];
            v.z = sm.positions[vi * 3 + 2];
            v.nx = sm.normals[vi * 3 + 0];
            v.ny = sm.normals[vi * 3 + 1];
            v.nz = sm.normals[vi * 3 + 2];
            v.u = sm.texcoords[vi * 2 + 0];
            v.v = sm.texcoords[vi * 2 + 1];
        }
        mesh.indices = sm.indices;
        mesh.calculateBounds();
        model.meshes.push_back(std::move(mesh));
    }
    return model;
}

static bool exportModel(Model& model, const std::string& path, bool useFbx) {
    if (model.meshes.empty()) return false;
    convertModelToYUp(model);
    ExportOptions opts;
    opts.doubleSided = true;
    if (useFbx) return exportToFBX(model, {}, path, opts);
    return exportToGLB(model, {}, path, opts);
}

static bool exportSubModel(const Model& src, size_t meshStart, size_t meshEnd,
                           const std::string& name, const std::string& path, bool useFbx) {
    if (meshStart >= meshEnd || meshEnd > src.meshes.size()) return false;
    Model sub;
    sub.name = name;
    std::map<int, int> matRemap;
    for (size_t i = meshStart; i < meshEnd; i++) {
        int idx = src.meshes[i].materialIndex;
        if (idx >= 0 && idx < (int)src.materials.size() && matRemap.find(idx) == matRemap.end()) {
            matRemap[idx] = (int)sub.materials.size();
            sub.materials.push_back(src.materials[idx]);
        }
    }
    for (size_t i = meshStart; i < meshEnd; i++) {
        Mesh m = src.meshes[i];
        auto it = matRemap.find(m.materialIndex);
        if (it != matRemap.end()) m.materialIndex = it->second;
        sub.meshes.push_back(std::move(m));
    }
    return exportModel(sub, path, useFbx);
}

struct PropGroup {
    std::string modelName;
    std::string fileName;
    std::vector<size_t> instanceIndices;
    bool isTerrain = false;
};

struct TreeGroup {
    int32_t treeId;
    std::string sptFileName;
    std::string baseName;
    std::string fileName;
    std::vector<size_t> instanceIndices;
};

static std::vector<std::pair<std::string, PropGroup>> s_propGroups;
static std::vector<TreeGroup> s_treeGroups;
static Model s_propModel;
static bool s_propModelBuilt = false;
static std::map<std::string, std::unique_ptr<ERFFile>> s_erfCache;

struct PropRange { size_t start; size_t end; int groupIdx; };
static std::vector<PropRange> s_propRanges;
static std::vector<TerrainMatExport> s_terrainMats;

void startLevelExport(AppState& state, const std::string& outputDir, const LevelExportOptions& opts) {
    auto& ex = state.levelExport;
    auto& ll = state.levelLoad;
    ex = {};
    s_propGroups.clear();
    s_treeGroups.clear();
    s_propRanges.clear();
    s_terrainMats.clear();
    s_propModel = Model();
    s_propModelBuilt = false;
    s_erfCache.clear();

    ex.useFbx = opts.useFbx;
    std::string ext = opts.useFbx ? ".fbx" : ".glb";
    std::string oldExt = opts.useFbx ? ".glb" : ".fbx";
    ex.rimStem = fs::path(state.currentRIMPath).stem().string();
    ex.outputDir = (fs::path(outputDir) / ex.rimStem).string();
    ex.modelsDir = ex.outputDir + "/models";
    fs::create_directories(ex.modelsDir);

    try {
        for (auto& entry : fs::directory_iterator(ex.modelsDir)) {
            if (entry.is_regular_file() && entry.path().extension() == oldExt) {
                fs::remove(entry.path());
            }
        }
    } catch (...) {}

    std::string rimLowerForProps = fs::path(state.currentRIMPath).stem().string();
    std::transform(rimLowerForProps.begin(), rimLowerForProps.end(), rimLowerForProps.begin(), ::tolower);

    auto looksLikeTerrainName = [&](const std::string& name) -> bool {
        std::string nameLower = name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        if (nameLower.size() <= rimLowerForProps.size() + 1) return false;
        if (nameLower.substr(0, rimLowerForProps.size()) != rimLowerForProps) return false;
        std::string rest = nameLower.substr(rimLowerForProps.size());
        if (rest[0] != '_') return false;
        for (size_t ci = 1; ci < rest.size(); ci++) {
            char ch = rest[ci];
            if (ch != '_' && (ch < '0' || ch > '9') && ch != 'l' && ch != 'c') return false;
        }
        return true;
    };

    std::map<std::string, int> propKeyToIdx;
    for (size_t i = 0; i < ll.propQueue.size(); i++) {
        const auto& pw = ll.propQueue[i];
        if (pw.modelName.empty()) continue;
        std::string key = pw.modelName;
        bool isTerrain = looksLikeTerrainName(pw.modelName);
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        auto it = propKeyToIdx.find(key);
        if (it == propKeyToIdx.end()) {
            propKeyToIdx[key] = (int)s_propGroups.size();
            PropGroup g;
            g.modelName = pw.modelName;
            g.fileName = sanitizeName(stripExt(pw.modelName)) + ext;
            g.isTerrain = isTerrain;
            g.instanceIndices.push_back(i);
            s_propGroups.push_back({key, std::move(g)});
        } else {
            s_propGroups[it->second].second.instanceIndices.push_back(i);
        }
    }

    std::map<int32_t, int> treeIdToIdx;
    for (size_t i = 0; i < ll.sptQueue.size(); i++) {
        int32_t tid = ll.sptQueue[i].treeId;
        auto it = treeIdToIdx.find(tid);
        if (it == treeIdToIdx.end()) {
            treeIdToIdx[tid] = (int)s_treeGroups.size();
            TreeGroup g;
            g.treeId = tid;
            auto fit = ll.sptIdToFile.find(tid);
            if (fit != ll.sptIdToFile.end()) {
                g.sptFileName = fit->second;
                g.baseName = stripExt(fit->second);
                g.fileName = sanitizeName(g.baseName) + ext;
            }
            g.instanceIndices.push_back(i);
            s_treeGroups.push_back(std::move(g));
        } else {
            s_treeGroups[it->second].instanceIndices.push_back(i);
        }
    }

    ex.totalProps = (int)s_propGroups.size();
    ex.totalTrees = (int)s_treeGroups.size();
    ex.stage = 1;
    ex.stageLabel = "Exporting terrain...";
}

void tickLevelExport(AppState& state) {
    auto& ex = state.levelExport;
    auto& ll = state.levelLoad;
    if (ex.stage <= 0) return;

    std::string ext = ex.useFbx ? ".fbx" : ".glb";

    if (ex.stage == 1) {
        std::string rimLower = ex.rimStem;
        std::transform(rimLower.begin(), rimLower.end(), rimLower.begin(), ::tolower);
        Model terrainModel;
        terrainModel.name = ex.rimStem + "_terrain";
        std::set<int> usedMats;
        for (const auto& mesh : state.currentModel.meshes) {
            std::string nameLower = mesh.name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            if (nameLower.size() <= rimLower.size() + 1) continue;
            if (nameLower.substr(0, rimLower.size()) != rimLower) continue;
            std::string rest = nameLower.substr(rimLower.size());
            if (rest[0] != '_') continue;
            bool looksLikeTerrain = true;
            for (size_t ci = 1; ci < rest.size(); ci++) {
                char ch = rest[ci];
                if (ch != '_' && (ch < '0' || ch > '9') && ch != 'l' && ch != 'c') {
                    looksLikeTerrain = false;
                    break;
                }
            }
            if (looksLikeTerrain) {
                if (mesh.materialIndex >= 0) usedMats.insert(mesh.materialIndex);
                terrainModel.meshes.push_back(mesh);
            }
        }
        std::map<int, int> remap;
        for (int idx : usedMats) {
            if (idx < (int)state.currentModel.materials.size()) {
                remap[idx] = (int)terrainModel.materials.size();
                terrainModel.materials.push_back(state.currentModel.materials[idx]);
            }
        }
        for (auto& m : terrainModel.meshes)
            if (remap.count(m.materialIndex)) m.materialIndex = remap[m.materialIndex];

        s_terrainMats = exportTerrainTextures(state, terrainModel, ex.modelsDir);

        ex.stage = 2;
        ex.itemIndex = 0;
        ex.stageLabel = "Building prop models...";
        s_propModel = Model();
        s_propModelBuilt = false;
        s_propRanges.clear();
    }
    else if (ex.stage == 2) {
        if (!s_propModelBuilt) {
            Model savedModel = std::move(state.currentModel);
            bool savedHas = state.hasModel;
            state.currentModel = Model();
            state.hasModel = false;

            state.modelErfIndex.build(state.modelErfs);
            state.materialErfIndex.build(state.materialErfs);
            state.textureErfIndex.build(state.textureErfs);

            for (int gi = 0; gi < (int)s_propGroups.size(); gi++) {
                auto& group = s_propGroups[gi].second;
                size_t before = state.currentModel.meshes.size();
                if (mergeModelByName(state, group.modelName, 0, 0, 0, 0, 0, 0, 1, 1.0f)) {
                    s_propRanges.push_back({before, state.currentModel.meshes.size(), gi});
                }
            }
            finalizeLevelMaterials(state);

            s_propModel = std::move(state.currentModel);
            state.currentModel = std::move(savedModel);
            state.hasModel = savedHas;
            clearErfIndices();
            s_propModelBuilt = true;
        }

        {
            const int BATCH = 16;
            int processed = 0;
            while (ex.itemIndex < (int)s_propRanges.size() && processed < BATCH) {
                const auto& range = s_propRanges[ex.itemIndex];
                auto& group = s_propGroups[range.groupIdx].second;
                exportSubModel(s_propModel, range.start, range.end,
                               group.modelName, ex.modelsDir + "/" + group.fileName, ex.useFbx);
                ex.propsExported++;
                ex.itemIndex++;
                processed++;
            }
            ex.stageLabel = "Exporting props...";
        }

        if (ex.itemIndex >= (int)s_propRanges.size()) {
            s_propModel = Model();
            ex.stage = 3;
            ex.itemIndex = 0;
            ex.stageLabel = "Exporting trees...";
        }
    }
    else if (ex.stage == 3) {
        const int BATCH = 4;
        int processed = 0;
        while (ex.itemIndex < (int)s_treeGroups.size() && processed < BATCH) {
            auto& group = s_treeGroups[ex.itemIndex];
            if (!group.sptFileName.empty()) {
                auto erfIt = ll.sptFileToErf.find(group.sptFileName);
                if (erfIt != ll.sptFileToErf.end()) {
                    auto& erfPtr = s_erfCache[erfIt->second];
                    if (!erfPtr) {
                        erfPtr = std::make_unique<ERFFile>();
                        erfPtr->open(erfIt->second);
                    }
                    ERFFile& erf = *erfPtr;
                    for (const auto& entry : erf.entries()) {
                            if (entry.name != group.sptFileName) continue;
                            auto sptData = erf.readEntry(entry);
                            if (sptData.empty()) break;
                            std::string tempDir;
#ifdef _WIN32
                            char tmp[MAX_PATH]; GetTempPathA(MAX_PATH, tmp); tempDir = tmp;
#else
                            tempDir = "/tmp/";
#endif
                            std::string tempSpt = tempDir + "haven_export_temp.spt";
                            { std::ofstream f(tempSpt, std::ios::binary);
                              f.write((char*)sptData.data(), sptData.size()); }
                            SptModel sptModel;
                            if (loadSptModel(tempSpt, sptModel)) {
                                extractSptTextures(sptData, sptModel);
                                Model treeModel = buildModelFromSpt(sptModel, group.baseName);
                                for (auto& mat : treeModel.materials) {
                                    if (mat.diffuseMap.empty()) continue;
                                    std::string texLower = mat.diffuseMap;
                                    std::transform(texLower.begin(), texLower.end(), texLower.begin(), ::tolower);
                                    for (const auto& te : erf.entries()) {
                                        std::string eLower = te.name;
                                        std::transform(eLower.begin(), eLower.end(), eLower.begin(), ::tolower);
                                        bool match = (eLower == texLower + ".tga" || eLower == texLower + ".dds");
                                        if (!match) continue;
                                        auto texData = erf.readEntry(te);
                                        if (texData.empty()) break;
                                        std::vector<uint8_t> rgba; int w, h;
                                        bool ok = (eLower.size() > 4 && eLower.substr(eLower.size() - 4) == ".tga")
                                            ? decodeTGAToRGBA(texData, rgba, w, h)
                                            : decodeDDSToRGBA(texData, rgba, w, h);
                                        if (ok) { mat.diffuseData = std::move(rgba); mat.diffuseWidth = w; mat.diffuseHeight = h; }
                                        break;
                                    }
                                }
                                exportModel(treeModel, ex.modelsDir + "/" + group.fileName, ex.useFbx);
                                ex.treesExported++;
                            }
#ifdef _WIN32
                            DeleteFileA(tempSpt.c_str());
#else
                            remove(tempSpt.c_str());
#endif
                            break;
                    }
                }
            }
            ex.itemIndex++;
            processed++;
        }

        if (ex.itemIndex >= (int)s_treeGroups.size()) {
            ex.stage = 4;
            ex.stageLabel = "Writing area data...";
        }
    }
    else if (ex.stage == 4) {
        std::string formatName = ex.useFbx ? "fbx" : "glb";
        std::ostringstream json;
        json << "{\n";
        json << "  \"level\": " << jsonStr(state.currentModel.name) << ",\n";
        json << "  \"rim\": " << jsonStr(ex.rimStem) << ",\n";
        json << "  \"format\": " << jsonStr(formatName) << ",\n";
        json << "  \"coordinate_system\": \"z_up\",\n";
        json << "  \"terrain\": {\n";
        json << "    \"materials\": [\n";
        for (size_t ti = 0; ti < s_terrainMats.size(); ti++) {
            const auto& tm = s_terrainMats[ti];
            if (ti > 0) json << ",\n";
            json << "      {\n";
            json << "        \"name\": " << jsonStr(tm.matName) << ",\n";
            json << "        \"palette\": " << jsonStr(tm.palettePath) << ",\n";
            json << "        \"maskA\": " << jsonStr(tm.maskAPath) << ",\n";
            if (!tm.maskA2Path.empty())
                json << "        \"maskA2\": " << jsonStr(tm.maskA2Path) << ",\n";
            json << "        \"totalCells\": " << tm.totalCells << ",\n";
            char buf[256];
            snprintf(buf, sizeof(buf), "[%.6f, %.6f, %.6f, %.6f]",
                     tm.palDim[0], tm.palDim[1], tm.palDim[2], tm.palDim[3]);
            json << "        \"palDim\": " << buf << ",\n";
            snprintf(buf, sizeof(buf), "[%.6f, %.6f, %.6f, %.6f]",
                     tm.palParam[0], tm.palParam[1], tm.palParam[2], tm.palParam[3]);
            json << "        \"palParam\": " << buf << ",\n";
            snprintf(buf, sizeof(buf), "[%.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f]",
                     tm.uvScales[0], tm.uvScales[1], tm.uvScales[2], tm.uvScales[3],
                     tm.uvScales[4], tm.uvScales[5], tm.uvScales[6], tm.uvScales[7]);
            json << "        \"uvScales\": " << buf << "\n";
            json << "      }";
        }
        json << "\n    ],\n";
        json << "    \"patches\": {\n";
        bool first = true;
        for (const auto& [key, group] : s_propGroups) {
            if (!group.isTerrain) continue;
            if (!first) json << ",\n";
            first = false;
            json << "      " << jsonStr(group.modelName) << ": {\n";
            json << "        \"file\": " << jsonStr("models/" + group.fileName) << ",\n";
            json << "        \"instances\": [\n";
            for (size_t ii = 0; ii < group.instanceIndices.size(); ii++) {
                const auto& pw = ll.propQueue[group.instanceIndices[ii]];
                json << "          {\"position\": " << jsonVec3(pw.px, pw.py, pw.pz)
                     << ", \"rotation\": " << jsonVec4(pw.qx, pw.qy, pw.qz, pw.qw)
                     << ", \"scale\": " << pw.scale << "}";
                if (ii + 1 < group.instanceIndices.size()) json << ",";
                json << "\n";
            }
            json << "        ]\n      }";
        }
        json << "\n    }\n  },\n";
        json << "  \"props\": {\n";
        first = true;
        for (const auto& [key, group] : s_propGroups) {
            if (group.isTerrain) continue;
            if (!first) json << ",\n";
            first = false;
            json << "    " << jsonStr(group.modelName) << ": {\n";
            json << "      \"file\": " << jsonStr("models/" + group.fileName) << ",\n";
            json << "      \"instances\": [\n";
            for (size_t ii = 0; ii < group.instanceIndices.size(); ii++) {
                const auto& pw = ll.propQueue[group.instanceIndices[ii]];
                json << "        {\"position\": " << jsonVec3(pw.px, pw.py, pw.pz)
                     << ", \"rotation\": " << jsonVec4(pw.qx, pw.qy, pw.qz, pw.qw)
                     << ", \"scale\": " << pw.scale << "}";
                if (ii + 1 < group.instanceIndices.size()) json << ",";
                json << "\n";
            }
            json << "      ]\n    }";
        }
        json << "\n  },\n  \"trees\": {\n";
        first = true;
        for (const auto& group : s_treeGroups) {
            if (group.sptFileName.empty()) continue;
            if (!first) json << ",\n";
            first = false;
            json << "    " << jsonStr(group.baseName) << ": {\n";
            json << "      \"file\": " << jsonStr("models/" + group.fileName) << ",\n";
            json << "      \"instances\": [\n";
            for (size_t ii = 0; ii < group.instanceIndices.size(); ii++) {
                const auto& sw = ll.sptQueue[group.instanceIndices[ii]];
                json << "        {\"position\": " << jsonVec3(sw.px, sw.py, sw.pz)
                     << ", \"rotation\": " << jsonVec4(sw.qx, sw.qy, sw.qz, sw.qw)
                     << ", \"scale\": " << sw.scale << "}";
                if (ii + 1 < group.instanceIndices.size()) json << ",";
                json << "\n";
            }
            json << "      ]\n    }";
        }
        json << "\n  }\n}\n";
        std::ofstream jsonFile(ex.outputDir + "/" + ex.rimStem + ".havenarea");
        jsonFile << json.str();

        state.statusMessage = "Exported " + ex.rimStem + ": " + std::to_string(ex.propsExported) +
            " props, " + std::to_string(ex.treesExported) + " trees";
        s_propGroups.clear();
        s_treeGroups.clear();
        s_propRanges.clear();
        s_terrainMats.clear();
        s_erfCache.clear();
        ex.stage = 0;
    }
}