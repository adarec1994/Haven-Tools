#include "mmh_loader.h"
#include "model_loader.h"
#include "dds_loader.h"
#include "rml_loader.h"
#include "animation.h"
#include "Gff.h"
#include "erf.h"
#include "Shaders/d3d_context.h"
#include "renderer.h"
#include <algorithm>
#include <functional>
#include <cmath>

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
    for (const auto& erfPath : state.erfFiles) {
        std::string filename = fs::path(erfPath).filename().string();
        std::string filenameLower = filename;
        std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);
        if (filenameLower.find("texture") != std::string::npos) {
            auto erf = std::make_unique<ERFFile>();
            if (erf->open(erfPath) && erf->encryption() == 0) {
                state.textureErfs.push_back(std::move(erf));
            }
        }
    }
    state.textureErfsLoaded = true;
}

static void loadModelErfs(AppState& state) {
    if (state.modelErfsLoaded) return;
    state.modelErfs.clear();
    for (const auto& erfPath : state.erfFiles) {
        std::string filename = fs::path(erfPath).filename().string();
        std::string filenameLower = filename;
        std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);
        if (filenameLower.find("modelhierarch") != std::string::npos) {
            auto erf = std::make_unique<ERFFile>();
            if (erf->open(erfPath) && erf->encryption() == 0) {
                state.modelErfs.push_back(std::move(erf));
            }
        }
    }
    state.modelErfsLoaded = true;
}

