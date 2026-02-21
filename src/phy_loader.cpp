#include "mmh_loader.h"
#include "model_loader.h"
#include "dds_loader.h"
#include "rml_loader.h"
#include "animation.h"
#include "Gff.h"
#include "erf.h"
#include "Shaders/d3d_context.h"
#include "renderer.h"
#include "terrain_loader.h"
#include <algorithm>
#include <functional>
#include <cmath>
#include <iostream>

static void applyMeshLocalTransforms(Model& model) {
    auto quatRot = [](float qx, float qy, float qz, float qw,
                      float vx, float vy, float vz,
                      float& ox, float& oy, float& oz) {
        float tx = 2.0f * (qy * vz - qz * vy);
        float ty = 2.0f * (qz * vx - qx * vz);
        float tz = 2.0f * (qx * vy - qy * vx);
        ox = vx + qw * tx + (qy * tz - qz * ty);
        oy = vy + qw * ty + (qz * tx - qx * tz);
        oz = vz + qw * tz + (qx * ty - qy * tx);
    };

    for (auto& mesh : model.meshes) {
        if (mesh.hasSkinning) continue;

        float posX = mesh.localPosX, posY = mesh.localPosY, posZ = mesh.localPosZ;
        float rotX = mesh.localRotX, rotY = mesh.localRotY, rotZ = mesh.localRotZ, rotW = mesh.localRotW;

        if (!mesh.parentBoneName.empty()) {
            int boneIdx = model.skeleton.findBone(mesh.parentBoneName);
            if (boneIdx >= 0) {
                const Bone& bone = model.skeleton.bones[boneIdx];
                float rp_x, rp_y, rp_z;
                quatRot(bone.worldRotX, bone.worldRotY, bone.worldRotZ, bone.worldRotW,
                        posX, posY, posZ, rp_x, rp_y, rp_z);
                posX = rp_x + bone.worldPosX;
                posY = rp_y + bone.worldPosY;
                posZ = rp_z + bone.worldPosZ;
                float cw = bone.worldRotW*rotW - bone.worldRotX*rotX - bone.worldRotY*rotY - bone.worldRotZ*rotZ;
                float cx = bone.worldRotW*rotX + bone.worldRotX*rotW + bone.worldRotY*rotZ - bone.worldRotZ*rotY;
                float cy = bone.worldRotW*rotY - bone.worldRotX*rotZ + bone.worldRotY*rotW + bone.worldRotZ*rotX;
                float cz = bone.worldRotW*rotZ + bone.worldRotX*rotY - bone.worldRotY*rotX + bone.worldRotZ*rotW;
                rotX = cx; rotY = cy; rotZ = cz; rotW = cw;
            }
        }

        float posMag = posX*posX + posY*posY + posZ*posZ;
        bool hasRot = !(std::abs(rotW) > 0.9999f);
        if (posMag < 0.0001f && !hasRot) continue;

        for (auto& v : mesh.vertices) {
            float rx, ry, rz;
            quatRot(rotX, rotY, rotZ, rotW, v.x, v.y, v.z, rx, ry, rz);
            v.x = rx + posX;
            v.y = ry + posY;
            v.z = rz + posZ;

            float rnx, rny, rnz;
            quatRot(rotX, rotY, rotZ, rotW, v.nx, v.ny, v.nz, rnx, rny, rnz);
            v.nx = rnx; v.ny = rny; v.nz = rnz;
        }
        mesh.calculateBounds();
    }
}
#include <set>
#include <unordered_map>
#include <filesystem>
namespace fs = std::filesystem;

