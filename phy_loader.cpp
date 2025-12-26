#include "mmh_loader.h"
#include "model_loader.h"
#include "dds_loader.h"
#include "animation.h"
#include "Gff.h"
#include "erf.h"
#include <iostream>
#include <algorithm>
#include <functional>
#include <cmath>
#include <set>
#include <filesystem>

namespace fs = std::filesystem;

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#else
#include <GL/gl.h>
#endif

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

            int boneIdx = model.skeleton.findBone(currentBoneName);
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

static std::vector<uint8_t> readFromModelErfs(AppState& state, const std::string& name) {
    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

    for (const auto& erf : state.modelErfs) {
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

static std::vector<uint8_t> readFromMaterialErfs(AppState& state, const std::string& name) {
    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

    for (const auto& erf : state.materialErfs) {
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

static uint32_t loadTextureByName(AppState& state, const std::string& texName) {
    if (texName.empty()) return 0;

    std::string texNameLower = texName;
    std::transform(texNameLower.begin(), texNameLower.end(), texNameLower.begin(), ::tolower);

    for (const auto& erf : state.textureErfs) {
        for (const auto& entry : erf->entries()) {
            std::string entryLower = entry.name;
            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
            if (entryLower == texNameLower) {
                std::vector<uint8_t> texData = erf->readEntry(entry);
                if (!texData.empty()) return loadDDSTexture(texData);
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
        if (mat.diffuseTexId != 0) glDeleteTextures(1, &mat.diffuseTexId);
        if (mat.normalTexId != 0) glDeleteTextures(1, &mat.normalTexId);
        if (mat.specularTexId != 0) glDeleteTextures(1, &mat.specularTexId);
    }

    state.currentModel = model;
    state.currentModel.name = entry.name;
    state.hasModel = true;
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

    for (const auto& candidate : mmhCandidates) {
        std::vector<uint8_t> mmhData = readFromModelErfs(state, candidate);
        if (!mmhData.empty()) {
            loadMMH(mmhData, state.currentModel);
            break;
        }
    }

    std::vector<std::string> phyCandidates = {baseName + ".phy", baseName + "a.phy"};
    if (lastUnderscore != std::string::npos) {
        std::string variantA = baseName;
        variantA.insert(lastUnderscore, "a");
        phyCandidates.push_back(variantA + ".phy");
    }
    for (const auto& candidate : phyCandidates) {
        std::vector<uint8_t> phyData = readFromModelErfs(state, candidate);
        if (!phyData.empty()) {
            loadPHY(phyData, state.currentModel);
            break;
        }
    }

    std::set<std::string> materialNames;
    for (const auto& mesh : state.currentModel.meshes) {
        if (!mesh.materialName.empty()) materialNames.insert(mesh.materialName);
    }

    for (const std::string& matName : materialNames) {
        std::vector<uint8_t> maoData = readFromMaterialErfs(state, matName + ".mao");
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
        if (!mesh.materialName.empty()) {
            mesh.materialIndex = state.currentModel.findMaterial(mesh.materialName);
        }
    }

    for (auto& mat : state.currentModel.materials) {
        if (!mat.diffuseMap.empty() && mat.diffuseTexId == 0) mat.diffuseTexId = loadTextureByName(state, mat.diffuseMap);
        if (!mat.normalMap.empty() && mat.normalTexId == 0) mat.normalTexId = loadTextureByName(state, mat.normalMap);
        if (!mat.specularMap.empty() && mat.specularTexId == 0) mat.specularTexId = loadTextureByName(state, mat.specularMap);
        if (!mat.tintMap.empty() && mat.tintTexId == 0) mat.tintTexId = loadTextureByName(state, mat.tintMap);
    }

    // Center camera
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