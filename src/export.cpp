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

static void generateBoxMesh(float hx, float hy, float hz,
                            std::vector<float>& outVerts, std::vector<uint32_t>& outIndices) {
    float v[8][3] = {
        {-hx, -hy, -hz}, { hx, -hy, -hz}, { hx,  hy, -hz}, {-hx,  hy, -hz},
        {-hx, -hy,  hz}, { hx, -hy,  hz}, { hx,  hy,  hz}, {-hx,  hy,  hz}
    };
    for (int i = 0; i < 8; i++) {
        outVerts.push_back(v[i][0]);
        outVerts.push_back(v[i][1]);
        outVerts.push_back(v[i][2]);
    }
    uint32_t idx[] = {
        0,1,2, 0,2,3,
        4,6,5, 4,7,6,
        0,4,5, 0,5,1,
        2,6,7, 2,7,3,
        0,3,7, 0,7,4,
        1,5,6, 1,6,2
    };
    for (int i = 0; i < 36; i++) outIndices.push_back(idx[i]);
}

static void generateSphereMesh(float radius, std::vector<float>& outVerts, std::vector<uint32_t>& outIndices) {
    const int segments = 16;
    const int rings = 8;
    outVerts.push_back(0); outVerts.push_back(0); outVerts.push_back(radius);
    for (int r = 1; r < rings; r++) {
        float phi = 3.14159f * r / rings;
        float z = radius * std::cos(phi);
        float ringRadius = radius * std::sin(phi);
        for (int s = 0; s < segments; s++) {
            float theta = 2.0f * 3.14159f * s / segments;
            outVerts.push_back(ringRadius * std::cos(theta));
            outVerts.push_back(ringRadius * std::sin(theta));
            outVerts.push_back(z);
        }
    }
    outVerts.push_back(0); outVerts.push_back(0); outVerts.push_back(-radius);
    for (int s = 0; s < segments; s++) {
        outIndices.push_back(0);
        outIndices.push_back(1 + s);
        outIndices.push_back(1 + (s + 1) % segments);
    }
    for (int r = 0; r < rings - 2; r++) {
        int ringStart = 1 + r * segments;
        int nextRingStart = 1 + (r + 1) * segments;
        for (int s = 0; s < segments; s++) {
            int s1 = (s + 1) % segments;
            outIndices.push_back(ringStart + s);
            outIndices.push_back(nextRingStart + s);
            outIndices.push_back(nextRingStart + s1);
            outIndices.push_back(ringStart + s);
            outIndices.push_back(nextRingStart + s1);
            outIndices.push_back(ringStart + s1);
        }
    }
    int lastVert = 1 + (rings - 1) * segments;
    int lastRingStart = 1 + (rings - 2) * segments;
    for (int s = 0; s < segments; s++) {
        outIndices.push_back(lastVert);
        outIndices.push_back(lastRingStart + (s + 1) % segments);
        outIndices.push_back(lastRingStart + s);
    }
}

static void generateCapsuleMesh(float radius, float height,
                                std::vector<float>& outVerts, std::vector<uint32_t>& outIndices) {
    const int segments = 16;
    const int capRings = 4;
    float halfHeight = height / 2.0f;
    outVerts.push_back(0); outVerts.push_back(0); outVerts.push_back(halfHeight + radius);
    for (int r = 1; r <= capRings; r++) {
        float phi = 3.14159f / 2.0f * r / capRings;
        float z = halfHeight + radius * std::cos(phi);
        float ringRadius = radius * std::sin(phi);
        for (int s = 0; s < segments; s++) {
            float theta = 2.0f * 3.14159f * s / segments;
            outVerts.push_back(ringRadius * std::cos(theta));
            outVerts.push_back(ringRadius * std::sin(theta));
            outVerts.push_back(z);
        }
    }
    for (int r = 0; r < capRings; r++) {
        float phi = 3.14159f / 2.0f + 3.14159f / 2.0f * r / capRings;
        float z = -halfHeight + radius * std::cos(phi);
        float ringRadius = radius * std::sin(phi);
        for (int s = 0; s < segments; s++) {
            float theta = 2.0f * 3.14159f * s / segments;
            outVerts.push_back(ringRadius * std::cos(theta));
            outVerts.push_back(ringRadius * std::sin(theta));
            outVerts.push_back(z);
        }
    }
    outVerts.push_back(0); outVerts.push_back(0); outVerts.push_back(-halfHeight - radius);
    int totalRings = capRings * 2;
    for (int s = 0; s < segments; s++) {
        outIndices.push_back(0);
        outIndices.push_back(1 + s);
        outIndices.push_back(1 + (s + 1) % segments);
    }
    for (int r = 0; r < totalRings - 1; r++) {
        int ringStart = 1 + r * segments;
        int nextRingStart = 1 + (r + 1) * segments;
        for (int s = 0; s < segments; s++) {
            int s1 = (s + 1) % segments;
            outIndices.push_back(ringStart + s);
            outIndices.push_back(nextRingStart + s);
            outIndices.push_back(nextRingStart + s1);
            outIndices.push_back(ringStart + s);
            outIndices.push_back(nextRingStart + s1);
            outIndices.push_back(ringStart + s1);
        }
    }
    int lastVert = 1 + totalRings * segments;
    int lastRingStart = 1 + (totalRings - 1) * segments;
    for (int s = 0; s < segments; s++) {
        outIndices.push_back(lastVert);
        outIndices.push_back(lastRingStart + (s + 1) % segments);
        outIndices.push_back(lastRingStart + s);
    }
}

static void transformVerts(std::vector<float>& verts, float px, float py, float pz,
                           float qx, float qy, float qz, float qw) {
    for (size_t i = 0; i < verts.size(); i += 3) {
        float x = verts[i], y = verts[i+1], z = verts[i+2];
        float tx = 2.0f * (qy * z - qz * y);
        float ty = 2.0f * (qz * x - qx * z);
        float tz = 2.0f * (qx * y - qy * x);
        float nx = x + qw * tx + (qy * tz - qz * ty);
        float ny = y + qw * ty + (qz * tx - qx * tz);
        float nz = z + qw * tz + (qx * ty - qy * tx);
        verts[i] = nx + px;
        verts[i+1] = ny + py;
        verts[i+2] = nz + pz;
    }
}