bool loadPHY(const std::vector<uint8_t>& data, Model& model) {
    GFFFile gff;
    if (!gff.load(data)) return false;
    auto quatMul = [](float q1x, float q1y, float q1z, float q1w,
                      float q2x, float q2y, float q2z, float q2w,
                      float& rx, float& ry, float& rz, float& rw) {
        rw = q1w*q2w - q1x*q2x - q1y*q2y - q1z*q2z;
        rx = q1w*q2x + q1x*q2w + q1y*q2z - q1z*q2y;
        ry = q1w*q2y - q1x*q2z + q1y*q2w + q1z*q2x;
        rz = q1w*q2z + q1x*q2y - q1y*q2x + q1z*q2w;
    };
    auto quatRotate = [](float qx, float qy, float qz, float qw,
                         float vx, float vy, float vz,
                         float& rx, float& ry, float& rz) {
        float cx = qy*vz - qz*vy;
        float cy = qz*vx - qx*vz;
        float cz = qx*vy - qy*vx;
        float cx2 = qy*cz - qz*cy;
        float cy2 = qz*cx - qx*cz;
        float cz2 = qx*cy - qy*cx;
        rx = vx + 2.0f*(qw*cx + cx2);
        ry = vy + 2.0f*(qw*cy + cy2);
        rz = vz + 2.0f*(qw*cz + cz2);
    };
    std::function<void(size_t, uint32_t, const std::string&)> processStruct =
        [&](size_t structIdx, uint32_t offset, const std::string& parentBoneName) {
        if (structIdx >= gff.structs().size()) return;
        const auto& st = gff.structs()[structIdx];
        std::string structType(st.structType);
        std::string currentBoneName = parentBoneName;
        if (structType == "node") {
            std::string name = gff.readStringByLabel(structIdx, 6000, offset);
            if (!name.empty()) currentBoneName = name;
        }
        if (structType == "shap") {
            CollisionShape shape;
            shape.name = gff.readStringByLabel(structIdx, 6241, offset);
            if (shape.name.empty()) shape.name = "collision_" + std::to_string(model.collisionShapes.size());
            shape.boneName = currentBoneName;
            float localPosX = 0, localPosY = 0, localPosZ = 0;
            const GFFField* posField = gff.findField(structIdx, 6061);
            if (posField) {
                uint32_t posOffset = gff.dataOffset() + posField->dataOffset + offset;
                localPosX = gff.readFloatAt(posOffset);
                localPosY = gff.readFloatAt(posOffset + 4);
                localPosZ = gff.readFloatAt(posOffset + 8);
            }
            float localRotX = 0, localRotY = 0, localRotZ = 0, localRotW = 1;
            const GFFField* rotField = gff.findField(structIdx, 6060);
            if (rotField) {
                uint32_t rotOffset = gff.dataOffset() + rotField->dataOffset + offset;
                localRotX = gff.readFloatAt(rotOffset);
                localRotY = gff.readFloatAt(rotOffset + 4);
                localRotZ = gff.readFloatAt(rotOffset + 8);
                localRotW = gff.readFloatAt(rotOffset + 12);
            }
            shape.localPosX = localPosX; shape.localPosY = localPosY; shape.localPosZ = localPosZ;
            shape.localRotX = localRotX; shape.localRotY = localRotY; shape.localRotZ = localRotZ; shape.localRotW = localRotW;
            int boneIdx = model.skeleton.findBone(currentBoneName);
            shape.boneIndex = boneIdx;
            if (boneIdx >= 0) {
                const Bone& bone = model.skeleton.bones[boneIdx];
                float rx, ry, rz;
                quatRotate(bone.worldRotX, bone.worldRotY, bone.worldRotZ, bone.worldRotW,
                           localPosX, localPosY, localPosZ, rx, ry, rz);
                shape.posX = bone.worldPosX + rx;
                shape.posY = bone.worldPosY + ry;
                shape.posZ = bone.worldPosZ + rz;
                quatMul(bone.worldRotX, bone.worldRotY, bone.worldRotZ, bone.worldRotW,
                        localRotX, localRotY, localRotZ, localRotW,
                        shape.rotX, shape.rotY, shape.rotZ, shape.rotW);
            } else {
                shape.posX = localPosX; shape.posY = localPosY; shape.posZ = localPosZ;
                shape.rotX = localRotX; shape.rotY = localRotY; shape.rotZ = localRotZ; shape.rotW = localRotW;
                shape.meshVertsWorldSpace = true;
            }
            const GFFField* shapeTypeField = gff.findField(structIdx, 6998);
            GFFStructRef dataRef;
            bool hasShapeData = false;
            if (shapeTypeField) {
                bool isList = (shapeTypeField->flags & 0x8000) != 0;
                bool isStruct = (shapeTypeField->flags & 0x4000) != 0;
                bool isRef = (shapeTypeField->flags & 0x2000) != 0;
                uint32_t dataPos = gff.dataOffset() + shapeTypeField->dataOffset + offset;
                if (isRef && !isList && !isStruct) {
                    uint16_t refStructIdx = gff.readUInt16At(dataPos);
                    uint32_t refOffset = gff.readUInt32At(dataPos + 4);
                    if (refStructIdx < gff.structs().size()) {
                        dataRef.structIndex = refStructIdx;
                        dataRef.offset = refOffset;
                        hasShapeData = true;
                    }
                } else if (isStruct && !isList) {
                    int32_t ref = gff.readInt32At(dataPos);
                    if (ref >= 0) {
                        dataRef.structIndex = shapeTypeField->typeId;
                        dataRef.offset = ref;
                        hasShapeData = true;
                    }
                } else {
                    std::vector<GFFStructRef> shapeData = gff.readStructList(structIdx, 6998, offset);
                    if (!shapeData.empty()) { dataRef = shapeData[0]; hasShapeData = true; }
                }
            }
            bool shapeValid = false;
            if (hasShapeData && dataRef.structIndex < gff.structs().size()) {
                std::string dataType(gff.structs()[dataRef.structIndex].structType);
                if (dataType == "boxs") {
                    shape.type = CollisionShapeType::Box;
                    const GFFField* dimField = gff.findField(dataRef.structIndex, 6071);
                    if (dimField) {
                        uint32_t dimOffset = gff.dataOffset() + dimField->dataOffset + dataRef.offset;
                        shape.boxX = gff.readFloatAt(dimOffset);
                        shape.boxY = gff.readFloatAt(dimOffset + 4);
                        shape.boxZ = gff.readFloatAt(dimOffset + 8);
                        shapeValid = (shape.boxX != 0 || shape.boxY != 0 || shape.boxZ != 0);
                    }
                } else if (dataType == "sphs") {
                    shape.type = CollisionShapeType::Sphere;
                    const GFFField* radField = gff.findField(dataRef.structIndex, 6072);
                    if (radField) shape.radius = gff.readFloatAt(gff.dataOffset() + radField->dataOffset + dataRef.offset);
                    shapeValid = (shape.radius > 0.0f);
                } else if (dataType == "caps") {
                    shape.type = CollisionShapeType::Capsule;
                    const GFFField* radField = gff.findField(dataRef.structIndex, 6072);
                    const GFFField* htField = gff.findField(dataRef.structIndex, 6073);
                    if (radField) shape.radius = gff.readFloatAt(gff.dataOffset() + radField->dataOffset + dataRef.offset);
                    if (htField) shape.height = gff.readFloatAt(gff.dataOffset() + htField->dataOffset + dataRef.offset);
                    shapeValid = (shape.radius > 0.0f && shape.height > 0.0f);
                } else if (dataType == "mshs") {
                    shape.type = CollisionShapeType::Mesh;
                    const GFFField* meshDataField = gff.findField(dataRef.structIndex, 6077);
                    if (meshDataField) {
                        uint32_t meshDataPos = gff.dataOffset() + meshDataField->dataOffset + dataRef.offset;
                        int32_t listRef = gff.readInt32At(meshDataPos);
                        if (listRef >= 0) {
                            uint32_t nxsPos = gff.dataOffset() + listRef + 4;
                            const auto& rawData = gff.rawData();
                            if (nxsPos + 36 < rawData.size()) {
                                nxsPos += 28;
                                uint32_t vertCount = gff.readUInt32At(nxsPos);
                                nxsPos += 4;
                                uint32_t faceCount = gff.readUInt32At(nxsPos);
                                nxsPos += 4;
                                size_t vertsDataSize = vertCount * 3 * sizeof(float);
                                if (nxsPos + vertsDataSize <= rawData.size()) {
                                    shape.meshVerts.reserve(vertCount * 3);
                                    for (uint32_t v = 0; v < vertCount; v++) {
                                        shape.meshVerts.push_back(gff.readFloatAt(nxsPos));
                                        shape.meshVerts.push_back(gff.readFloatAt(nxsPos + 4));
                                        shape.meshVerts.push_back(gff.readFloatAt(nxsPos + 8));
                                        nxsPos += 12;
                                    }
                                    size_t facesDataSize = faceCount * 3;
                                    if (nxsPos + facesDataSize <= rawData.size()) {
                                        shape.meshIndices.reserve(faceCount * 3);
                                        for (uint32_t f = 0; f < faceCount; f++) {
                                            shape.meshIndices.push_back(gff.readUInt8At(nxsPos));
                                            shape.meshIndices.push_back(gff.readUInt8At(nxsPos + 1));
                                            shape.meshIndices.push_back(gff.readUInt8At(nxsPos + 2));
                                            nxsPos += 3;
                                        }
                                        shapeValid = !shape.meshVerts.empty();
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (shapeValid) model.collisionShapes.push_back(shape);
        }
        std::vector<GFFStructRef> children = gff.readStructList(structIdx, 6999, offset);
        for (const auto& child : children) processStruct(child.structIndex, child.offset, currentBoneName);
    };
    processStruct(0, 0, "");
    return !model.collisionShapes.empty();
}

static void loadTextureErfs(AppState& state) {
    if (state.textureErfsLoaded) return;
    state.textureErfs.clear();
    state.textureErfPaths.clear();
    for (const auto& erfPath : state.erfFiles) {
        std::string filename = fs::path(erfPath).filename().string();
        std::string filenameLower = filename;
        std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);
        if (filenameLower.find("texture") != std::string::npos) {
            auto erf = std::make_unique<ERFFile>();
            if (erf->open(erfPath) && erf->encryption() == 0) {
                std::cout << "[TEX-ERFS] Loaded: " << filename << " (" << erf->entries().size() << " entries)" << std::endl;
                state.textureErfs.push_back(std::move(erf));
                state.textureErfPaths.push_back(erfPath);
            } else {
                auto erf2 = std::make_unique<ERFFile>();
                if (erf2->open(erfPath))
                    std::cout << "[TEX-ERFS] SKIPPED: " << filename << " (encryption=" << erf2->encryption() << ", entries=" << erf2->entries().size() << ")" << std::endl;
                else
                    std::cout << "[TEX-ERFS] FAILED: " << filename << std::endl;
            }
        }
    }
    std::cout << "[TEX-ERFS] Total: " << state.textureErfs.size() << " texture ERFs loaded" << std::endl;
    state.textureErfsLoaded = true;
}

static void loadModelErfs(AppState& state) {
    if (state.modelErfsLoaded) return;
    state.modelErfs.clear();
    state.modelErfPaths.clear();
    for (const auto& erfPath : state.erfFiles) {
        std::string extLower = fs::path(erfPath).extension().string();
        std::transform(extLower.begin(), extLower.end(), extLower.begin(), ::tolower);
        if (extLower == ".lvl") continue;

        std::string pathLower = erfPath;
        std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);
        bool isModel = pathLower.find("model") != std::string::npos ||
                       pathLower.find("morph") != std::string::npos ||
                       pathLower.find("face") != std::string::npos ||
                       pathLower.find("chargen") != std::string::npos;
        if (isModel) {
            auto erf = std::make_unique<ERFFile>();
            if (erf->open(erfPath) && erf->encryption() == 0) {
                state.modelErfs.push_back(std::move(erf));
                state.modelErfPaths.push_back(erfPath);
            }
        }
    }
    state.modelErfsLoaded = true;
}

static void loadMaterialErfs(AppState& state) {
    if (state.materialErfsLoaded) return;
    state.materialErfs.clear();
    state.materialErfPaths.clear();
    for (const auto& erfPath : state.erfFiles) {
        std::string filename = fs::path(erfPath).filename().string();
        std::string filenameLower = filename;
        std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);
        if (filenameLower.find("materialobject") != std::string::npos) {
            auto erf = std::make_unique<ERFFile>();
            if (erf->open(erfPath) && erf->encryption() == 0) {
                state.materialErfs.push_back(std::move(erf));
                state.materialErfPaths.push_back(erfPath);
            }
        }
    }
    state.materialErfsLoaded = true;
}

void ensureBaseErfsLoaded(AppState& state) {
    loadTextureErfs(state);
    loadModelErfs(state);
    loadMaterialErfs(state);
}

// Forward declarations for ERF index system (defined later in file)
struct ErfIndexEntry {
    ERFFile* erf;
    size_t entryIdx;
};
static std::unordered_map<std::string, ErfIndexEntry> s_erfIndex;
static std::unordered_map<std::string, ErfIndexEntry> s_erfIndexNoExt;
static bool s_erfIndexBuilt = false;
static std::vector<std::unique_ptr<ERFFile>> s_indexedErfs;
static std::unordered_map<std::string, uint32_t> s_texIdCache;
static std::vector<uint8_t> readFromErfIndex(const std::string& name, std::string* sourceOut = nullptr);

static std::vector<uint8_t> readFromModelErfs(AppState& state, const std::string& name) {
    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
    for (const auto& erf : state.modelErfs) {
        for (const auto& entry : erf->entries()) {
            std::string entryLower = entry.name;
            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
            if (entryLower == nameLower) return erf->readEntry(entry);
        }
    }
    // Fallback: search the source ERF the model was loaded from
    if (state.currentErf) {
        std::string noExtLower = nameLower;
        std::string nameExt;
        size_t dp = noExtLower.rfind('.');
        if (dp != std::string::npos) {
            nameExt = noExtLower.substr(dp); // e.g. ".mmh"
            noExtLower = noExtLower.substr(0, dp);
        }
        for (const auto& entry : state.currentErf->entries()) {
            std::string entryLower = entry.name;
            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
            if (entryLower == nameLower) return state.currentErf->readEntry(entry);
            // No-ext fallback: only match if extensions also match
            std::string entryNoExt = entryLower;
            std::string entryExt;
            size_t edp = entryNoExt.rfind('.');
            if (edp != std::string::npos) {
                entryExt = entryNoExt.substr(edp);
                entryNoExt = entryNoExt.substr(0, edp);
            }
            if (entryNoExt == noExtLower && entryExt == nameExt) return state.currentErf->readEntry(entry);
        }
    }
    return {};
}

static std::vector<uint8_t> readFromMaterialErfs(AppState& state, const std::string& name) {
    if (s_erfIndexBuilt) {
        auto result = readFromErfIndex(name);
        if (!result.empty()) return result;
    }

    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
    std::string noExtLower = nameLower;
    std::string nameExt;
    size_t dp = noExtLower.rfind('.');
    if (dp != std::string::npos) {
        nameExt = noExtLower.substr(dp);
        noExtLower = noExtLower.substr(0, dp);
    }

    const std::vector<std::unique_ptr<ERFFile>>* erfSets[] = {
        &state.materialErfs, &state.modelErfs, &state.textureErfs
    };
    for (const auto* erfs : erfSets) {
        for (const auto& erf : *erfs) {
            for (const auto& entry : erf->entries()) {
                std::string entryLower = entry.name;
                std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
                if (entryLower == nameLower) return erf->readEntry(entry);
                // No-ext fallback: only match if extensions also match
                std::string entryNoExt = entryLower;
                std::string entryExt;
                size_t edp = entryNoExt.rfind('.');
                if (edp != std::string::npos) {
                    entryExt = entryNoExt.substr(edp);
                    entryNoExt = entryNoExt.substr(0, edp);
                }
                if (entryNoExt == noExtLower && entryExt == nameExt) return erf->readEntry(entry);
            }
        }
    }

    if (state.currentErf) {
        for (const auto& entry : state.currentErf->entries()) {
            std::string entryLower = entry.name;
            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
            if (entryLower == nameLower) return state.currentErf->readEntry(entry);
            std::string entryNoExt = entryLower;
            std::string entryExt;
            size_t edp = entryNoExt.rfind('.');
            if (edp != std::string::npos) {
                entryExt = entryNoExt.substr(edp);
                entryNoExt = entryNoExt.substr(0, edp);
            }
            if (entryNoExt == noExtLower && entryExt == nameExt) return state.currentErf->readEntry(entry);
        }
    }

    return {};
}

static uint32_t loadCubemapByName(AppState& state, const std::string& texName) {
    if (texName.empty()) return 0;
    std::string lower = texName;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    std::string withDds = lower;
    if (withDds.size() < 4 || withDds.substr(withDds.size() - 4) != ".dds")
        withDds += ".dds";

    if (s_erfIndexBuilt) {
        auto tryKey = [&](const std::string& key) -> uint32_t {
            auto it = s_erfIndex.find(key);
            if (it != s_erfIndex.end()) {
                const auto& entries = it->second.erf->entries();
                if (it->second.entryIdx < entries.size()) {
                    auto data = it->second.erf->readEntry(entries[it->second.entryIdx]);
                    if (!data.empty() && isDDSCubemap(data))
                        return createTextureCubeFromDDS(data);
                }
            }
            return 0;
        };
        uint32_t id = tryKey(withDds);
        if (!id) id = tryKey(lower);
        if (!id) {
            std::string noext = lower;
            size_t dp = noext.rfind('.');
            if (dp != std::string::npos) noext = noext.substr(0, dp);
            auto it = s_erfIndexNoExt.find(noext);
            if (it != s_erfIndexNoExt.end()) {
                const auto& entries = it->second.erf->entries();
                if (it->second.entryIdx < entries.size()) {
                    auto data = it->second.erf->readEntry(entries[it->second.entryIdx]);
                    if (!data.empty() && isDDSCubemap(data))
                        id = createTextureCubeFromDDS(data);
                }
            }
        }
        return id;
    }

    auto tryErf = [&](ERFFile& erf) -> uint32_t {
        for (const auto& e : erf.entries()) {
            std::string eName = e.name;
            std::transform(eName.begin(), eName.end(), eName.begin(), ::tolower);
            if (eName == lower || eName == withDds) {
                auto data = erf.readEntry(e);
                if (!data.empty() && isDDSCubemap(data)) {
                    return createTextureCubeFromDDS(data);
                }
            }
        }
        return 0;
    };

    for (auto& erf : state.textureErfs)   { uint32_t id = tryErf(*erf); if (id) return id; }
    for (auto& erf : state.materialErfs)   { uint32_t id = tryErf(*erf); if (id) return id; }
    for (auto& erf : state.modelErfs)      { uint32_t id = tryErf(*erf); if (id) return id; }

    return 0;
}

static uint32_t findAnyCubemapInErfs(AppState& state) {
    auto scanErf = [&](ERFFile& erf) -> uint32_t {
        for (const auto& e : erf.entries()) {
            std::string eName = e.name;
            std::transform(eName.begin(), eName.end(), eName.begin(), ::tolower);
            if (eName.find("cubemap") != std::string::npos ||
                eName.find("probe") != std::string::npos ||
                eName.find("envmap") != std::string::npos) {
                std::vector<uint8_t> data = erf.readEntry(e);
                if (!data.empty() && isDDSCubemap(data)) {
                    uint32_t id = createTextureCubeFromDDS(data);
                    if (id) {
                        std::cout << "[CUBEMAP] Found cubemap: '" << e.name << "'" << std::endl;
                        return id;
                    }
                }
            }
        }
        return 0;
    };

    for (auto& erf : state.textureErfs) { uint32_t id = scanErf(*erf); if (id) return id; }
    return 0;
}

static uint32_t loadTextureByName(AppState& state, const std::string& texName,
                                  std::vector<uint8_t>* rgbaOut = nullptr,
                                  int* wOut = nullptr, int* hOut = nullptr) {
    if (texName.empty()) return 0;
    std::string texNameLower = texName;
    std::transform(texNameLower.begin(), texNameLower.end(), texNameLower.begin(), ::tolower);

    std::string noExtLower = texNameLower;
    size_t dp = noExtLower.rfind('.');
    if (dp != std::string::npos) noExtLower = noExtLower.substr(0, dp);

    std::string withDdsLower = noExtLower + ".dds";
    std::string withXdsLower = noExtLower + ".xds";

    // Check dedup cache (skip if caller wants RGBA data back)
    if (!rgbaOut) {
        auto cit = s_texIdCache.find(noExtLower);
        if (cit != s_texIdCache.end()) return cit->second;
    }

    // Helper: create GL texture from raw data (DDS, XDS, or TGA)
    auto createTextureFromAny = [&](const std::vector<uint8_t>& texData) -> uint32_t {
        if (isXDS(texData)) {
            std::vector<uint8_t> rgba; int w, h;
            if (decodeXDSToRGBA(texData, rgba, w, h)) {
                if (rgbaOut && wOut && hOut) { *rgbaOut = rgba; *wOut = w; *hOut = h; }
                return createTexture2D(rgba.data(), w, h);
            }
            return 0;
        }
        if (rgbaOut && wOut && hOut) decodeDDSToRGBA(texData, *rgbaOut, *wOut, *hOut);
        return createTextureFromDDS(texData);
    };

    // Fast path: use the global ERF index if available
    if (s_erfIndexBuilt) {
        auto tryIndex = [&](const std::string& key) -> std::vector<uint8_t> {
            auto it = s_erfIndex.find(key);
            if (it != s_erfIndex.end()) {
                const auto& entries = it->second.erf->entries();
                if (it->second.entryIdx < entries.size())
                    return it->second.erf->readEntry(entries[it->second.entryIdx]);
            }
            return {};
        };

        std::vector<uint8_t> texData = tryIndex(withDdsLower);
        if (texData.empty()) texData = tryIndex(withXdsLower);
        if (texData.empty()) texData = tryIndex(texNameLower);
        if (texData.empty()) {
            auto it = s_erfIndexNoExt.find(noExtLower);
            if (it != s_erfIndexNoExt.end()) {
                const auto& entries = it->second.erf->entries();
                if (it->second.entryIdx < entries.size())
                    texData = it->second.erf->readEntry(entries[it->second.entryIdx]);
            }
        }
        if (!texData.empty()) {
            uint32_t id = createTextureFromAny(texData);
            if (id && !rgbaOut) s_texIdCache[noExtLower] = id;
            return id;
        }
        // Index had nothing — fall through to currentErf fallback below
    }

    // Slow fallback: linear scan (only used if index not built yet)
    auto tryLoadFromErf = [&](ERFFile& erf) -> uint32_t {
        for (const auto& entry : erf.entries()) {
            std::string entryLower = entry.name;
            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
            std::string entryNoExt = entryLower;
            size_t edp = entryNoExt.rfind('.');
            if (edp != std::string::npos) entryNoExt = entryNoExt.substr(0, edp);
            if (entryLower == texNameLower || entryLower == withDdsLower ||
                entryLower == withXdsLower || entryNoExt == noExtLower) {
                std::vector<uint8_t> texData = erf.readEntry(entry);
                if (!texData.empty()) {
                    return createTextureFromAny(texData);
                }
            }
        }
        return 0;
    };

    if (!s_erfIndexBuilt) {
        const std::vector<std::unique_ptr<ERFFile>>* erfSets[] = {
            &state.textureErfs, &state.materialErfs, &state.modelErfs
        };
        for (int s = 0; s < 3; s++) {
            for (const auto& erf : *erfSets[s]) {
                uint32_t id = tryLoadFromErf(*erf);
                if (id != 0) return id;
            }
        }
    }
    if (state.currentErf) {
        uint32_t id = tryLoadFromErf(*state.currentErf);
        if (id != 0) return id;
    }

    return 0;
}

bool loadModelFromEntry(AppState& state, const ERFEntry& entry) {
    if (!state.currentErf) return false;
    loadTextureErfs(state);
    loadModelErfs(state);
    loadMaterialErfs(state);
    std::vector<uint8_t> data = state.currentErf->readEntry(entry);
    if (data.empty()) return false;
    Model model;
    if (!loadMSH(data, model)) {
        state.currentModel = Model();
        state.currentModel.name = entry.name + " (failed to parse)";
        state.hasModel = true;
        return false;
    }

    for (const auto& mat : state.currentModel.materials) {
        if (mat.diffuseTexId != 0)          destroyTexture(mat.diffuseTexId);
        if (mat.normalTexId != 0)           destroyTexture(mat.normalTexId);
        if (mat.specularTexId != 0)         destroyTexture(mat.specularTexId);
        if (mat.tintTexId != 0)             destroyTexture(mat.tintTexId);
        if (mat.ageDiffuseTexId != 0)       destroyTexture(mat.ageDiffuseTexId);
        if (mat.ageNormalTexId != 0)        destroyTexture(mat.ageNormalTexId);
        if (mat.tattooTexId != 0)           destroyTexture(mat.tattooTexId);
        if (mat.browStubbleTexId != 0)      destroyTexture(mat.browStubbleTexId);
        if (mat.browStubbleNormalTexId != 0) destroyTexture(mat.browStubbleNormalTexId);
        if (mat.paletteTexId != 0)          destroyTexture(mat.paletteTexId);
        if (mat.palNormalTexId != 0)        destroyTexture(mat.palNormalTexId);
        if (mat.maskVTexId != 0)            destroyTexture(mat.maskVTexId);
        if (mat.maskATexId != 0)            destroyTexture(mat.maskATexId);
        if (mat.maskA2TexId != 0)           destroyTexture(mat.maskA2TexId);
        if (mat.reliefTexId != 0)           destroyTexture(mat.reliefTexId);
    }
    for (const auto& mat : state.skyboxModel.materials) {
        if (mat.diffuseTexId != 0) destroyTexture(mat.diffuseTexId);
        if (mat.normalTexId != 0)  destroyTexture(mat.normalTexId);
    }
    state.skyboxModel = Model();
    state.skyboxLoaded = false;
    state.envSettings = EnvironmentSettings();
    destroyLevelBuffers();

    state.currentModel = model;
    state.currentModel.name = entry.name;
    state.hasModel = true;
    state.selectedLevelChunk = -1;
    state.renderSettings.initMeshVisibility(model.meshes.size());
    std::string baseName = entry.name;
    size_t dotPos = baseName.rfind('.');
    if (dotPos != std::string::npos) baseName = baseName.substr(0, dotPos);

    std::vector<std::string> mmhCandidates = {baseName + ".mmh", baseName + "a.mmh"};
    size_t lastUnderscore = baseName.find_last_of('_');
    if (lastUnderscore != std::string::npos) {
        std::string variantA = baseName;
        variantA.insert(lastUnderscore, "a");
        mmhCandidates.push_back(variantA + ".mmh");
    }
    bool mmhFound = false;
    for (const auto& candidate : mmhCandidates) {
        std::cout << "[LOAD-DEBUG] Looking for MMH: '" << candidate << "'" << std::endl;
        std::vector<uint8_t> mmhData = readFromModelErfs(state, candidate);
        if (!mmhData.empty()) {
            std::cout << "[LOAD-DEBUG] Found MMH: '" << candidate << "' (" << mmhData.size() << " bytes)" << std::endl;
            if (mmhData.size() >= 12) {
                std::string hdr(mmhData.begin(), mmhData.begin() + std::min((size_t)12, mmhData.size()));
                std::cout << "[LOAD-DEBUG] MMH header: '" << hdr << "'" << std::endl;
            }
            loadMMH(mmhData, state.currentModel);
            mmhFound = true;
            std::cout << "[LOAD-DEBUG] After loadMMH: meshes=" << state.currentModel.meshes.size()
                      << " bones=" << state.currentModel.boneIndexArray.size() << std::endl;
            for (size_t mi = 0; mi < state.currentModel.meshes.size(); mi++) {
                std::cout << "[LOAD-DEBUG]   mesh[" << mi << "] name='" << state.currentModel.meshes[mi].name
                          << "' materialName='" << state.currentModel.meshes[mi].materialName
                          << "' hasSkinning=" << state.currentModel.meshes[mi].hasSkinning << std::endl;
            }
            break;
        }
    }
    if (!mmhFound) {
        std::cout << "[LOAD-DEBUG] MMH NOT FOUND for baseName='" << baseName << "'" << std::endl;
    }

    applyMeshLocalTransforms(state.currentModel);

    std::vector<std::string> phyCandidates = {baseName + ".phy", baseName + "a.phy"};
    if (lastUnderscore != std::string::npos) {
        std::string variantA = baseName;
        variantA.insert(lastUnderscore, "a");
        phyCandidates.push_back(variantA + ".phy");
    }
    for (const auto& candidate : phyCandidates) {
        std::vector<uint8_t> phyData = readFromModelErfs(state, candidate);
        if (!phyData.empty()) {
            std::cout << "[LOAD-DEBUG] Found PHY: '" << candidate << "' (" << phyData.size() << " bytes)" << std::endl;
            loadPHY(phyData, state.currentModel);
            std::cout << "[LOAD-DEBUG] After loadPHY: collisionShapes=" << state.currentModel.collisionShapes.size() << std::endl;
            break;
        }
    }

    std::set<std::string> materialNames;
    for (const auto& mesh : state.currentModel.meshes) {
        if (!mesh.materialName.empty()) materialNames.insert(mesh.materialName);
    }
    std::cout << "[LOAD-DEBUG] Material names to look up: " << materialNames.size() << std::endl;
    for (const std::string& matName : materialNames) {
        std::cout << "[LOAD-DEBUG]   material: '" << matName << "'" << std::endl;

        std::string maoLookup = matName;
        {
            std::string lower = matName;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.size() < 4 || lower.substr(lower.size() - 4) != ".mao")
                maoLookup += ".mao";
        }
        std::vector<uint8_t> maoData = readFromMaterialErfs(state, maoLookup);
        if (!maoData.empty()) {
            std::cout << "[LOAD-DEBUG]   MAO found: '" << maoLookup << "' (" << maoData.size() << " bytes)" << std::endl;
            std::string maoContent(maoData.begin(), maoData.end());
            Material mat = parseMAO(maoContent, matName);
            mat.maoContent = maoContent;
            std::cout << "[LOAD-DEBUG]   MAO result: diffuse='" << mat.diffuseMap << "' normal='" << mat.normalMap << "'" << std::endl;
            state.currentModel.materials.push_back(mat);
        } else {
            std::cout << "[LOAD-DEBUG]   MAO NOT FOUND: '" << maoLookup << "'" << std::endl;
            Material mat;
            mat.name = matName;
            state.currentModel.materials.push_back(mat);
        }
    }
    for (auto& mesh : state.currentModel.meshes) {
        if (!mesh.materialName.empty())
            mesh.materialIndex = state.currentModel.findMaterial(mesh.materialName);
    }
    for (auto& mat : state.currentModel.materials) {
        if (!mat.diffuseMap.empty() && mat.diffuseTexId == 0) {
            mat.diffuseTexId = loadTextureByName(state, mat.diffuseMap, &mat.diffuseData, &mat.diffuseWidth, &mat.diffuseHeight);
        }
        if (!mat.normalMap.empty() && mat.normalTexId == 0) {
            mat.normalTexId = loadTextureByName(state, mat.normalMap, &mat.normalData, &mat.normalWidth, &mat.normalHeight);
        }
        if (!mat.specularMap.empty() && mat.specularTexId == 0) {
            mat.specularTexId = loadTextureByName(state, mat.specularMap, &mat.specularData, &mat.specularWidth, &mat.specularHeight);
        }
        if (!mat.tintMap.empty() && mat.tintTexId == 0) {
            mat.tintTexId = loadTextureByName(state, mat.tintMap, &mat.tintData, &mat.tintWidth, &mat.tintHeight);
        }
        if (mat.isTerrain) {
            mat.paletteTexId = mat.diffuseTexId;
            mat.palNormalTexId = mat.normalTexId;
            if (!mat.maskVMap.empty() && mat.maskVTexId == 0)
                mat.maskVTexId = loadTextureByName(state, mat.maskVMap, nullptr, nullptr, nullptr);
            if (!mat.maskAMap.empty() && mat.maskATexId == 0)
                mat.maskATexId = loadTextureByName(state, mat.maskAMap, nullptr, nullptr, nullptr);
            if (!mat.maskA2Map.empty() && mat.maskA2TexId == 0)
                mat.maskA2TexId = loadTextureByName(state, mat.maskA2Map, nullptr, nullptr, nullptr);
            if (!mat.reliefMap.empty() && mat.reliefTexId == 0)
                mat.reliefTexId = loadTextureByName(state, mat.reliefMap, nullptr, nullptr, nullptr);
        }
    }

    if (!state.currentModel.meshes.empty()) {
        float minX = state.currentModel.meshes[0].minX, maxX = state.currentModel.meshes[0].maxX;
        float minY = state.currentModel.meshes[0].minY, maxY = state.currentModel.meshes[0].maxY;
        float minZ = state.currentModel.meshes[0].minZ, maxZ = state.currentModel.meshes[0].maxZ;
        for (const auto& mesh : state.currentModel.meshes) {
            if (mesh.minX < minX) minX = mesh.minX; if (mesh.maxX > maxX) maxX = mesh.maxX;
            if (mesh.minY < minY) minY = mesh.minY; if (mesh.maxY > maxY) maxY = mesh.maxY;
            if (mesh.minZ < minZ) minZ = mesh.minZ; if (mesh.maxZ > maxZ) maxZ = mesh.maxZ;
        }
        float cx = (minX + maxX) / 2.0f, cy = (minY + maxY) / 2.0f, cz = (minZ + maxZ) / 2.0f;
        float dx = maxX - minX, dy = maxY - minY, dz = maxZ - minZ;
        float radius = std::sqrt(dx*dx + dy*dy + dz*dz) / 2.0f;
        state.camera.lookAt(cx, cy, cz, radius * 2.5f);
    }
    findAnimationsForModel(state, baseName);
    if (!state.availableAnimFiles.empty()) state.showAnimWindow = true;
    return true;
}

bool loadModelFromOverride(AppState& state, const std::string& mshPath) {
    loadTextureErfs(state);
    loadModelErfs(state);
    loadMaterialErfs(state);

    auto readFile = [](const std::string& path) -> std::vector<uint8_t> {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) return {};
        size_t sz = static_cast<size_t>(f.tellg());
        f.seekg(0);
        std::vector<uint8_t> data(sz);
        f.read(reinterpret_cast<char*>(data.data()), sz);
        return data;
    };

    auto findInDir = [](const fs::path& dir, const std::string& nameLower) -> std::string {
        if (!fs::exists(dir)) return "";
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            std::string fn = entry.path().filename().string();
            std::string fnLower = fn;
            std::transform(fnLower.begin(), fnLower.end(), fnLower.begin(), ::tolower);
            if (fnLower == nameLower) return entry.path().string();
        }
        return "";
    };

    std::vector<uint8_t> mshData = readFile(mshPath);
    if (mshData.empty()) return false;

    Model model;
    if (!loadMSH(mshData, model)) {
        state.currentModel = Model();
        state.currentModel.name = fs::path(mshPath).filename().string() + " (failed to parse)";
        state.hasModel = true;
        return false;
    }

    for (const auto& mat : state.currentModel.materials) {
        if (mat.diffuseTexId != 0)          destroyTexture(mat.diffuseTexId);
        if (mat.normalTexId != 0)           destroyTexture(mat.normalTexId);
        if (mat.specularTexId != 0)         destroyTexture(mat.specularTexId);
        if (mat.tintTexId != 0)             destroyTexture(mat.tintTexId);
        if (mat.ageDiffuseTexId != 0)       destroyTexture(mat.ageDiffuseTexId);
        if (mat.ageNormalTexId != 0)        destroyTexture(mat.ageNormalTexId);
        if (mat.tattooTexId != 0)           destroyTexture(mat.tattooTexId);
        if (mat.browStubbleTexId != 0)      destroyTexture(mat.browStubbleTexId);
        if (mat.browStubbleNormalTexId != 0) destroyTexture(mat.browStubbleNormalTexId);
        if (mat.paletteTexId != 0)          destroyTexture(mat.paletteTexId);
        if (mat.palNormalTexId != 0)        destroyTexture(mat.palNormalTexId);
        if (mat.maskVTexId != 0)            destroyTexture(mat.maskVTexId);
        if (mat.maskATexId != 0)            destroyTexture(mat.maskATexId);
        if (mat.maskA2TexId != 0)           destroyTexture(mat.maskA2TexId);
        if (mat.reliefTexId != 0)           destroyTexture(mat.reliefTexId);
    }
    for (const auto& mat : state.skyboxModel.materials) {
        if (mat.diffuseTexId != 0) destroyTexture(mat.diffuseTexId);
        if (mat.normalTexId != 0)  destroyTexture(mat.normalTexId);
    }
    state.skyboxModel = Model();
    state.skyboxLoaded = false;
    state.envSettings = EnvironmentSettings();
    destroyLevelBuffers();

    state.currentModel = model;
    state.currentModel.name = fs::path(mshPath).filename().string();
    state.hasModel = true;
    state.selectedLevelChunk = -1;
    state.renderSettings.initMeshVisibility(model.meshes.size());

    std::string baseName = fs::path(mshPath).stem().string();
    fs::path mshDir = fs::path(mshPath).parent_path();

    fs::path overrideRoot = fs::path(state.selectedFolder) / "packages" / "core" / "override";
    std::vector<fs::path> searchDirs = { mshDir };
    if (mshDir != overrideRoot) searchDirs.push_back(overrideRoot);

    std::vector<std::string> mmhCandidates = {baseName + ".mmh", baseName + "a.mmh"};
    size_t lastUnderscore = baseName.find_last_of('_');
    if (lastUnderscore != std::string::npos) {
        std::string variantA = baseName;
        variantA.insert(lastUnderscore, "a");
        mmhCandidates.push_back(variantA + ".mmh");
    }
    bool mmhFound = false;
    for (const auto& candidate : mmhCandidates) {
        std::string candLower = candidate;
        std::transform(candLower.begin(), candLower.end(), candLower.begin(), ::tolower);
        for (const auto& dir : searchDirs) {
            std::string found = findInDir(dir, candLower);
            if (!found.empty()) {
                std::vector<uint8_t> mmhData = readFile(found);
                if (!mmhData.empty()) { loadMMH(mmhData, state.currentModel); mmhFound = true; }
                break;
            }
        }
        if (mmhFound) break;
    }
    if (!mmhFound) {
        for (const auto& candidate : mmhCandidates) {
            std::vector<uint8_t> mmhData = readFromModelErfs(state, candidate);
            if (!mmhData.empty()) { loadMMH(mmhData, state.currentModel); break; }
        }
    }

    applyMeshLocalTransforms(state.currentModel);

    std::vector<std::string> phyCandidates = {baseName + ".phy", baseName + "a.phy"};
    if (lastUnderscore != std::string::npos) {
        std::string variantA = baseName;
        variantA.insert(lastUnderscore, "a");
        phyCandidates.push_back(variantA + ".phy");
    }
    for (const auto& candidate : phyCandidates) {
        std::string candLower = candidate;
        std::transform(candLower.begin(), candLower.end(), candLower.begin(), ::tolower);
        bool phyFound = false;
        for (const auto& dir : searchDirs) {
            std::string found = findInDir(dir, candLower);
            if (!found.empty()) {
                std::vector<uint8_t> phyData = readFile(found);
                if (!phyData.empty()) { loadPHY(phyData, state.currentModel); phyFound = true; }
                break;
            }
        }
        if (phyFound) break;
        std::vector<uint8_t> phyData = readFromModelErfs(state, candidate);
        if (!phyData.empty()) { loadPHY(phyData, state.currentModel); break; }
    }

    std::set<std::string> materialNames;
    for (const auto& mesh : state.currentModel.meshes) {
        if (!mesh.materialName.empty()) materialNames.insert(mesh.materialName);
    }
    for (const std::string& matName : materialNames) {
        std::string maoFile = matName;
        {
            std::string lower = matName;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.size() < 4 || lower.substr(lower.size() - 4) != ".mao")
                maoFile += ".mao";
        }
        std::string maoLower = maoFile;
        std::transform(maoLower.begin(), maoLower.end(), maoLower.begin(), ::tolower);

        std::vector<uint8_t> maoData;
        for (const auto& dir : searchDirs) {
            std::string found = findInDir(dir, maoLower);
            if (!found.empty()) { maoData = readFile(found); break; }
        }
        if (maoData.empty()) {
            maoData = readFromMaterialErfs(state, maoFile);
        }
        if (!maoData.empty()) {
            std::string maoContent(maoData.begin(), maoData.end());
            Material mat = parseMAO(maoContent, matName);
            mat.maoContent = maoContent;
            state.currentModel.materials.push_back(mat);
        } else {
            Material mat;
            mat.name = matName;
            state.currentModel.materials.push_back(mat);
        }
    }
    for (auto& mesh : state.currentModel.meshes) {
        if (!mesh.materialName.empty())
            mesh.materialIndex = state.currentModel.findMaterial(mesh.materialName);
    }

    auto loadTexOverride = [&](const std::string& texName) -> uint32_t {
        if (texName.empty()) return 0;
        std::string texLower = texName;
        std::transform(texLower.begin(), texLower.end(), texLower.begin(), ::tolower);
        std::string noExt = texLower;
        size_t dp = noExt.rfind('.');
        if (dp != std::string::npos) noExt = noExt.substr(0, dp);

        // Try both .dds and .xds
        for (const std::string& ext : { ".dds", ".xds" }) {
            std::string withExt = noExt + ext;
            for (const auto& dir : searchDirs) {
                std::string found = findInDir(dir, withExt);
                if (!found.empty()) {
                    std::vector<uint8_t> texData = readFile(found);
                    if (!texData.empty()) {
                        if (isXDS(texData)) {
                            std::vector<uint8_t> rgba; int w, h;
                            if (decodeXDSToRGBA(texData, rgba, w, h))
                                return createTexture2D(rgba.data(), w, h);
                            return 0;
                        }
                        return createTextureFromDDS(texData);
                    }
                }
            }
        }
        return 0;
    };

    for (auto& mat : state.currentModel.materials) {
        if (!mat.diffuseMap.empty() && mat.diffuseTexId == 0) {
            mat.diffuseTexId = loadTexOverride(mat.diffuseMap);
            if (mat.diffuseTexId == 0)
                mat.diffuseTexId = loadTextureByName(state, mat.diffuseMap, &mat.diffuseData, &mat.diffuseWidth, &mat.diffuseHeight);
        }
        if (!mat.normalMap.empty() && mat.normalTexId == 0) {
            mat.normalTexId = loadTexOverride(mat.normalMap);
            if (mat.normalTexId == 0)
                mat.normalTexId = loadTextureByName(state, mat.normalMap, &mat.normalData, &mat.normalWidth, &mat.normalHeight);
        }
        if (!mat.specularMap.empty() && mat.specularTexId == 0) {
            mat.specularTexId = loadTexOverride(mat.specularMap);
            if (mat.specularTexId == 0)
                mat.specularTexId = loadTextureByName(state, mat.specularMap, &mat.specularData, &mat.specularWidth, &mat.specularHeight);
        }
        if (!mat.tintMap.empty() && mat.tintTexId == 0) {
            mat.tintTexId = loadTexOverride(mat.tintMap);
            if (mat.tintTexId == 0)
                mat.tintTexId = loadTextureByName(state, mat.tintMap, &mat.tintData, &mat.tintWidth, &mat.tintHeight);
        }
    }

    if (!state.currentModel.meshes.empty()) {
        float minX = state.currentModel.meshes[0].minX, maxX = state.currentModel.meshes[0].maxX;
        float minY = state.currentModel.meshes[0].minY, maxY = state.currentModel.meshes[0].maxY;
        float minZ = state.currentModel.meshes[0].minZ, maxZ = state.currentModel.meshes[0].maxZ;
        for (const auto& mesh : state.currentModel.meshes) {
            if (mesh.minX < minX) minX = mesh.minX; if (mesh.maxX > maxX) maxX = mesh.maxX;
            if (mesh.minY < minY) minY = mesh.minY; if (mesh.maxY > maxY) maxY = mesh.maxY;
            if (mesh.minZ < minZ) minZ = mesh.minZ; if (mesh.maxZ > maxZ) maxZ = mesh.maxZ;
        }
        float cx = (minX + maxX) / 2.0f, cy = (minY + maxY) / 2.0f, cz = (minZ + maxZ) / 2.0f;
        float dx = maxX - minX, dy = maxY - minY, dz = maxZ - minZ;
        float radius = std::sqrt(dx*dx + dy*dy + dz*dz) / 2.0f;
        state.camera.lookAt(cx, cy, cz, radius * 2.5f);
    }
    findAnimationsForModel(state, baseName);
    if (!state.availableAnimFiles.empty()) state.showAnimWindow = true;
    return true;
}