static void loadMaterialErfs(AppState& state) {
    if (state.materialErfsLoaded) return;
    state.materialErfs.clear();
    for (const auto& erfPath : state.erfFiles) {
        std::string filename = fs::path(erfPath).filename().string();
        std::string filenameLower = filename;
        std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);
        if (filenameLower.find("materialobject") != std::string::npos) {
            auto erf = std::make_unique<ERFFile>();
            if (erf->open(erfPath) && erf->encryption() == 0) {
                state.materialErfs.push_back(std::move(erf));
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
    return {};
}

static std::vector<uint8_t> readFromMaterialErfs(AppState& state, const std::string& name) {
    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
    std::string noExtLower = nameLower;
    size_t dp = noExtLower.rfind('.');
    if (dp != std::string::npos) noExtLower = noExtLower.substr(0, dp);

    const std::vector<std::unique_ptr<ERFFile>>* erfSets[] = {
        &state.materialErfs, &state.modelErfs, &state.textureErfs
    };
    for (const auto* erfs : erfSets) {
        for (const auto& erf : *erfs) {
            for (const auto& entry : erf->entries()) {
                std::string entryLower = entry.name;
                std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
                std::string entryNoExt = entryLower;
                size_t edp = entryNoExt.rfind('.');
                if (edp != std::string::npos) entryNoExt = entryNoExt.substr(0, edp);
                if (entryLower == nameLower || entryNoExt == noExtLower) return erf->readEntry(entry);
            }
        }
    }

    if (state.currentErf) {
        for (const auto& entry : state.currentErf->entries()) {
            std::string entryLower = entry.name;
            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
            std::string entryNoExt = entryLower;
            size_t edp = entryNoExt.rfind('.');
            if (edp != std::string::npos) entryNoExt = entryNoExt.substr(0, edp);
            if (entryLower == nameLower || entryNoExt == noExtLower) return state.currentErf->readEntry(entry);
        }
    }

    for (const auto& erfPath : state.erfFiles) {
        ERFFile erf;
        if (erf.open(erfPath) && erf.encryption() == 0) {
            for (const auto& entry : erf.entries()) {
                std::string entryLower = entry.name;
                std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
                std::string entryNoExt = entryLower;
                size_t edp = entryNoExt.rfind('.');
                if (edp != std::string::npos) entryNoExt = entryNoExt.substr(0, edp);
                if (entryLower == nameLower || entryNoExt == noExtLower) {
                    return erf.readEntry(entry);
                }
            }
        }
    }
    return {};
}

static uint32_t loadTextureByName(AppState& state, const std::string& texName,
                                  std::vector<uint8_t>* rgbaOut = nullptr,
                                  int* wOut = nullptr, int* hOut = nullptr) {
    if (texName.empty()) return 0;
    std::string texNameLower = texName;
    std::transform(texNameLower.begin(), texNameLower.end(), texNameLower.begin(), ::tolower);

    std::string withDdsLower = texNameLower;
    if (withDdsLower.size() < 4 || withDdsLower.substr(withDdsLower.size() - 4) != ".dds")
        withDdsLower += ".dds";
    std::string noExtLower = texNameLower;
    size_t dp = noExtLower.rfind('.');
    if (dp != std::string::npos) noExtLower = noExtLower.substr(0, dp);

    auto tryLoadFromErf = [&](ERFFile& erf) -> uint32_t {
        for (const auto& entry : erf.entries()) {
            std::string entryLower = entry.name;
            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
            std::string entryNoExt = entryLower;
            size_t edp = entryNoExt.rfind('.');
            if (edp != std::string::npos) entryNoExt = entryNoExt.substr(0, edp);
            if (entryLower == texNameLower || entryLower == withDdsLower || entryNoExt == noExtLower) {
                std::vector<uint8_t> texData = erf.readEntry(entry);
                if (!texData.empty()) {
                    if (rgbaOut && wOut && hOut) decodeDDSToRGBA(texData, *rgbaOut, *wOut, *hOut);
                    return createTextureFromDDS(texData);
                }
            }
        }
        return 0;
    };

    const std::vector<std::unique_ptr<ERFFile>>* erfSets[] = {
        &state.textureErfs, &state.materialErfs, &state.modelErfs
    };
    for (const auto* erfs : erfSets) {
        for (const auto& erf : *erfs) {
            uint32_t id = tryLoadFromErf(*erf);
            if (id != 0) return id;
        }
    }
    if (state.currentErf) {
        uint32_t id = tryLoadFromErf(*state.currentErf);
        if (id != 0) return id;
    }

    for (const auto& erfPath : state.erfFiles) {
        ERFFile erf;
        if (erf.open(erfPath) && erf.encryption() == 0) {
            uint32_t id = tryLoadFromErf(erf);
            if (id != 0) return id;
        }
    }

    bool found = false;
    for (const auto& erfPath : state.erfFiles) {
        ERFFile erf;
        if (erf.open(erfPath) && erf.encryption() == 0) {
            for (const auto& entry : erf.entries()) {
                std::string entryLower = entry.name;
                std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
                if (entryLower.find(noExtLower) != std::string::npos) {
                    std::string erfName = erfPath;
                    size_t slash = erfName.find_last_of("/\\");
                    if (slash != std::string::npos) erfName = erfName.substr(slash + 1);
                    found = true;
                }
            }
        }
    }
    const char* setNames[] = {"textureErfs", "materialErfs", "modelErfs"};
    const std::vector<std::unique_ptr<ERFFile>>* erfSets2[] = {
        &state.textureErfs, &state.materialErfs, &state.modelErfs
    };
    for (int s = 0; s < 3; s++) {
        for (size_t ei = 0; ei < erfSets2[s]->size(); ei++) {
            for (const auto& entry : (*erfSets2[s])[ei]->entries()) {
                std::string entryLower = entry.name;
                std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
                if (entryLower.find(noExtLower) != std::string::npos) {
                    found = true;
                }
            }
        }
    }
    if (state.currentErf) {
        for (const auto& entry : state.currentErf->entries()) {
            std::string entryLower = entry.name;
            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
            if (entryLower.find(noExtLower) != std::string::npos) {
                found = true;
            }
        }
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
        std::vector<uint8_t> mmhData = readFromModelErfs(state, candidate);
        if (!mmhData.empty()) {
            loadMMH(mmhData, state.currentModel);
            mmhFound = true;
            break;
        }
    }
    if (!mmhFound) {
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
        if (!phyData.empty()) { loadPHY(phyData, state.currentModel); break; }
    }

    std::set<std::string> materialNames;
    for (const auto& mesh : state.currentModel.meshes) {
        if (!mesh.materialName.empty()) materialNames.insert(mesh.materialName);
    }
    for (const std::string& matName : materialNames) {

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

bool mergeModelEntry(AppState& state, const ERFEntry& entry) {
    if (!state.currentErf) return false;
    std::vector<uint8_t> data = state.currentErf->readEntry(entry);
    if (data.empty()) {
        return false;
    }
    Model tempModel;
    if (!loadMSH(data, tempModel)) {
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

struct ErfIndexEntry {
    ERFFile* erf;
    size_t entryIdx;
};

static std::unordered_map<std::string, ErfIndexEntry> s_erfIndex;
static bool s_erfIndexBuilt = false;
static std::vector<std::unique_ptr<ERFFile>> s_indexedErfs;

void buildErfIndex(AppState& state) {
    s_erfIndex.clear();
    s_indexedErfs.clear();

    auto indexErfSet = [&](const std::vector<std::unique_ptr<ERFFile>>& erfs) {
        for (const auto& erf : erfs) {
            const auto& entries = erf->entries();
            for (size_t i = 0; i < entries.size(); i++) {
                std::string key = entries[i].name;
                std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                if (s_erfIndex.find(key) == s_erfIndex.end())
                    s_erfIndex[key] = {erf.get(), i};
            }
        }
    };

    indexErfSet(state.modelErfs);
    indexErfSet(state.materialErfs);
    indexErfSet(state.textureErfs);

    if (state.currentErf) {
        const auto& entries = state.currentErf->entries();
        for (size_t i = 0; i < entries.size(); i++) {
            std::string key = entries[i].name;
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            if (s_erfIndex.find(key) == s_erfIndex.end())
                s_erfIndex[key] = {state.currentErf.get(), i};
        }
    }

    for (const auto& erfPath : state.erfFiles) {
        auto erf = std::make_unique<ERFFile>();
        if (erf->open(erfPath) && erf->encryption() == 0) {
            const auto& entries = erf->entries();
            for (size_t i = 0; i < entries.size(); i++) {
                std::string key = entries[i].name;
                std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                if (s_erfIndex.find(key) == s_erfIndex.end())
                    s_erfIndex[key] = {erf.get(), i};
            }
            s_indexedErfs.push_back(std::move(erf));
        }
    }

    s_erfIndexBuilt = true;
}

static std::vector<uint8_t> readFromErfIndex(const std::string& name) {
    std::string key = name;
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    auto it = s_erfIndex.find(key);
    if (it != s_erfIndex.end()) {
        const auto& entries = it->second.erf->entries();
        if (it->second.entryIdx < entries.size())
            return it->second.erf->readEntry(entries[it->second.entryIdx]);
    }

    std::string noExt = key;
    size_t dp = noExt.rfind('.');
    std::string ext = (dp != std::string::npos) ? key.substr(dp) : "";
    if (dp != std::string::npos) noExt = key.substr(0, dp);

    if (!ext.empty()) {
        for (const auto& [k, v] : s_erfIndex) {
            std::string kNoExt = k;
            size_t kdp = kNoExt.rfind('.');
            if (kdp != std::string::npos) kNoExt = k.substr(0, kdp);
            if (kNoExt == noExt) {
                const auto& entries = v.erf->entries();
                if (v.entryIdx < entries.size())
                    return v.erf->readEntry(entries[v.entryIdx]);
            }
        }
    }
    return {};
}

static std::vector<uint8_t> readFromAnyErf(AppState& state, const std::string& name) {
    if (s_erfIndexBuilt) return readFromErfIndex(name);

    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

    auto tryErfSet = [&](const std::vector<std::unique_ptr<ERFFile>>& erfs) -> std::vector<uint8_t> {
        for (const auto& erf : erfs) {
            for (const auto& entry : erf->entries()) {
                std::string entryLower = entry.name;
                std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
                if (entryLower == nameLower) return erf->readEntry(entry);
            }
        }
        return {};
    };

    auto result = tryErfSet(state.modelErfs);
    if (!result.empty()) return result;
    result = tryErfSet(state.materialErfs);
    if (!result.empty()) return result;
    result = tryErfSet(state.textureErfs);
    if (!result.empty()) return result;

    if (state.currentErf) {
        for (const auto& entry : state.currentErf->entries()) {
            std::string entryLower = entry.name;
            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
            if (entryLower == nameLower) return state.currentErf->readEntry(entry);
        }
    }

    return {};
}

static std::unordered_map<std::string, Model> s_propModelCache;
static std::set<std::string> s_propMissingModels;

void clearPropCache() {
    s_propModelCache.clear();
    s_propMissingModels.clear();
    s_erfIndex.clear();
    s_indexedErfs.clear();
    s_erfIndexBuilt = false;
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
        std::vector<uint8_t> mshData = readFromAnyErf(state, mshName);
        if (mshData.empty()) {
            mshName = modelName + "_0.msh";
            mshData = readFromAnyErf(state, mshName);
        }
        if (mshData.empty()) {
            s_propMissingModels.insert(nameLower);
            return false;
        }

        Model tempModel;
        if (!loadMSH(mshData, tempModel)) {
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
        if (!mat.diffuseMap.empty() && mat.diffuseTexId == 0)
            mat.diffuseTexId = loadTextureByName(state, mat.diffuseMap, &mat.diffuseData, &mat.diffuseWidth, &mat.diffuseHeight);
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

    state.renderSettings.initMeshVisibility(state.currentModel.meshes.size());
}