bool exportToGLB(const Model& model, const std::vector<Animation>& animations, const std::string& outputPath, const ExportOptions& options) {
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
    struct CollisionMeshData { std::string name; int posAcc, idxAcc, boneIdx; };
    std::vector<CollisionMeshData> collisionExports;
    if (options.includeCollision && !model.collisionShapes.empty()) {
        std::string baseName = model.name;
        size_t dotPos = baseName.rfind('.');
        if (dotPos != std::string::npos) baseName = baseName.substr(0, dotPos);
        int collisionCount = 0;
        for (const auto& shape : model.collisionShapes) {
            std::vector<float> verts;
            std::vector<uint32_t> indices;
            switch (shape.type) {
                case CollisionShapeType::Box:
                    generateBoxMesh(shape.boxX, shape.boxY, shape.boxZ, verts, indices);
                    break;
                case CollisionShapeType::Sphere:
                    generateSphereMesh(shape.radius, verts, indices);
                    break;
                case CollisionShapeType::Capsule:
                    generateCapsuleMesh(shape.radius, shape.height, verts, indices);
                    break;
                case CollisionShapeType::Mesh:
                    verts = shape.meshVerts;
                    indices = shape.meshIndices;
                    break;
            }
            if (verts.empty() || indices.empty()) continue;
            if (!shape.meshVertsWorldSpace) {
                transformVerts(verts, shape.posX, shape.posY, shape.posZ,
                              shape.rotX, shape.rotY, shape.rotZ, shape.rotW);
            }
            std::string collisionName;
            int boneIdx = -1;
            if (!shape.boneName.empty()) {
                collisionName = "UCX_" + baseName + "_" + shape.boneName;
                boneIdx = findSkeletonBone(model.skeleton, shape.boneName);
            } else if (!shape.name.empty()) {
                collisionName = "UCX_" + baseName + "_" + shape.name;
            } else {
                collisionName = "UCX_" + baseName + "_" + std::to_string(collisionCount);
            }
            collisionCount++;
            float minPos[3] = {1e30f, 1e30f, 1e30f};
            float maxPos[3] = {-1e30f, -1e30f, -1e30f};
            for (size_t i = 0; i < verts.size(); i += 3) {
                if (verts[i] < minPos[0]) minPos[0] = verts[i];
                if (verts[i+1] < minPos[1]) minPos[1] = verts[i+1];
                if (verts[i+2] < minPos[2]) minPos[2] = verts[i+2];
                if (verts[i] > maxPos[0]) maxPos[0] = verts[i];
                if (verts[i+1] > maxPos[1]) maxPos[1] = verts[i+1];
                if (verts[i+2] > maxPos[2]) maxPos[2] = verts[i+2];
            }
            size_t posOff = binBuffer.size();
            for (size_t i = 0; i < verts.size(); i++) {
                writeFloat(binBuffer, verts[i]);
            }
            int posView = (int)bufferViews.size();
            bufferViews.push_back({posOff, binBuffer.size() - posOff, TARGET_ARRAY_BUFFER});
            int posAcc = (int)accessors.size();
            accessors.push_back({posView, COMPONENT_TYPE_FLOAT, (int)(verts.size() / 3), "VEC3",
                                {minPos[0], minPos[1], minPos[2]}, {maxPos[0], maxPos[1], maxPos[2]}, 3});
            size_t idxOff = binBuffer.size();
            bool use32bit = (verts.size() / 3) > 65535;
            for (uint32_t idx : indices) {
                if (use32bit) writeU32(binBuffer, idx);
                else writeU16(binBuffer, (uint16_t)idx);
            }
            padTo4(binBuffer);
            int idxView = (int)bufferViews.size();
            bufferViews.push_back({idxOff, binBuffer.size() - idxOff, TARGET_ELEMENT_ARRAY_BUFFER});
            int idxAcc = (int)accessors.size();
            accessors.push_back({idxView, use32bit ? COMPONENT_TYPE_UNSIGNED_INT : COMPONENT_TYPE_UNSIGNED_SHORT,
                                (int)indices.size(), "SCALAR", {}, {}, 0});
            collisionExports.push_back({collisionName, posAcc, idxAcc, boneIdx});
        }
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
            std::string boneNameLower = model.skeleton.bones[boneIdx].name;
            std::transform(boneNameLower.begin(), boneNameLower.end(), boneNameLower.begin(), ::tolower);
            bool isGodBone = (boneNameLower == "god" || boneNameLower == "gob");
            if (track.isTranslation && isGodBone) {
                continue;
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
                const Bone& bone = model.skeleton.bones[boneIdx];
                float baseX = bone.posX, baseY = bone.posY, baseZ = bone.posZ;
                for (const auto& kf : track.keyframes) {
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
    struct MaterialTextures { int diffuse = -1; int normal = -1; int specular = -1; int tint = -1; };
    std::vector<MaterialTextures> materialTextures;
    auto encodePNGToBuffer = [&](const std::vector<uint8_t>& rgbaData, int w, int h, bool forceOpaqueAlpha) -> int {
        if (rgbaData.empty() || w <= 0 || h <= 0) return -1;
        size_t imgOff = binBuffer.size();
        std::vector<uint8_t> png;
        std::vector<uint8_t> exportData = rgbaData;
        if (forceOpaqueAlpha) {
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
        int texIdx = (int)textures.size();
        textures.push_back({imgIdx});
        return texIdx;
    };
    for (size_t mi = 0; mi < model.materials.size(); mi++) {
        const auto& mat = model.materials[mi];
        MaterialTextures mtex;
        std::string matNameLower = mat.name;
        std::transform(matNameLower.begin(), matNameLower.end(), matNameLower.begin(), ::tolower);
        bool isHairMaterial = (matNameLower.find("_har_") != std::string::npos ||
                               matNameLower.find("hair") != std::string::npos ||
                               matNameLower.find("_ubm_") != std::string::npos ||
                               matNameLower.find("_ulm_") != std::string::npos) &&
                              matNameLower.find("bld") == std::string::npos;
        if (!mat.diffuseData.empty() && mat.diffuseWidth > 0 && mat.diffuseHeight > 0) {
            mtex.diffuse = encodePNGToBuffer(mat.diffuseData, mat.diffuseWidth, mat.diffuseHeight, isHairMaterial);
        }
        if (!mat.normalData.empty() && mat.normalWidth > 0 && mat.normalHeight > 0) {
            mtex.normal = encodePNGToBuffer(mat.normalData, mat.normalWidth, mat.normalHeight, false);
        }
        if (!mat.specularData.empty() && mat.specularWidth > 0 && mat.specularHeight > 0) {
            mtex.specular = encodePNGToBuffer(mat.specularData, mat.specularWidth, mat.specularHeight, false);
        }
        if (!mat.tintData.empty() && mat.tintWidth > 0 && mat.tintHeight > 0) {
            mtex.tint = encodePNGToBuffer(mat.tintData, mat.tintWidth, mat.tintHeight, false);
        }
        materialTextures.push_back(mtex);
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
    for (size_t i = 0; i < collisionExports.size(); i++) {
        if (collisionExports[i].boneIdx < 0) {
            json += "," + std::to_string(model.meshes.size() + 1 + i);
        }
    }
    if (hasSkeleton) {
        for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
            if (model.skeleton.bones[i].parentIndex < 0) {
                json += "," + std::to_string(model.meshes.size() + 1 + collisionExports.size() + i);
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
    for (size_t i = 0; i < collisionExports.size(); i++) {
        json += ",{\"name\":\"" + escapeJson(collisionExports[i].name) + "\"";
        json += ",\"mesh\":" + std::to_string(model.meshes.size() + i);
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
            for (size_t ci = 0; ci < collisionExports.size(); ci++) {
                if (collisionExports[ci].boneIdx == (int)i) {
                    children.push_back(-(int)(ci + 1));
                }
            }
            if (!children.empty()) {
                json += ",\"children\":[";
                bool first = true;
                for (int child : children) {
                    if (!first) json += ",";
                    first = false;
                    if (child < 0) {
                        json += std::to_string(model.meshes.size() + 1 + ((-child) - 1));
                    } else {
                        json += std::to_string(model.meshes.size() + 1 + collisionExports.size() + child);
                    }
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
    for (size_t i = 0; i < collisionExports.size(); i++) {
        json += ",{\"name\":\"" + escapeJson(collisionExports[i].name) + "\"";
        json += ",\"primitives\":[{\"attributes\":{\"POSITION\":" + std::to_string(collisionExports[i].posAcc);
        json += "},\"indices\":" + std::to_string(collisionExports[i].idxAcc);
        json += "}]}";
    }
    json += "],";
    if (hasSkeleton) {
        json += "\"skins\":[{\"inverseBindMatrices\":" + std::to_string(ibmAccessor) + ",\"joints\":[";
        for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
            if (i > 0) json += ",";
            json += std::to_string(model.meshes.size() + 1 + collisionExports.size() + i);
        }
        json += "]";
        for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
            if (model.skeleton.bones[i].parentIndex < 0) {
                json += ",\"skeleton\":" + std::to_string(model.meshes.size() + 1 + collisionExports.size() + i);
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
                json += ",\"target\":{\"node\":" + std::to_string(model.meshes.size() + 1 + collisionExports.size() + ae.channels[ci].first);
                json += ",\"path\":\"" + ae.channels[ci].second + "\"}}";
            }
            json += "]";
            json += "}";
        }
        json += "],";
    }
    std::set<int> alphaMaterialIndices;
    for (size_t mi = 0; mi < model.meshes.size(); mi++) {
        if (model.meshes[mi].alphaTest && model.meshes[mi].materialIndex >= 0)
            alphaMaterialIndices.insert(model.meshes[mi].materialIndex);
    }
    if (!model.materials.empty()) {
        json += "\"materials\":[";
        for (size_t i = 0; i < model.materials.size(); i++) {
            if (i > 0) json += ",";
            const auto& mat = model.materials[i];
            json += "{\"name\":\"" + escapeJson(mat.name) + "\"";
            json += ",\"pbrMetallicRoughness\":{\"metallicFactor\":0.0,\"roughnessFactor\":0.5";
            if (i < materialTextures.size() && materialTextures[i].diffuse >= 0) {
                json += ",\"baseColorTexture\":{\"index\":" + std::to_string(materialTextures[i].diffuse) + "}";
            }
            json += "}";
            if (i < materialTextures.size() && materialTextures[i].normal >= 0) {
                json += ",\"normalTexture\":{\"index\":" + std::to_string(materialTextures[i].normal) + ",\"scale\":1.0}";
            }
            bool hasExtras = (i < materialTextures.size()) &&
                (materialTextures[i].specular >= 0 || materialTextures[i].tint >= 0);
            if (hasExtras) {
                json += ",\"extras\":{";
                bool first = true;
                if (materialTextures[i].specular >= 0) {
                    json += "\"specularTexture\":{\"index\":" + std::to_string(materialTextures[i].specular) + "}";
                    first = false;
                }
                if (materialTextures[i].tint >= 0) {
                    if (!first) json += ",";
                    json += "\"tintTexture\":{\"index\":" + std::to_string(materialTextures[i].tint) + "}";
                }
                json += "}";
            }
            if (options.doubleSided) {
                json += ",\"doubleSided\":true";
            }
            if (alphaMaterialIndices.count((int)i)) {
                json += ",\"alphaMode\":\"MASK\",\"alphaCutoff\":0.5";
            }
            json += "}";
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

bool exportToFBX(const Model& model, const std::vector<Animation>& animations, const std::string& outputPath, const ExportOptions& options) {
    if (model.meshes.empty()) return false;
    std::vector<uint8_t> output;
    auto writeBytes = [&](const void* data, size_t len) {
        const uint8_t* p = (const uint8_t*)data;
        output.insert(output.end(), p, p + len);
    };
    const int64_t ROOT_MODEL_ID = 1000000LL;
    auto getGeoID = [](int i) -> int64_t { return 10000000LL + i; };
    auto getBoneID = [](int i) -> int64_t { return 20000000LL + i; };
    auto getModelID = [](int i) -> int64_t { return 30000000LL + i; };
    auto getSkinID = [](int i) -> int64_t { return 40000000LL + i; };
    auto getClusterID = [](int m, int b) -> int64_t { return 50000000LL + (int64_t)m * 10000 + b; };
    auto getMaterialID = [](int i) -> int64_t { return 60000000LL + i; };
    auto getTextureID = [](int i) -> int64_t { return 70000000LL + i; };
    auto getVideoID = [](int i) -> int64_t { return 80000000LL + i; };
    auto getAnimStackID = [](int i) -> int64_t { return 90000000LL + i; };
    auto getAnimLayerID = [](int i) -> int64_t { return 100000000LL + i; };
    auto getAnimCurveNodeID = [](int a, int t, int c) -> int64_t { return 110000000LL + (int64_t)a * 100000 + (int64_t)t * 1000 + c; };
    auto getAnimCurveID = [](int a, int t, int c, int axis) -> int64_t { return 120000000LL + (int64_t)a * 1000000 + (int64_t)t * 10000 + (int64_t)c * 100 + axis; };
    auto getNodeAttrID = [](int i) -> int64_t { return 130000000LL + i; };
    bool hasSkeleton = !model.skeleton.bones.empty() && options.includeArmature;
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
    std::vector<int> meshBoneMap;
    if (hasSkeleton && !model.boneIndexArray.empty()) {
        meshBoneMap.resize(model.boneIndexArray.size(), -1);
        for (size_t i = 0; i < model.boneIndexArray.size(); i++) {
            meshBoneMap[i] = findBoneIndex(model.boneIndexArray[i]);
        }
    }
    std::vector<bool> meshHasSkin(model.meshes.size(), false);
    std::vector<std::set<int>> meshBoneSets(model.meshes.size());
    std::vector<std::map<int, std::vector<std::pair<int, float>>>> meshBoneInfluences(model.meshes.size());
    if (hasSkeleton) {
        for (size_t mi = 0; mi < model.meshes.size(); mi++) {
            const auto& mesh = model.meshes[mi];
            if (!mesh.hasSkinning) continue;
            for (size_t vi = 0; vi < mesh.vertices.size(); vi++) {
                const auto& v = mesh.vertices[vi];
                float weightSum = 0;
                for (int i = 0; i < 4; i++) weightSum += v.boneWeights[i];
                if (weightSum < 0.0001f) continue;
                for (int i = 0; i < 4; i++) {
                    float weight = v.boneWeights[i] / weightSum;
                    if (weight < 0.0001f) continue;
                    int meshLocalIdx = v.boneIndices[i];
                    int globalIdx = meshLocalIdx;
                    if (!mesh.bonesUsed.empty() && meshLocalIdx >= 0 && meshLocalIdx < (int)mesh.bonesUsed.size()) {
                        globalIdx = mesh.bonesUsed[meshLocalIdx];
                    }
                    int skelIdx = -1;
                    if (globalIdx >= 0 && globalIdx < (int)meshBoneMap.size()) {
                        skelIdx = meshBoneMap[globalIdx];
                    }
                    if (skelIdx >= 0) {
                        meshHasSkin[mi] = true;
                        meshBoneSets[mi].insert(skelIdx);
                        meshBoneInfluences[mi][skelIdx].push_back({(int)vi, weight});
                    }
                }
            }
        }
    }
    enum class TexType { Diffuse, Normal, Specular, Tint };
    struct TextureEntry {
        int materialIndex;
        TexType type;
        std::string suffix;
        std::vector<uint8_t> pngData;
    };
    std::vector<TextureEntry> textureEntries;
    struct MaterialTexIndices { int diffuse = -1; int normal = -1; int specular = -1; int tint = -1; };
    std::vector<MaterialTexIndices> materialTexIndices(model.materials.size());
    auto encodePNG = [](const std::vector<uint8_t>& rgba, int w, int h, bool forceOpaqueAlpha) -> std::vector<uint8_t> {
        std::vector<uint8_t> png;
        std::vector<uint8_t> exportData = rgba;
        if (forceOpaqueAlpha) {
            for (size_t i = 3; i < exportData.size(); i += 4) exportData[i] = 255;
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
            deflated.push_back(blockLen & 0xff); deflated.push_back((blockLen >> 8) & 0xff);
            deflated.push_back(~blockLen & 0xff); deflated.push_back((~blockLen >> 8) & 0xff);
            deflated.insert(deflated.end(), raw.begin() + pos, raw.begin() + pos + blockLen);
            pos += blockLen;
        }
        uint32_t adler = 1;
        for (size_t i = 0; i < raw.size(); i++) {
            uint32_t s1 = adler & 0xffff, s2 = (adler >> 16) & 0xffff;
            s1 = (s1 + raw[i]) % 65521; s2 = (s2 + s1) % 65521;
            adler = (s2 << 16) | s1;
        }
        deflated.push_back((adler >> 24) & 0xff); deflated.push_back((adler >> 16) & 0xff);
        deflated.push_back((adler >> 8) & 0xff); deflated.push_back(adler & 0xff);
        writeChunk("IDAT", deflated);
        writeChunk("IEND", {});
        return png;
    };
    for (size_t mi = 0; mi < model.materials.size(); mi++) {
        const auto& mat = model.materials[mi];
        std::string matNameLower = mat.name;
        std::transform(matNameLower.begin(), matNameLower.end(), matNameLower.begin(), ::tolower);
        bool isHairMaterial = (matNameLower.find("_har_") != std::string::npos ||
                               matNameLower.find("hair") != std::string::npos ||
                               matNameLower.find("_ubm_") != std::string::npos ||
                               matNameLower.find("_ulm_") != std::string::npos) &&
                              matNameLower.find("bld") == std::string::npos;
        if (!mat.diffuseData.empty() && mat.diffuseWidth > 0 && mat.diffuseHeight > 0) {
            TextureEntry te;
            te.materialIndex = (int)mi;
            te.type = TexType::Diffuse;
            te.suffix = "_d";
            te.pngData = encodePNG(mat.diffuseData, mat.diffuseWidth, mat.diffuseHeight, isHairMaterial);
            materialTexIndices[mi].diffuse = (int)textureEntries.size();
            textureEntries.push_back(std::move(te));
        }
        if (!mat.normalData.empty() && mat.normalWidth > 0 && mat.normalHeight > 0) {
            TextureEntry te;
            te.materialIndex = (int)mi;
            te.type = TexType::Normal;
            te.suffix = "_n";
            te.pngData = encodePNG(mat.normalData, mat.normalWidth, mat.normalHeight, false);
            materialTexIndices[mi].normal = (int)textureEntries.size();
            textureEntries.push_back(std::move(te));
        }
        if (!mat.specularData.empty() && mat.specularWidth > 0 && mat.specularHeight > 0) {
            TextureEntry te;
            te.materialIndex = (int)mi;
            te.type = TexType::Specular;
            te.suffix = "_s";
            te.pngData = encodePNG(mat.specularData, mat.specularWidth, mat.specularHeight, false);
            materialTexIndices[mi].specular = (int)textureEntries.size();
            textureEntries.push_back(std::move(te));
        }
        if (!mat.tintData.empty() && mat.tintWidth > 0 && mat.tintHeight > 0) {
            TextureEntry te;
            te.materialIndex = (int)mi;
            te.type = TexType::Tint;
            te.suffix = "_t";
            te.pngData = encodePNG(mat.tintData, mat.tintWidth, mat.tintHeight, false);
            materialTexIndices[mi].tint = (int)textureEntries.size();
            textureEntries.push_back(std::move(te));
        }
    }
    struct AnimExportData {
        std::string name;
        float duration;
        struct Track {
            int boneIdx;
            bool isRotation;
            bool isTranslation;
            std::vector<float> times;
            std::vector<float> valuesX, valuesY, valuesZ;
        };
        std::vector<Track> tracks;
    };
    std::vector<AnimExportData> animExports;
    for (const auto& anim : animations) {
        if (anim.tracks.empty()) continue;
        AnimExportData ae;
        ae.name = anim.name;
        ae.duration = 0;
        for (const auto& track : anim.tracks) {
            int boneIdx = findBoneIndex(track.boneName);
            if (boneIdx < 0) continue;
            if (track.keyframes.empty()) continue;
            std::string boneNameLower = model.skeleton.bones[boneIdx].name;
            std::transform(boneNameLower.begin(), boneNameLower.end(), boneNameLower.begin(), ::tolower);
            bool isGodBone = (boneNameLower == "god" || boneNameLower == "gob");
            if (track.isTranslation && isGodBone) continue;
            AnimExportData::Track t;
            t.boneIdx = boneIdx;
            t.isRotation = track.isRotation;
            t.isTranslation = track.isTranslation;
            for (const auto& kf : track.keyframes) {
                t.times.push_back(kf.time);
                if (kf.time > ae.duration) ae.duration = kf.time;
                if (track.isRotation) {
                    float qx = kf.x, qy = kf.y, qz = kf.z, qw = kf.w;
                    float sinr_cosp = 2 * (qw * qx + qy * qz);
                    float cosr_cosp = 1 - 2 * (qx * qx + qy * qy);
                    float ex = std::atan2(sinr_cosp, cosr_cosp) * 180.0f / 3.14159265358979323846f;
                    float sinp = 2 * (qw * qy - qz * qx);
                    float ey = (std::abs(sinp) >= 1) ? std::copysign(90.0f, sinp) : std::asin(sinp) * 180.0f / 3.14159265358979323846f;
                    float siny_cosp = 2 * (qw * qz + qx * qy);
                    float cosy_cosp = 1 - 2 * (qy * qy + qz * qz);
                    float ez = std::atan2(siny_cosp, cosy_cosp) * 180.0f / 3.14159265358979323846f;
                    t.valuesX.push_back(ex);
                    t.valuesY.push_back(ey);
                    t.valuesZ.push_back(ez);
                } else if (track.isTranslation) {
                    const Bone& bone = model.skeleton.bones[boneIdx];
                    t.valuesX.push_back(bone.posX + kf.x);
                    t.valuesY.push_back(bone.posY + kf.y);
                    t.valuesZ.push_back(bone.posZ + kf.z);
                }
            }
            if (!t.times.empty()) ae.tracks.push_back(t);
        }
        if (!ae.tracks.empty()) animExports.push_back(ae);
    }
    int skinCount = 0, clusterCount = 0;
    for (size_t mi = 0; mi < model.meshes.size(); mi++) {
        if (meshHasSkin[mi]) {
            skinCount++;
            clusterCount += (int)meshBoneSets[mi].size();
        }
    }
    int textureCount = (int)textureEntries.size();
    int animStackCount = (int)animExports.size();
    int animLayerCount = animStackCount;
    int animCurveNodeCount = 0, animCurveCount = 0;
    for (const auto& ae : animExports) {
        animCurveNodeCount += (int)ae.tracks.size();
        animCurveCount += (int)ae.tracks.size() * 3;
    }
    auto fbxName = [](const std::string& name, const std::string& cls) -> std::string {
        std::string r = name;
        r.push_back('\x00');
        r.push_back('\x01');
        r += cls;
        return r;
    };

    struct NodeWriter {
        std::vector<uint8_t>& out;
        std::vector<size_t> nodeStack;
        NodeWriter(std::vector<uint8_t>& o) : out(o) {}
        void beginNode(const std::string& name) {
            nodeStack.push_back(out.size());
            for (int i = 0; i < 13; i++) out.push_back(0);
            out.insert(out.end(), name.begin(), name.end());
            out[nodeStack.back() + 12] = (uint8_t)name.size();
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
        void beginProps() { propStart = out.size(); propCount = 0; }
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
            for (int32_t v : arr) { uint32_t u = (uint32_t)v; out.push_back(u & 0xFF); out.push_back((u >> 8) & 0xFF); out.push_back((u >> 16) & 0xFF); out.push_back((u >> 24) & 0xFF); }
        }
        void addPropI64Array(const std::vector<int64_t>& arr) {
            out.push_back('l'); propCount++;
            uint32_t len = (uint32_t)arr.size();
            out.push_back(len & 0xFF); out.push_back((len >> 8) & 0xFF); out.push_back((len >> 16) & 0xFF); out.push_back((len >> 24) & 0xFF);
            out.push_back(0); out.push_back(0); out.push_back(0); out.push_back(0);
            uint32_t byteLen = len * 8;
            out.push_back(byteLen & 0xFF); out.push_back((byteLen >> 8) & 0xFF); out.push_back((byteLen >> 16) & 0xFF); out.push_back((byteLen >> 24) & 0xFF);
            for (int64_t v : arr) { uint64_t u = (uint64_t)v; for (int i = 0; i < 8; i++) out.push_back((u >> (i * 8)) & 0xFF); }
        }
        void addPropF32Array(const std::vector<float>& arr) {
            out.push_back('f'); propCount++;
            uint32_t len = (uint32_t)arr.size();
            out.push_back(len & 0xFF); out.push_back((len >> 8) & 0xFF); out.push_back((len >> 16) & 0xFF); out.push_back((len >> 24) & 0xFF);
            out.push_back(0); out.push_back(0); out.push_back(0); out.push_back(0);
            uint32_t byteLen = len * 4;
            out.push_back(byteLen & 0xFF); out.push_back((byteLen >> 8) & 0xFF); out.push_back((byteLen >> 16) & 0xFF); out.push_back((byteLen >> 24) & 0xFF);
            for (float v : arr) { uint32_t bits; std::memcpy(&bits, &v, 4); out.push_back(bits & 0xFF); out.push_back((bits >> 8) & 0xFF); out.push_back((bits >> 16) & 0xFF); out.push_back((bits >> 24) & 0xFF); }
        }
        void addPropF64Array(const std::vector<double>& arr) {
            out.push_back('d'); propCount++;
            uint32_t len = (uint32_t)arr.size();
            out.push_back(len & 0xFF); out.push_back((len >> 8) & 0xFF); out.push_back((len >> 16) & 0xFF); out.push_back((len >> 24) & 0xFF);
            out.push_back(0); out.push_back(0); out.push_back(0); out.push_back(0);
            uint32_t byteLen = len * 8;
            out.push_back(byteLen & 0xFF); out.push_back((byteLen >> 8) & 0xFF); out.push_back((byteLen >> 16) & 0xFF); out.push_back((byteLen >> 24) & 0xFF);
            for (double v : arr) { uint64_t bits; std::memcpy(&bits, &v, 8); for (int i = 0; i < 8; i++) out.push_back((bits >> (i * 8)) & 0xFF); }
        }
        void endProps() { uint32_t listLen = (uint32_t)(out.size() - propStart); setPropertyCount(propCount, listLen); }
    };
    auto quatToEuler = [](float qx, float qy, float qz, float qw, double& ex, double& ey, double& ez) {
        double sinr_cosp = 2.0 * (qw * qx + qy * qz);
        double cosr_cosp = 1.0 - 2.0 * (qx * qx + qy * qy);
        ex = std::atan2(sinr_cosp, cosr_cosp) * 180.0 / 3.14159265358979323846;
        double sinp = 2.0 * (qw * qy - qz * qx);
        if (std::abs(sinp) >= 1.0)
            ey = std::copysign(90.0, sinp);
        else
            ey = std::asin(sinp) * 180.0 / 3.14159265358979323846;
        double siny_cosp = 2.0 * (qw * qz + qx * qy);
        double cosy_cosp = 1.0 - 2.0 * (qy * qy + qz * qz);
        ez = std::atan2(siny_cosp, cosy_cosp) * 180.0 / 3.14159265358979323846;
    };
    const char* header = "Kaydara FBX Binary  ";
    writeBytes(header, 21);
    output.push_back(0x1A);
    output.push_back(0x00);
    uint32_t version = 7400;
    output.push_back(version & 0xFF); output.push_back((version >> 8) & 0xFF);
    output.push_back((version >> 16) & 0xFF); output.push_back((version >> 24) & 0xFF);
    NodeWriter nw(output);
    nw.beginNode("FBXHeaderExtension"); nw.beginProps(); nw.endProps();
    nw.beginNode("FBXHeaderVersion"); nw.beginProps(); nw.addPropI32(1003); nw.endProps(); nw.endNodeNoNested();
    nw.beginNode("FBXVersion"); nw.beginProps(); nw.addPropI32(7400); nw.endProps(); nw.endNodeNoNested();
    nw.beginNode("Creator"); nw.beginProps(); nw.addPropString("HavenTools"); nw.endProps(); nw.endNodeNoNested();
    nw.endNode();
    nw.beginNode("GlobalSettings"); nw.beginProps(); nw.endProps();
    nw.beginNode("Version"); nw.beginProps(); nw.addPropI32(1000); nw.endProps(); nw.endNodeNoNested();
    nw.beginNode("Properties70"); nw.beginProps(); nw.endProps();
    {
        auto addPI = [&](const std::string& name, const std::string& t1, const std::string& t2, const std::string& flags, int val) {
            nw.beginNode("P"); nw.beginProps(); nw.addPropString(name); nw.addPropString(t1); nw.addPropString(t2); nw.addPropString(flags); nw.addPropI32(val); nw.endProps(); nw.endNodeNoNested();
        };
        auto addPD = [&](const std::string& name, const std::string& t1, const std::string& t2, const std::string& flags, double val) {
            nw.beginNode("P"); nw.beginProps(); nw.addPropString(name); nw.addPropString(t1); nw.addPropString(t2); nw.addPropString(flags); nw.addPropF64(val); nw.endProps(); nw.endNodeNoNested();
        };
        addPI("UpAxis", "int", "Integer", "", 1);
        addPI("UpAxisSign", "int", "Integer", "", 1);
        addPI("FrontAxis", "int", "Integer", "", 2);
        addPI("FrontAxisSign", "int", "Integer", "", 1);
        addPI("CoordAxis", "int", "Integer", "", 0);
        addPI("CoordAxisSign", "int", "Integer", "", 1);
        addPI("OriginalUpAxis", "int", "Integer", "", -1);
        addPI("OriginalUpAxisSign", "int", "Integer", "", 1);
        addPD("UnitScaleFactor", "double", "Number", "", (double)options.fbxScale);
        addPD("OriginalUnitScaleFactor", "double", "Number", "", (double)options.fbxScale);
    }
    nw.endNode();
    nw.endNode();
    nw.beginNode("Documents"); nw.beginProps(); nw.endProps();
    nw.beginNode("Count"); nw.beginProps(); nw.addPropI32(1); nw.endProps(); nw.endNodeNoNested();
    nw.beginNode("Document"); nw.beginProps(); nw.addPropI64(1000000000LL); nw.addPropString(""); nw.addPropString("Scene"); nw.endProps();
    nw.beginNode("Properties70"); nw.beginProps(); nw.endProps();
    nw.beginNode("P"); nw.beginProps(); nw.addPropString("SourceObject"); nw.addPropString("object"); nw.addPropString(""); nw.addPropString(""); nw.endProps(); nw.endNodeNoNested();
    nw.beginNode("P"); nw.beginProps(); nw.addPropString("ActiveAnimStackName"); nw.addPropString("KString"); nw.addPropString(""); nw.addPropString("");
    nw.addPropString(animExports.empty() ? "" : animExports[0].name); nw.endProps(); nw.endNodeNoNested();
    nw.endNode();
    nw.beginNode("RootNode"); nw.beginProps(); nw.addPropI64(0); nw.endProps(); nw.endNodeNoNested();
    nw.endNode();
    nw.endNode();
    nw.beginNode("References"); nw.beginProps(); nw.endProps(); nw.endNode();
    nw.beginNode("Definitions"); nw.beginProps(); nw.endProps();
    nw.beginNode("Version"); nw.beginProps(); nw.addPropI32(100); nw.endProps(); nw.endNodeNoNested();
    int defCount = 2;
    if (!model.meshes.empty()) defCount++;
    if (!model.materials.empty()) defCount++;
    if (skinCount > 0) defCount++;
    if (textureCount > 0) defCount += 2;
    if (animStackCount > 0) defCount += 3;
    if (animCurveCount > 0) defCount++;
    if (hasSkeleton) defCount++;
    nw.beginNode("Count"); nw.beginProps(); nw.addPropI32(defCount); nw.endProps(); nw.endNodeNoNested();
    nw.beginNode("ObjectType"); nw.beginProps(); nw.addPropString("GlobalSettings"); nw.endProps();
    nw.beginNode("Count"); nw.beginProps(); nw.addPropI32(1); nw.endProps(); nw.endNodeNoNested();
    nw.endNode();
    int modelCount = 1 + (int)model.meshes.size() + (hasSkeleton ? (int)model.skeleton.bones.size() : 0);
    nw.beginNode("ObjectType"); nw.beginProps(); nw.addPropString("Model"); nw.endProps();
    nw.beginNode("Count"); nw.beginProps(); nw.addPropI32(modelCount); nw.endProps(); nw.endNodeNoNested();
    nw.endNode();
    if (hasSkeleton) {
        nw.beginNode("ObjectType"); nw.beginProps(); nw.addPropString("NodeAttribute"); nw.endProps();
        nw.beginNode("Count"); nw.beginProps(); nw.addPropI32((int)model.skeleton.bones.size()); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
    }
    if (!model.meshes.empty()) {
        nw.beginNode("ObjectType"); nw.beginProps(); nw.addPropString("Geometry"); nw.endProps();
        nw.beginNode("Count"); nw.beginProps(); nw.addPropI32((int)model.meshes.size()); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
    }
    if (!model.materials.empty()) {
        nw.beginNode("ObjectType"); nw.beginProps(); nw.addPropString("Material"); nw.endProps();
        nw.beginNode("Count"); nw.beginProps(); nw.addPropI32((int)model.materials.size()); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
    }
    if (skinCount > 0) {
        nw.beginNode("ObjectType"); nw.beginProps(); nw.addPropString("Deformer"); nw.endProps();
        nw.beginNode("Count"); nw.beginProps(); nw.addPropI32(skinCount + clusterCount); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
    }
    if (textureCount > 0) {
        nw.beginNode("ObjectType"); nw.beginProps(); nw.addPropString("Texture"); nw.endProps();
        nw.beginNode("Count"); nw.beginProps(); nw.addPropI32(textureCount); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
        nw.beginNode("ObjectType"); nw.beginProps(); nw.addPropString("Video"); nw.endProps();
        nw.beginNode("Count"); nw.beginProps(); nw.addPropI32(textureCount); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
    }
    if (animStackCount > 0) {
        nw.beginNode("ObjectType"); nw.beginProps(); nw.addPropString("AnimationStack"); nw.endProps();
        nw.beginNode("Count"); nw.beginProps(); nw.addPropI32(animStackCount); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
        nw.beginNode("ObjectType"); nw.beginProps(); nw.addPropString("AnimationLayer"); nw.endProps();
        nw.beginNode("Count"); nw.beginProps(); nw.addPropI32(animLayerCount); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
        nw.beginNode("ObjectType"); nw.beginProps(); nw.addPropString("AnimationCurveNode"); nw.endProps();
        nw.beginNode("Count"); nw.beginProps(); nw.addPropI32(animCurveNodeCount); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
    }
    if (animCurveCount > 0) {
        nw.beginNode("ObjectType"); nw.beginProps(); nw.addPropString("AnimationCurve"); nw.endProps();
        nw.beginNode("Count"); nw.beginProps(); nw.addPropI32(animCurveCount); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
    }
    nw.endNode();
    nw.beginNode("Objects"); nw.beginProps(); nw.endProps();
    std::string rootName = fbxName(model.name, "Model");
    nw.beginNode("Model"); nw.beginProps(); nw.addPropI64(ROOT_MODEL_ID); nw.addPropString(rootName); nw.addPropString("Null"); nw.endProps();
    nw.beginNode("Version"); nw.beginProps(); nw.addPropI32(232); nw.endProps(); nw.endNodeNoNested();
    nw.beginNode("Properties70"); nw.beginProps(); nw.endProps();
    nw.endNode();
    nw.beginNode("Shading"); nw.beginProps(); nw.addPropString("Y"); nw.endProps(); nw.endNodeNoNested();
    nw.beginNode("Culling"); nw.beginProps(); nw.addPropString("CullingOff"); nw.endProps(); nw.endNodeNoNested();
    nw.endNode();
    for (size_t mi = 0; mi < model.meshes.size(); mi++) {
        const auto& mesh = model.meshes[mi];
        std::string meshName = mesh.name.empty() ? "Mesh" + std::to_string(mi) : mesh.name;
        std::string geoName = fbxName(meshName, "Geometry");
        nw.beginNode("Geometry"); nw.beginProps(); nw.addPropI64(getGeoID(mi)); nw.addPropString(geoName); nw.addPropString("Mesh"); nw.endProps();
        nw.beginNode("GeometryVersion"); nw.beginProps(); nw.addPropI32(124); nw.endProps(); nw.endNodeNoNested();
        std::vector<double> vertices;
        for (const auto& v : mesh.vertices) { vertices.push_back(v.x); vertices.push_back(v.y); vertices.push_back(v.z); }
        nw.beginNode("Vertices"); nw.beginProps(); nw.addPropF64Array(vertices); nw.endProps(); nw.endNodeNoNested();
        std::vector<int32_t> polyIdx;
        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            polyIdx.push_back((int32_t)mesh.indices[i]);
            polyIdx.push_back((int32_t)mesh.indices[i + 1]);
            polyIdx.push_back(-((int32_t)mesh.indices[i + 2]) - 1);
        }
        nw.beginNode("PolygonVertexIndex"); nw.beginProps(); nw.addPropI32Array(polyIdx); nw.endProps(); nw.endNodeNoNested();
        std::vector<int32_t> edges;
        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            edges.push_back((int32_t)(i));
            edges.push_back((int32_t)(i + 1));
            edges.push_back((int32_t)(i + 2));
        }
        nw.beginNode("Edges"); nw.beginProps(); nw.addPropI32Array(edges); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("LayerElementNormal"); nw.beginProps(); nw.addPropI32(0); nw.endProps();
        nw.beginNode("Version"); nw.beginProps(); nw.addPropI32(102); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("Name"); nw.beginProps(); nw.addPropString(""); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("MappingInformationType"); nw.beginProps(); nw.addPropString("ByPolygonVertex"); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("ReferenceInformationType"); nw.beginProps(); nw.addPropString("Direct"); nw.endProps(); nw.endNodeNoNested();
        std::vector<double> normals;
        for (uint32_t idx : mesh.indices) {
            normals.push_back(mesh.vertices[idx].nx);
            normals.push_back(mesh.vertices[idx].ny);
            normals.push_back(mesh.vertices[idx].nz);
        }
        nw.beginNode("Normals"); nw.beginProps(); nw.addPropF64Array(normals); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
        nw.beginNode("LayerElementUV"); nw.beginProps(); nw.addPropI32(0); nw.endProps();
        nw.beginNode("Version"); nw.beginProps(); nw.addPropI32(101); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("Name"); nw.beginProps(); nw.addPropString("UVMap"); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("MappingInformationType"); nw.beginProps(); nw.addPropString("ByPolygonVertex"); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("ReferenceInformationType"); nw.beginProps(); nw.addPropString("IndexToDirect"); nw.endProps(); nw.endNodeNoNested();
        std::vector<double> uvs;
        std::vector<int32_t> uvIdx;
        for (size_t vi = 0; vi < mesh.vertices.size(); vi++) {
            uvs.push_back(mesh.vertices[vi].u);
            uvs.push_back(mesh.vertices[vi].v);
        }
        for (uint32_t idx : mesh.indices) {
            uvIdx.push_back((int32_t)idx);
        }
        nw.beginNode("UV"); nw.beginProps(); nw.addPropF64Array(uvs); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("UVIndex"); nw.beginProps(); nw.addPropI32Array(uvIdx); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
        nw.beginNode("LayerElementMaterial"); nw.beginProps(); nw.addPropI32(0); nw.endProps();
        nw.beginNode("Version"); nw.beginProps(); nw.addPropI32(101); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("Name"); nw.beginProps(); nw.addPropString(""); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("MappingInformationType"); nw.beginProps(); nw.addPropString("AllSame"); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("ReferenceInformationType"); nw.beginProps(); nw.addPropString("IndexToDirect"); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("Materials"); nw.beginProps(); nw.addPropI32Array({0}); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
        nw.beginNode("Layer"); nw.beginProps(); nw.addPropI32(0); nw.endProps();
        nw.beginNode("Version"); nw.beginProps(); nw.addPropI32(100); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("LayerElement"); nw.beginProps(); nw.endProps();
        nw.beginNode("Type"); nw.beginProps(); nw.addPropString("LayerElementNormal"); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("TypedIndex"); nw.beginProps(); nw.addPropI32(0); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
        nw.beginNode("LayerElement"); nw.beginProps(); nw.endProps();
        nw.beginNode("Type"); nw.beginProps(); nw.addPropString("LayerElementUV"); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("TypedIndex"); nw.beginProps(); nw.addPropI32(0); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
        nw.beginNode("LayerElement"); nw.beginProps(); nw.endProps();
        nw.beginNode("Type"); nw.beginProps(); nw.addPropString("LayerElementMaterial"); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("TypedIndex"); nw.beginProps(); nw.addPropI32(0); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
        nw.endNode();
        nw.endNode();
        std::string modelMeshName = fbxName(meshName, "Model");
        nw.beginNode("Model"); nw.beginProps(); nw.addPropI64(getModelID(mi)); nw.addPropString(modelMeshName); nw.addPropString("Mesh"); nw.endProps();
        nw.beginNode("Version"); nw.beginProps(); nw.addPropI32(232); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("Properties70"); nw.beginProps(); nw.endProps(); nw.endNode();
        nw.beginNode("Shading"); nw.beginProps(); nw.addPropString("T"); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("Culling"); nw.beginProps(); nw.addPropString("CullingOff"); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
    }
    if (hasSkeleton) {
        for (size_t bi = 0; bi < model.skeleton.bones.size(); bi++) {
            const auto& bone = model.skeleton.bones[bi];
            std::string attrName = fbxName(bone.name, "NodeAttribute");
            nw.beginNode("NodeAttribute"); nw.beginProps(); nw.addPropI64(getNodeAttrID(bi)); nw.addPropString(attrName); nw.addPropString("LimbNode"); nw.endProps();
            nw.beginNode("TypeFlags"); nw.beginProps(); nw.addPropString("Skeleton"); nw.endProps(); nw.endNodeNoNested();
            nw.endNode();
        }
        for (size_t bi = 0; bi < model.skeleton.bones.size(); bi++) {
            const auto& bone = model.skeleton.bones[bi];
            std::string boneName = fbxName(bone.name, "Model");
            nw.beginNode("Model"); nw.beginProps(); nw.addPropI64(getBoneID(bi)); nw.addPropString(boneName); nw.addPropString("LimbNode"); nw.endProps();
            nw.beginNode("Version"); nw.beginProps(); nw.addPropI32(232); nw.endProps(); nw.endNodeNoNested();
            nw.beginNode("Properties70"); nw.beginProps(); nw.endProps();
            nw.beginNode("P"); nw.beginProps(); nw.addPropString("RotationOrder"); nw.addPropString("enum"); nw.addPropString(""); nw.addPropString("");
            nw.addPropI32(0); nw.endProps(); nw.endNodeNoNested();
            nw.beginNode("P"); nw.beginProps(); nw.addPropString("Lcl Translation"); nw.addPropString("Lcl Translation"); nw.addPropString(""); nw.addPropString("A");
            nw.addPropF64(bone.posX); nw.addPropF64(bone.posY); nw.addPropF64(bone.posZ); nw.endProps(); nw.endNodeNoNested();
            nw.beginNode("P"); nw.beginProps(); nw.addPropString("Lcl Rotation"); nw.addPropString("Lcl Rotation"); nw.addPropString(""); nw.addPropString("A");
            double ex, ey, ez;
            quatToEuler(bone.rotX, bone.rotY, bone.rotZ, bone.rotW, ex, ey, ez);
            nw.addPropF64(ex); nw.addPropF64(ey); nw.addPropF64(ez); nw.endProps(); nw.endNodeNoNested();
            nw.endNode();
            nw.endNode();
        }
        for (size_t mi = 0; mi < model.meshes.size(); mi++) {
            if (!meshHasSkin[mi]) continue;
            nw.beginNode("Deformer"); nw.beginProps(); nw.addPropI64(getSkinID(mi)); nw.addPropString(fbxName("Skin", "Deformer")); nw.addPropString("Skin"); nw.endProps();
            nw.beginNode("Version"); nw.beginProps(); nw.addPropI32(101); nw.endProps(); nw.endNodeNoNested();
            nw.beginNode("Link_DeformAcuracy"); nw.beginProps(); nw.addPropF64(50.0); nw.endProps(); nw.endNodeNoNested();
            nw.endNode();
            for (int boneIdx : meshBoneSets[mi]) {
                const auto& infs = meshBoneInfluences[mi][boneIdx];
                const auto& bone = model.skeleton.bones[boneIdx];
                nw.beginNode("Deformer"); nw.beginProps(); nw.addPropI64(getClusterID(mi, boneIdx));
                nw.addPropString(fbxName(bone.name, "SubDeformer"));
                nw.addPropString("Cluster"); nw.endProps();
                nw.beginNode("Version"); nw.beginProps(); nw.addPropI32(100); nw.endProps(); nw.endNodeNoNested();
                nw.beginNode("UserData"); nw.beginProps(); nw.addPropString(""); nw.addPropString(""); nw.endProps(); nw.endNodeNoNested();
                std::vector<int32_t> indices;
                std::vector<double> weights;
                for (const auto& inf : infs) {
                    indices.push_back(inf.first);
                    weights.push_back(inf.second);
                }
                nw.beginNode("Indexes"); nw.beginProps(); nw.addPropI32Array(indices); nw.endProps(); nw.endNodeNoNested();
                nw.beginNode("Weights"); nw.beginProps(); nw.addPropF64Array(weights); nw.endProps(); nw.endNodeNoNested();
                std::vector<double> transform = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
                nw.beginNode("Transform"); nw.beginProps(); nw.addPropF64Array(transform); nw.endProps(); nw.endNodeNoNested();
                double qx = bone.worldRotX, qy = bone.worldRotY, qz = bone.worldRotZ, qw = bone.worldRotW;
                double tx = bone.worldPosX, ty = bone.worldPosY, tz = bone.worldPosZ;
                double xx = qx*qx, yy = qy*qy, zz = qz*qz;
                double xy = qx*qy, xz = qx*qz, yz = qy*qz;
                double wx = qw*qx, wy = qw*qy, wz = qw*qz;
                double m00 = 1.0 - 2.0*(yy+zz);
                double m01 = 2.0*(xy+wz);
                double m02 = 2.0*(xz-wy);
                double m10 = 2.0*(xy-wz);
                double m11 = 1.0 - 2.0*(xx+zz);
                double m12 = 2.0*(yz+wx);
                double m20 = 2.0*(xz+wy);
                double m21 = 2.0*(yz-wx);
                double m22 = 1.0 - 2.0*(xx+yy);
                std::vector<double> transformLink = {
                    m00, m01, m02, 0,
                    m10, m11, m12, 0,
                    m20, m21, m22, 0,
                    tx, ty, tz, 1
                };
                nw.beginNode("TransformLink"); nw.beginProps(); nw.addPropF64Array(transformLink); nw.endProps(); nw.endNodeNoNested();
                nw.endNode();
            }
        }
    }
    for (size_t mi = 0; mi < model.materials.size(); mi++) {
        const auto& mat = model.materials[mi];
        std::string matName = fbxName(mat.name, "Material");
        nw.beginNode("Material"); nw.beginProps(); nw.addPropI64(getMaterialID(mi)); nw.addPropString(matName); nw.addPropString(""); nw.endProps();
        nw.beginNode("Version"); nw.beginProps(); nw.addPropI32(102); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("ShadingModel"); nw.beginProps(); nw.addPropString("Phong"); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("MultiLayer"); nw.beginProps(); nw.addPropI32(0); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("Properties70"); nw.beginProps(); nw.endProps();
        nw.beginNode("P"); nw.beginProps(); nw.addPropString("DiffuseColor"); nw.addPropString("Color"); nw.addPropString(""); nw.addPropString("A");
        nw.addPropF64(0.8); nw.addPropF64(0.8); nw.addPropF64(0.8); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("P"); nw.beginProps(); nw.addPropString("Shininess"); nw.addPropString("Number"); nw.addPropString(""); nw.addPropString("A");
        nw.addPropF64(0.0); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("P"); nw.beginProps(); nw.addPropString("ShininessExponent"); nw.addPropString("Number"); nw.addPropString(""); nw.addPropString("A");
        nw.addPropF64(0.0); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("P"); nw.beginProps(); nw.addPropString("ReflectionFactor"); nw.addPropString("Number"); nw.addPropString(""); nw.addPropString("A");
        nw.addPropF64(0.5); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
        nw.endNode();
    }
    for (size_t ti = 0; ti < textureEntries.size(); ti++) {
        const auto& te = textureEntries[ti];
        const auto& mat = model.materials[te.materialIndex];
        std::string texBaseName = mat.name + te.suffix;
        std::string videoName = fbxName(texBaseName, "Video");
        nw.beginNode("Video"); nw.beginProps(); nw.addPropI64(getVideoID(ti)); nw.addPropString(videoName); nw.addPropString("Clip"); nw.endProps();
        nw.beginNode("Type"); nw.beginProps(); nw.addPropString("Clip"); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("Properties70"); nw.beginProps(); nw.endProps(); nw.endNode();
        nw.beginNode("UseMipMap"); nw.beginProps(); nw.addPropI32(0); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("Filename"); nw.beginProps(); nw.addPropString(texBaseName + ".png"); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("RelativeFilename"); nw.beginProps(); nw.addPropString(texBaseName + ".png"); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("Content"); nw.beginProps(); nw.addPropRawBytes(te.pngData.data(), te.pngData.size()); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
        std::string texName = fbxName(texBaseName, "Texture");
        nw.beginNode("Texture"); nw.beginProps(); nw.addPropI64(getTextureID(ti)); nw.addPropString(texName); nw.addPropString(""); nw.endProps();
        nw.beginNode("Type"); nw.beginProps(); nw.addPropString("TextureVideoClip"); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("Version"); nw.beginProps(); nw.addPropI32(202); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("TextureName"); nw.beginProps(); nw.addPropString(texBaseName); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("Properties70"); nw.beginProps(); nw.endProps(); nw.endNode();
        nw.beginNode("Media"); nw.beginProps(); nw.addPropString(texBaseName); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("FileName"); nw.beginProps(); nw.addPropString(texBaseName + ".png"); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("RelativeFilename"); nw.beginProps(); nw.addPropString(texBaseName + ".png"); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("ModelUVTranslation"); nw.beginProps(); nw.addPropF64(0.0); nw.addPropF64(0.0); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("ModelUVScaling"); nw.beginProps(); nw.addPropF64(1.0); nw.addPropF64(1.0); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("Texture_Alpha_Source"); nw.beginProps(); nw.addPropString("None"); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("Cropping"); nw.beginProps(); nw.addPropI32(0); nw.addPropI32(0); nw.addPropI32(0); nw.addPropI32(0); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
    }
    for (size_t ai = 0; ai < animExports.size(); ai++) {
        const auto& ae = animExports[ai];
        int64_t fbxTimeStart = 0;
        int64_t fbxTimeStop = (int64_t)(ae.duration * 46186158000.0);
        std::string stackName = fbxName("AnimStack::" + ae.name, "AnimStack");
        nw.beginNode("AnimationStack"); nw.beginProps(); nw.addPropI64(getAnimStackID(ai)); nw.addPropString(stackName); nw.addPropString(""); nw.endProps();
        nw.beginNode("Properties70"); nw.beginProps(); nw.endProps();
        nw.beginNode("P"); nw.beginProps(); nw.addPropString("LocalStart"); nw.addPropString("KTime"); nw.addPropString("Time"); nw.addPropString(""); nw.addPropI64(fbxTimeStart); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("P"); nw.beginProps(); nw.addPropString("LocalStop"); nw.addPropString("KTime"); nw.addPropString("Time"); nw.addPropString(""); nw.addPropI64(fbxTimeStop); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("P"); nw.beginProps(); nw.addPropString("ReferenceStart"); nw.addPropString("KTime"); nw.addPropString("Time"); nw.addPropString(""); nw.addPropI64(fbxTimeStart); nw.endProps(); nw.endNodeNoNested();
        nw.beginNode("P"); nw.beginProps(); nw.addPropString("ReferenceStop"); nw.addPropString("KTime"); nw.addPropString("Time"); nw.addPropString(""); nw.addPropI64(fbxTimeStop); nw.endProps(); nw.endNodeNoNested();
        nw.endNode();
        nw.endNode();
        std::string layerName = fbxName("AnimLayer::BaseLayer", "AnimLayer");
        nw.beginNode("AnimationLayer"); nw.beginProps(); nw.addPropI64(getAnimLayerID(ai)); nw.addPropString(layerName); nw.addPropString(""); nw.endProps();
        nw.beginNode("Properties70"); nw.beginProps(); nw.endProps(); nw.endNode();
        nw.endNode();
        for (size_t ti = 0; ti < ae.tracks.size(); ti++) {
            const auto& track = ae.tracks[ti];
            std::string curveNodeType = track.isRotation ? "R" : "T";
            std::string curveNodeName = fbxName("AnimCurveNode::" + curveNodeType, "AnimCurveNode");
            nw.beginNode("AnimationCurveNode"); nw.beginProps(); nw.addPropI64(getAnimCurveNodeID(ai, ti, 0)); nw.addPropString(curveNodeName); nw.addPropString(""); nw.endProps();
            nw.beginNode("Properties70"); nw.beginProps(); nw.endProps();
            nw.beginNode("P"); nw.beginProps(); nw.addPropString("d|X"); nw.addPropString("Number"); nw.addPropString(""); nw.addPropString("A"); nw.addPropF64(track.valuesX.empty() ? 0.0 : track.valuesX[0]); nw.endProps(); nw.endNodeNoNested();
            nw.beginNode("P"); nw.beginProps(); nw.addPropString("d|Y"); nw.addPropString("Number"); nw.addPropString(""); nw.addPropString("A"); nw.addPropF64(track.valuesY.empty() ? 0.0 : track.valuesY[0]); nw.endProps(); nw.endNodeNoNested();
            nw.beginNode("P"); nw.beginProps(); nw.addPropString("d|Z"); nw.addPropString("Number"); nw.addPropString(""); nw.addPropString("A"); nw.addPropF64(track.valuesZ.empty() ? 0.0 : track.valuesZ[0]); nw.endProps(); nw.endNodeNoNested();
            nw.endNode();
            nw.endNode();
            for (int axis = 0; axis < 3; axis++) {
                const std::vector<float>& vals = (axis == 0) ? track.valuesX : (axis == 1) ? track.valuesY : track.valuesZ;
                nw.beginNode("AnimationCurve"); nw.beginProps(); nw.addPropI64(getAnimCurveID(ai, ti, 0, axis)); nw.addPropString(fbxName("AnimCurve::", "AnimCurve")); nw.addPropString(""); nw.endProps();
                nw.beginNode("Default"); nw.beginProps(); nw.addPropF64(vals.empty() ? 0.0 : vals[0]); nw.endProps(); nw.endNodeNoNested();
                nw.beginNode("KeyVer"); nw.beginProps(); nw.addPropI32(4009); nw.endProps(); nw.endNodeNoNested();
                std::vector<int64_t> keyTimes;
                for (float t : track.times) keyTimes.push_back((int64_t)(t * 46186158000.0));
                nw.beginNode("KeyTime"); nw.beginProps(); nw.addPropI64Array(keyTimes); nw.endProps(); nw.endNodeNoNested();
                nw.beginNode("KeyValueFloat"); nw.beginProps(); nw.addPropF32Array(vals); nw.endProps(); nw.endNodeNoNested();
                nw.beginNode("KeyAttrFlags"); nw.beginProps(); nw.addPropI32Array({0x00000108}); nw.endProps(); nw.endNodeNoNested();
                nw.beginNode("KeyAttrDataFloat"); nw.beginProps(); nw.addPropF32Array({0.0f, 0.0f, 0.0f, 0.0f}); nw.endProps(); nw.endNodeNoNested();
                nw.beginNode("KeyAttrRefCount"); nw.beginProps(); nw.addPropI32Array({(int32_t)track.times.size()}); nw.endProps(); nw.endNodeNoNested();
                nw.endNode();
            }
        }
    }
    nw.endNode();
    nw.beginNode("Connections"); nw.beginProps(); nw.endProps();
    auto addConn = [&](const std::string& type, int64_t child, int64_t parent) {
        nw.beginNode("C"); nw.beginProps(); nw.addPropString(type); nw.addPropI64(child); nw.addPropI64(parent); nw.endProps(); nw.endNodeNoNested();
    };
    auto addConnProp = [&](const std::string& type, int64_t child, int64_t parent, const std::string& prop) {
        nw.beginNode("C"); nw.beginProps(); nw.addPropString(type); nw.addPropI64(child); nw.addPropI64(parent); nw.addPropString(prop); nw.endProps(); nw.endNodeNoNested();
    };
    addConn("OO", ROOT_MODEL_ID, 0);
    for (size_t i = 0; i < model.meshes.size(); i++) {
        addConn("OO", getModelID(i), ROOT_MODEL_ID);
        addConn("OO", getGeoID(i), getModelID(i));
    }
    if (hasSkeleton) {
        for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
            addConn("OO", getNodeAttrID(i), getBoneID(i));
        }
        for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
            int parentIdx = model.skeleton.bones[i].parentIndex;
            addConn("OO", getBoneID(i), parentIdx >= 0 ? getBoneID(parentIdx) : ROOT_MODEL_ID);
        }
        for (size_t mi = 0; mi < model.meshes.size(); mi++) {
            if (!meshHasSkin[mi]) continue;
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
        const auto& mti = materialTexIndices[mi];
        if (mti.diffuse >= 0) {
            addConnProp("OP", getTextureID(mti.diffuse), getMaterialID(mi), "DiffuseColor");
            addConn("OO", getVideoID(mti.diffuse), getTextureID(mti.diffuse));
        }
        if (mti.normal >= 0) {
            addConnProp("OP", getTextureID(mti.normal), getMaterialID(mi), "Bump");
            addConn("OO", getVideoID(mti.normal), getTextureID(mti.normal));
        }
        if (mti.specular >= 0) {
            addConnProp("OP", getTextureID(mti.specular), getMaterialID(mi), "SpecularColor");
            addConn("OO", getVideoID(mti.specular), getTextureID(mti.specular));
        }
        if (mti.tint >= 0) {
            addConnProp("OP", getTextureID(mti.tint), getMaterialID(mi), "TransparentColor");
            addConn("OO", getVideoID(mti.tint), getTextureID(mti.tint));
        }
    }
    for (size_t ai = 0; ai < animExports.size(); ai++) {
        const auto& ae = animExports[ai];
        addConn("OO", getAnimLayerID(ai), getAnimStackID(ai));
        for (size_t ti = 0; ti < ae.tracks.size(); ti++) {
            const auto& track = ae.tracks[ti];
            std::string propName = track.isRotation ? "Lcl Rotation" : "Lcl Translation";
            addConn("OO", getAnimCurveNodeID(ai, ti, 0), getAnimLayerID(ai));
            addConnProp("OP", getAnimCurveNodeID(ai, ti, 0), getBoneID(track.boneIdx), propName);
            addConnProp("OP", getAnimCurveID(ai, ti, 0, 0), getAnimCurveNodeID(ai, ti, 0), "d|X");
            addConnProp("OP", getAnimCurveID(ai, ti, 0, 1), getAnimCurveNodeID(ai, ti, 0), "d|Y");
            addConnProp("OP", getAnimCurveID(ai, ti, 0, 2), getAnimCurveNodeID(ai, ti, 0), "d|Z");
        }
    }
    nw.endNode();
    if (!animExports.empty()) {
        nw.beginNode("Takes"); nw.beginProps(); nw.endProps();
        nw.beginNode("Current"); nw.beginProps(); nw.addPropString(animExports[0].name); nw.endProps(); nw.endNodeNoNested();
        for (size_t ai = 0; ai < animExports.size(); ai++) {
            const auto& ae = animExports[ai];
            int64_t fbxTimeStart = 0;
            int64_t fbxTimeStop = (int64_t)(ae.duration * 46186158000.0);
            nw.beginNode("Take"); nw.beginProps(); nw.addPropString(ae.name); nw.endProps();
            nw.beginNode("FileName"); nw.beginProps(); nw.addPropString(ae.name + ".tak"); nw.endProps(); nw.endNodeNoNested();
            nw.beginNode("LocalTime"); nw.beginProps(); nw.addPropI64(fbxTimeStart); nw.addPropI64(fbxTimeStop); nw.endProps(); nw.endNodeNoNested();
            nw.beginNode("ReferenceTime"); nw.beginProps(); nw.addPropI64(fbxTimeStart); nw.addPropI64(fbxTimeStop); nw.endProps(); nw.endNodeNoNested();
            nw.endNode();
        }
        nw.endNode();
    }
    for (int i = 0; i < 13; i++) output.push_back(0);
    std::ofstream out(outputPath, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(output.data()), output.size());
    return out.good();
}

bool saveRGBAToPNG(const std::string& path, const std::vector<uint8_t>& rgba, int width, int height) {
    if (rgba.empty() || width <= 0 || height <= 0) return false;
    std::vector<uint8_t> png;
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
    ihdr[0] = (width >> 24) & 0xff; ihdr[1] = (width >> 16) & 0xff;
    ihdr[2] = (width >> 8) & 0xff; ihdr[3] = width & 0xff;
    ihdr[4] = (height >> 24) & 0xff; ihdr[5] = (height >> 16) & 0xff;
    ihdr[6] = (height >> 8) & 0xff; ihdr[7] = height & 0xff;
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    writeChunk("IHDR", ihdr);
    std::vector<uint8_t> raw;
    for (int y = 0; y < height; y++) {
        raw.push_back(0);
        raw.insert(raw.end(), rgba.begin() + y * width * 4, rgba.begin() + (y + 1) * width * 4);
    }
    std::vector<uint8_t> deflated;
    deflated.push_back(0x78); deflated.push_back(0x01);
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t remain = raw.size() - pos;
        size_t blockLen = remain > 65535 ? 65535 : remain;
        bool last = (pos + blockLen >= raw.size());
        deflated.push_back(last ? 1 : 0);
        deflated.push_back(blockLen & 0xff); deflated.push_back((blockLen >> 8) & 0xff);
        deflated.push_back(~blockLen & 0xff); deflated.push_back((~blockLen >> 8) & 0xff);
        deflated.insert(deflated.end(), raw.begin() + pos, raw.begin() + pos + blockLen);
        pos += blockLen;
    }
    uint32_t adler = 1;
    for (size_t i = 0; i < raw.size(); i++) {
        uint32_t s1 = adler & 0xffff, s2 = (adler >> 16) & 0xffff;
        s1 = (s1 + raw[i]) % 65521; s2 = (s2 + s1) % 65521;
        adler = (s2 << 16) | s1;
    }
    deflated.push_back((adler >> 24) & 0xff); deflated.push_back((adler >> 16) & 0xff);
    deflated.push_back((adler >> 8) & 0xff); deflated.push_back(adler & 0xff);
    writeChunk("IDAT", deflated);
    writeChunk("IEND", {});
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(png.data()), png.size());
    return f.good();
}