bool mergeModelEntry(AppState& state, const ERFEntry& entry) {
    if (!state.currentErf) return false;
    std::vector<uint8_t> data = state.currentErf->readEntry(entry);
    if (data.empty()) {
        std::cout << "[LEVEL] FAILED to read terrain entry: " << entry.name << std::endl;
        return false;
    }
    Model tempModel;
    if (!loadMSH(data, tempModel)) {
        std::cout << "[LEVEL] FAILED to parse terrain MSH: " << entry.name << std::endl;
        return false;
    }


    std::string baseName = entry.name;
    size_t dotPos = baseName.rfind('.');
    if (dotPos != std::string::npos) baseName = baseName.substr(0, dotPos);

    std::vector<std::string> mmhCandidates = {baseName + ".mmh", baseName + "a.mmh"};
    size_t lastUnderscore = baseName.find_last_of('_');
    if (lastUnderscore != std::string::npos) {
        std::string variantA = baseName;
        variantA.insert(lastUnderscore, "a");
        mmhCandidates.push_back(variantA + ".mmh");
    }
    bool mmhFound = false;
    for (const auto& candidate : mmhCandidates) {
        std::vector<uint8_t> mmhData = readFromModelErfs(state, candidate);
        if (!mmhData.empty()) {
            loadMMH(mmhData, tempModel);
            mmhFound = true;
            break;
        }
    }
    if (!mmhFound) {
        std::cout << "[LEVEL] WARNING: no MMH found for terrain: " << baseName << std::endl;
    }

    applyMeshLocalTransforms(tempModel);

    std::set<std::string> newMaterials;
    for (const auto& mesh : tempModel.meshes) {
        if (!mesh.materialName.empty() && state.currentModel.findMaterial(mesh.materialName) < 0) {
            newMaterials.insert(mesh.materialName);
        }
    }


    for (auto& mesh : tempModel.meshes) {
        state.currentModel.meshes.push_back(std::move(mesh));
    }
    state.renderSettings.initMeshVisibility(state.currentModel.meshes.size());


    for (const std::string& matName : newMaterials) {
        std::string maoLookup = matName;
        {
            std::string lower = matName;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.size() < 4 || lower.substr(lower.size() - 4) != ".mao")
                maoLookup += ".mao";
        }
        std::vector<uint8_t> maoData = readFromMaterialErfs(state, maoLookup);
        if (!maoData.empty()) {
            std::string maoContent(maoData.begin(), maoData.end());
            Material mat = parseMAO(maoContent, matName);
            mat.maoContent = maoContent;
            state.currentModel.materials.push_back(mat);
        } else {
            Material mat;
            mat.name = matName;
            state.currentModel.materials.push_back(mat);
        }
    }


    for (auto& mesh : state.currentModel.meshes) {
        if (!mesh.materialName.empty())
            mesh.materialIndex = state.currentModel.findMaterial(mesh.materialName);
    }


    for (auto& mat : state.currentModel.materials) {
        if (!mat.diffuseMap.empty() && mat.diffuseTexId == 0) {
            mat.diffuseTexId = loadTextureByName(state, mat.diffuseMap, &mat.diffuseData, &mat.diffuseWidth, &mat.diffuseHeight);
        }
        if (!mat.normalMap.empty() && mat.normalTexId == 0)
            mat.normalTexId = loadTextureByName(state, mat.normalMap, &mat.normalData, &mat.normalWidth, &mat.normalHeight);
        if (!mat.specularMap.empty() && mat.specularTexId == 0)
            mat.specularTexId = loadTextureByName(state, mat.specularMap, &mat.specularData, &mat.specularWidth, &mat.specularHeight);
        if (!mat.tintMap.empty() && mat.tintTexId == 0)
            mat.tintTexId = loadTextureByName(state, mat.tintMap, &mat.tintData, &mat.tintWidth, &mat.tintHeight);
        if (mat.isTerrain) {
            mat.paletteTexId = mat.diffuseTexId;
            mat.palNormalTexId = mat.normalTexId;
            if (!mat.maskVMap.empty() && mat.maskVTexId == 0)
                mat.maskVTexId = loadTextureByName(state, mat.maskVMap, nullptr, nullptr, nullptr);
            if (!mat.maskAMap.empty() && mat.maskATexId == 0)
                mat.maskATexId = loadTextureByName(state, mat.maskAMap, nullptr, nullptr, nullptr);
            if (!mat.maskA2Map.empty() && mat.maskA2TexId == 0)
                mat.maskA2TexId = loadTextureByName(state, mat.maskA2Map, nullptr, nullptr, nullptr);
            if (!mat.reliefMap.empty() && mat.reliefTexId == 0)
                mat.reliefTexId = loadTextureByName(state, mat.reliefMap, nullptr, nullptr, nullptr);
        }
    }

    return true;
}


