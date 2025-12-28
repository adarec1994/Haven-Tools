#include "mmh_loader.h"
#include "model_loader.h"
#include "dds_loader.h"
#include "animation.h"
#include "Gff.h"
#include "erf.h"
#include <algorithm>
#include <map>
#include <set>
#include <functional>
#include <cmath>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#else
#include <GL/gl.h>
#endif

Material parseMAO(const std::string& maoContent, const std::string& materialName) {
    Material mat;
    mat.name = materialName;

    size_t pos = 0;
    while ((pos = maoContent.find("<Texture", pos)) != std::string::npos) {
        size_t endTag = maoContent.find("/>", pos);
        if (endTag == std::string::npos) {
            endTag = maoContent.find("</Texture>", pos);
            if (endTag == std::string::npos) break;
        }

        std::string tag = maoContent.substr(pos, endTag - pos);
        std::string texName;
        size_t namePos = tag.find("Name=\"");
        if (namePos != std::string::npos) {
            namePos += 6;
            size_t nameEnd = tag.find("\"", namePos);
            if (nameEnd != std::string::npos) {
                texName = tag.substr(namePos, nameEnd - namePos);
            }
        }

        std::string resName;
        size_t resPos = tag.find("ResName=\"");
        if (resPos != std::string::npos) {
            resPos += 9;
            size_t resEnd = tag.find("\"", resPos);
            if (resEnd != std::string::npos) {
                resName = tag.substr(resPos, resEnd - resPos);
            }
        }

        if (!texName.empty() && !resName.empty()) {
            std::string texNameLower = texName;
            std::transform(texNameLower.begin(), texNameLower.end(), texNameLower.begin(), ::tolower);

            std::string resNameLower = resName;
            std::transform(resNameLower.begin(), resNameLower.end(), resNameLower.begin(), ::tolower);

            if (texNameLower.find("agediffuse") != std::string::npos ||
                texNameLower.find("age_diffuse") != std::string::npos) {
                mat.ageDiffuseMap = resName;
            } else if (texNameLower.find("agenormal") != std::string::npos ||
                       texNameLower.find("age_normal") != std::string::npos) {
                mat.ageNormalMap = resName;
            } else if (texNameLower.find("tattoo") != std::string::npos) {
                mat.tattooMap = resName;
            } else if (texNameLower.find("browstubble") != std::string::npos &&
                       texNameLower.find("normal") == std::string::npos) {
                mat.browStubbleMap = resName;
            }

            else if (texNameLower.find("diffuse") != std::string::npos ||
                texNameLower.find("packedtexture") != std::string::npos ||
                texNameLower.find("_d") != std::string::npos) {
                mat.diffuseMap = resName;
            } else if (texNameLower.find("normal") != std::string::npos ||
                       texNameLower.find("_n") != std::string::npos) {
                mat.normalMap = resName;
            } else if (texNameLower.find("specular") != std::string::npos ||
                       texNameLower.find("_s") != std::string::npos) {
                mat.specularMap = resName;
            } else if (texNameLower.find("tintmask") != std::string::npos) {

            } else if (texNameLower.find("tint") != std::string::npos) {
                mat.tintMap = resName;
            } else if (mat.diffuseMap.empty()) {
                if (resNameLower.find("_d.") != std::string::npos ||
                    resNameLower.find("0d.") != std::string::npos ||
                    resNameLower.find("_d_") != std::string::npos) {
                    mat.diffuseMap = resName;
                } else if (resNameLower.find("_n.") != std::string::npos ||
                           resNameLower.find("0n.") != std::string::npos) {
                    mat.normalMap = resName;
                } else if (resNameLower.find("_s.") != std::string::npos ||
                           resNameLower.find("0s.") != std::string::npos) {
                    mat.specularMap = resName;
                } else if (resNameLower.find("_t.") != std::string::npos ||
                           resNameLower.find("0t.") != std::string::npos) {
                    mat.tintMap = resName;
                }
            }
        }
        pos = endTag + 2;
    }

    return mat;
}

static void quatMulWorld(float ax, float ay, float az, float aw,
                         float bx, float by, float bz, float bw,
                         float& rx, float& ry, float& rz, float& rw) {
    rw = aw*bw - ax*bx - ay*by - az*bz;
    rx = aw*bx + ax*bw + ay*bz - az*by;
    ry = aw*by - ax*bz + ay*bw + az*bx;
    rz = aw*bz + ax*by - ay*bx + az*bw;
}

