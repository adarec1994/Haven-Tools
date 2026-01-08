#include "export.h"
#include <fstream>
#include <cstring>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <set>

static const uint32_t GLTF_MAGIC = 0x46546C67;
static const uint32_t GLTF_VERSION = 2;
static const uint32_t CHUNK_TYPE_JSON = 0x4E4F534A;
static const uint32_t CHUNK_TYPE_BIN = 0x004E4942;

static const int COMPONENT_TYPE_UNSIGNED_BYTE = 5121;
static const int COMPONENT_TYPE_UNSIGNED_SHORT = 5123;
static const int COMPONENT_TYPE_UNSIGNED_INT = 5125;
static const int COMPONENT_TYPE_FLOAT = 5126;

static const int TARGET_ARRAY_BUFFER = 34962;
static const int TARGET_ELEMENT_ARRAY_BUFFER = 34963;

static std::string escapeJson(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

static void writeU32(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(val & 0xFF);
    buf.push_back((val >> 8) & 0xFF);
    buf.push_back((val >> 16) & 0xFF);
    buf.push_back((val >> 24) & 0xFF);
}

static void writeFloat(std::vector<uint8_t>& buf, float val) {
    uint32_t bits;
    std::memcpy(&bits, &val, 4);
    writeU32(buf, bits);
}

static void writeU16(std::vector<uint8_t>& buf, uint16_t val) {
    buf.push_back(val & 0xFF);
    buf.push_back((val >> 8) & 0xFF);
}

static void padTo4(std::vector<uint8_t>& buf) {
    while (buf.size() % 4 != 0) buf.push_back(0);
}

static void padJsonTo4(std::string& json) {
    while (json.size() % 4 != 0) json += ' ';
}

static int findSkeletonBone(const Skeleton& skeleton, const std::string& name) {
    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
    for (size_t i = 0; i < skeleton.bones.size(); i++) {
        std::string boneLower = skeleton.bones[i].name;
        std::transform(boneLower.begin(), boneLower.end(), boneLower.begin(), ::tolower);
        if (boneLower == nameLower) return (int)i;
    }
    return -1;
}

bool exportToGLB(const Model& model, const std::vector<Animation>& animations, const std::string& outputPath) {
    if (model.meshes.empty()) return false;

    std::vector<uint8_t> binBuffer;
    std::string json;

    struct BufferViewInfo { size_t offset; size_t length; int target; };
    std::vector<BufferViewInfo> bufferViews;

    struct AccessorInfo { int bufferView; int componentType; int count; std::string type; float minVals[3]; float maxVals[3]; int minMaxCount; };
    std::vector<AccessorInfo> accessors;

    struct ImageInfo { size_t bufferView; std::string mimeType; };
    std::vector<ImageInfo> images;

    struct TextureInfo { int imageIdx; };
    std::vector<TextureInfo> textures;

    bool hasSkeleton = !model.skeleton.bones.empty();

    std::vector<int> meshBoneMap;
    if (hasSkeleton) {
        meshBoneMap.resize(model.boneIndexArray.size(), -1);
        for (size_t i = 0; i < model.boneIndexArray.size(); i++) {
            meshBoneMap[i] = findSkeletonBone(model.skeleton, model.boneIndexArray[i]);
        }
    }

    struct MeshExportData { int posAcc, normAcc, uvAcc, jointsAcc, weightsAcc, idxAcc, matIdx; bool hasSkin; };
    std::vector<MeshExportData> meshExports;

    for (size_t mi = 0; mi < model.meshes.size(); mi++) {
        const auto& mesh = model.meshes[mi];
        MeshExportData md = {-1, -1, -1, -1, -1, -1, mesh.materialIndex, false};

        bool meshHasSkin = hasSkeleton && mesh.hasSkinning;
        md.hasSkin = meshHasSkin;

        float minPos[3] = {1e30f, 1e30f, 1e30f};
        float maxPos[3] = {-1e30f, -1e30f, -1e30f};

        size_t posOff = binBuffer.size();
        for (const auto& v : mesh.vertices) {
            writeFloat(binBuffer, v.x);
            writeFloat(binBuffer, v.y);
            writeFloat(binBuffer, v.z);
            if (v.x < minPos[0]) minPos[0] = v.x;
            if (v.y < minPos[1]) minPos[1] = v.y;
            if (v.z < minPos[2]) minPos[2] = v.z;
            if (v.x > maxPos[0]) maxPos[0] = v.x;
            if (v.y > maxPos[1]) maxPos[1] = v.y;
            if (v.z > maxPos[2]) maxPos[2] = v.z;
        }
        int posView = (int)bufferViews.size();
        bufferViews.push_back({posOff, binBuffer.size() - posOff, TARGET_ARRAY_BUFFER});
        md.posAcc = (int)accessors.size();
        accessors.push_back({posView, COMPONENT_TYPE_FLOAT, (int)mesh.vertices.size(), "VEC3", {minPos[0], minPos[1], minPos[2]}, {maxPos[0], maxPos[1], maxPos[2]}, 3});

        size_t normOff = binBuffer.size();
        for (const auto& v : mesh.vertices) {
            writeFloat(binBuffer, v.nx);
            writeFloat(binBuffer, v.ny);
            writeFloat(binBuffer, v.nz);
        }
        int normView = (int)bufferViews.size();
        bufferViews.push_back({normOff, binBuffer.size() - normOff, TARGET_ARRAY_BUFFER});
        md.normAcc = (int)accessors.size();
        accessors.push_back({normView, COMPONENT_TYPE_FLOAT, (int)mesh.vertices.size(), "VEC3", {}, {}, 0});

        size_t uvOff = binBuffer.size();
        for (const auto& v : mesh.vertices) {
            writeFloat(binBuffer, v.u);
            writeFloat(binBuffer, 1.0f - v.v);
        }
        int uvView = (int)bufferViews.size();
        bufferViews.push_back({uvOff, binBuffer.size() - uvOff, TARGET_ARRAY_BUFFER});
        md.uvAcc = (int)accessors.size();
        accessors.push_back({uvView, COMPONENT_TYPE_FLOAT, (int)mesh.vertices.size(), "VEC2", {}, {}, 0});

        if (meshHasSkin) {
            size_t jointsOff = binBuffer.size();
            for (const auto& v : mesh.vertices) {
                for (int i = 0; i < 4; i++) {
                    int meshLocalIdx = v.boneIndices[i];
                    int skelIdx = 0;
                    if (meshLocalIdx >= 0) {
                        int globalIdx = meshLocalIdx;
                        if (!mesh.bonesUsed.empty() && meshLocalIdx < (int)mesh.bonesUsed.size()) {
                            globalIdx = mesh.bonesUsed[meshLocalIdx];
                        }
                        if (globalIdx >= 0 && globalIdx < (int)meshBoneMap.size()) {
                            int mapped = meshBoneMap[globalIdx];
                            if (mapped >= 0) skelIdx = mapped;
                        }
                    }
                    binBuffer.push_back((uint8_t)skelIdx);
                }
            }
            int jointsView = (int)bufferViews.size();
            bufferViews.push_back({jointsOff, binBuffer.size() - jointsOff, TARGET_ARRAY_BUFFER});
            md.jointsAcc = (int)accessors.size();
            accessors.push_back({jointsView, COMPONENT_TYPE_UNSIGNED_BYTE, (int)mesh.vertices.size(), "VEC4", {}, {}, 0});

            size_t weightsOff = binBuffer.size();
            for (const auto& v : mesh.vertices) {
                float sum = 0;
                for (int i = 0; i < 4; i++) sum += v.boneWeights[i];
                for (int i = 0; i < 4; i++) {
                    float w = (sum > 0.0001f) ? v.boneWeights[i] / sum : (i == 0 ? 1.0f : 0.0f);
                    writeFloat(binBuffer, w);
                }
            }
            int weightsView = (int)bufferViews.size();
            bufferViews.push_back({weightsOff, binBuffer.size() - weightsOff, TARGET_ARRAY_BUFFER});
            md.weightsAcc = (int)accessors.size();
            accessors.push_back({weightsView, COMPONENT_TYPE_FLOAT, (int)mesh.vertices.size(), "VEC4", {}, {}, 0});
        }

        size_t idxOff = binBuffer.size();
        bool use32bit = mesh.vertices.size() > 65535;
        for (uint32_t idx : mesh.indices) {
            if (use32bit) writeU32(binBuffer, idx);
            else writeU16(binBuffer, (uint16_t)idx);
        }
        size_t idxLen = binBuffer.size() - idxOff;
        padTo4(binBuffer);
        int idxView = (int)bufferViews.size();
        bufferViews.push_back({idxOff, idxLen, TARGET_ELEMENT_ARRAY_BUFFER});
        md.idxAcc = (int)accessors.size();
        accessors.push_back({idxView, use32bit ? COMPONENT_TYPE_UNSIGNED_INT : COMPONENT_TYPE_UNSIGNED_SHORT, (int)mesh.indices.size(), "SCALAR", {}, {}, 0});

        meshExports.push_back(md);
    }

    int ibmAccessor = -1;
    if (hasSkeleton) {
        size_t ibmOff = binBuffer.size();
        for (const auto& bone : model.skeleton.bones) {
            float qx = bone.invBindRotX, qy = bone.invBindRotY, qz = bone.invBindRotZ, qw = bone.invBindRotW;
            float tx = bone.invBindPosX, ty = bone.invBindPosY, tz = bone.invBindPosZ;

            float xx = qx*qx, yy = qy*qy, zz = qz*qz;
            float xy = qx*qy, xz = qx*qz, yz = qy*qz;
            float wx = qw*qx, wy = qw*qy, wz = qw*qz;

            writeFloat(binBuffer, 1.0f - 2.0f*(yy+zz));
            writeFloat(binBuffer, 2.0f*(xy+wz));
            writeFloat(binBuffer, 2.0f*(xz-wy));
            writeFloat(binBuffer, 0.0f);
            writeFloat(binBuffer, 2.0f*(xy-wz));
            writeFloat(binBuffer, 1.0f - 2.0f*(xx+zz));
            writeFloat(binBuffer, 2.0f*(yz+wx));
            writeFloat(binBuffer, 0.0f);
            writeFloat(binBuffer, 2.0f*(xz+wy));
            writeFloat(binBuffer, 2.0f*(yz-wx));
            writeFloat(binBuffer, 1.0f - 2.0f*(xx+yy));
            writeFloat(binBuffer, 0.0f);
            writeFloat(binBuffer, tx);
            writeFloat(binBuffer, ty);
            writeFloat(binBuffer, tz);
            writeFloat(binBuffer, 1.0f);
        }
        int ibmView = (int)bufferViews.size();
        bufferViews.push_back({ibmOff, binBuffer.size() - ibmOff, 0});
        ibmAccessor = (int)accessors.size();
        accessors.push_back({ibmView, COMPONENT_TYPE_FLOAT, (int)model.skeleton.bones.size(), "MAT4", {}, {}, 0});
    }

    struct AnimExportData {
        std::string name;
        std::vector<int> samplerIndices;
        std::vector<std::pair<int, std::string>> channels;
    };
    std::vector<AnimExportData> animExports;

    struct SamplerData {
        int inputAcc;
        int outputAcc;
        std::string interpolation;
    };
    std::vector<SamplerData> allSamplers;

    for (const auto& anim : animations) {
        if (anim.tracks.empty()) continue;

        AnimExportData ae;
        ae.name = anim.name;

        for (const auto& track : anim.tracks) {
            int boneIdx = findSkeletonBone(model.skeleton, track.boneName);
            if (boneIdx < 0) continue;
            if (track.keyframes.empty()) continue;

            // Check if this is GOD/GOB bone - skip translation for these
            std::string boneNameLower = model.skeleton.bones[boneIdx].name;
            std::transform(boneNameLower.begin(), boneNameLower.end(), boneNameLower.begin(), ::tolower);
            bool isGodBone = (boneNameLower == "god" || boneNameLower == "gob");

            if (track.isTranslation && isGodBone) {
                continue; // Skip GOD/GOB translation
            }

            size_t timeOff = binBuffer.size();
            float minTime = track.keyframes[0].time;
            float maxTime = track.keyframes[0].time;
            for (const auto& kf : track.keyframes) {
                writeFloat(binBuffer, kf.time);
                if (kf.time < minTime) minTime = kf.time;
                if (kf.time > maxTime) maxTime = kf.time;
            }
            int timeView = (int)bufferViews.size();
            bufferViews.push_back({timeOff, binBuffer.size() - timeOff, 0});
            int timeAcc = (int)accessors.size();
            AccessorInfo tai = {timeView, COMPONENT_TYPE_FLOAT, (int)track.keyframes.size(), "SCALAR", {minTime, 0, 0}, {maxTime, 0, 0}, 1};
            accessors.push_back(tai);

            size_t valOff = binBuffer.size();
            if (track.isRotation) {
                for (const auto& kf : track.keyframes) {
                    writeFloat(binBuffer, kf.x);
                    writeFloat(binBuffer, kf.y);
                    writeFloat(binBuffer, kf.z);
                    writeFloat(binBuffer, kf.w);
                }
                int valView = (int)bufferViews.size();
                bufferViews.push_back({valOff, binBuffer.size() - valOff, 0});
                int valAcc = (int)accessors.size();
                accessors.push_back({valView, COMPONENT_TYPE_FLOAT, (int)track.keyframes.size(), "VEC4", {}, {}, 0});

                int samplerIdx = (int)allSamplers.size();
                allSamplers.push_back({timeAcc, valAcc, "LINEAR"});
                ae.samplerIndices.push_back(samplerIdx);
                ae.channels.push_back({boneIdx, "rotation"});
            } else if (track.isTranslation) {
                // Get bind pose offset for this bone
                const Bone& bone = model.skeleton.bones[boneIdx];
                float baseX = bone.posX, baseY = bone.posY, baseZ = bone.posZ;

                for (const auto& kf : track.keyframes) {
                    // Animation translation is additive to bind pose
                    writeFloat(binBuffer, baseX + kf.x);
                    writeFloat(binBuffer, baseY + kf.y);
                    writeFloat(binBuffer, baseZ + kf.z);
                }
                int valView = (int)bufferViews.size();
                bufferViews.push_back({valOff, binBuffer.size() - valOff, 0});
                int valAcc = (int)accessors.size();
                accessors.push_back({valView, COMPONENT_TYPE_FLOAT, (int)track.keyframes.size(), "VEC3", {}, {}, 0});

                int samplerIdx = (int)allSamplers.size();
                allSamplers.push_back({timeAcc, valAcc, "LINEAR"});
                ae.samplerIndices.push_back(samplerIdx);
                ae.channels.push_back({boneIdx, "translation"});
            }
        }

        if (!ae.channels.empty()) {
            animExports.push_back(ae);
        }
    }

    std::map<uint32_t, int> texIdToImageIdx;
    std::vector<std::pair<int, int>> materialTextures;

    for (size_t mi = 0; mi < model.materials.size(); mi++) {
        const auto& mat = model.materials[mi];
        int diffuseTexIdx = -1;

        if (!mat.diffuseData.empty() && mat.diffuseWidth > 0 && mat.diffuseHeight > 0) {
            size_t imgOff = binBuffer.size();

            int w = mat.diffuseWidth, h = mat.diffuseHeight;
            std::vector<uint8_t> png;

            std::string matNameLower = mat.name;
            std::transform(matNameLower.begin(), matNameLower.end(), matNameLower.begin(), ::tolower);
            bool isHairMaterial = (matNameLower.find("_har_") != std::string::npos ||
                                   matNameLower.find("hair") != std::string::npos ||
                                   matNameLower.find("_ubm_") != std::string::npos ||
                                   matNameLower.find("_ulm_") != std::string::npos) &&
                                  matNameLower.find("bld") == std::string::npos;

            std::vector<uint8_t> exportData = mat.diffuseData;
            if (isHairMaterial) {
                for (size_t i = 3; i < exportData.size(); i += 4) {
                    exportData[i] = 255;
                }
            }

            uint32_t crc_table[256];
            for (int n = 0; n < 256; n++) {
                uint32_t c = n;
                for (int k = 0; k < 8; k++) c = (c & 1) ? 0xedb88320 ^ (c >> 1) : c >> 1;
                crc_table[n] = c;
            }
            auto crc32 = [&](const uint8_t* data, size_t len) {
                uint32_t c = 0xffffffff;
                for (size_t i = 0; i < len; i++) c = crc_table[(c ^ data[i]) & 0xff] ^ (c >> 8);
                return c ^ 0xffffffff;
            };
            auto write32be = [](std::vector<uint8_t>& v, uint32_t val) {
                v.push_back((val >> 24) & 0xff); v.push_back((val >> 16) & 0xff);
                v.push_back((val >> 8) & 0xff); v.push_back(val & 0xff);
            };
            auto writeChunk = [&](const char* type, const std::vector<uint8_t>& data) {
                write32be(png, (uint32_t)data.size());
                png.insert(png.end(), type, type + 4);
                png.insert(png.end(), data.begin(), data.end());
                std::vector<uint8_t> forCrc(type, type + 4);
                forCrc.insert(forCrc.end(), data.begin(), data.end());
                write32be(png, crc32(forCrc.data(), forCrc.size()));
            };

            png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

            std::vector<uint8_t> ihdr(13);
            ihdr[0] = (w >> 24) & 0xff; ihdr[1] = (w >> 16) & 0xff; ihdr[2] = (w >> 8) & 0xff; ihdr[3] = w & 0xff;
            ihdr[4] = (h >> 24) & 0xff; ihdr[5] = (h >> 16) & 0xff; ihdr[6] = (h >> 8) & 0xff; ihdr[7] = h & 0xff;
            ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
            writeChunk("IHDR", ihdr);

            std::vector<uint8_t> raw;
            for (int y = 0; y < h; y++) {
                raw.push_back(0);
                raw.insert(raw.end(), exportData.begin() + y * w * 4, exportData.begin() + (y + 1) * w * 4);
            }

            std::vector<uint8_t> deflated;
            deflated.push_back(0x78); deflated.push_back(0x01);

            size_t pos = 0;
            while (pos < raw.size()) {
                size_t remain = raw.size() - pos;
                size_t blockLen = remain > 65535 ? 65535 : remain;
                bool last = (pos + blockLen >= raw.size());
                deflated.push_back(last ? 1 : 0);
                deflated.push_back(blockLen & 0xff);
                deflated.push_back((blockLen >> 8) & 0xff);
                deflated.push_back(~blockLen & 0xff);
                deflated.push_back((~blockLen >> 8) & 0xff);
                deflated.insert(deflated.end(), raw.begin() + pos, raw.begin() + pos + blockLen);
                pos += blockLen;
            }

            uint32_t adler = 1;
            for (size_t i = 0; i < raw.size(); i++) {
                uint32_t s1 = adler & 0xffff, s2 = (adler >> 16) & 0xffff;
                s1 = (s1 + raw[i]) % 65521;
                s2 = (s2 + s1) % 65521;
                adler = (s2 << 16) | s1;
            }
            deflated.push_back((adler >> 24) & 0xff);
            deflated.push_back((adler >> 16) & 0xff);
            deflated.push_back((adler >> 8) & 0xff);
            deflated.push_back(adler & 0xff);

            writeChunk("IDAT", deflated);
            writeChunk("IEND", {});

            binBuffer.insert(binBuffer.end(), png.begin(), png.end());
            padTo4(binBuffer);

            int imgView = (int)bufferViews.size();
            bufferViews.push_back({imgOff, png.size(), 0});

            int imgIdx = (int)images.size();
            images.push_back({(size_t)imgView, "image/png"});

            diffuseTexIdx = (int)textures.size();
            textures.push_back({imgIdx});
        }

        materialTextures.push_back({diffuseTexIdx, -1});
    }

    padTo4(binBuffer);

    json = "{\"asset\":{\"version\":\"2.0\",\"generator\":\"HavenTools\"},";
    json += "\"scene\":0,\"scenes\":[{\"nodes\":[0]}],";

    json += "\"nodes\":[{\"name\":\"" + escapeJson(model.name) + "\"";
    json += ",\"rotation\":[-0.7071068,0,0,0.7071068]";
    json += ",\"children\":[";
    for (size_t i = 0; i < model.meshes.size(); i++) {
        if (i > 0) json += ",";
        json += std::to_string(i + 1);
    }
    if (hasSkeleton) {
        for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
            if (model.skeleton.bones[i].parentIndex < 0) {
                json += "," + std::to_string(model.meshes.size() + 1 + i);
            }
        }
    }
    json += "]}";

    for (size_t i = 0; i < model.meshes.size(); i++) {
        json += ",{\"name\":\"" + escapeJson(model.meshes[i].name.empty() ? "Mesh" + std::to_string(i) : model.meshes[i].name) + "\"";
        json += ",\"mesh\":" + std::to_string(i);
        if (meshExports[i].hasSkin) json += ",\"skin\":0";
        json += "}";
    }

    if (hasSkeleton) {
        for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
            const auto& bone = model.skeleton.bones[i];
            json += ",{\"name\":\"" + escapeJson(bone.name) + "\"";
            json += ",\"translation\":[" + std::to_string(bone.posX) + "," + std::to_string(bone.posY) + "," + std::to_string(bone.posZ) + "]";
            json += ",\"rotation\":[" + std::to_string(bone.rotX) + "," + std::to_string(bone.rotY) + "," + std::to_string(bone.rotZ) + "," + std::to_string(bone.rotW) + "]";
            std::vector<int> children;
            for (size_t j = 0; j < model.skeleton.bones.size(); j++) {
                if (model.skeleton.bones[j].parentIndex == (int)i) children.push_back((int)j);
            }
            if (!children.empty()) {
                json += ",\"children\":[";
                for (size_t c = 0; c < children.size(); c++) {
                    if (c > 0) json += ",";
                    json += std::to_string(model.meshes.size() + 1 + children[c]);
                }
                json += "]";
            }
            json += "}";
        }
    }
    json += "],";

    json += "\"meshes\":[";
    for (size_t i = 0; i < model.meshes.size(); i++) {
        if (i > 0) json += ",";
        const auto& md = meshExports[i];
        json += "{\"name\":\"" + escapeJson(model.meshes[i].name.empty() ? "Mesh" + std::to_string(i) : model.meshes[i].name) + "\"";
        json += ",\"primitives\":[{\"attributes\":{\"POSITION\":" + std::to_string(md.posAcc);
        json += ",\"NORMAL\":" + std::to_string(md.normAcc);
        json += ",\"TEXCOORD_0\":" + std::to_string(md.uvAcc);
        if (md.hasSkin) {
            json += ",\"JOINTS_0\":" + std::to_string(md.jointsAcc);
            json += ",\"WEIGHTS_0\":" + std::to_string(md.weightsAcc);
        }
        json += "},\"indices\":" + std::to_string(md.idxAcc);
        if (md.matIdx >= 0) json += ",\"material\":" + std::to_string(md.matIdx);
        json += "}]}";
    }
    json += "],";

    if (hasSkeleton) {
        json += "\"skins\":[{\"inverseBindMatrices\":" + std::to_string(ibmAccessor) + ",\"joints\":[";
        for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
            if (i > 0) json += ",";
            json += std::to_string(model.meshes.size() + 1 + i);
        }
        json += "]";
        for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
            if (model.skeleton.bones[i].parentIndex < 0) {
                json += ",\"skeleton\":" + std::to_string(model.meshes.size() + 1 + i);
                break;
            }
        }
        json += "}],";
    }

    if (!animExports.empty()) {
        json += "\"animations\":[";
        int globalSamplerOffset = 0;
        for (size_t ai = 0; ai < animExports.size(); ai++) {
            if (ai > 0) json += ",";
            const auto& ae = animExports[ai];
            json += "{\"name\":\"" + escapeJson(ae.name) + "\"";

            json += ",\"samplers\":[";
            for (size_t si = 0; si < ae.samplerIndices.size(); si++) {
                if (si > 0) json += ",";
                const auto& samp = allSamplers[ae.samplerIndices[si]];
                json += "{\"input\":" + std::to_string(samp.inputAcc);
                json += ",\"output\":" + std::to_string(samp.outputAcc);
                json += ",\"interpolation\":\"" + samp.interpolation + "\"}";
            }
            json += "]";

            json += ",\"channels\":[";
            for (size_t ci = 0; ci < ae.channels.size(); ci++) {
                if (ci > 0) json += ",";
                json += "{\"sampler\":" + std::to_string(ci);
                json += ",\"target\":{\"node\":" + std::to_string(model.meshes.size() + 1 + ae.channels[ci].first);
                json += ",\"path\":\"" + ae.channels[ci].second + "\"}}";
            }
            json += "]";

            json += "}";
        }
        json += "],";
    }

    if (!model.materials.empty()) {
        json += "\"materials\":[";
        for (size_t i = 0; i < model.materials.size(); i++) {
            if (i > 0) json += ",";
            const auto& mat = model.materials[i];
            json += "{\"name\":\"" + escapeJson(mat.name) + "\"";
            json += ",\"pbrMetallicRoughness\":{\"metallicFactor\":0.0,\"roughnessFactor\":0.8";
            if (i < materialTextures.size() && materialTextures[i].first >= 0) {
                json += ",\"baseColorTexture\":{\"index\":" + std::to_string(materialTextures[i].first) + "}";
            }
            json += "}}";
        }
        json += "],";
    }

    if (!textures.empty()) {
        json += "\"textures\":[";
        for (size_t i = 0; i < textures.size(); i++) {
            if (i > 0) json += ",";
            json += "{\"source\":" + std::to_string(textures[i].imageIdx) + "}";
        }
        json += "],";
    }

    if (!images.empty()) {
        json += "\"images\":[";
        for (size_t i = 0; i < images.size(); i++) {
            if (i > 0) json += ",";
            json += "{\"mimeType\":\"" + images[i].mimeType + "\",\"bufferView\":" + std::to_string(images[i].bufferView) + "}";
        }
        json += "],";
    }

    json += "\"accessors\":[";
    for (size_t i = 0; i < accessors.size(); i++) {
        if (i > 0) json += ",";
        const auto& acc = accessors[i];
        json += "{\"bufferView\":" + std::to_string(acc.bufferView);
        json += ",\"componentType\":" + std::to_string(acc.componentType);
        json += ",\"count\":" + std::to_string(acc.count);
        json += ",\"type\":\"" + acc.type + "\"";
        if (acc.minMaxCount > 0) {
            json += ",\"min\":[";
            for (int m = 0; m < acc.minMaxCount; m++) { if (m > 0) json += ","; json += std::to_string(acc.minVals[m]); }
            json += "],\"max\":[";
            for (int m = 0; m < acc.minMaxCount; m++) { if (m > 0) json += ","; json += std::to_string(acc.maxVals[m]); }
            json += "]";
        }
        json += "}";
    }
    json += "],";

    json += "\"bufferViews\":[";
    for (size_t i = 0; i < bufferViews.size(); i++) {
        if (i > 0) json += ",";
        json += "{\"buffer\":0,\"byteOffset\":" + std::to_string(bufferViews[i].offset);
        json += ",\"byteLength\":" + std::to_string(bufferViews[i].length);
        if (bufferViews[i].target != 0) json += ",\"target\":" + std::to_string(bufferViews[i].target);
        json += "}";
    }
    json += "],";

    json += "\"buffers\":[{\"byteLength\":" + std::to_string(binBuffer.size()) + "}]}";

    padJsonTo4(json);

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) return false;

    uint32_t totalSize = 12 + 8 + (uint32_t)json.size() + 8 + (uint32_t)binBuffer.size();
    uint32_t magic = GLTF_MAGIC, version = GLTF_VERSION;
    out.write(reinterpret_cast<const char*>(&magic), 4);
    out.write(reinterpret_cast<const char*>(&version), 4);
    out.write(reinterpret_cast<const char*>(&totalSize), 4);

    uint32_t jsonLen = (uint32_t)json.size(), jsonType = CHUNK_TYPE_JSON;
    out.write(reinterpret_cast<const char*>(&jsonLen), 4);
    out.write(reinterpret_cast<const char*>(&jsonType), 4);
    out.write(json.data(), json.size());

    uint32_t binLen = (uint32_t)binBuffer.size(), binType = CHUNK_TYPE_BIN;
    out.write(reinterpret_cast<const char*>(&binLen), 4);
    out.write(reinterpret_cast<const char*>(&binType), 4);
    out.write(reinterpret_cast<const char*>(binBuffer.data()), binBuffer.size());
    
    return out.good();
}