void buildErfIndex(AppState& state) {
    s_erfIndex.clear();
    s_erfIndexNoExt.clear();
    s_indexedErfs.clear();
    s_texIdCache.clear();

    auto addToIndex = [&](ERFFile* erf, size_t entryIdx, const std::string& name) {
        std::string key = name;
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        if (s_erfIndex.find(key) == s_erfIndex.end())
            s_erfIndex[key] = {erf, entryIdx};
        std::string noext = key;
        size_t dp = noext.rfind('.');
        if (dp != std::string::npos) noext = noext.substr(0, dp);
        if (s_erfIndexNoExt.find(noext) == s_erfIndexNoExt.end())
            s_erfIndexNoExt[noext] = {erf, entryIdx};
    };

    auto indexErfSet = [&](const std::vector<std::unique_ptr<ERFFile>>& erfs) {
        for (const auto& erf : erfs) {
            const auto& entries = erf->entries();
            for (size_t i = 0; i < entries.size(); i++)
                addToIndex(erf.get(), i, entries[i].name);
        }
    };

    indexErfSet(state.modelErfs);
    indexErfSet(state.materialErfs);
    indexErfSet(state.textureErfs);

    if (state.currentErf) {
        const auto& entries = state.currentErf->entries();
        for (size_t i = 0; i < entries.size(); i++)
            addToIndex(state.currentErf.get(), i, entries[i].name);
    }

    for (const auto& erfPath : state.erfFiles) {
        auto erf = std::make_unique<ERFFile>();
        if (erf->open(erfPath) && erf->encryption() == 0) {
            const auto& entries = erf->entries();
            for (size_t i = 0; i < entries.size(); i++)
                addToIndex(erf.get(), i, entries[i].name);
            s_indexedErfs.push_back(std::move(erf));
        }
    }

    s_erfIndexBuilt = true;
}