static void quatRotateWorld(float qx, float qy, float qz, float qw,
                            float px, float py, float pz,
                            float& rx, float& ry, float& rz) {
    float tx = 2.0f * (qy * pz - qz * py);
    float ty = 2.0f * (qz * px - qx * pz);
    float tz = 2.0f * (qx * py - qy * px);
    rx = px + qw * tx + (qy * tz - qz * ty);
    ry = py + qw * ty + (qz * tx - qx * tz);
    rz = pz + qw * tz + (qx * ty - qy * tx);
}

void loadMMH(const std::vector<uint8_t>& data, Model& model) {
    GFFFile gff;
    if (!gff.load(data)) return;

    auto normalizeQuat = [](float& x, float& y, float& z, float& w) {
        float len = std::sqrt(x*x + y*y + z*z + w*w);
        if (len > 0.00001f) { x /= len; y /= len; z /= len; w /= len; }
        else { x = 0; y = 0; z = 0; w = 1; }
    };

    auto readUInt32List = [&gff](uint32_t structIdx, uint32_t label, uint32_t baseOffset) -> std::vector<uint32_t> {
        std::vector<uint32_t> result;
        const GFFField* field = gff.findField(structIdx, label);
        if (!field) return result;

        bool isList = (field->flags & 0x8000) != 0;
        bool isStruct = (field->flags & 0x4000) != 0;
        bool isRef = (field->flags & 0x2000) != 0;

        if (!isList || isStruct || isRef) return result;
        if (field->typeId != 4) return result;

        uint32_t dataPos = gff.dataOffset() + field->dataOffset + baseOffset;
        int32_t ref = gff.readInt32At(dataPos);
        if (ref < 0) return result;

        uint32_t listPos = gff.dataOffset() + ref;
        uint32_t listCount = gff.readUInt32At(listPos);
        listPos += 4;

        for (uint32_t i = 0; i < listCount; i++) {
            result.push_back(gff.readUInt32At(listPos));
            listPos += 4;
        }
        return result;
    };

    std::map<std::string, std::string> meshMaterials;
    std::map<std::string, std::vector<int>> meshBonesUsed;
    std::vector<Bone> tempBones;
    std::map<int, std::string> boneIndexMap;

    std::function<void(size_t, uint32_t, const std::string&)> findNodes = [&](size_t structIdx, uint32_t offset, const std::string& parentName) {
        if (structIdx >= gff.structs().size()) return;
        const auto& s = gff.structs()[structIdx];
        std::string structType(s.structType);

        if (structType == "mshh") {
            std::string meshName = gff.readStringByLabel(structIdx, 6006, offset);
            std::string materialName = gff.readStringByLabel(structIdx, 6001, offset);
            if (!meshName.empty() && !materialName.empty()) meshMaterials[meshName] = materialName;

            std::vector<uint32_t> bonesUsedRaw = readUInt32List(structIdx, 6255, offset);
            if (!bonesUsedRaw.empty()) {
                std::vector<int> boneIndices;
                for (uint32_t idx : bonesUsedRaw) boneIndices.push_back(static_cast<int>(idx));
                meshBonesUsed[meshName] = boneIndices;
            }

            std::vector<GFFStructRef> children = gff.readStructList(structIdx, 6999, offset);
            for (const auto& child : children) findNodes(child.structIndex, child.offset, parentName);
            return;
        }

        if (structType == "node") {
            Bone bone;
            bone.name = gff.readStringByLabel(structIdx, 6000, offset);
            bone.parentName = parentName;

            const GFFField* indexField = gff.findField(structIdx, 6254);
            if (indexField) {
                int32_t boneIndex = gff.readInt32At(gff.dataOffset() + indexField->dataOffset + offset);
                if (boneIndex >= 0) boneIndexMap[boneIndex] = bone.name;
            }

            std::vector<GFFStructRef> children = gff.readStructList(structIdx, 6999, offset);
            for (const auto& child : children) {
                const GFFField* posField = gff.findField(child.structIndex, 6047);
                if (posField) {
                    uint32_t posOffset = gff.dataOffset() + posField->dataOffset + child.offset;
                    bone.posX = gff.readFloatAt(posOffset);
                    bone.posY = gff.readFloatAt(posOffset + 4);
                    bone.posZ = gff.readFloatAt(posOffset + 8);
                }
                const GFFField* rotField = gff.findField(child.structIndex, 6048);
                if (rotField) {
                    uint32_t rotOffset = gff.dataOffset() + rotField->dataOffset + child.offset;
                    bone.rotX = gff.readFloatAt(rotOffset);
                    bone.rotY = gff.readFloatAt(rotOffset + 4);
                    bone.rotZ = gff.readFloatAt(rotOffset + 8);
                    bone.rotW = gff.readFloatAt(rotOffset + 12);
                    normalizeQuat(bone.rotX, bone.rotY, bone.rotZ, bone.rotW);
                }
            }
            if (!bone.name.empty()) tempBones.push_back(bone);
            for (const auto& child : children) findNodes(child.structIndex, child.offset, bone.name);
            return;
        }

        std::vector<GFFStructRef> children = gff.readStructList(structIdx, 6999, offset);
        for (const auto& child : children) findNodes(child.structIndex, child.offset, parentName);
    };
    findNodes(0, 0, "");

    if (!boneIndexMap.empty()) {
        int maxIndex = 0;
        for (const auto& [idx, name] : boneIndexMap) {
            if (idx > maxIndex) maxIndex = idx;
        }
        model.boneIndexArray.resize(maxIndex + 1);
        for (const auto& [idx, name] : boneIndexMap) {
            model.boneIndexArray[idx] = name;
        }
    }

    for (auto& mesh : model.meshes) {
        auto it = meshMaterials.find(mesh.name);
        if (it != meshMaterials.end()) mesh.materialName = it->second;

        auto bonesIt = meshBonesUsed.find(mesh.name);
        if (bonesIt != meshBonesUsed.end()) {
            mesh.bonesUsed = bonesIt->second;
        } else {
            std::string meshLower = mesh.name;
            std::transform(meshLower.begin(), meshLower.end(), meshLower.begin(), ::tolower);
            for (const auto& [mmhName, indices] : meshBonesUsed) {
                std::string mmhLower = mmhName;
                std::transform(mmhLower.begin(), mmhLower.end(), mmhLower.begin(), ::tolower);
                if (meshLower == mmhLower) {
                    mesh.bonesUsed = indices;
                    break;
                }
            }
        }
    }

    model.skeleton.bones = tempBones;
    for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
        Bone& bone = model.skeleton.bones[i];
        if (!bone.parentName.empty()) {
            bone.parentIndex = model.skeleton.findBone(bone.parentName);
        }
    }

    for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
        Bone& bone = model.skeleton.bones[i];
        if (bone.parentIndex < 0) {
            bone.worldPosX = bone.posX; bone.worldPosY = bone.posY; bone.worldPosZ = bone.posZ;
            bone.worldRotX = bone.rotX; bone.worldRotY = bone.rotY; bone.worldRotZ = bone.rotZ; bone.worldRotW = bone.rotW;
        } else {
            const Bone& parent = model.skeleton.bones[bone.parentIndex];
            float rx, ry, rz;
            quatRotateWorld(parent.worldRotX, parent.worldRotY, parent.worldRotZ, parent.worldRotW,
                           bone.posX, bone.posY, bone.posZ, rx, ry, rz);
            bone.worldPosX = parent.worldPosX + rx;
            bone.worldPosY = parent.worldPosY + ry;
            bone.worldPosZ = parent.worldPosZ + rz;
            quatMulWorld(parent.worldRotX, parent.worldRotY, parent.worldRotZ, parent.worldRotW,
                        bone.rotX, bone.rotY, bone.rotZ, bone.rotW,
                        bone.worldRotX, bone.worldRotY, bone.worldRotZ, bone.worldRotW);
            normalizeQuat(bone.worldRotX, bone.worldRotY, bone.worldRotZ, bone.worldRotW);
        }
    }

    for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
        Bone& bone = model.skeleton.bones[i];
        bone.invBindRotX = -bone.worldRotX; bone.invBindRotY = -bone.worldRotY;
        bone.invBindRotZ = -bone.worldRotZ; bone.invBindRotW = bone.worldRotW;
        float nx, ny, nz;
        quatRotateWorld(bone.invBindRotX, bone.invBindRotY, bone.invBindRotZ, bone.invBindRotW,
                       -bone.worldPosX, -bone.worldPosY, -bone.worldPosZ, nx, ny, nz);
        bone.invBindPosX = nx; bone.invBindPosY = ny; bone.invBindPosZ = nz;
    }
}