#include "mmh_loader.h"
#include "model_loader.h"
#include "dds_loader.h"
#include "animation.h"
#include "Gff.h"
#include <iostream>
#include "gff32.h"
#include "erf.h"
#include <algorithm>
#include <sstream>
#include <map>
#include <set>
#include <functional>
#include <cmath>

Material parseMAO(const std::string& maoContent, const std::string& materialName) {
    Material mat;
    mat.name = materialName;

    std::vector<uint8_t> rawBytes(maoContent.begin(), maoContent.end());
    if (rawBytes.size() >= 8 && GFF32::GFF32File::isGFF32(rawBytes)) {
        GFF32::GFF32File gff32;
        if (gff32.load(rawBytes) && gff32.root()) {
            const auto& root = *gff32.root();

            for (const auto& fieldName : root.fieldOrder) {
                auto it = root.fields.find(fieldName);
                if (it == root.fields.end()) continue;
                const auto& field = it->second;
                if (field.typeId != GFF32::TypeID::ExoString &&
                    field.typeId != GFF32::TypeID::ResRef) continue;
                const std::string* valPtr = std::get_if<std::string>(&field.value);
                if (!valPtr) continue;
                std::string valLower = *valPtr;
                std::transform(valLower.begin(), valLower.end(), valLower.begin(), ::tolower);
                if (valLower.find("terrain.mat") != std::string::npos ||
                    valLower.find("terrain_low.mat") != std::string::npos) {
                    mat.isTerrain = true;
                    break;
                }
                if (valLower == "water.mat" ||
                    valLower == "flowingwater" ||
                    valLower == "water") {
                    mat.isWater = true;
                    break;
                }
            }

            for (const auto& fieldName : root.fieldOrder) {
                auto it = root.fields.find(fieldName);
                if (it == root.fields.end()) continue;
                const auto& field = it->second;

                if (field.typeId == GFF32::TypeID::ExoString ||
                    field.typeId == GFF32::TypeID::ResRef) {
                    const std::string* valPtr = std::get_if<std::string>(&field.value);
                    if (!valPtr || valPtr->empty()) continue;
                    std::string resName = *valPtr;
                    std::string labelLower = fieldName;
                    std::transform(labelLower.begin(), labelLower.end(), labelLower.begin(), ::tolower);
                    std::string resLower = resName;
                    std::transform(resLower.begin(), resLower.end(), resLower.begin(), ::tolower);

                    if (mat.isTerrain) {
                        if (labelLower == "palette") {
                            mat.paletteMap = resName;
                            mat.diffuseMap = resName;
                        } else if (labelLower == "normal" || labelLower == "normalmap") {
                            mat.palNormalMap = resName;
                            mat.normalMap = resName;
                        } else if (labelLower == "maskv") {
                            mat.maskVMap = resName;
                        } else if (labelLower == "maska") {
                            mat.maskAMap = resName;
                        } else if (labelLower == "maska2") {
                            mat.maskA2Map = resName;
                        } else if (labelLower == "mml_treliefmappalette") {
                            mat.reliefMap = resName;
                        }
                    } else if (mat.isWater) {
                        if (labelLower == "mat_tnormalmap" ||
                            labelLower.find("normalmap") != std::string::npos ||
                            labelLower == "normal") {
                            mat.normalMap = resName;
                            mat.waterNormalMap = resName;
                        } else if (labelLower == "mml_tdecal") {
                            mat.waterDecalMap = resName;
                        } else if (labelLower == "mml_twatermask") {
                            mat.waterMaskMap = resName;
                        } else if (labelLower.find("diffuse") != std::string::npos ||
                                   labelLower.find("texture") != std::string::npos) {
                            if (mat.diffuseMap.empty()) mat.diffuseMap = resName;
                        }
                    } else {
                        if (labelLower.find("diffuse") != std::string::npos ||
                            labelLower.find("packedtexture") != std::string::npos ||
                            labelLower == "palette") {
                            if (mat.diffuseMap.empty()) mat.diffuseMap = resName;
                        } else if (labelLower.find("normalmap") != std::string::npos ||
                                   labelLower.find("normal") != std::string::npos) {
                            if (mat.normalMap.empty()) mat.normalMap = resName;
                        } else if (labelLower.find("specular") != std::string::npos) {
                            if (mat.specularMap.empty()) mat.specularMap = resName;
                        } else if (labelLower.find("tint") != std::string::npos) {
                            if (mat.tintMap.empty()) mat.tintMap = resName;
                        } else if (labelLower.find("lowlod") != std::string::npos) {
                            mat.diffuseMap = resName;
                            mat.normalMap.clear();
                        } else if (labelLower.find("texture") != std::string::npos) {
                            if (resLower.find("_n.") != std::string::npos ||
                                resLower.find("_nrm") != std::string::npos ||
                                resLower.find("_n_") != std::string::npos) {
                                if (mat.normalMap.empty()) mat.normalMap = resName;
                            } else if (resLower.find("_s.") != std::string::npos ||
                                       resLower.find("_spec") != std::string::npos) {
                                if (mat.specularMap.empty()) mat.specularMap = resName;
                            } else {
                                if (mat.diffuseMap.empty()) mat.diffuseMap = resName;
                            }
                        } else {
                            // Fallback: unrecognized field name (skybox, custom shaders, etc)
                            if (mat.diffuseMap.empty() &&
                                resLower.find(".mat") == std::string::npos &&
                                resLower.find(".fx") == std::string::npos &&
                                resLower.find(".mfx") == std::string::npos) {
                                mat.diffuseMap = resName;
                                std::cout << "[MAO] Fallback: using field '" << fieldName
                                          << "' value '" << resName << "' as diffuse for '" << materialName << "'" << std::endl;
                            }
                        }
                    }
                }

                if ((mat.isTerrain || mat.isWater) && field.typeId == GFF32::TypeID::Structure) {
                    std::string labelLower = fieldName;
                    std::transform(labelLower.begin(), labelLower.end(), labelLower.begin(), ::tolower);
                    const auto* structPtr = std::get_if<GFF32::StructurePtr>(&field.value);
                    if (!structPtr || !*structPtr) continue;
                    const auto& st = **structPtr;

                    auto extractFloats = [&](const GFF32::Structure& s, float* out, int maxCount) {
                        int idx = 0;
                        for (const auto& fn : s.fieldOrder) {
                            if (idx >= maxCount) break;
                            auto fit = s.fields.find(fn);
                            if (fit == s.fields.end()) continue;
                            if (fit->second.typeId == GFF32::TypeID::FLOAT) {
                                const float* fp = std::get_if<float>(&fit->second.value);
                                if (fp) out[idx] = *fp;
                            }
                            idx++;
                        }
                    };

                    if (labelLower.find("mml_vpalette_parameters") != std::string::npos) {
                        extractFloats(st, mat.palParam, 4);
                    } else if (labelLower.find("mml_vpalette_dimensions") != std::string::npos) {
                        extractFloats(st, mat.palDim, 4);
                    } else if (labelLower.find("mml_muvscalevalues") != std::string::npos) {
                        float uvMatrix[16] = {};
                        int floatIdx = 0;
                        for (const auto& fn : st.fieldOrder) {
                            auto fit = st.fields.find(fn);
                            if (fit == st.fields.end()) continue;
                            if (fit->second.typeId == GFF32::TypeID::FLOAT) {
                                const float* fp = std::get_if<float>(&fit->second.value);
                                if (fp && floatIdx < 16) uvMatrix[floatIdx] = *fp;
                                floatIdx++;
                            } else if (fit->second.typeId == GFF32::TypeID::Structure) {
                                const auto* rowPtr = std::get_if<GFF32::StructurePtr>(&fit->second.value);
                                if (rowPtr && *rowPtr) {
                                    extractFloats(**rowPtr, &uvMatrix[floatIdx], 4);
                                    floatIdx += 4;
                                }
                            }
                        }
                        for (int i = 0; i < 4; i++) mat.uvScales[i] = uvMatrix[i];
                        for (int i = 0; i < 4; i++) mat.uvScales[4+i] = uvMatrix[4+i];
                    } else if (labelLower.find("mml_mreliefscale") != std::string::npos) {
                        float relMatrix[16] = {};
                        int floatIdx = 0;
                        for (const auto& fn : st.fieldOrder) {
                            auto fit = st.fields.find(fn);
                            if (fit == st.fields.end()) continue;
                            if (fit->second.typeId == GFF32::TypeID::FLOAT) {
                                const float* fp = std::get_if<float>(&fit->second.value);
                                if (fp && floatIdx < 16) relMatrix[floatIdx] = *fp;
                                floatIdx++;
                            } else if (fit->second.typeId == GFF32::TypeID::Structure) {
                                const auto* rowPtr = std::get_if<GFF32::StructurePtr>(&fit->second.value);
                                if (rowPtr && *rowPtr) {
                                    extractFloats(**rowPtr, &relMatrix[floatIdx], 4);
                                    floatIdx += 4;
                                }
                            }
                        }
                        for (int i = 0; i < 4; i++) mat.reliefScales[i] = relMatrix[i];
                        for (int i = 0; i < 4; i++) mat.reliefScales[4+i] = relMatrix[4+i];
                    } else if (labelLower.find("mat_vvshwaterparams") != std::string::npos) {
                        extractFloats(st, mat.waveParams, 12);
                    } else if (labelLower.find("mat_vpshwaterparams") != std::string::npos) {
                        float pshParams[8] = {};
                        extractFloats(st, pshParams, 8);
                        for (int i = 0; i < 4; i++) mat.waterColor[i] = pshParams[i];
                        for (int i = 0; i < 4; i++) mat.waterVisual[i] = pshParams[4+i];
                    }
                }
            }

            if (mat.diffuseMap.empty()) {
                for (const auto& fieldName : root.fieldOrder) {
                    auto it = root.fields.find(fieldName);
                    if (it == root.fields.end()) continue;
                    const auto& field = it->second;
                    if (field.typeId != GFF32::TypeID::ResRef) continue;
                    const std::string* valPtr = std::get_if<std::string>(&field.value);
                    if (!valPtr || valPtr->empty()) continue;
                    mat.diffuseMap = *valPtr;
                    break;
                }
            }
        }
        return mat;
    }

    if (maoContent.find("\"terrain.mat\"") != std::string::npos ||
        maoContent.find("\"terrain_low.mat\"") != std::string::npos) {
        mat.isTerrain = true;
    }
    if (maoContent.find("\"Water.mat\"") != std::string::npos ||
        maoContent.find("\"water.mat\"") != std::string::npos ||
        maoContent.find("\"FlowingWater\"") != std::string::npos ||
        maoContent.find("\"Water\"") != std::string::npos) {
        mat.isWater = true;
    }

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

            if (mat.isTerrain) {
                if (texNameLower == "palette") {
                    mat.paletteMap = resName;
                    mat.diffuseMap = resName;
                } else if (texNameLower == "normal") {
                    mat.palNormalMap = resName;
                    mat.normalMap = resName;
                } else if (texNameLower == "maskv") {
                    mat.maskVMap = resName;
                } else if (texNameLower == "maska") {
                    mat.maskAMap = resName;
                } else if (texNameLower == "maska2") {
                    mat.maskA2Map = resName;
                } else if (texNameLower == "mml_treliefmappalette") {
                    mat.reliefMap = resName;
                }
            } else if (mat.isWater) {
                if (texNameLower == "mat_tnormalmap" ||
                    texNameLower.find("normalmap") != std::string::npos ||
                    texNameLower.find("normal") != std::string::npos) {
                    mat.normalMap = resName;
                    mat.waterNormalMap = resName;
                } else if (texNameLower == "mml_tdecal") {
                    mat.waterDecalMap = resName;
                } else if (texNameLower == "mml_twatermask") {
                    mat.waterMaskMap = resName;
                } else if (texNameLower.find("distortion") != std::string::npos) {
                } else if (texNameLower.find("diffuse") != std::string::npos ||
                           texNameLower.find("texture") != std::string::npos) {
                    if (mat.diffuseMap.empty()) mat.diffuseMap = resName;
                }
            } else {
                if (texNameLower.find("corneanormal") != std::string::npos ||
                    texNameLower.find("lightmap") != std::string::npos ||
                    texNameLower.find("emotionsmask") != std::string::npos ||
                    texNameLower.find("emotionsnormal") != std::string::npos ||
                    texNameLower.find("reliefmap") != std::string::npos ||
                    texNameLower.find("maskv") != std::string::npos ||
                    texNameLower.find("maska") != std::string::npos) {

                } else if (texNameLower.find("agediffuse") != std::string::npos ||
                    texNameLower.find("age_diffuse") != std::string::npos ||
                    texNameLower.find("agediffusemap") != std::string::npos) {
                    mat.ageDiffuseMap = resName;
                } else if (texNameLower.find("agenormal") != std::string::npos ||
                           texNameLower.find("age_normal") != std::string::npos ||
                           texNameLower.find("agenormalmap") != std::string::npos) {
                    mat.ageNormalMap = resName;
                } else if (texNameLower.find("tattoo") != std::string::npos) {
                    mat.tattooMap = resName;
                } else if (texNameLower.find("browstubblenormal") != std::string::npos) {
                    mat.browStubbleNormalMap = resName;
                } else if (texNameLower.find("browstubble") != std::string::npos) {
                    mat.browStubbleMap = resName;
                } else if (texNameLower.find("diffuse") != std::string::npos ||
                           texNameLower.find("packedtexture") != std::string::npos ||
                           texNameLower == "palette") {
                    mat.diffuseMap = resName;
                } else if (texNameLower.find("normalmap") != std::string::npos ||
                           texNameLower == "normal") {
                    if (mat.normalMap.empty()) mat.normalMap = resName;
                } else if (texNameLower.find("specular") != std::string::npos) {
                    mat.specularMap = resName;
                } else if (texNameLower.find("tintmask") != std::string::npos ||
                           texNameLower.find("tint") != std::string::npos) {
                    mat.tintMap = resName;
                } else if (texNameLower.find("lowlod") != std::string::npos) {
                    mat.diffuseMap = resName;
                    mat.normalMap.clear();
                } else {
                    if (resNameLower.find("_d.") != std::string::npos ||
                        resNameLower.find("0d.") != std::string::npos ||
                        resNameLower.find("_d_") != std::string::npos) {
                        if (mat.diffuseMap.empty()) mat.diffuseMap = resName;
                    } else if (resNameLower.find("_n.") != std::string::npos ||
                               resNameLower.find("0n.") != std::string::npos ||
                               resNameLower.find("_nrm") != std::string::npos) {
                        if (mat.normalMap.empty()) mat.normalMap = resName;
                    } else if (resNameLower.find("_s.") != std::string::npos ||
                               resNameLower.find("0s.") != std::string::npos) {
                        if (mat.specularMap.empty()) mat.specularMap = resName;
                    } else if (resNameLower.find("_t.") != std::string::npos ||
                               resNameLower.find("0t.") != std::string::npos) {
                        if (mat.tintMap.empty()) mat.tintMap = resName;
                    } else if (mat.diffuseMap.empty()) {
                        mat.diffuseMap = resName;
                    }
                }
            }
        }
        pos = endTag + 2;
    }

    auto parseFloats = [&](const std::string& tagName, float* out, int count) {
        size_t p = maoContent.find("Name=\"" + tagName + "\"");
        if (p == std::string::npos) return;
        size_t vp = maoContent.find("value=\"", p);
        if (vp == std::string::npos) return;
        vp += 7;
        size_t ve = maoContent.find("\"", vp);
        if (ve == std::string::npos) return;
        std::string vals = maoContent.substr(vp, ve - vp);
        std::istringstream ss(vals);
        for (int i = 0; i < count; i++) ss >> out[i];
    };

    if (mat.isTerrain) {
        parseFloats("mml_vPalette_parameters", mat.palParam, 4);
        parseFloats("mml_vPalette_dimensions", mat.palDim, 4);
        float uvMatrix[16] = {};
        parseFloats("mml_mUVScaleValues", uvMatrix, 16);
        for (int i = 0; i < 4; i++) mat.uvScales[i] = uvMatrix[i];
        for (int i = 0; i < 4; i++) mat.uvScales[4+i] = uvMatrix[4+i];
        float relMatrix[16] = {};
        parseFloats("mml_mReliefScale", relMatrix, 16);
        for (int i = 0; i < 4; i++) mat.reliefScales[i] = relMatrix[i];
        for (int i = 0; i < 4; i++) mat.reliefScales[4+i] = relMatrix[4+i];
    }

    if (mat.isWater) {
        parseFloats("mat_vVSHWaterParams", mat.waveParams, 12);
        float pshParams[8] = {};
        parseFloats("mat_vPSHWaterParams", pshParams, 8);
        for (int i = 0; i < 4; i++) mat.waterColor[i] = pshParams[i];
        for (int i = 0; i < 4; i++) mat.waterVisual[i] = pshParams[4+i];
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
    std::map<std::string, std::string> meshParentBone;
    std::map<std::string, std::array<float,3>> meshLocalPos;
    std::map<std::string, std::array<float,4>> meshLocalRot;
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
            if (!meshName.empty() && !parentName.empty()) meshParentBone[meshName] = parentName;

            std::vector<uint32_t> bonesUsedRaw = readUInt32List(structIdx, 6255, offset);
            if (!bonesUsedRaw.empty()) {
                std::vector<int> boneIndices;
                for (uint32_t idx : bonesUsedRaw) boneIndices.push_back(static_cast<int>(idx));
                meshBonesUsed[meshName] = boneIndices;
            }

            std::vector<GFFStructRef> children = gff.readStructList(structIdx, 6999, offset);
            for (const auto& child : children) {
                const GFFField* posField = gff.findField(child.structIndex, 6047);
                if (posField && !meshName.empty()) {
                    uint32_t posOffset = gff.dataOffset() + posField->dataOffset + child.offset;
                    meshLocalPos[meshName] = {
                        gff.readFloatAt(posOffset),
                        gff.readFloatAt(posOffset + 4),
                        gff.readFloatAt(posOffset + 8)
                    };
                }
                const GFFField* rotField = gff.findField(child.structIndex, 6048);
                if (rotField && !meshName.empty()) {
                    uint32_t rotOffset = gff.dataOffset() + rotField->dataOffset + child.offset;
                    meshLocalRot[meshName] = {
                        gff.readFloatAt(rotOffset),
                        gff.readFloatAt(rotOffset + 4),
                        gff.readFloatAt(rotOffset + 8),
                        gff.readFloatAt(rotOffset + 12)
                    };
                }
            }
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

        auto parentIt = meshParentBone.find(mesh.name);
        if (parentIt != meshParentBone.end()) {
            mesh.parentBoneName = parentIt->second;
        } else {
            std::string meshLower = mesh.name;
            std::transform(meshLower.begin(), meshLower.end(), meshLower.begin(), ::tolower);
            for (const auto& [mmhName, boneName] : meshParentBone) {
                std::string mmhLower = mmhName;
                std::transform(mmhLower.begin(), mmhLower.end(), mmhLower.begin(), ::tolower);
                if (meshLower == mmhLower) {
                    mesh.parentBoneName = boneName;
                    break;
                }
            }
        }

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

        auto posIt = meshLocalPos.find(mesh.name);
        if (posIt == meshLocalPos.end()) {
            std::string meshLower = mesh.name;
            std::transform(meshLower.begin(), meshLower.end(), meshLower.begin(), ::tolower);
            for (auto& [k, v] : meshLocalPos) {
                std::string kl = k;
                std::transform(kl.begin(), kl.end(), kl.begin(), ::tolower);
                if (meshLower == kl) { posIt = meshLocalPos.find(k); break; }
            }
        }
        if (posIt != meshLocalPos.end()) {
            mesh.localPosX = posIt->second[0];
            mesh.localPosY = posIt->second[1];
            mesh.localPosZ = posIt->second[2];
        }
        auto rotIt = meshLocalRot.find(mesh.name);
        if (rotIt == meshLocalRot.end()) {
            std::string meshLower = mesh.name;
            std::transform(meshLower.begin(), meshLower.end(), meshLower.begin(), ::tolower);
            for (auto& [k, v] : meshLocalRot) {
                std::string kl = k;
                std::transform(kl.begin(), kl.end(), kl.begin(), ::tolower);
                if (meshLower == kl) { rotIt = meshLocalRot.find(k); break; }
            }
        }
        if (rotIt != meshLocalRot.end()) {
            mesh.localRotX = rotIt->second[0];
            mesh.localRotY = rotIt->second[1];
            mesh.localRotZ = rotIt->second[2];
            mesh.localRotW = rotIt->second[3];
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