static std::vector<uint8_t> readFromErfIndex(const std::string& name, std::string* sourceOut) {
    std::string key = name;
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

    auto tryRead = [&](const ErfIndexEntry& entry) -> std::vector<uint8_t> {
        const auto& entries = entry.erf->entries();
        if (entry.entryIdx < entries.size()) {
            if (sourceOut) {
                namespace fs = std::filesystem;
                *sourceOut = "indexed: " + fs::path(entry.erf->path()).filename().string();
            }
            return entry.erf->readEntry(entries[entry.entryIdx]);
        }
        return {};
    };

    // Exact match (including extension) - always preferred
    auto it = s_erfIndex.find(key);
    if (it != s_erfIndex.end()) return tryRead(it->second);

    // No-ext fallback: only use if the indexed entry has the same extension
    std::string noext = key;
    std::string nameExt;
    size_t dp = noext.rfind('.');
    if (dp != std::string::npos) {
        nameExt = noext.substr(dp);
        noext = noext.substr(0, dp);
    }
    auto it2 = s_erfIndexNoExt.find(noext);
    if (it2 != s_erfIndexNoExt.end()) {
        // Verify extension matches before returning
        const auto& entries = it2->second.erf->entries();
        if (it2->second.entryIdx < entries.size()) {
            std::string entryName = entries[it2->second.entryIdx].name;
            std::transform(entryName.begin(), entryName.end(), entryName.begin(), ::tolower);
            std::string entryExt;
            size_t edp = entryName.rfind('.');
            if (edp != std::string::npos) entryExt = entryName.substr(edp);
            // If we requested a specific extension, only match if extensions agree
            // If no extension was requested (nameExt empty), allow any match
            if (nameExt.empty() || entryExt == nameExt) {
                return tryRead(it2->second);
            }
        }
    }

    return {};
}