struct Mat4 {
    float m[16];

    static Mat4 Identity() {
        return {{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}};
    }

    static Mat4 FromTRS(float tx, float ty, float tz, float qx, float qy, float qz, float qw) {
        float x2 = qx + qx, y2 = qy + qy, z2 = qz + qz;
        float xx = qx * x2, xy = qx * y2, xz = qx * z2;
        float yy = qy * y2, yz = qy * z2, zz = qz * z2;
        float wx = qw * x2, wy = qw * y2, wz = qw * z2;

        Mat4 res;
        res.m[0] = 1.0f - (yy + zz); res.m[1] = xy + wz;          res.m[2] = xz - wy;          res.m[3] = 0;
        res.m[4] = xy - wz;          res.m[5] = 1.0f - (xx + zz); res.m[6] = yz + wx;          res.m[7] = 0;
        res.m[8] = xz + wy;          res.m[9] = yz - wx;          res.m[10] = 1.0f - (xx + yy); res.m[11] = 0;
        res.m[12] = tx;              res.m[13] = ty;              res.m[14] = tz;              res.m[15] = 1;
        return res;
    }

    Mat4 operator*(const Mat4& b) const {
        Mat4 res;
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                res.m[r + c * 4] = m[r + 0 * 4] * b.m[0 + c * 4] +
                                   m[r + 1 * 4] * b.m[1 + c * 4] +
                                   m[r + 2 * 4] * b.m[2 + c * 4] +
                                   m[r + 3 * 4] * b.m[3 + c * 4];
            }
        }
        return res;
    }
};