static std::vector<uint8_t> readFromAnyErf(AppState& state, const std::string& name, std::string* sourceOut = nullptr) {
    if (s_erfIndexBuilt) return readFromErfIndex(name, sourceOut);

    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

    auto tryErfSet = [&](const std::vector<std::unique_ptr<ERFFile>>& erfs, const std::string& setName) -> std::vector<uint8_t> {
        for (const auto& erf : erfs) {
            for (const auto& entry : erf->entries()) {
                std::string entryLower = entry.name;
                std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
                if (entryLower == nameLower) {
                    if (sourceOut) *sourceOut = setName + ": " + erf->path();
                    return erf->readEntry(entry);
                }
            }
        }
        return {};
    };

    auto result = tryErfSet(state.modelErfs, "modelErfs");
    if (!result.empty()) return result;
    result = tryErfSet(state.materialErfs, "materialErfs");
    if (!result.empty()) return result;
    result = tryErfSet(state.textureErfs, "textureErfs");
    if (!result.empty()) return result;

    if (state.currentErf) {
        for (const auto& entry : state.currentErf->entries()) {
            std::string entryLower = entry.name;
            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
            if (entryLower == nameLower) {
                if (sourceOut) *sourceOut = "currentErf";
                return state.currentErf->readEntry(entry);
            }
        }
    }

    for (const auto& erfPath : state.erfFiles) {
        ERFFile erf;
        if (erf.open(erfPath) && erf.encryption() == 0) {
            for (const auto& entry : erf.entries()) {
                std::string entryLower = entry.name;
                std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
                if (entryLower == nameLower) {
                    if (sourceOut) {
                        namespace fs = std::filesystem;
                        *sourceOut = "global: " + fs::path(erfPath).filename().string();
                    }
                    return erf.readEntry(entry);
                }
            }
        }
    }

    return {};
}

static std::vector<uint8_t> readFromModelMeshDataErfs(AppState& state, const std::string& name) {
    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
    namespace fs = std::filesystem;
    for (const auto& erfPath : state.erfFiles) {
        std::string pathLower = erfPath;
        std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);
        if (pathLower.find("modelmeshdata") == std::string::npos &&
            pathLower.find("meshdata") == std::string::npos) continue;
        ERFFile erf;
        if (!erf.open(erfPath) || erf.encryption() != 0) continue;
        for (const auto& entry : erf.entries()) {
            std::string eLower = entry.name;
            std::transform(eLower.begin(), eLower.end(), eLower.begin(), ::tolower);
            if (eLower == nameLower) {
                auto data = erf.readEntry(entry);
                if (!data.empty()) {
                    std::cout << "[LEVEL] Found '" << name << "' in ModelMeshData: "
                              << fs::path(erfPath).filename().string() << std::endl;
                    return data;
                }
            }
        }
    }
    return {};
}

static std::vector<uint8_t> readFromSiblingRims(AppState& state, const std::string& name) {
    if (state.currentRIMPath.empty()) return {};
    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
    namespace fs = std::filesystem;
    std::string rimDir = fs::path(state.currentRIMPath).parent_path().string();
    try {
        for (const auto& dirEntry : fs::directory_iterator(rimDir)) {
            if (!dirEntry.is_regular_file()) continue;
            std::string dpath = dirEntry.path().string();
            if (dpath == state.currentRIMPath) continue;
            std::string fnameLower = dirEntry.path().filename().string();
            std::transform(fnameLower.begin(), fnameLower.end(), fnameLower.begin(), ::tolower);
            if (fnameLower.size() < 5 || fnameLower.substr(fnameLower.size() - 4) != ".rim") continue;
            ERFFile rim;
            if (!rim.open(dpath)) continue;
            for (const auto& entry : rim.entries()) {
                std::string eLower = entry.name;
                std::transform(eLower.begin(), eLower.end(), eLower.begin(), ::tolower);
                if (eLower == nameLower) {
                    auto data = rim.readEntry(entry);
                    if (!data.empty()) {
                        std::cout << "[LEVEL] Found '" << name << "' in sibling rim: " << dirEntry.path().filename().string() << std::endl;
                        return data;
                    }
                }
            }
        }
    } catch (...) {}
    return {};
}

static std::unordered_map<std::string, Model> s_propModelCache;
static std::set<std::string> s_propMissingModels;

void clearPropCache() {
    s_propModelCache.clear();
    s_propMissingModels.clear();
    s_erfIndex.clear();
    s_erfIndexNoExt.clear();
    s_indexedErfs.clear();
    s_erfIndexBuilt = false;
    s_texIdCache.clear();
}

bool mergeModelByName(AppState& state, const std::string& modelName,
                      float px, float py, float pz,
                      float qx, float qy, float qz, float qw,
                      float scale) {

    std::string nameLower = modelName;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

    if (s_propMissingModels.count(nameLower)) return false;

    auto cacheIt = s_propModelCache.find(nameLower);
    if (cacheIt == s_propModelCache.end()) {
        std::string mshName = modelName + ".msh";
        std::string mshSource;
        std::vector<uint8_t> mshData = readFromAnyErf(state, mshName, &mshSource);
        if (mshData.empty()) {
            mshName = modelName + "_0.msh";
            mshData = readFromAnyErf(state, mshName, &mshSource);
        }

        Model tempModel;
        bool parsed = false;
        if (!mshData.empty()) {
            parsed = loadMSH(mshData, tempModel);
            if (!parsed) {
                std::cout << "[LEVEL] MSH FOUND BUT PARSE FAILED: " << mshName
                          << " (" << mshData.size() << " bytes, header: ";
                for (size_t i = 0; i < std::min(mshData.size(), (size_t)8); i++)
                    printf("%02X ", mshData[i]);
                std::cout << ") from [" << mshSource << "] - trying sibling rims..." << std::endl;
                mshName = modelName + ".msh";
                mshData = readFromSiblingRims(state, mshName);
                if (mshData.empty()) {
                    mshName = modelName + "_0.msh";
                    mshData = readFromSiblingRims(state, mshName);
                }
                if (!mshData.empty()) {
                    parsed = loadMSH(mshData, tempModel);
                    if (!parsed) {
                        std::cout << "[LEVEL] Sibling rim MSH also failed to parse: " << mshName
                                  << " (" << mshData.size() << " bytes)" << std::endl;
                    }
                }
                if (!parsed) {
                    mshName = modelName + ".msh";
                    mshData = readFromModelMeshDataErfs(state, mshName);
                    if (mshData.empty()) {
                        mshName = modelName + "_0.msh";
                        mshData = readFromModelMeshDataErfs(state, mshName);
                    }
                    if (!mshData.empty()) {
                        parsed = loadMSH(mshData, tempModel);
                        if (!parsed) {
                            std::cout << "[LEVEL] ModelMeshData MSH also failed to parse: " << mshName
                                      << " (" << mshData.size() << " bytes, header: ";
                            for (size_t i = 0; i < std::min(mshData.size(), (size_t)8); i++)
                                printf("%02X ", mshData[i]);
                            std::cout << ")" << std::endl;
                        }
                    }
                }
            }
        } else {
            mshName = modelName + ".msh";
            mshData = readFromSiblingRims(state, mshName);
            if (mshData.empty()) {
                mshName = modelName + "_0.msh";
                mshData = readFromSiblingRims(state, mshName);
            }
            if (mshData.empty()) {
                mshName = modelName + ".msh";
                mshData = readFromModelMeshDataErfs(state, mshName);
            }
            if (mshData.empty()) {
                mshName = modelName + "_0.msh";
                mshData = readFromModelMeshDataErfs(state, mshName);
            }
            if (!mshData.empty()) {
                parsed = loadMSH(mshData, tempModel);
                if (!parsed) {
                    std::cout << "[LEVEL] Fallback MSH parse failed: " << mshName
                              << " (" << mshData.size() << " bytes)" << std::endl;
                }
            }
        }

        if (!parsed) {
            if (mshData.empty()) {
                std::cout << "[LEVEL] MSH NOT FOUND: " << modelName
                          << " (searched ERFs + sibling rims + ModelMeshData)" << std::endl;
            }
            s_propMissingModels.insert(nameLower);
            return false;
        }

        std::vector<std::string> mmhCandidates = {modelName + ".mmh", modelName + "a.mmh", modelName + "_0.mmh"};
        bool mmhFound = false;
        for (const auto& candidate : mmhCandidates) {
            std::vector<uint8_t> mmhData = readFromAnyErf(state, candidate);
            if (!mmhData.empty()) {
                loadMMH(mmhData, tempModel);
                mmhFound = true;
                break;
            }
        }
        if (!mmhFound) {
            std::cout << "[LEVEL] WARNING: no MMH found for prop: " << modelName << std::endl;
        }

        applyMeshLocalTransforms(tempModel);

        s_propModelCache[nameLower] = tempModel;
        cacheIt = s_propModelCache.find(nameLower);
    }

    Model instance = cacheIt->second;
    transformModelVertices(instance, px, py, pz, qx, qy, qz, qw, scale);

    for (auto& mesh : instance.meshes) {
        state.currentModel.meshes.push_back(std::move(mesh));
    }

    return true;
}