bool exportToFBX(const Model& model, const std::vector<Animation>& animations, const std::string& outputPath) {
    if (model.meshes.empty()) return false;

    std::vector<uint8_t> output;

    auto writeU8 = [&](uint8_t v) { output.push_back(v); };
    auto writeU16 = [&](uint16_t v) { output.push_back(v & 0xFF); output.push_back((v >> 8) & 0xFF); };
    auto writeU32 = [&](uint32_t v) {
        output.push_back(v & 0xFF); output.push_back((v >> 8) & 0xFF);
        output.push_back((v >> 16) & 0xFF); output.push_back((v >> 24) & 0xFF);
    };
    auto writeU64 = [&](uint64_t v) {
        for (int i = 0; i < 8; i++) output.push_back((v >> (i * 8)) & 0xFF);
    };
    auto writeI16 = [&](int16_t v) { writeU16((uint16_t)v); };
    auto writeI32 = [&](int32_t v) { writeU32((uint32_t)v); };
    auto writeI64 = [&](int64_t v) { writeU64((uint64_t)v); };
    auto writeF32 = [&](float v) { uint32_t bits; std::memcpy(&bits, &v, 4); writeU32(bits); };
    auto writeF64 = [&](double v) { uint64_t bits; std::memcpy(&bits, &v, 8); writeU64(bits); };
    auto writeBytes = [&](const void* data, size_t len) {
        const uint8_t* p = (const uint8_t*)data;
        output.insert(output.end(), p, p + len);
    };
    auto writeString = [&](const std::string& s) {
        writeU8((uint8_t)s.size());
        writeBytes(s.data(), s.size());
    };
    auto writeLongString = [&](const std::string& s) {
        writeU32((uint32_t)s.size());
        writeBytes(s.data(), s.size());
    };

    auto getGeoID = [](int i) -> int64_t { return 200000LL + i; };
    auto getBoneID = [](int i) -> int64_t { return 400000LL + i; };
    auto getModelID = [](int i) -> int64_t { return 500000LL + i; };
    auto getSkinID = [](int i) -> int64_t { return 600000LL + i; };
    auto getClusterID = [](int m, int b) -> int64_t { return 7000000LL + (int64_t)m * 10000 + b; };
    auto getMaterialID = [](int i) -> int64_t { return 800000LL + i; };
    auto getTextureID = [](int i) -> int64_t { return 900000LL + i; };
    auto getVideoID = [](int i) -> int64_t { return 1000000LL + i; };

    bool hasSkeleton = !model.skeleton.bones.empty();

    std::map<std::string, int> boneLookup;
    if (hasSkeleton) {
        for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
            std::string nameLower = model.skeleton.bones[i].name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            boneLookup[nameLower] = (int)i;
        }
    }

    auto findBoneIndex = [&](const std::string& name) -> int {
        std::string nameLower = name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        auto it = boneLookup.find(nameLower);
        return (it != boneLookup.end()) ? it->second : -1;
    };

    std::vector<Mat4> globalBoneTransforms;
    Mat4 rootRotation = Mat4::FromTRS(0, 0, 0, -0.7071068f, 0, 0, 0.7071068f);

    if (hasSkeleton) {
        globalBoneTransforms.resize(model.skeleton.bones.size());
        for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
            const auto& bone = model.skeleton.bones[i];
            Mat4 local = Mat4::FromTRS(bone.posX, bone.posY, bone.posZ, bone.rotX, bone.rotY, bone.rotZ, bone.rotW);
            if (bone.parentIndex >= 0) {
                globalBoneTransforms[i] = globalBoneTransforms[bone.parentIndex] * local;
            } else {
                globalBoneTransforms[i] = rootRotation * local;
            }
        }
    }

    std::vector<std::set<int>> meshBoneSets(model.meshes.size());
    if (hasSkeleton) {
        for (size_t i = 0; i < model.meshes.size(); i++) {
            if (!model.meshes[i].hasSkinning) continue;
            const auto& mesh = model.meshes[i];
            std::set<int> uniqueBones;
            for (const auto& v : mesh.vertices) {
                for (int b = 0; b < 4; b++) {
                    if (v.boneWeights[b] > 0.001f) {
                        int local = v.boneIndices[b];
                        int global = -1;
                        if (local >= 0) {
                            if (!mesh.bonesUsed.empty() && local < (int)mesh.bonesUsed.size()) {
                                int used = mesh.bonesUsed[local];
                                if (used >= 0 && used < (int)model.boneIndexArray.size()) {
                                    global = findBoneIndex(model.boneIndexArray[used]);
                                }
                            } else if (local < (int)model.boneIndexArray.size()) {
                                global = findBoneIndex(model.boneIndexArray[local]);
                            }
                        }
                        if (global != -1) uniqueBones.insert(global);
                    }
                }
            }
            meshBoneSets[i] = uniqueBones;
        }
    }

    struct NodeWriter {
        std::vector<uint8_t>& out;
        std::vector<size_t> nodeStack;

        NodeWriter(std::vector<uint8_t>& o) : out(o) {}

        void beginNode(const std::string& name) {
            nodeStack.push_back(out.size());
            for (int i = 0; i < 13; i++) out.push_back(0);
            out.push_back((uint8_t)name.size());
            out.insert(out.end(), name.begin(), name.end());
        }

        void endNode() {
            size_t nodeStart = nodeStack.back();
            nodeStack.pop_back();

            for (int i = 0; i < 13; i++) out.push_back(0);

            uint32_t endOffset = (uint32_t)out.size();
            out[nodeStart] = endOffset & 0xFF;
            out[nodeStart + 1] = (endOffset >> 8) & 0xFF;
            out[nodeStart + 2] = (endOffset >> 16) & 0xFF;
            out[nodeStart + 3] = (endOffset >> 24) & 0xFF;
        }

        void endNodeNoNested() {
            size_t nodeStart = nodeStack.back();
            nodeStack.pop_back();

            uint32_t endOffset = (uint32_t)out.size();
            out[nodeStart] = endOffset & 0xFF;
            out[nodeStart + 1] = (endOffset >> 8) & 0xFF;
            out[nodeStart + 2] = (endOffset >> 16) & 0xFF;
            out[nodeStart + 3] = (endOffset >> 24) & 0xFF;
        }

        void setPropertyCount(uint32_t count, uint32_t listLen) {
            size_t nodeStart = nodeStack.back();
            out[nodeStart + 4] = count & 0xFF;
            out[nodeStart + 5] = (count >> 8) & 0xFF;
            out[nodeStart + 6] = (count >> 16) & 0xFF;
            out[nodeStart + 7] = (count >> 24) & 0xFF;
            out[nodeStart + 8] = listLen & 0xFF;
            out[nodeStart + 9] = (listLen >> 8) & 0xFF;
            out[nodeStart + 10] = (listLen >> 16) & 0xFF;
            out[nodeStart + 11] = (listLen >> 24) & 0xFF;
        }

        size_t propStart;
        uint32_t propCount;

        void beginProps() {
            propStart = out.size();
            propCount = 0;
        }

        void addPropI16(int16_t v) { out.push_back('Y'); propCount++; out.push_back(v & 0xFF); out.push_back((v >> 8) & 0xFF); }
        void addPropI32(int32_t v) { out.push_back('I'); propCount++; uint32_t u = (uint32_t)v; out.push_back(u & 0xFF); out.push_back((u >> 8) & 0xFF); out.push_back((u >> 16) & 0xFF); out.push_back((u >> 24) & 0xFF); }
        void addPropI64(int64_t v) { out.push_back('L'); propCount++; uint64_t u = (uint64_t)v; for (int i = 0; i < 8; i++) out.push_back((u >> (i * 8)) & 0xFF); }
        void addPropF32(float v) { out.push_back('F'); propCount++; uint32_t bits; std::memcpy(&bits, &v, 4); out.push_back(bits & 0xFF); out.push_back((bits >> 8) & 0xFF); out.push_back((bits >> 16) & 0xFF); out.push_back((bits >> 24) & 0xFF); }
        void addPropF64(double v) { out.push_back('D'); propCount++; uint64_t bits; std::memcpy(&bits, &v, 8); for (int i = 0; i < 8; i++) out.push_back((bits >> (i * 8)) & 0xFF); }
        void addPropString(const std::string& s) {
            out.push_back('S'); propCount++;
            uint32_t len = (uint32_t)s.size();
            out.push_back(len & 0xFF); out.push_back((len >> 8) & 0xFF); out.push_back((len >> 16) & 0xFF); out.push_back((len >> 24) & 0xFF);
            out.insert(out.end(), s.begin(), s.end());
        }
        void addPropRaw(const std::string& s) {
            out.push_back('R'); propCount++;
            uint32_t len = (uint32_t)s.size();
            out.push_back(len & 0xFF); out.push_back((len >> 8) & 0xFF); out.push_back((len >> 16) & 0xFF); out.push_back((len >> 24) & 0xFF);
            out.insert(out.end(), s.begin(), s.end());
        }
        void addPropRawBytes(const uint8_t* data, size_t len) {
            out.push_back('R'); propCount++;
            uint32_t l = (uint32_t)len;
            out.push_back(l & 0xFF); out.push_back((l >> 8) & 0xFF); out.push_back((l >> 16) & 0xFF); out.push_back((l >> 24) & 0xFF);
            out.insert(out.end(), data, data + len);
        }

        void addPropI32Array(const std::vector<int32_t>& arr) {
            out.push_back('i'); propCount++;
            uint32_t len = (uint32_t)arr.size();
            out.push_back(len & 0xFF); out.push_back((len >> 8) & 0xFF); out.push_back((len >> 16) & 0xFF); out.push_back((len >> 24) & 0xFF);
            out.push_back(0); out.push_back(0); out.push_back(0); out.push_back(0);
            uint32_t byteLen = len * 4;
            out.push_back(byteLen & 0xFF); out.push_back((byteLen >> 8) & 0xFF); out.push_back((byteLen >> 16) & 0xFF); out.push_back((byteLen >> 24) & 0xFF);
            for (int32_t v : arr) {
                uint32_t u = (uint32_t)v;
                out.push_back(u & 0xFF); out.push_back((u >> 8) & 0xFF); out.push_back((u >> 16) & 0xFF); out.push_back((u >> 24) & 0xFF);
            }
        }

        void addPropF64Array(const std::vector<double>& arr) {
            out.push_back('d'); propCount++;
            uint32_t len = (uint32_t)arr.size();
            out.push_back(len & 0xFF); out.push_back((len >> 8) & 0xFF); out.push_back((len >> 16) & 0xFF); out.push_back((len >> 24) & 0xFF);
            out.push_back(0); out.push_back(0); out.push_back(0); out.push_back(0);
            uint32_t byteLen = len * 8;
            out.push_back(byteLen & 0xFF); out.push_back((byteLen >> 8) & 0xFF); out.push_back((byteLen >> 16) & 0xFF); out.push_back((byteLen >> 24) & 0xFF);
            for (double v : arr) {
                uint64_t bits; std::memcpy(&bits, &v, 8);
                for (int i = 0; i < 8; i++) out.push_back((bits >> (i * 8)) & 0xFF);
            }
        }

        void endProps() {
            uint32_t listLen = (uint32_t)(out.size() - propStart);
            setPropertyCount(propCount, listLen);
        }
    };

    auto quatToEuler = [](float qx, float qy, float qz, float qw, double& ex, double& ey, double& ez) {
        float sinr_cosp = 2 * (qw * qx + qy * qz);
        float cosr_cosp = 1 - 2 * (qx * qx + qy * qy);
        ex = std::atan2(sinr_cosp, cosr_cosp) * 180.0 / 3.14159265358979323846;
        float sinp = 2 * (qw * qy - qz * qx);
        if (std::abs(sinp) >= 1) ey = std::copysign(90.0, sinp);
        else ey = std::asin(sinp) * 180.0 / 3.14159265358979323846;
        float siny_cosp = 2 * (qw * qz + qx * qy);
        float cosy_cosp = 1 - 2 * (qy * qy + qz * qz);
        ez = std::atan2(siny_cosp, cosy_cosp) * 180.0 / 3.14159265358979323846;
    };

    const char* header = "Kaydara FBX Binary  ";
    writeBytes(header, 21);
    writeU8(0x1A);
    writeU8(0x00);
    writeU32(7500);

    NodeWriter nw(output);

    nw.beginNode("FBXHeaderExtension");
    nw.beginProps(); nw.endProps();
    {
        nw.beginNode("FBXHeaderVersion");
        nw.beginProps(); nw.addPropI32(1003); nw.endProps();
        nw.endNodeNoNested();

        nw.beginNode("FBXVersion");
        nw.beginProps(); nw.addPropI32(7500); nw.endProps();
        nw.endNodeNoNested();

        nw.beginNode("Creator");
        nw.beginProps(); nw.addPropString("HavenTools"); nw.endProps();
        nw.endNodeNoNested();
    }
    nw.endNode();

    nw.beginNode("GlobalSettings");
    nw.beginProps(); nw.endProps();
    {
        nw.beginNode("Version");
        nw.beginProps(); nw.addPropI32(1000); nw.endProps();
        nw.endNodeNoNested();

        nw.beginNode("Properties70");
        nw.beginProps(); nw.endProps();
        {
            auto addP = [&](const std::string& name, const std::string& t1, const std::string& t2, const std::string& flags, int val) {
                nw.beginNode("P");
                nw.beginProps();
                nw.addPropString(name); nw.addPropString(t1); nw.addPropString(t2); nw.addPropString(flags);
                nw.addPropI32(val);
                nw.endProps();
                nw.endNodeNoNested();
            };
            auto addPD = [&](const std::string& name, const std::string& t1, const std::string& t2, const std::string& flags, double val) {
                nw.beginNode("P");
                nw.beginProps();
                nw.addPropString(name); nw.addPropString(t1); nw.addPropString(t2); nw.addPropString(flags);
                nw.addPropF64(val);
                nw.endProps();
                nw.endNodeNoNested();
            };
            addP("UpAxis", "int", "Integer", "", 1);
            addP("UpAxisSign", "int", "Integer", "", 1);
            addP("FrontAxis", "int", "Integer", "", 2);
            addP("FrontAxisSign", "int", "Integer", "", 1);
            addP("CoordAxis", "int", "Integer", "", 0);
            addP("CoordAxisSign", "int", "Integer", "", 1);
            addP("OriginalUpAxis", "int", "Integer", "", 2);
            addP("OriginalUpAxisSign", "int", "Integer", "", 1);
            addPD("UnitScaleFactor", "double", "Number", "", 1.0);
        }
        nw.endNode();
    }
    nw.endNode();

    nw.beginNode("Objects");
    nw.beginProps(); nw.endProps();
    {
        nw.beginNode("Model");
        nw.beginProps();
        nw.addPropI64(100000);
        nw.addPropString("Model::" + model.name + "\x00\x01Null");
        nw.addPropString("Null");
        nw.endProps();
        {
            nw.beginNode("Version");
            nw.beginProps(); nw.addPropI32(232); nw.endProps();
            nw.endNodeNoNested();

            nw.beginNode("Properties70");
            nw.beginProps(); nw.endProps();
            {
                nw.beginNode("P");
                nw.beginProps();
                nw.addPropString("Lcl Rotation"); nw.addPropString("Lcl Rotation"); nw.addPropString(""); nw.addPropString("A");
                nw.addPropF64(-90.0); nw.addPropF64(0.0); nw.addPropF64(0.0);
                nw.endProps();
                nw.endNodeNoNested();
            }
            nw.endNode();
        }
        nw.endNode();

        for (size_t mi = 0; mi < model.meshes.size(); mi++) {
            const auto& mesh = model.meshes[mi];
            std::string meshName = mesh.name.empty() ? "Mesh" + std::to_string(mi) : mesh.name;

            nw.beginNode("Geometry");
            nw.beginProps();
            nw.addPropI64(getGeoID(mi));
            nw.addPropString("Geometry::" + meshName + "\x00\x01Mesh");
            nw.addPropString("Mesh");
            nw.endProps();
            {
                std::vector<double> vertices;
                for (const auto& v : mesh.vertices) {
                    vertices.push_back(v.x); vertices.push_back(v.y); vertices.push_back(v.z);
                }
                nw.beginNode("Vertices");
                nw.beginProps(); nw.addPropF64Array(vertices); nw.endProps();
                nw.endNodeNoNested();

                std::vector<int32_t> polyIdx;
                for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                    polyIdx.push_back((int32_t)mesh.indices[i]);
                    polyIdx.push_back((int32_t)mesh.indices[i + 1]);
                    polyIdx.push_back(-((int32_t)mesh.indices[i + 2]) - 1);
                }
                nw.beginNode("PolygonVertexIndex");
                nw.beginProps(); nw.addPropI32Array(polyIdx); nw.endProps();
                nw.endNodeNoNested();

                nw.beginNode("LayerElementNormal");
                nw.beginProps(); nw.addPropI32(0); nw.endProps();
                {
                    nw.beginNode("Version");
                    nw.beginProps(); nw.addPropI32(102); nw.endProps();
                    nw.endNodeNoNested();
                    nw.beginNode("Name");
                    nw.beginProps(); nw.addPropString("Normals"); nw.endProps();
                    nw.endNodeNoNested();
                    nw.beginNode("MappingInformationType");
                    nw.beginProps(); nw.addPropString("ByPolygonVertex"); nw.endProps();
                    nw.endNodeNoNested();
                    nw.beginNode("ReferenceInformationType");
                    nw.beginProps(); nw.addPropString("Direct"); nw.endProps();
                    nw.endNodeNoNested();

                    std::vector<double> normals;
                    for (uint32_t idx : mesh.indices) {
                        normals.push_back(mesh.vertices[idx].nx);
                        normals.push_back(mesh.vertices[idx].ny);
                        normals.push_back(mesh.vertices[idx].nz);
                    }
                    nw.beginNode("Normals");
                    nw.beginProps(); nw.addPropF64Array(normals); nw.endProps();
                    nw.endNodeNoNested();
                }
                nw.endNode();

                nw.beginNode("LayerElementUV");
                nw.beginProps(); nw.addPropI32(0); nw.endProps();
                {
                    nw.beginNode("Version");
                    nw.beginProps(); nw.addPropI32(101); nw.endProps();
                    nw.endNodeNoNested();
                    nw.beginNode("Name");
                    nw.beginProps(); nw.addPropString("UVMap"); nw.endProps();
                    nw.endNodeNoNested();
                    nw.beginNode("MappingInformationType");
                    nw.beginProps(); nw.addPropString("ByPolygonVertex"); nw.endProps();
                    nw.endNodeNoNested();
                    nw.beginNode("ReferenceInformationType");
                    nw.beginProps(); nw.addPropString("Direct"); nw.endProps();
                    nw.endNodeNoNested();

                    std::vector<double> uvs;
                    for (uint32_t idx : mesh.indices) {
                        uvs.push_back(mesh.vertices[idx].u);
                        uvs.push_back(1.0 - mesh.vertices[idx].v);
                    }
                    nw.beginNode("UV");
                    nw.beginProps(); nw.addPropF64Array(uvs); nw.endProps();
                    nw.endNodeNoNested();
                }
                nw.endNode();

                if (mesh.materialIndex >= 0) {
                    nw.beginNode("LayerElementMaterial");
                    nw.beginProps(); nw.addPropI32(0); nw.endProps();
                    {
                        nw.beginNode("Version");
                        nw.beginProps(); nw.addPropI32(101); nw.endProps();
                        nw.endNodeNoNested();
                        nw.beginNode("Name");
                        nw.beginProps(); nw.addPropString("Material"); nw.endProps();
                        nw.endNodeNoNested();
                        nw.beginNode("MappingInformationType");
                        nw.beginProps(); nw.addPropString("AllSame"); nw.endProps();
                        nw.endNodeNoNested();
                        nw.beginNode("ReferenceInformationType");
                        nw.beginProps(); nw.addPropString("IndexToDirect"); nw.endProps();
                        nw.endNodeNoNested();
                        nw.beginNode("Materials");
                        nw.beginProps(); nw.addPropI32Array({0}); nw.endProps();
                        nw.endNodeNoNested();
                    }
                    nw.endNode();
                }

                nw.beginNode("Layer");
                nw.beginProps(); nw.addPropI32(0); nw.endProps();
                {
                    nw.beginNode("Version");
                    nw.beginProps(); nw.addPropI32(100); nw.endProps();
                    nw.endNodeNoNested();

                    nw.beginNode("LayerElement");
                    nw.beginProps(); nw.endProps();
                    {
                        nw.beginNode("Type");
                        nw.beginProps(); nw.addPropString("LayerElementNormal"); nw.endProps();
                        nw.endNodeNoNested();
                        nw.beginNode("TypedIndex");
                        nw.beginProps(); nw.addPropI32(0); nw.endProps();
                        nw.endNodeNoNested();
                    }
                    nw.endNode();

                    nw.beginNode("LayerElement");
                    nw.beginProps(); nw.endProps();
                    {
                        nw.beginNode("Type");
                        nw.beginProps(); nw.addPropString("LayerElementUV"); nw.endProps();
                        nw.endNodeNoNested();
                        nw.beginNode("TypedIndex");
                        nw.beginProps(); nw.addPropI32(0); nw.endProps();
                        nw.endNodeNoNested();
                    }
                    nw.endNode();

                    if (mesh.materialIndex >= 0) {
                        nw.beginNode("LayerElement");
                        nw.beginProps(); nw.endProps();
                        {
                            nw.beginNode("Type");
                            nw.beginProps(); nw.addPropString("LayerElementMaterial"); nw.endProps();
                            nw.endNodeNoNested();
                            nw.beginNode("TypedIndex");
                            nw.beginProps(); nw.addPropI32(0); nw.endProps();
                            nw.endNodeNoNested();
                        }
                        nw.endNode();
                    }
                }
                nw.endNode();
            }
            nw.endNode();

            nw.beginNode("Model");
            nw.beginProps();
            nw.addPropI64(getModelID(mi));
            nw.addPropString("Model::" + meshName + "\x00\x01Mesh");
            nw.addPropString("Mesh");
            nw.endProps();
            {
                nw.beginNode("Version");
                nw.beginProps(); nw.addPropI32(232); nw.endProps();
                nw.endNodeNoNested();

                nw.beginNode("Properties70");
                nw.beginProps(); nw.endProps();
                nw.endNode();
            }
            nw.endNode();
        }

        if (hasSkeleton) {
            for (size_t bi = 0; bi < model.skeleton.bones.size(); bi++) {
                const auto& bone = model.skeleton.bones[bi];

                nw.beginNode("Model");
                nw.beginProps();
                nw.addPropI64(getBoneID(bi));
                nw.addPropString("Model::" + bone.name + "\x00\x01LimbNode");
                nw.addPropString("LimbNode");
                nw.endProps();
                {
                    nw.beginNode("Version");
                    nw.beginProps(); nw.addPropI32(232); nw.endProps();
                    nw.endNodeNoNested();

                    nw.beginNode("Properties70");
                    nw.beginProps(); nw.endProps();
                    {
                        nw.beginNode("P");
                        nw.beginProps();
                        nw.addPropString("Lcl Translation"); nw.addPropString("Lcl Translation"); nw.addPropString(""); nw.addPropString("A");
                        nw.addPropF64(bone.posX); nw.addPropF64(bone.posY); nw.addPropF64(bone.posZ);
                        nw.endProps();
                        nw.endNodeNoNested();

                        double ex, ey, ez;
                        quatToEuler(bone.rotX, bone.rotY, bone.rotZ, bone.rotW, ex, ey, ez);
                        nw.beginNode("P");
                        nw.beginProps();
                        nw.addPropString("Lcl Rotation"); nw.addPropString("Lcl Rotation"); nw.addPropString(""); nw.addPropString("A");
                        nw.addPropF64(ex); nw.addPropF64(ey); nw.addPropF64(ez);
                        nw.endProps();
                        nw.endNodeNoNested();
                    }
                    nw.endNode();
                }
                nw.endNode();
            }

            for (size_t mi = 0; mi < model.meshes.size(); mi++) {
                const auto& mesh = model.meshes[mi];
                if (!mesh.hasSkinning || meshBoneSets[mi].empty()) continue;

                nw.beginNode("Deformer");
                nw.beginProps();
                nw.addPropI64(getSkinID(mi));
                nw.addPropString("Deformer::Skin\x00\x01Skin");
                nw.addPropString("Skin");
                nw.endProps();
                {
                    nw.beginNode("Version");
                    nw.beginProps(); nw.addPropI32(101); nw.endProps();
                    nw.endNodeNoNested();
                    nw.beginNode("Link_DeformAcuracy");
                    nw.beginProps(); nw.addPropF64(50.0); nw.endProps();
                    nw.endNodeNoNested();
                }
                nw.endNode();

                std::map<int, std::vector<std::pair<int, float>>> boneInfluences;
                for (size_t vIdx = 0; vIdx < mesh.vertices.size(); vIdx++) {
                    const auto& v = mesh.vertices[vIdx];
                    float totalWeight = 0;
                    for (int b = 0; b < 4; b++) totalWeight += v.boneWeights[b];

                    for (int b = 0; b < 4; b++) {
                        if (v.boneWeights[b] > 0.001f) {
                            int local = v.boneIndices[b];
                            int global = -1;
                            if (local >= 0) {
                                if (!mesh.bonesUsed.empty() && local < (int)mesh.bonesUsed.size()) {
                                    int used = mesh.bonesUsed[local];
                                    if (used >= 0 && used < (int)model.boneIndexArray.size()) {
                                        global = findBoneIndex(model.boneIndexArray[used]);
                                    }
                                } else if (local < (int)model.boneIndexArray.size()) {
                                    global = findBoneIndex(model.boneIndexArray[local]);
                                }
                            }
                            if (global != -1) {
                                float nw = (totalWeight > 0.0001f) ? v.boneWeights[b] / totalWeight : (b == 0 ? 1.0f : 0.0f);
                                boneInfluences[global].push_back({(int)vIdx, nw});
                            }
                        }
                    }
                }

                for (int boneIdx : meshBoneSets[mi]) {
                    const auto& infs = boneInfluences[boneIdx];

                    nw.beginNode("Deformer");
                    nw.beginProps();
                    nw.addPropI64(getClusterID(mi, boneIdx));
                    nw.addPropString("SubDeformer::Cluster\x00\x01Cluster");
                    nw.addPropString("Cluster");
                    nw.endProps();
                    {
                        nw.beginNode("Version");
                        nw.beginProps(); nw.addPropI32(100); nw.endProps();
                        nw.endNodeNoNested();

                        std::vector<int32_t> indices;
                        std::vector<double> weights;
                        for (const auto& inf : infs) {
                            indices.push_back(inf.first);
                            weights.push_back(inf.second);
                        }

                        nw.beginNode("Indexes");
                        nw.beginProps(); nw.addPropI32Array(indices); nw.endProps();
                        nw.endNodeNoNested();

                        nw.beginNode("Weights");
                        nw.beginProps(); nw.addPropF64Array(weights); nw.endProps();
                        nw.endNodeNoNested();

                        std::vector<double> transform = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
                        nw.beginNode("Transform");
                        nw.beginProps(); nw.addPropF64Array(transform); nw.endProps();
                        nw.endNodeNoNested();

                        std::vector<double> transformLink;
                        const Mat4& m = globalBoneTransforms[boneIdx];
                        for (int i = 0; i < 16; i++) transformLink.push_back(m.m[i]);
                        nw.beginNode("TransformLink");
                        nw.beginProps(); nw.addPropF64Array(transformLink); nw.endProps();
                        nw.endNodeNoNested();
                    }
                    nw.endNode();
                }
            }
        }

        for (size_t mi = 0; mi < model.materials.size(); mi++) {
            const auto& mat = model.materials[mi];

            nw.beginNode("Material");
            nw.beginProps();
            nw.addPropI64(getMaterialID(mi));
            nw.addPropString("Material::" + mat.name + "\x00\x01");
            nw.addPropString("");
            nw.endProps();
            {
                nw.beginNode("Version");
                nw.beginProps(); nw.addPropI32(102); nw.endProps();
                nw.endNodeNoNested();
                nw.beginNode("ShadingModel");
                nw.beginProps(); nw.addPropString("phong"); nw.endProps();
                nw.endNodeNoNested();
                nw.beginNode("MultiLayer");
                nw.beginProps(); nw.addPropI32(0); nw.endProps();
                nw.endNodeNoNested();

                nw.beginNode("Properties70");
                nw.beginProps(); nw.endProps();
                {
                    nw.beginNode("P");
                    nw.beginProps();
                    nw.addPropString("DiffuseColor"); nw.addPropString("Color"); nw.addPropString(""); nw.addPropString("A");
                    nw.addPropF64(0.8); nw.addPropF64(0.8); nw.addPropF64(0.8);
                    nw.endProps();
                    nw.endNodeNoNested();
                }
                nw.endNode();
            }
            nw.endNode();

            if (!mat.diffuseData.empty() && mat.diffuseWidth > 0 && mat.diffuseHeight > 0) {
                int w = mat.diffuseWidth, h = mat.diffuseHeight;
                std::vector<uint8_t> png;

                std::string matNameLower = mat.name;
                std::transform(matNameLower.begin(), matNameLower.end(), matNameLower.begin(), ::tolower);
                bool isHairMaterial = (matNameLower.find("_har_") != std::string::npos ||
                                       matNameLower.find("hair") != std::string::npos ||
                                       matNameLower.find("_ubm_") != std::string::npos ||
                                       matNameLower.find("_ulm_") != std::string::npos) &&
                                      matNameLower.find("bld") == std::string::npos;

                std::vector<uint8_t> exportData = mat.diffuseData;
                if (isHairMaterial) {
                    for (size_t i = 3; i < exportData.size(); i += 4) {
                        exportData[i] = 255;
                    }
                }

                uint32_t crc_table[256];
                for (int n = 0; n < 256; n++) {
                    uint32_t c = n;
                    for (int k = 0; k < 8; k++) c = (c & 1) ? 0xedb88320 ^ (c >> 1) : c >> 1;
                    crc_table[n] = c;
                }
                auto crc32 = [&](const uint8_t* data, size_t len) {
                    uint32_t c = 0xffffffff;
                    for (size_t i = 0; i < len; i++) c = crc_table[(c ^ data[i]) & 0xff] ^ (c >> 8);
                    return c ^ 0xffffffff;
                };
                auto write32be = [](std::vector<uint8_t>& v, uint32_t val) {
                    v.push_back((val >> 24) & 0xff); v.push_back((val >> 16) & 0xff);
                    v.push_back((val >> 8) & 0xff); v.push_back(val & 0xff);
                };
                auto writeChunk = [&](const char* type, const std::vector<uint8_t>& data) {
                    write32be(png, (uint32_t)data.size());
                    png.insert(png.end(), type, type + 4);
                    png.insert(png.end(), data.begin(), data.end());
                    std::vector<uint8_t> forCrc(type, type + 4);
                    forCrc.insert(forCrc.end(), data.begin(), data.end());
                    write32be(png, crc32(forCrc.data(), forCrc.size()));
                };

                png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

                std::vector<uint8_t> ihdr(13);
                ihdr[0] = (w >> 24) & 0xff; ihdr[1] = (w >> 16) & 0xff; ihdr[2] = (w >> 8) & 0xff; ihdr[3] = w & 0xff;
                ihdr[4] = (h >> 24) & 0xff; ihdr[5] = (h >> 16) & 0xff; ihdr[6] = (h >> 8) & 0xff; ihdr[7] = h & 0xff;
                ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
                writeChunk("IHDR", ihdr);

                std::vector<uint8_t> raw;
                for (int y = 0; y < h; y++) {
                    raw.push_back(0);
                    raw.insert(raw.end(), exportData.begin() + y * w * 4, exportData.begin() + (y + 1) * w * 4);
                }

                std::vector<uint8_t> deflated;
                deflated.push_back(0x78); deflated.push_back(0x01);
                size_t pos = 0;
                while (pos < raw.size()) {
                    size_t remain = raw.size() - pos;
                    size_t blockLen = remain > 65535 ? 65535 : remain;
                    bool last = (pos + blockLen >= raw.size());
                    deflated.push_back(last ? 1 : 0);
                    deflated.push_back(blockLen & 0xff);
                    deflated.push_back((blockLen >> 8) & 0xff);
                    deflated.push_back(~blockLen & 0xff);
                    deflated.push_back((~blockLen >> 8) & 0xff);
                    deflated.insert(deflated.end(), raw.begin() + pos, raw.begin() + pos + blockLen);
                    pos += blockLen;
                }
                uint32_t adler = 1;
                for (size_t i = 0; i < raw.size(); i++) {
                    uint32_t s1 = adler & 0xffff, s2 = (adler >> 16) & 0xffff;
                    s1 = (s1 + raw[i]) % 65521;
                    s2 = (s2 + s1) % 65521;
                    adler = (s2 << 16) | s1;
                }
                deflated.push_back((adler >> 24) & 0xff);
                deflated.push_back((adler >> 16) & 0xff);
                deflated.push_back((adler >> 8) & 0xff);
                deflated.push_back(adler & 0xff);
                writeChunk("IDAT", deflated);
                writeChunk("IEND", {});

                nw.beginNode("Video");
                nw.beginProps();
                nw.addPropI64(getVideoID(mi));
                nw.addPropString("Video::" + mat.name + "\x00\x01Clip");
                nw.addPropString("Clip");
                nw.endProps();
                {
                    nw.beginNode("Type");
                    nw.beginProps(); nw.addPropString("Clip"); nw.endProps();
                    nw.endNodeNoNested();
                    nw.beginNode("Content");
                    nw.beginProps(); nw.addPropRawBytes(png.data(), png.size()); nw.endProps();
                    nw.endNodeNoNested();
                }
                nw.endNode();

                nw.beginNode("Texture");
                nw.beginProps();
                nw.addPropI64(getTextureID(mi));
                nw.addPropString("Texture::" + mat.name + "\x00\x01");
                nw.addPropString("");
                nw.endProps();
                {
                    nw.beginNode("Type");
                    nw.beginProps(); nw.addPropString("TextureVideoClip"); nw.endProps();
                    nw.endNodeNoNested();
                    nw.beginNode("Version");
                    nw.beginProps(); nw.addPropI32(202); nw.endProps();
                    nw.endNodeNoNested();
                    nw.beginNode("TextureName");
                    nw.beginProps(); nw.addPropString("Texture::" + mat.name); nw.endProps();
                    nw.endNodeNoNested();
                    nw.beginNode("Media");
                    nw.beginProps(); nw.addPropString("Video::" + mat.name); nw.endProps();
                    nw.endNodeNoNested();
                    nw.beginNode("FileName");
                    nw.beginProps(); nw.addPropString(mat.name + ".png"); nw.endProps();
                    nw.endNodeNoNested();
                    nw.beginNode("RelativeFilename");
                    nw.beginProps(); nw.addPropString(mat.name + ".png"); nw.endProps();
                    nw.endNodeNoNested();
                }
                nw.endNode();
            }
        }
    }
    nw.endNode();

    nw.beginNode("Connections");
    nw.beginProps(); nw.endProps();
    {
        auto addConn = [&](const std::string& type, int64_t child, int64_t parent) {
            nw.beginNode("C");
            nw.beginProps();
            nw.addPropString(type);
            nw.addPropI64(child);
            nw.addPropI64(parent);
            nw.endProps();
            nw.endNodeNoNested();
        };
        auto addConnProp = [&](const std::string& type, int64_t child, int64_t parent, const std::string& prop) {
            nw.beginNode("C");
            nw.beginProps();
            nw.addPropString(type);
            nw.addPropI64(child);
            nw.addPropI64(parent);
            nw.addPropString(prop);
            nw.endProps();
            nw.endNodeNoNested();
        };

        addConn("OO", 100000, 0);

        for (size_t i = 0; i < model.meshes.size(); i++) {
            addConn("OO", getModelID(i), 100000);
            addConn("OO", getGeoID(i), getModelID(i));
        }

        if (hasSkeleton) {
            for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
                int parentIdx = model.skeleton.bones[i].parentIndex;
                if (parentIdx >= 0) {
                    addConn("OO", getBoneID(i), getBoneID(parentIdx));
                } else {
                    addConn("OO", getBoneID(i), 100000);
                }
            }

            for (size_t mi = 0; mi < model.meshes.size(); mi++) {
                if (!model.meshes[mi].hasSkinning || meshBoneSets[mi].empty()) continue;
                addConn("OO", getSkinID(mi), getGeoID(mi));
                for (int boneIdx : meshBoneSets[mi]) {
                    addConn("OO", getClusterID(mi, boneIdx), getSkinID(mi));
                    addConn("OO", getBoneID(boneIdx), getClusterID(mi, boneIdx));
                }
            }
        }

        for (size_t mi = 0; mi < model.meshes.size(); mi++) {
            int matIdx = model.meshes[mi].materialIndex;
            if (matIdx >= 0 && matIdx < (int)model.materials.size()) {
                addConn("OO", getMaterialID(matIdx), getModelID(mi));
            }
        }

        for (size_t mi = 0; mi < model.materials.size(); mi++) {
            const auto& mat = model.materials[mi];
            if (!mat.diffuseData.empty() && mat.diffuseWidth > 0 && mat.diffuseHeight > 0) {
                addConnProp("OP", getTextureID(mi), getMaterialID(mi), "DiffuseColor");
                addConn("OO", getVideoID(mi), getTextureID(mi));
            }
        }
    }
    nw.endNode();

    for (int i = 0; i < 13; i++) output.push_back(0);

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(output.data()), output.size());
    return out.good();
}