void finalizeLevelMaterials(AppState& state) {
    std::set<std::string> newMaterials;
    for (const auto& mesh : state.currentModel.meshes) {
        if (!mesh.materialName.empty() && state.currentModel.findMaterial(mesh.materialName) < 0) {
            newMaterials.insert(mesh.materialName);
        }
    }

    for (const std::string& matName : newMaterials) {
        std::string maoLookup = matName;
        {
            std::string lower = matName;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.size() < 4 || lower.substr(lower.size() - 4) != ".mao")
                maoLookup += ".mao";
        }
        std::vector<uint8_t> maoData = readFromMaterialErfs(state, maoLookup);
        if (maoData.empty()) maoData = readFromAnyErf(state, maoLookup);
        if (!maoData.empty()) {
            std::string maoContent(maoData.begin(), maoData.end());
            Material mat = parseMAO(maoContent, matName);
            mat.maoContent = maoContent;
            state.currentModel.materials.push_back(mat);
        } else {
            std::cout << "[LEVEL] MISSING MAO for material: " << matName << " (looked up: " << maoLookup << ")" << std::endl;
            Material mat;
            mat.name = matName;
            state.currentModel.materials.push_back(mat);
        }
    }

    for (auto& mesh : state.currentModel.meshes) {
        if (!mesh.materialName.empty())
            mesh.materialIndex = state.currentModel.findMaterial(mesh.materialName);
    }

    for (auto& mat : state.currentModel.materials) {
        if (mat.isWater) {
            std::cout << "[WATER] Material '" << mat.name << "'" << std::endl;
            std::cout << "[WATER]   normalMap='" << mat.normalMap << "' waterNormalMap='" << mat.waterNormalMap << "'" << std::endl;
            std::cout << "[WATER]   diffuseMap='" << mat.diffuseMap << "' waterDecalMap='" << mat.waterDecalMap << "'" << std::endl;
            std::cout << "[WATER]   specularMap='" << mat.specularMap << "' waterMaskMap='" << mat.waterMaskMap << "'" << std::endl;
            if (mat.normalMap.empty() && !mat.waterNormalMap.empty())
                mat.normalMap = mat.waterNormalMap;
            if (mat.diffuseMap.empty() && !mat.waterDecalMap.empty())
                mat.diffuseMap = mat.waterDecalMap;
            if (mat.specularMap.empty() && !mat.waterMaskMap.empty())
                mat.specularMap = mat.waterMaskMap;
            if (g_terrainLoader.isLoaded()) {
                const auto& tw = g_terrainLoader.getTerrain().water;
                float rSum = 0, gSum = 0, bSum = 0, aSum = 0;
                int count = 0;
                for (const auto& wm : tw) {
                    for (const auto& v : wm.vertices) {
                        rSum += v.r; gSum += v.g; bSum += v.b; aSum += v.a;
                        count++;
                    }
                }
                if (count > 0) {
                    mat.waterBodyColor[0] = rSum / count;
                    mat.waterBodyColor[1] = gSum / count;
                    mat.waterBodyColor[2] = bSum / count;
                    mat.waterBodyColor[3] = aSum / count;
                    std::cout << "[WATER]   bodyColor from terrain: ("
                              << mat.waterBodyColor[0] << ", " << mat.waterBodyColor[1] << ", "
                              << mat.waterBodyColor[2] << ", " << mat.waterBodyColor[3] << ") from "
                              << count << " vertices" << std::endl;
                }
            }
            std::cout << "[WATER]   AFTER: normalMap='" << mat.normalMap << "' diffuseMap='" << mat.diffuseMap << "'" << std::endl;
            if (mat.envCubemapTexId == 0) {
                mat.envCubemapTexId = loadCubemapByName(state, "Default_BlackCubemap");
            }
            if (mat.envCubemapTexId == 0) {
                mat.envCubemapTexId = findAnyCubemapInErfs(state);
            }
            if (mat.envCubemapTexId != 0)
                std::cout << "[WATER]   cubemap texId=" << mat.envCubemapTexId << std::endl;
            else
                std::cout << "[WATER]   no cubemap found, using procedural sky reflection" << std::endl;
        }
        if (!mat.diffuseMap.empty() && mat.diffuseTexId == 0)
            mat.diffuseTexId = loadTextureByName(state, mat.diffuseMap, nullptr, nullptr, nullptr);
        if (!mat.normalMap.empty() && mat.normalTexId == 0)
            mat.normalTexId = loadTextureByName(state, mat.normalMap, nullptr, nullptr, nullptr);
        if (!mat.specularMap.empty() && mat.specularTexId == 0)
            mat.specularTexId = loadTextureByName(state, mat.specularMap, nullptr, nullptr, nullptr);
        if (!mat.tintMap.empty() && mat.tintTexId == 0)
            mat.tintTexId = loadTextureByName(state, mat.tintMap, nullptr, nullptr, nullptr);

        if (!mat.diffuseMap.empty() && mat.diffuseTexId == 0)
            std::cout << "[LEVEL] MISSING diffuse texture: " << mat.diffuseMap << " (mat=" << mat.name << ")" << std::endl;
        if (!mat.normalMap.empty() && mat.normalTexId == 0)
            std::cout << "[LEVEL] MISSING normal texture: " << mat.normalMap << " (mat=" << mat.name << ")" << std::endl;
        if (!mat.specularMap.empty() && mat.specularTexId == 0)
            std::cout << "[LEVEL] MISSING specular texture: " << mat.specularMap << " (mat=" << mat.name << ")" << std::endl;
        if (!mat.tintMap.empty() && mat.tintTexId == 0)
            std::cout << "[LEVEL] MISSING tint texture: " << mat.tintMap << " (mat=" << mat.name << ")" << std::endl;
        if (mat.isTerrain) {
            mat.paletteTexId = mat.diffuseTexId;
            mat.palNormalTexId = mat.normalTexId;
            if (!mat.maskVMap.empty() && mat.maskVTexId == 0)
                mat.maskVTexId = loadTextureByName(state, mat.maskVMap, nullptr, nullptr, nullptr);
            if (!mat.maskAMap.empty() && mat.maskATexId == 0)
                mat.maskATexId = loadTextureByName(state, mat.maskAMap, nullptr, nullptr, nullptr);
            if (!mat.maskA2Map.empty() && mat.maskA2TexId == 0)
                mat.maskA2TexId = loadTextureByName(state, mat.maskA2Map, nullptr, nullptr, nullptr);
            if (!mat.reliefMap.empty() && mat.reliefTexId == 0)
                mat.reliefTexId = loadTextureByName(state, mat.reliefMap, nullptr, nullptr, nullptr);
        }
    }

    state.renderSettings.initMeshVisibility(state.currentModel.meshes.size());
}
void finalizeModelMaterials(AppState& state, Model& model) {
    loadTextureErfs(state);
    loadModelErfs(state);
    loadMaterialErfs(state);
    std::set<std::string> newMaterials;
    for (const auto& mesh : model.meshes) {
        if (!mesh.materialName.empty() && model.findMaterial(mesh.materialName) < 0)
            newMaterials.insert(mesh.materialName);
    }
    for (const std::string& matName : newMaterials) {
        std::string maoLookup = matName;
        {
            std::string lower = matName;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.size() < 4 || lower.substr(lower.size() - 4) != ".mao")
                maoLookup += ".mao";
        }
        std::vector<uint8_t> maoData = readFromMaterialErfs(state, maoLookup);
        if (maoData.empty()) maoData = readFromAnyErf(state, maoLookup);
        if (!maoData.empty()) {
            std::string maoContent(maoData.begin(), maoData.end());
            Material mat = parseMAO(maoContent, matName);
            mat.maoContent = maoContent;
            if (mat.diffuseMap.empty()) {
                if (maoData.size() >= 8 && GFF32::GFF32File::isGFF32(maoData)) {
                    GFF32::GFF32File gff32;
                    if (gff32.load(maoData) && gff32.root()) {
                        if (gff32.root()->fieldOrder.empty()) {
                            std::cout << "[MAO] WARNING: '" << matName << "' is valid GFF32 but has NO fields" << std::endl;
                        } else {
                            std::cout << "[MAO] WARNING: No diffuse map for '" << matName << "'. Fields:" << std::endl;
                            for (const auto& fn : gff32.root()->fieldOrder) {
                                auto it = gff32.root()->fields.find(fn);
                                if (it == gff32.root()->fields.end()) continue;
                                const auto& field = it->second;
                                if (field.typeId == GFF32::TypeID::ExoString || field.typeId == GFF32::TypeID::ResRef) {
                                    const std::string* v = std::get_if<std::string>(&field.value);
                                    if (v) std::cout << "[MAO]   STRING '" << fn << "' = '" << *v << "'" << std::endl;
                                } else if (field.typeId == GFF32::TypeID::FLOAT) {
                                    const float* v = std::get_if<float>(&field.value);
                                    if (v) std::cout << "[MAO]   FLOAT  '" << fn << "' = " << *v << std::endl;
                                } else if (field.typeId == GFF32::TypeID::Structure) {
                                    std::cout << "[MAO]   STRUCT '" << fn << "'" << std::endl;
                                } else {
                                    std::cout << "[MAO]   TYPE(" << (int)field.typeId << ") '" << fn << "'" << std::endl;
                                }
                            }
                        }
                    } else {
                        std::cout << "[MAO] WARNING: '" << matName << "' GFF32 load failed" << std::endl;
                    }
                } else {
                    std::cout << "[MAO] WARNING: '" << matName << "' not GFF32 format (" << maoData.size() << " bytes, header: ";
                    for (size_t i = 0; i < std::min(maoData.size(), (size_t)16); i++)
                        printf("%02X ", maoData[i]);
                    std::cout << ")" << std::endl;
                }
            }
            model.materials.push_back(mat);
        } else {
            std::cout << "[MAO] NOT FOUND: '" << maoLookup << "' for material '" << matName << "'" << std::endl;
            Material mat;
            mat.name = matName;
            model.materials.push_back(mat);
        }
    }
    for (auto& mesh : model.meshes) {
        if (!mesh.materialName.empty())
            mesh.materialIndex = model.findMaterial(mesh.materialName);
    }
    for (auto& mat : model.materials) {
        if (mat.isWater) {
            if (mat.normalMap.empty() && !mat.waterNormalMap.empty())
                mat.normalMap = mat.waterNormalMap;
            if (mat.diffuseMap.empty() && !mat.waterDecalMap.empty())
                mat.diffuseMap = mat.waterDecalMap;
            if (mat.specularMap.empty() && !mat.waterMaskMap.empty())
                mat.specularMap = mat.waterMaskMap;
        }
        if (!mat.diffuseMap.empty() && mat.diffuseTexId == 0)
            mat.diffuseTexId = loadTextureByName(state, mat.diffuseMap, &mat.diffuseData, &mat.diffuseWidth, &mat.diffuseHeight);
        if (!mat.normalMap.empty() && mat.normalTexId == 0)
            mat.normalTexId = loadTextureByName(state, mat.normalMap, &mat.normalData, &mat.normalWidth, &mat.normalHeight);
        if (!mat.specularMap.empty() && mat.specularTexId == 0)
            mat.specularTexId = loadTextureByName(state, mat.specularMap, &mat.specularData, &mat.specularWidth, &mat.specularHeight);
        if (!mat.tintMap.empty() && mat.tintTexId == 0)
            mat.tintTexId = loadTextureByName(state, mat.tintMap, &mat.tintData, &mat.tintWidth, &mat.tintHeight);
    }
}