#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <cmath>
#include <map>
#include <functional>
#include <iostream>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ImGuiFileDialog.h"
#include "erf.h"
#include "Gff.h"
#include "Mesh.h"
#include "model_loader.h"

namespace fs = std::filesystem;

// Fly camera (game-style)
struct Camera {
    float x = 0.0f, y = 0.0f, z = 5.0f;  // Position
    float yaw = 0.0f;
    float pitch = 0.0f;
    float moveSpeed = 5.0f;
    float lookSensitivity = 0.003f;

    void setPosition(float px, float py, float pz) {
        x = px;
        y = py;
        z = pz;
    }

    void lookAt(float tx, float ty, float tz, float dist) {
        // Position camera to look at target (remembering Z is up after rotation)
        // Camera is in Y-up space, model is rotated to Z-up
        // So we position in Y-up space: target's Z becomes our Y
        x = tx;
        y = tz + dist * 0.5f;  // tz is the model's "up", position camera above
        z = ty + dist;          // ty is the model's "forward", position camera back
        yaw = 0.0f;
        pitch = -0.2f;  // Look slightly down
        moveSpeed = dist * 0.5f;
        if (moveSpeed < 1.0f) moveSpeed = 1.0f;
    }

    void getForward(float& fx, float& fy, float& fz) const {
        // Forward direction based on yaw and pitch
        fx = -std::sin(yaw) * std::cos(pitch);
        fy = std::sin(pitch);
        fz = -std::cos(yaw) * std::cos(pitch);
    }

    void getRight(float& rx, float& ry, float& rz) const {
        // Right is perpendicular to forward on XZ plane
        rx = std::cos(yaw);
        ry = 0.0f;
        rz = -std::sin(yaw);
    }

    void moveForward(float amount) {
        // Move in the direction we're looking
        float fx, fy, fz;
        getForward(fx, fy, fz);
        x += fx * amount;
        y += fy * amount;
        z += fz * amount;
    }

    void moveRight(float amount) {
        // Strafe left/right
        float rx, ry, rz;
        getRight(rx, ry, rz);
        x += rx * amount;
        z += rz * amount;
    }

    void moveUp(float amount) {
        // Move along world Y axis
        y += amount;
    }

    void rotate(float deltaYaw, float deltaPitch) {
        yaw += deltaYaw;
        pitch += deltaPitch;
        // Clamp pitch
        if (pitch > 1.5f) pitch = 1.5f;
        if (pitch < -1.5f) pitch = -1.5f;
    }
};

// Render settings
struct RenderSettings {
    bool wireframe = false;
    bool showAxes = true;
    bool showGrid = true;
    bool showCollision = true;
    bool collisionWireframe = true;  // Draw collision as wireframe or solid
    bool showSkeleton = true;
    bool showBoneNames = false;
    std::vector<uint8_t> meshVisible;  // Per-mesh visibility (using uint8_t because vector<bool> is special)

    void initMeshVisibility(size_t count) {
        meshVisible.resize(count, 1);
    }
};

struct AppState {
    // Browser state
    bool showBrowser = true;
    bool showRenderSettings = false;
    bool showMaoViewer = false;
    bool showUvViewer = false;
    std::string maoContent;
    std::string maoFileName;
    int selectedMeshForUv = -1;
    std::string selectedFolder;
    std::vector<std::string> erfFiles;
    int selectedErfIndex = -1;
    std::unique_ptr<ERFFile> currentErf;
    int selectedEntryIndex = -1;
    std::string statusMessage;
    std::string extractPath;

    // Model state
    Model currentModel;
    bool hasModel = false;
    Camera camera;
    RenderSettings renderSettings;

    // Mouse state for camera control
    bool isPanning = false;       // Right mouse - look around
    double lastMouseX = 0;
    double lastMouseY = 0;
};

std::string getExeDir() {
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return fs::path(path).parent_path().string();
#else
    return fs::current_path().string();
#endif
}

void ensureExtractDir(const std::string& exeDir) {
    fs::path extractPath = fs::path(exeDir) / "extracted";
    if (!fs::exists(extractPath)) {
        fs::create_directories(extractPath);
    }
}

std::string versionToString(ERFVersion v) {
    switch (v) {
        case ERFVersion::V1_0: return "V1.0";
        case ERFVersion::V1_1: return "V1.1";
        case ERFVersion::V2_0: return "V2.0";
        case ERFVersion::V2_2: return "V2.2";
        case ERFVersion::V3_0: return "V3.0";
        default: return "Unknown";
    }
}

// Check if entry is a model file
bool isModelFile(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Check for MMH (model) or MSH (mesh) files
    size_t dotPos = lower.rfind('.');
    if (dotPos != std::string::npos) {
        std::string ext = lower.substr(dotPos);
        return ext == ".mmh" || ext == ".msh";
    }
    return false;
}

// Check if entry is a MAO (material) file
bool isMaoFile(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    size_t dotPos = lower.rfind('.');
    if (dotPos != std::string::npos) {
        std::string ext = lower.substr(dotPos);
        return ext == ".mao";
    }
    return false;
}

// Check if entry is a PHY (physics/collision) file
bool isPhyFile(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    size_t dotPos = lower.rfind('.');
    if (dotPos != std::string::npos) {
        std::string ext = lower.substr(dotPos);
        return ext == ".phy";
    }
    return false;
}

// Load collision shapes from PHY file
bool loadPHY(const std::vector<uint8_t>& data, Model& model) {
    GFFFile gff;
    if (!gff.load(data)) {
        return false;
    }

    // Helper to multiply quaternions: result = q1 * q2
    auto quatMul = [](float q1x, float q1y, float q1z, float q1w,
                      float q2x, float q2y, float q2z, float q2w,
                      float& rx, float& ry, float& rz, float& rw) {
        rw = q1w*q2w - q1x*q2x - q1y*q2y - q1z*q2z;
        rx = q1w*q2x + q1x*q2w + q1y*q2z - q1z*q2y;
        ry = q1w*q2y - q1x*q2z + q1y*q2w + q1z*q2x;
        rz = q1w*q2z + q1x*q2y - q1y*q2x + q1z*q2w;
    };

    // Helper to rotate a vector by quaternion
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

    // Recursive function to find collision shapes, tracking the bone hierarchy
    std::function<void(size_t, uint32_t, const std::string&)> processStruct =
        [&](size_t structIdx, uint32_t offset, const std::string& parentBoneName) {

        if (structIdx >= gff.structs().size()) return;

        const auto& st = gff.structs()[structIdx];
        std::string structType(st.structType);

        // Determine the bone name for this level
        std::string currentBoneName = parentBoneName;
        if (structType == "node") {
            std::string name = gff.readStringByLabel(structIdx, 6000, offset);
            if (!name.empty()) {
                currentBoneName = name;
            }
        }

        // If this is a "shap" struct, extract the collision shape
        if (structType == "shap") {
            CollisionShape shape;

            // Read shape name (field 6241)
            shape.name = gff.readStringByLabel(structIdx, 6241, offset);
            if (shape.name.empty()) {
                shape.name = "collision_" + std::to_string(model.collisionShapes.size());
            }

            // Read local position (field 6061) - Vector3f
            float localPosX = 0, localPosY = 0, localPosZ = 0;
            const GFFField* posField = gff.findField(structIdx, 6061);
            if (posField) {
                uint32_t posOffset = gff.dataOffset() + posField->dataOffset + offset;
                localPosX = gff.readFloatAt(posOffset);
                localPosY = gff.readFloatAt(posOffset + 4);
                localPosZ = gff.readFloatAt(posOffset + 8);
            }

            // Read local rotation (field 6060) - Quaternion
            float localRotX = 0, localRotY = 0, localRotZ = 0, localRotW = 1;
            const GFFField* rotField = gff.findField(structIdx, 6060);
            if (rotField) {
                uint32_t rotOffset = gff.dataOffset() + rotField->dataOffset + offset;
                localRotX = gff.readFloatAt(rotOffset);
                localRotY = gff.readFloatAt(rotOffset + 4);
                localRotZ = gff.readFloatAt(rotOffset + 8);
                localRotW = gff.readFloatAt(rotOffset + 12);
            }

            // --- TRANSFORM FIX: Convert Local Space -> World Space using Skeleton ---
            int boneIdx = model.skeleton.findBone(currentBoneName);
            if (boneIdx >= 0) {
                const Bone& bone = model.skeleton.bones[boneIdx];

                // 1. Rotate the shape's local position by the Bone's World Rotation
                float rotatedPosX, rotatedPosY, rotatedPosZ;
                quatRotate(bone.worldRotX, bone.worldRotY, bone.worldRotZ, bone.worldRotW,
                           localPosX, localPosY, localPosZ,
                           rotatedPosX, rotatedPosY, rotatedPosZ);

                // 2. Add Bone's World Position
                shape.posX = bone.worldPosX + rotatedPosX;
                shape.posY = bone.worldPosY + rotatedPosY;
                shape.posZ = bone.worldPosZ + rotatedPosZ;

                // 3. Combine Rotations: ShapeWorldRot = BoneWorldRot * ShapeLocalRot
                quatMul(bone.worldRotX, bone.worldRotY, bone.worldRotZ, bone.worldRotW,
                        localRotX, localRotY, localRotZ, localRotW,
                        shape.rotX, shape.rotY, shape.rotZ, shape.rotW);
            } else {
                // Fallback: If no bone matches, use local coordinates
                shape.posX = localPosX;
                shape.posY = localPosY;
                shape.posZ = localPosZ;
                shape.rotX = localRotX;
                shape.rotY = localRotY;
                shape.rotZ = localRotZ;
                shape.rotW = localRotW;
            }

            // Read shape type struct (field 6998)
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
                }
                else if (isStruct && !isList) {
                    int32_t ref = gff.readInt32At(dataPos);
                    if (ref >= 0) {
                        dataRef.structIndex = shapeTypeField->typeId;
                        dataRef.offset = ref;
                        hasShapeData = true;
                    }
                }
                else {
                    std::vector<GFFStructRef> shapeData = gff.readStructList(structIdx, 6998, offset);
                    if (!shapeData.empty()) {
                        dataRef = shapeData[0];
                        hasShapeData = true;
                    }
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
                }
                else if (dataType == "sphs") {
                    shape.type = CollisionShapeType::Sphere;
                    const GFFField* radField = gff.findField(dataRef.structIndex, 6072);
                    if (radField) {
                        uint32_t radOffset = gff.dataOffset() + radField->dataOffset + dataRef.offset;
                        shape.radius = gff.readFloatAt(radOffset);
                    }
                    shapeValid = (shape.radius > 0.0f);
                }
                else if (dataType == "caps") {
                    shape.type = CollisionShapeType::Capsule;
                    const GFFField* radField = gff.findField(dataRef.structIndex, 6072);
                    const GFFField* htField = gff.findField(dataRef.structIndex, 6073);
                    if (radField) {
                        uint32_t radOffset = gff.dataOffset() + radField->dataOffset + dataRef.offset;
                        shape.radius = gff.readFloatAt(radOffset);
                    }
                    if (htField) {
                        uint32_t htOffset = gff.dataOffset() + htField->dataOffset + dataRef.offset;
                        shape.height = gff.readFloatAt(htOffset);
                    }
                    shapeValid = (shape.radius > 0.0f && shape.height > 0.0f);
                }
                else if (dataType == "mshs") {
                    shape.type = CollisionShapeType::Mesh;
                    const GFFField* meshDataField = gff.findField(dataRef.structIndex, 6077);
                    if (meshDataField) {
                        uint32_t meshDataPos = gff.dataOffset() + meshDataField->dataOffset + dataRef.offset;
                        int32_t listRef = gff.readInt32At(meshDataPos);

                        if (listRef >= 0) {
                            uint32_t nxsPos = gff.dataOffset() + listRef + 4;
                            const auto& rawData = gff.rawData();

                            if (nxsPos + 36 < rawData.size()) {
                                nxsPos += 28; // Skip header
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

            if (shapeValid) {
                model.collisionShapes.push_back(shape);
            }
        }

        // Recurse into children (field 6999), passing current bone name
        std::vector<GFFStructRef> children = gff.readStructList(structIdx, 6999, offset);
        for (const auto& child : children) {
            processStruct(child.structIndex, child.offset, currentBoneName);
        }
    };

    // Start from root struct (index 0) with empty bone name (or "Root" if you prefer)
    processStruct(0, 0, "");

    std::cout << "Loaded " << model.collisionShapes.size() << " collision shapes from PHY" << std::endl;
    return !model.collisionShapes.empty();
}
// Quaternion multiply helper for world transform calculation
void quatMulWorld(float ax, float ay, float az, float aw,
                  float bx, float by, float bz, float bw,
                  float& rx, float& ry, float& rz, float& rw) {
    rw = aw*bw - ax*bx - ay*by - az*bz;
    rx = aw*bx + ax*bw + ay*bz - az*by;
    ry = aw*by - ax*bz + ay*bw + az*bx;
    rz = aw*bz + ax*by - ay*bx + az*bw;
}

// Rotate point by quaternion for world transform
void quatRotateWorld(float qx, float qy, float qz, float qw,
                     float px, float py, float pz,
                     float& rx, float& ry, float& rz) {
    // v' = q * v * q^-1  (for unit quaternion, q^-1 = conjugate)
    float tx = 2.0f * (qy * pz - qz * py);
    float ty = 2.0f * (qz * px - qx * pz);
    float tz = 2.0f * (qx * py - qy * px);

    rx = px + qw * tx + (qy * tz - qz * ty);
    ry = py + qw * ty + (qz * tx - qx * tz);
    rz = pz + qw * tz + (qx * ty - qy * tx);
}

// Load skeleton and material names from MMH file with FIXED transform lookup
void loadMMH(const std::vector<uint8_t>& data, Model& model) {
    std::cout << "--- [DEBUG] Loading MMH ---" << std::endl;
    GFFFile gff;
    if (!gff.load(data)) {
        std::cout << "ERROR: Failed to parse GFF data for MMH." << std::endl;
        return;
    }

    std::cout << "MMH GFF loaded. Total Structs: " << gff.structs().size() << std::endl;

    auto normalizeQuat = [](float& x, float& y, float& z, float& w) {
        float len = std::sqrt(x*x + y*y + z*z + w*w);
        if (len > 0.00001f) {
            float invLen = 1.0f / len;
            x *= invLen; y *= invLen; z *= invLen; w *= invLen;
        } else {
            x = 0; y = 0; z = 0; w = 1;
        }
    };

    std::map<std::string, std::string> meshMaterials;
    std::vector<Bone> tempBones;

    std::function<void(size_t, uint32_t, const std::string&)> findNodes =
        [&](size_t structIdx, uint32_t offset, const std::string& parentName) {

        if (structIdx >= gff.structs().size()) return;

        const auto& s = gff.structs()[structIdx];
        std::string structType(s.structType);

        if (structType == "mesh") {
            std::string meshName = gff.readStringByLabel(structIdx, 6006, offset);
            std::string materialName = gff.readStringByLabel(structIdx, 6001, offset);
            if (!meshName.empty() && !materialName.empty()) {
                meshMaterials[meshName] = materialName;
            }
        }

        if (structType == "node") {
            Bone bone;
            bone.name = gff.readStringByLabel(structIdx, 6000, offset);
            bone.parentName = parentName;

            // Get children to scan for transform data AND to recurse
            std::vector<GFFStructRef> children = gff.readStructList(structIdx, 6999, offset);

            bool foundPos = false;
            bool foundRot = false;

            // FIX: Scan children for transformation data (trsl/rota structs)
            for (const auto& child : children) {
                // Check for Position (6047) in child
                const GFFField* posField = gff.findField(child.structIndex, 6047);
                if (posField) {
                     uint32_t posOffset = gff.dataOffset() + posField->dataOffset + child.offset;
                     bone.posX = gff.readFloatAt(posOffset);
                     bone.posY = gff.readFloatAt(posOffset + 4);
                     bone.posZ = gff.readFloatAt(posOffset + 8);
                     foundPos = true;
                }

                // Check for Rotation (6048) in child
                const GFFField* rotField = gff.findField(child.structIndex, 6048);
                if (rotField) {
                     uint32_t rotOffset = gff.dataOffset() + rotField->dataOffset + child.offset;
                     bone.rotX = gff.readFloatAt(rotOffset);
                     bone.rotY = gff.readFloatAt(rotOffset + 4);
                     bone.rotZ = gff.readFloatAt(rotOffset + 8);
                     bone.rotW = gff.readFloatAt(rotOffset + 12);
                     normalizeQuat(bone.rotX, bone.rotY, bone.rotZ, bone.rotW);
                     foundRot = true;
                }
            }

            // Debug output to confirm we found data
            if (foundPos || foundRot) {
                // std::cout << "  Bone '" << bone.name << "': Pos[" << (foundPos?"X":" ") << "] Rot[" << (foundRot?"X":" ") << "]" << std::endl;
            } else {
                std::cout << "  WARNING: No transform data found for bone '" << bone.name << "' in children." << std::endl;
            }

            if (!bone.name.empty()) {
                tempBones.push_back(bone);
            }

            // Recurse into children nodes
            for (const auto& child : children) {
                findNodes(child.structIndex, child.offset, bone.name);
            }
            return;
        }

        // Generic recursion for non-node structs
        std::vector<GFFStructRef> children = gff.readStructList(structIdx, 6999, offset);
        for (const auto& child : children) {
            findNodes(child.structIndex, child.offset, parentName);
        }
    };

    findNodes(0, 0, "");

    std::cout << "Recursive search done. Found " << tempBones.size() << " bones." << std::endl;

    // Apply materials
    for (auto& mesh : model.meshes) {
        auto it = meshMaterials.find(mesh.name);
        if (it != meshMaterials.end()) {
            mesh.materialName = it->second;
        }
    }

    // Link parents
    model.skeleton.bones = tempBones;
    int links = 0;
    for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
        Bone& bone = model.skeleton.bones[i];
        if (!bone.parentName.empty()) {
            bone.parentIndex = model.skeleton.findBone(bone.parentName);
            if (bone.parentIndex >= 0) {
                links++;
            }
        }
    }
    std::cout << "Skeleton linking complete. Valid parent links: " << links << std::endl;

    // Compute transforms
    for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
        Bone& bone = model.skeleton.bones[i];

        if (bone.parentIndex < 0) {
            bone.worldPosX = bone.posX;
            bone.worldPosY = bone.posY;
            bone.worldPosZ = bone.posZ;
            bone.worldRotX = bone.rotX;
            bone.worldRotY = bone.rotY;
            bone.worldRotZ = bone.rotZ;
            bone.worldRotW = bone.rotW;
        } else {
            const Bone& parent = model.skeleton.bones[bone.parentIndex];

            float rotatedX, rotatedY, rotatedZ;
            quatRotateWorld(parent.worldRotX, parent.worldRotY, parent.worldRotZ, parent.worldRotW,
                           bone.posX, bone.posY, bone.posZ,
                           rotatedX, rotatedY, rotatedZ);

            bone.worldPosX = parent.worldPosX + rotatedX;
            bone.worldPosY = parent.worldPosY + rotatedY;
            bone.worldPosZ = parent.worldPosZ + rotatedZ;

            quatMulWorld(parent.worldRotX, parent.worldRotY, parent.worldRotZ, parent.worldRotW,
                        bone.rotX, bone.rotY, bone.rotZ, bone.rotW,
                        bone.worldRotX, bone.worldRotY, bone.worldRotZ, bone.worldRotW);

            normalizeQuat(bone.worldRotX, bone.worldRotY, bone.worldRotZ, bone.worldRotW);
        }
    }

    std::cout << "--- [DEBUG] MMH Load Finished ---" << std::endl;
}

// Try to load model from ERF entry
bool loadModelFromEntry(AppState& state, const ERFEntry& entry) {
    if (!state.currentErf) return false;

    std::cout << "Loading Model: " << entry.name << std::endl;

    std::vector<uint8_t> data = state.currentErf->readEntry(entry);
    if (data.empty()) return false;

    // Try to load as MSH
    Model model;
    if (!loadMSH(data, model)) {
        // If MSH loading fails, show a placeholder
        state.currentModel = Model();
        state.currentModel.name = entry.name + " (failed to parse)";

        // Create a simple cube as placeholder
        Mesh cube;
        cube.name = "placeholder";

        float s = 1.0f;
        cube.vertices = {
            {-s, -s,  s,  0, 0, 1,  0, 0},
            { s, -s,  s,  0, 0, 1,  1, 0},
            { s,  s,  s,  0, 0, 1,  1, 1},
            {-s,  s,  s,  0, 0, 1,  0, 1},
            { s, -s, -s,  0, 0,-1,  0, 0},
            {-s, -s, -s,  0, 0,-1,  1, 0},
            {-s,  s, -s,  0, 0,-1,  1, 1},
            { s,  s, -s,  0, 0,-1,  0, 1},
            {-s,  s,  s,  0, 1, 0,  0, 0},
            { s,  s,  s,  0, 1, 0,  1, 0},
            { s,  s, -s,  0, 1, 0,  1, 1},
            {-s,  s, -s,  0, 1, 0,  0, 1},
            {-s, -s, -s,  0,-1, 0,  0, 0},
            { s, -s, -s,  0,-1, 0,  1, 0},
            { s, -s,  s,  0,-1, 0,  1, 1},
            {-s, -s,  s,  0,-1, 0,  0, 1},
            { s, -s,  s,  1, 0, 0,  0, 0},
            { s, -s, -s,  1, 0, 0,  1, 0},
            { s,  s, -s,  1, 0, 0,  1, 1},
            { s,  s,  s,  1, 0, 0,  0, 1},
            {-s, -s, -s, -1, 0, 0,  0, 0},
            {-s, -s,  s, -1, 0, 0,  1, 0},
            {-s,  s,  s, -1, 0, 0,  1, 1},
            {-s,  s, -s, -1, 0, 0,  0, 1},
        };

        cube.indices = {
            0,  1,  2,  2,  3,  0,
            4,  5,  6,  6,  7,  4,
            8,  9,  10, 10, 11, 8,
            12, 13, 14, 14, 15, 12,
            16, 17, 18, 18, 19, 16,
            20, 21, 22, 22, 23, 20
        };

        cube.calculateBounds();
        state.currentModel.meshes.push_back(cube);
        state.hasModel = true;

        auto center = cube.center();
        state.camera.lookAt(center[0], center[1], center[2], cube.radius() * 3.0f);

        return false; // Indicate parsing failed but we have placeholder
    }

    // Successfully loaded
    state.currentModel = model;
    state.currentModel.name = entry.name;
    state.hasModel = true;

    // Init per-mesh visibility
    state.renderSettings.initMeshVisibility(model.meshes.size());

    // Get base name (remove extension)
    std::string baseName = entry.name;
    size_t dotPos = baseName.rfind('.');
    if (dotPos != std::string::npos) {
        baseName = baseName.substr(0, dotPos);
    }

    // ---------------------------------------------------------
    // GENERATE MMH CANDIDATES
    // ---------------------------------------------------------
    std::vector<std::string> mmhCandidates;

    // 1. Exact match (c_ashwraith_0.mmh)
    mmhCandidates.push_back(baseName + ".mmh");

    // 2. Insert 'a' before last underscore (c_ashwraith_0 -> c_ashwraitha_0.mmh)
    size_t lastUnderscore = baseName.find_last_of('_');
    if (lastUnderscore != std::string::npos) {
        std::string variantA = baseName;
        variantA.insert(lastUnderscore, "a");
        mmhCandidates.push_back(variantA + ".mmh");
    }

    // 3. Append 'a' (just in case)
    mmhCandidates.push_back(baseName + "a.mmh");

    std::cout << "Searching for MMH candidates:" << std::endl;
    for(const auto& c : mmhCandidates) std::cout << " - " << c << std::endl;

    // Search ALL loaded ERFs for any of the MMH candidates
    bool foundMMH = false;
    for (const auto& erfPath : state.erfFiles) {
        // Use current ERF pointer if it matches to save opening it again
        if (state.currentErf && state.currentErf->filename() == erfPath) {
            for (const auto& e : state.currentErf->entries()) {
                std::string eName = e.name;
                std::transform(eName.begin(), eName.end(), eName.begin(), ::tolower);

                // Check against all candidates
                for(const auto& candidate : mmhCandidates) {
                    std::string candLower = candidate;
                    std::transform(candLower.begin(), candLower.end(), candLower.begin(), ::tolower);

                    if (eName == candLower) {
                        std::cout << "  Found MMH (" << e.name << ") in current ERF!" << std::endl;
                        std::vector<uint8_t> mmhData = state.currentErf->readEntry(e);
                        if (!mmhData.empty()) {
                            loadMMH(mmhData, state.currentModel);
                            foundMMH = true;
                        }
                        break;
                    }
                }
                if (foundMMH) break;
            }
        } else {
            // Check other ERFs
            ERFFile searchErf;
            if (searchErf.open(erfPath)) {
                for (const auto& e : searchErf.entries()) {
                    std::string eName = e.name;
                    std::transform(eName.begin(), eName.end(), eName.begin(), ::tolower);

                    // Check against all candidates
                    for(const auto& candidate : mmhCandidates) {
                        std::string candLower = candidate;
                        std::transform(candLower.begin(), candLower.end(), candLower.begin(), ::tolower);

                        if (eName == candLower) {
                            std::cout << "  Found MMH (" << e.name << ") in: " << erfPath << std::endl;
                            std::vector<uint8_t> mmhData = searchErf.readEntry(e);
                            if (!mmhData.empty()) {
                                loadMMH(mmhData, state.currentModel);
                                foundMMH = true;
                            }
                            break;
                        }
                    }
                    if (foundMMH) break;
                }
            }
        }
        if (foundMMH) break;
    }

    if (!foundMMH) {
        std::cout << "WARNING: Could not find MMH for " << baseName << " in any loaded ERF." << std::endl;
    }

    // ---------------------------------------------------------
    // GENERATE PHY CANDIDATES (Logic reused for completeness)
    // ---------------------------------------------------------
    std::vector<std::string> phyCandidates;
    phyCandidates.push_back(baseName + ".phy");
    phyCandidates.push_back(baseName + "a.phy");

    if (lastUnderscore != std::string::npos) {
        std::string variantA = baseName;
        variantA.insert(lastUnderscore, "a");
        phyCandidates.push_back(variantA + ".phy");
    }

    // Scan all loaded ERFs for matching PHY files
    bool foundPhy = false;
    for (const auto& erfPath : state.erfFiles) {
        ERFFile phyErf;
        if (phyErf.open(erfPath)) {
            for (const auto& e : phyErf.entries()) {
                std::string eName = e.name;
                std::transform(eName.begin(), eName.end(), eName.begin(), ::tolower);

                // Check this entry against all candidates
                for (const auto& candidate : phyCandidates) {
                    std::string candLower = candidate;
                    std::transform(candLower.begin(), candLower.end(), candLower.begin(), ::tolower);

                    if (eName == candLower) {
                        std::cout << "  Found PHY (" << e.name << ")" << std::endl;
                        std::vector<uint8_t> phyData = phyErf.readEntry(e);
                        if (!phyData.empty()) {
                            loadPHY(phyData, state.currentModel);
                        }
                        foundPhy = true;
                        break;
                    }
                }
                if (foundPhy) break;
            }
        }
        if (foundPhy) break;
    }

    // Calculate bounds and position camera
    if (!model.meshes.empty()) {
        float minX = model.meshes[0].minX, maxX = model.meshes[0].maxX;
        float minY = model.meshes[0].minY, maxY = model.meshes[0].maxY;
        float minZ = model.meshes[0].minZ, maxZ = model.meshes[0].maxZ;

        for (const auto& mesh : model.meshes) {
            if (mesh.minX < minX) minX = mesh.minX;
            if (mesh.maxX > maxX) maxX = mesh.maxX;
            if (mesh.minY < minY) minY = mesh.minY;
            if (mesh.maxY > maxY) maxY = mesh.maxY;
            if (mesh.minZ < minZ) minZ = mesh.minZ;
            if (mesh.maxZ > maxZ) maxZ = mesh.maxZ;
        }

        float cx = (minX + maxX) / 2.0f;
        float cy = (minY + maxY) / 2.0f;
        float cz = (minZ + maxZ) / 2.0f;
        float dx = maxX - minX;
        float dy = maxY - minY;
        float dz = maxZ - minZ;
        float radius = std::sqrt(dx*dx + dy*dy + dz*dz) / 2.0f;

        state.camera.lookAt(cx, cy, cz, radius * 2.5f);
    }

    return true;
}
// Helper function to draw a solid box (for non-wireframe collision mode)
void drawSolidBox(float x, float y, float z) {
    glBegin(GL_QUADS);
    // Front face
    glNormal3f(0, 0, 1);
    glVertex3f(-x, -y, z); glVertex3f(x, -y, z);
    glVertex3f(x, y, z); glVertex3f(-x, y, z);
    // Back face
    glNormal3f(0, 0, -1);
    glVertex3f(x, -y, -z); glVertex3f(-x, -y, -z);
    glVertex3f(-x, y, -z); glVertex3f(x, y, -z);
    // Top face
    glNormal3f(0, 1, 0);
    glVertex3f(-x, y, -z); glVertex3f(-x, y, z);
    glVertex3f(x, y, z); glVertex3f(x, y, -z);
    // Bottom face
    glNormal3f(0, -1, 0);
    glVertex3f(-x, -y, -z); glVertex3f(x, -y, -z);
    glVertex3f(x, -y, z); glVertex3f(-x, -y, z);
    // Right face
    glNormal3f(1, 0, 0);
    glVertex3f(x, -y, -z); glVertex3f(x, y, -z);
    glVertex3f(x, y, z); glVertex3f(x, -y, z);
    // Left face
    glNormal3f(-1, 0, 0);
    glVertex3f(-x, -y, z); glVertex3f(-x, y, z);
    glVertex3f(-x, y, -z); glVertex3f(-x, -y, -z);
    glEnd();
}

// Helper function to draw a solid sphere
void drawSolidSphere(float radius, int slices, int stacks) {
    for (int i = 0; i < stacks; i++) {
        float lat0 = 3.14159f * (-0.5f + float(i) / stacks);
        float z0 = radius * std::sin(lat0);
        float zr0 = radius * std::cos(lat0);

        float lat1 = 3.14159f * (-0.5f + float(i + 1) / stacks);
        float z1 = radius * std::sin(lat1);
        float zr1 = radius * std::cos(lat1);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float lng = 2.0f * 3.14159f * float(j) / slices;
            float x = std::cos(lng);
            float y = std::sin(lng);

            glNormal3f(x * zr0 / radius, y * zr0 / radius, z0 / radius);
            glVertex3f(x * zr0, y * zr0, z0);

            glNormal3f(x * zr1 / radius, y * zr1 / radius, z1 / radius);
            glVertex3f(x * zr1, y * zr1, z1);
        }
        glEnd();
    }
}

// Helper function to draw a solid capsule
void drawSolidCapsule(float radius, float height, int slices, int stacks) {
    float halfHeight = height / 2.0f;

    // Draw cylinder body
    glBegin(GL_QUAD_STRIP);
    for (int j = 0; j <= slices; j++) {
        float lng = 2.0f * 3.14159f * float(j) / slices;
        float x = std::cos(lng);
        float y = std::sin(lng);

        glNormal3f(x, y, 0);
        glVertex3f(radius * x, radius * y, -halfHeight);
        glVertex3f(radius * x, radius * y, halfHeight);
    }
    glEnd();

    // Draw top hemisphere
    for (int i = 0; i < stacks / 2; i++) {
        float lat0 = 3.14159f * float(i) / stacks;
        float z0 = radius * std::sin(lat0);
        float zr0 = radius * std::cos(lat0);

        float lat1 = 3.14159f * float(i + 1) / stacks;
        float z1 = radius * std::sin(lat1);
        float zr1 = radius * std::cos(lat1);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float lng = 2.0f * 3.14159f * float(j) / slices;
            float x = std::cos(lng);
            float y = std::sin(lng);

            glNormal3f(x * zr0 / radius, y * zr0 / radius, z0 / radius);
            glVertex3f(x * zr0, y * zr0, z0 + halfHeight);

            glNormal3f(x * zr1 / radius, y * zr1 / radius, z1 / radius);
            glVertex3f(x * zr1, y * zr1, z1 + halfHeight);
        }
        glEnd();
    }

    // Draw bottom hemisphere
    for (int i = 0; i < stacks / 2; i++) {
        float lat0 = -3.14159f * float(i) / stacks;
        float z0 = radius * std::sin(lat0);
        float zr0 = radius * std::cos(lat0);

        float lat1 = -3.14159f * float(i + 1) / stacks;
        float z1 = radius * std::sin(lat1);
        float zr1 = radius * std::cos(lat1);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float lng = 2.0f * 3.14159f * float(j) / slices;
            float x = std::cos(lng);
            float y = std::sin(lng);

            glNormal3f(x * zr0 / radius, y * zr0 / radius, z0 / radius);
            glVertex3f(x * zr0, y * zr0, z0 - halfHeight);

            glNormal3f(x * zr1 / radius, y * zr1 / radius, z1 / radius);
            glVertex3f(x * zr1, y * zr1, z1 - halfHeight);
        }
        glEnd();
    }
}

// Render model using immediate mode OpenGL
void renderModel(const Model& model, const Camera& camera, const RenderSettings& settings, int width, int height) {
    glEnable(GL_DEPTH_TEST);

    // Set up projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    float fov = 45.0f * 3.14159f / 180.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    float top = nearPlane * std::tan(fov / 2.0f);
    float right = top * aspect;
    glFrustum(-right, right, -top, top, nearPlane, farPlane);

    // Set up modelview matrix for fly camera
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Apply camera rotation (pitch then yaw)
    glRotatef(-camera.pitch * 180.0f / 3.14159f, 1, 0, 0);
    glRotatef(-camera.yaw * 180.0f / 3.14159f, 0, 1, 0);
    // Then translate
    glTranslatef(-camera.x, -camera.y, -camera.z);

    // Rotate world so Z is up (Dragon Age uses Z-up, OpenGL uses Y-up)
    glRotatef(-90.0f, 1, 0, 0);
    // Flip model to face correct direction (rotate 180 around Z)
    glRotatef(180.0f, 0, 0, 1);

    // Draw grid if enabled (on XY plane since Z is up)
    if (settings.showGrid) {
        glLineWidth(1.0f);
        glBegin(GL_LINES);
        glColor3f(0.3f, 0.3f, 0.3f);
        float gridSize = 10.0f;
        float gridStep = 1.0f;
        for (float i = -gridSize; i <= gridSize; i += gridStep) {
            // Lines along X
            glVertex3f(-gridSize, i, 0);
            glVertex3f(gridSize, i, 0);
            // Lines along Y
            glVertex3f(i, -gridSize, 0);
            glVertex3f(i, gridSize, 0);
        }
        glEnd();
    }

    // Draw axes if enabled (X=red, Y=green, Z=blue pointing up)
    if (settings.showAxes) {
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        // X axis - red
        glColor3f(1, 0, 0);
        glVertex3f(0, 0, 0); glVertex3f(2, 0, 0);
        // Y axis - green
        glColor3f(0, 1, 0);
        glVertex3f(0, 0, 0); glVertex3f(0, 2, 0);
        // Z axis - blue (pointing UP)
        glColor3f(0, 0, 1);
        glVertex3f(0, 0, 0); glVertex3f(0, 0, 2);
        glEnd();
        glLineWidth(1.0f);
    }

    // Render meshes
    if (!model.meshes.empty()) {
        if (settings.wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glColor3f(0.8f, 0.8f, 0.8f);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_LIGHTING);
            glEnable(GL_LIGHT0);
            glEnable(GL_COLOR_MATERIAL);

            // Set up light
            float lightPos[] = {1.0f, 1.0f, 1.0f, 0.0f};
            float lightAmbient[] = {0.3f, 0.3f, 0.3f, 1.0f};
            float lightDiffuse[] = {0.7f, 0.7f, 0.7f, 1.0f};
            glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
            glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
            glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);

            glColor3f(0.7f, 0.7f, 0.7f);
        }

        for (size_t meshIdx = 0; meshIdx < model.meshes.size(); meshIdx++) {
            // Check per-mesh visibility
            if (meshIdx < settings.meshVisible.size() && settings.meshVisible[meshIdx] == 0) {
                continue;
            }

            const auto& mesh = model.meshes[meshIdx];
            glBegin(GL_TRIANGLES);
            for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                for (int j = 0; j < 3; j++) {
                    const auto& v = mesh.vertices[mesh.indices[i + j]];
                    glNormal3f(v.nx, v.ny, v.nz);
                    glVertex3f(v.x, v.y, v.z);
                }
            }
            glEnd();
        }

        if (!settings.wireframe) {
            glDisable(GL_LIGHTING);
            glDisable(GL_LIGHT0);
            glDisable(GL_COLOR_MATERIAL);
        }

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // Render collision shapes (cyan color)
    if (settings.showCollision && !model.collisionShapes.empty()) {
        bool wireframe = settings.collisionWireframe;

        if (wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glColor3f(0.0f, 1.0f, 1.0f);  // Cyan solid for wireframe
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(0.0f, 1.0f, 1.0f, 0.3f);  // Cyan transparent for solid
        }
        glLineWidth(2.0f);
        glDisable(GL_LIGHTING);

        for (const auto& shape : model.collisionShapes) {
            glPushMatrix();

            // Apply position
            glTranslatef(shape.posX, shape.posY, shape.posZ);

            // Apply rotation (quaternion to axis-angle)
            float rotW = shape.rotW;
            if (rotW > 1.0f) rotW = 1.0f;
            if (rotW < -1.0f) rotW = -1.0f;
            if (rotW < 0.9999f && rotW > -0.9999f) {
                float angle = 2.0f * std::acos(rotW) * 180.0f / 3.14159f;
                float s = std::sqrt(1.0f - rotW * rotW);
                if (s > 0.001f) {
                    glRotatef(angle, shape.rotX / s, shape.rotY / s, shape.rotZ / s);
                }
            }

            switch (shape.type) {
                case CollisionShapeType::Box: {
                    float x = shape.boxX, y = shape.boxY, z = shape.boxZ;
                    if (wireframe) {
                        // Wireframe box
                        glBegin(GL_LINE_LOOP);
                        glVertex3f(-x, -y, -z); glVertex3f(x, -y, -z);
                        glVertex3f(x, y, -z); glVertex3f(-x, y, -z);
                        glEnd();
                        glBegin(GL_LINE_LOOP);
                        glVertex3f(-x, -y, z); glVertex3f(x, -y, z);
                        glVertex3f(x, y, z); glVertex3f(-x, y, z);
                        glEnd();
                        glBegin(GL_LINES);
                        glVertex3f(-x, -y, -z); glVertex3f(-x, -y, z);
                        glVertex3f(x, -y, -z); glVertex3f(x, -y, z);
                        glVertex3f(x, y, -z); glVertex3f(x, y, z);
                        glVertex3f(-x, y, -z); glVertex3f(-x, y, z);
                        glEnd();
                    } else {
                        drawSolidBox(x, y, z);
                    }
                    break;
                }
                case CollisionShapeType::Sphere: {
                    float r = shape.radius;
                    if (wireframe) {
                        int segments = 24;
                        for (int plane = 0; plane < 3; plane++) {
                            glBegin(GL_LINE_LOOP);
                            for (int i = 0; i < segments; i++) {
                                float a = 2.0f * 3.14159f * float(i) / segments;
                                float c = r * std::cos(a);
                                float s = r * std::sin(a);
                                if (plane == 0) glVertex3f(c, s, 0);
                                else if (plane == 1) glVertex3f(c, 0, s);
                                else glVertex3f(0, c, s);
                            }
                            glEnd();
                        }
                    } else {
                        drawSolidSphere(r, 16, 12);
                    }
                    break;
                }
                case CollisionShapeType::Capsule: {
                    int segments = 24;
                    float r = shape.radius;
                    float h = shape.height / 2.0f;
                    if (wireframe) {
                        // Top and bottom circles
                        for (float zOff : {-h, h}) {
                            glBegin(GL_LINE_LOOP);
                            for (int i = 0; i < segments; i++) {
                                float a = 2.0f * 3.14159f * float(i) / segments;
                                glVertex3f(r * std::cos(a), r * std::sin(a), zOff);
                            }
                            glEnd();
                        }
                        // Vertical lines
                        glBegin(GL_LINES);
                        for (int i = 0; i < 4; i++) {
                            float a = 2.0f * 3.14159f * float(i) / 4;
                            glVertex3f(r * std::cos(a), r * std::sin(a), -h);
                            glVertex3f(r * std::cos(a), r * std::sin(a), h);
                        }
                        glEnd();
                        // Hemisphere caps
                        for (float zSign : {-1.0f, 1.0f}) {
                            for (int j = 1; j <= 4; j++) {
                                float lat = (3.14159f / 2.0f) * float(j) / 4;
                                float zOff = r * std::sin(lat) * zSign + h * zSign;
                                float rOff = r * std::cos(lat);
                                glBegin(GL_LINE_LOOP);
                                for (int i = 0; i < segments; i++) {
                                    float a = 2.0f * 3.14159f * float(i) / segments;
                                    glVertex3f(rOff * std::cos(a), rOff * std::sin(a), zOff);
                                }
                                glEnd();
                            }
                        }
                    } else {
                        drawSolidCapsule(r, shape.height, 16, 12);
                    }
                    break;
                }
                case CollisionShapeType::Mesh: {
                    // Draw collision mesh
                    if (!shape.meshVerts.empty() && !shape.meshIndices.empty()) {
                        if (wireframe) {
                            // Draw as wireframe triangles
                            for (size_t i = 0; i + 2 < shape.meshIndices.size(); i += 3) {
                                uint32_t i0 = shape.meshIndices[i];
                                uint32_t i1 = shape.meshIndices[i + 1];
                                uint32_t i2 = shape.meshIndices[i + 2];

                                if (i0 * 3 + 2 < shape.meshVerts.size() &&
                                    i1 * 3 + 2 < shape.meshVerts.size() &&
                                    i2 * 3 + 2 < shape.meshVerts.size()) {

                                    glBegin(GL_LINE_LOOP);
                                    glVertex3f(shape.meshVerts[i0 * 3],
                                              shape.meshVerts[i0 * 3 + 1],
                                              shape.meshVerts[i0 * 3 + 2]);
                                    glVertex3f(shape.meshVerts[i1 * 3],
                                              shape.meshVerts[i1 * 3 + 1],
                                              shape.meshVerts[i1 * 3 + 2]);
                                    glVertex3f(shape.meshVerts[i2 * 3],
                                              shape.meshVerts[i2 * 3 + 1],
                                              shape.meshVerts[i2 * 3 + 2]);
                                    glEnd();
                                }
                            }
                        } else {
                            // Draw as solid triangles with computed normals
                            glBegin(GL_TRIANGLES);
                            for (size_t i = 0; i + 2 < shape.meshIndices.size(); i += 3) {
                                uint32_t i0 = shape.meshIndices[i];
                                uint32_t i1 = shape.meshIndices[i + 1];
                                uint32_t i2 = shape.meshIndices[i + 2];

                                if (i0 * 3 + 2 < shape.meshVerts.size() &&
                                    i1 * 3 + 2 < shape.meshVerts.size() &&
                                    i2 * 3 + 2 < shape.meshVerts.size()) {

                                    float v0x = shape.meshVerts[i0 * 3];
                                    float v0y = shape.meshVerts[i0 * 3 + 1];
                                    float v0z = shape.meshVerts[i0 * 3 + 2];
                                    float v1x = shape.meshVerts[i1 * 3];
                                    float v1y = shape.meshVerts[i1 * 3 + 1];
                                    float v1z = shape.meshVerts[i1 * 3 + 2];
                                    float v2x = shape.meshVerts[i2 * 3];
                                    float v2y = shape.meshVerts[i2 * 3 + 1];
                                    float v2z = shape.meshVerts[i2 * 3 + 2];

                                    // Compute face normal
                                    float e1x = v1x - v0x, e1y = v1y - v0y, e1z = v1z - v0z;
                                    float e2x = v2x - v0x, e2y = v2y - v0y, e2z = v2z - v0z;
                                    float nx = e1y * e2z - e1z * e2y;
                                    float ny = e1z * e2x - e1x * e2z;
                                    float nz = e1x * e2y - e1y * e2x;
                                    float len = std::sqrt(nx*nx + ny*ny + nz*nz);
                                    if (len > 0.0001f) {
                                        nx /= len; ny /= len; nz /= len;
                                    }

                                    glNormal3f(nx, ny, nz);
                                    glVertex3f(v0x, v0y, v0z);
                                    glVertex3f(v1x, v1y, v1z);
                                    glVertex3f(v2x, v2y, v2z);
                                }
                            }
                            glEnd();
                        }
                    }
                    break;
                }
            }

            glPopMatrix();
        }

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glLineWidth(1.0f);
        glDisable(GL_BLEND);
    }

    // Draw skeleton
    if (settings.showSkeleton && !model.skeleton.bones.empty()) {
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);  // Draw on top
        glLineWidth(2.0f);

        // Draw bones as lines from parent to child
        glBegin(GL_LINES);
        for (const auto& bone : model.skeleton.bones) {
            if (bone.parentIndex >= 0) {
                const Bone& parent = model.skeleton.bones[bone.parentIndex];

                // Parent position (green)
                glColor3f(0.0f, 1.0f, 0.0f);
                glVertex3f(parent.worldPosX, parent.worldPosY, parent.worldPosZ);

                // Child position (yellow)
                glColor3f(1.0f, 1.0f, 0.0f);
                glVertex3f(bone.worldPosX, bone.worldPosY, bone.worldPosZ);
            }
        }
        glEnd();

        // Draw bone positions as points
        glPointSize(6.0f);
        glBegin(GL_POINTS);
        for (const auto& bone : model.skeleton.bones) {
            // Root bones in red, others in yellow
            if (bone.parentIndex < 0) {
                glColor3f(1.0f, 0.0f, 0.0f);
            } else {
                glColor3f(1.0f, 1.0f, 0.0f);
            }
            glVertex3f(bone.worldPosX, bone.worldPosY, bone.worldPosZ);
        }
        glEnd();

        glPointSize(1.0f);
        glLineWidth(1.0f);
        glEnable(GL_DEPTH_TEST);
    }

    glDisable(GL_DEPTH_TEST);
}

int main() {
    if (!glfwInit()) return -1;

    // Use OpenGL 2.1 for compatibility with immediate mode rendering
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Dragon Age Model Browser", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 120");

    AppState state;
    state.extractPath = (fs::path(getExeDir()) / "extracted").string();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Handle mouse input for camera - RIGHT CLICK to look around
        if (!io.WantCaptureMouse) {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);

            // Right mouse - look around (fly cam style)
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                // Clear ImGui focus so WASD works immediately
                ImGui::SetWindowFocus(nullptr);

                if (state.isPanning) {
                    float dx = static_cast<float>(mx - state.lastMouseX);
                    float dy = static_cast<float>(my - state.lastMouseY);
                    state.camera.rotate(-dx * state.camera.lookSensitivity, -dy * state.camera.lookSensitivity);
                }
                state.isPanning = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

                // RMB + scroll to adjust speed
                float scroll = io.MouseWheel;
                if (scroll != 0.0f) {
                    state.camera.moveSpeed *= (scroll > 0) ? 1.2f : 0.8f;
                    if (state.camera.moveSpeed < 0.1f) state.camera.moveSpeed = 0.1f;
                    if (state.camera.moveSpeed > 100.0f) state.camera.moveSpeed = 100.0f;
                }
            } else {
                if (state.isPanning) {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                }
                state.isPanning = false;
            }

            state.lastMouseX = mx;
            state.lastMouseY = my;
        }

        // WASD movement (fly cam style)
        if (!io.WantCaptureKeyboard) {
            float deltaTime = io.DeltaTime;
            float speed = state.camera.moveSpeed * deltaTime;

            // Move faster with shift
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
                speed *= 3.0f;
            }

            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                state.camera.moveForward(speed);
            }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                state.camera.moveForward(-speed);
            }
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                state.camera.moveRight(-speed);
            }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                state.camera.moveRight(speed);
            }
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
                state.camera.moveUp(speed);
            }
            if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
                state.camera.moveUp(-speed);
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Browser window (popup style)
        if (state.showBrowser) {
            ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
            ImGui::Begin("ERF Browser", &state.showBrowser, ImGuiWindowFlags_MenuBar);

            if (ImGui::BeginMenuBar()) {
                if (ImGui::Button("Open Folder")) {
                    IGFD::FileDialogConfig config;
                    config.path = state.selectedFolder.empty() ? "." : state.selectedFolder;
                    ImGuiFileDialog::Instance()->OpenDialog("ChooseFolder", "Choose Folder", nullptr, config);
                }

                if (!state.statusMessage.empty()) {
                    ImGui::SameLine();
                    ImGui::Text("%s", state.statusMessage.c_str());
                }
                ImGui::EndMenuBar();
            }

            // Two-column layout
            ImGui::Columns(2, "browser_columns");

            // Left: ERF file list
            ImGui::Text("ERF Files (%zu)", state.erfFiles.size());
            ImGui::Separator();

            ImGui::BeginChild("ERFList", ImVec2(0, 0), true);
            for (int i = 0; i < static_cast<int>(state.erfFiles.size()); i++) {
                std::string displayName = fs::path(state.erfFiles[i]).filename().string();
                bool selected = (i == state.selectedErfIndex);

                if (ImGui::Selectable(displayName.c_str(), selected)) {
                    if (state.selectedErfIndex != i) {
                        state.selectedErfIndex = i;
                        state.selectedEntryIndex = -1;
                        state.currentErf = std::make_unique<ERFFile>();
                        if (!state.currentErf->open(state.erfFiles[i])) {
                            state.statusMessage = "Failed to open";
                            state.currentErf.reset();
                        } else {
                            state.statusMessage = versionToString(state.currentErf->version());
                        }
                    }
                }
            }
            ImGui::EndChild();

            ImGui::NextColumn();

            // Right: Entry list
            if (state.currentErf) {
                ImGui::Text("Contents (%zu)", state.currentErf->entries().size());

                if (state.currentErf->encryption() != 0) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "[Enc]");
                }
                if (state.currentErf->compression() != 0) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 1, 1), "[Comp]");
                }

                ImGui::Separator();

                ImGui::BeginChild("EntryList", ImVec2(0, 0), true);
                for (int i = 0; i < static_cast<int>(state.currentErf->entries().size()); i++) {
                    const auto& entry = state.currentErf->entries()[i];
                    bool selected = (i == state.selectedEntryIndex);

                    // Highlight model and material files
                    bool isModel = isModelFile(entry.name);
                    bool isMao = isMaoFile(entry.name);
                    bool isPhy = isPhyFile(entry.name);

                    if (isModel) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));  // Green
                    } else if (isMao) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.4f, 1.0f));  // Orange
                    } else if (isPhy) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 1.0f, 1.0f));  // Magenta
                    }

                    char label[256];
                    snprintf(label, sizeof(label), "%s##%d", entry.name.c_str(), i);

                    if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                        state.selectedEntryIndex = i;

                        // Double-click to load model
                        if (ImGui::IsMouseDoubleClicked(0)) {
                            if (isModel) {
                                if (loadModelFromEntry(state, entry)) {
                                    state.statusMessage = "Loaded: " + entry.name + " (" +
                                        std::to_string(state.currentModel.meshes.size()) + " meshes)";
                                    state.showRenderSettings = true;
                                } else {
                                    state.statusMessage = "Failed to parse: " + entry.name;
                                    state.showRenderSettings = true;
                                }
                            } else if (isMao) {
                                // Load MAO as plaintext
                                std::vector<uint8_t> data = state.currentErf->readEntry(entry);
                                if (!data.empty()) {
                                    state.maoContent = std::string(data.begin(), data.end());
                                    state.maoFileName = entry.name;
                                    state.showMaoViewer = true;
                                    state.statusMessage = "Opened: " + entry.name;
                                }
                            }
                        }
                    }

                    if (isModel || isMao || isPhy) {
                        ImGui::PopStyleColor();
                    }

                    // Tooltip with more info
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("Size: %u bytes", entry.length);
                        if (entry.packed_length != entry.length) {
                            ImGui::Text("Packed: %u bytes", entry.packed_length);
                        }
                        if (isModel) {
                            ImGui::Text("Double-click to load model");
                        } else if (isMao) {
                            ImGui::Text("Double-click to view material");
                        } else if (isPhy) {
                            ImGui::Text("Collision data (auto-loaded with model)");
                        }
                        ImGui::EndTooltip();
                    }
                }
                ImGui::EndChild();
            } else {
                ImGui::Text("Select an ERF file");
            }

            ImGui::Columns(1);
            ImGui::End();
        }

        // File dialog
        if (ImGuiFileDialog::Instance()->Display("ChooseFolder", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                state.selectedFolder = ImGuiFileDialog::Instance()->GetCurrentPath();
                state.erfFiles = scanForERFFiles(state.selectedFolder);
                state.selectedErfIndex = -1;
                state.currentErf.reset();
                state.selectedEntryIndex = -1;
                state.statusMessage = "Found " + std::to_string(state.erfFiles.size()) + " ERF files";
            }
            ImGuiFileDialog::Instance()->Close();
        }

        // Main menu bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Browser", nullptr, &state.showBrowser);
                ImGui::MenuItem("Render Settings", nullptr, &state.showRenderSettings);
                ImGui::EndMenu();
            }

            if (state.hasModel) {
                ImGui::SameLine(ImGui::GetWindowWidth() - 500);
                ImGui::Text("Model: %s | RMB+Mouse: Look | WASD: Move | Space/Ctrl: Up/Down | Shift: Fast", state.currentModel.name.c_str());
            }

            ImGui::EndMainMenuBar();
        }

        // Render Settings window - auto-size to fit content
        if (state.showRenderSettings) {
            ImGui::SetNextWindowSizeConstraints(ImVec2(300, 100), ImVec2(500, 800));
            ImGui::Begin("Render Settings", &state.showRenderSettings, ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::Checkbox("Wireframe", &state.renderSettings.wireframe);
            ImGui::Checkbox("Show Axes", &state.renderSettings.showAxes);
            ImGui::Checkbox("Show Grid", &state.renderSettings.showGrid);
            ImGui::Checkbox("Show Collision", &state.renderSettings.showCollision);
            if (state.renderSettings.showCollision) {
                ImGui::SameLine();
                ImGui::Checkbox("Wireframe##coll", &state.renderSettings.collisionWireframe);
            }

            ImGui::Checkbox("Show Skeleton", &state.renderSettings.showSkeleton);
            if (state.renderSettings.showSkeleton && !state.currentModel.skeleton.bones.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%zu bones)", state.currentModel.skeleton.bones.size());
            }

            ImGui::Separator();
            ImGui::Text("Camera Speed: %.1f", state.camera.moveSpeed);
            ImGui::SliderFloat("##speed", &state.camera.moveSpeed, 0.1f, 100.0f, "%.1f");
            ImGui::TextDisabled("(RMB + Scroll to adjust)");

            if (state.hasModel) {
                ImGui::Separator();
                size_t totalVerts = 0, totalTris = 0;
                for (const auto& m : state.currentModel.meshes) {
                    totalVerts += m.vertices.size();
                    totalTris += m.indices.size() / 3;
                }
                ImGui::Text("Total: %zu meshes, %zu verts, %zu tris",
                    state.currentModel.meshes.size(), totalVerts, totalTris);

                // Mesh list with visibility and material names
                if (state.currentModel.meshes.size() >= 1) {
                    ImGui::Separator();
                    ImGui::Text("Meshes:");

                    // Ensure visibility array is correct size
                    if (state.renderSettings.meshVisible.size() != state.currentModel.meshes.size()) {
                        state.renderSettings.initMeshVisibility(state.currentModel.meshes.size());
                    }

                    // Calculate height based on number of meshes (max 300)
                    float listHeight = std::min(300.0f, state.currentModel.meshes.size() * 70.0f + 20.0f);
                    ImGui::BeginChild("MeshList", ImVec2(0, listHeight), true);
                    for (size_t i = 0; i < state.currentModel.meshes.size(); i++) {
                        const auto& mesh = state.currentModel.meshes[i];

                        ImGui::PushID(static_cast<int>(i));

                        // Visibility checkbox
                        bool visible = state.renderSettings.meshVisible[i] != 0;
                        if (ImGui::Checkbox("##vis", &visible)) {
                            state.renderSettings.meshVisible[i] = visible ? 1 : 0;
                        }
                        ImGui::SameLine();

                        // Mesh name
                        std::string meshLabel = mesh.name.empty() ?
                            ("Mesh " + std::to_string(i)) : mesh.name;
                        ImGui::Text("%s", meshLabel.c_str());

                        // Mesh details indented
                        ImGui::Indent();
                        ImGui::TextDisabled("%zu verts, %zu tris",
                            mesh.vertices.size(), mesh.indices.size() / 3);

                        // Material name (.mao)
                        if (!mesh.materialName.empty()) {
                            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f),
                                "Material: %s.mao", mesh.materialName.c_str());
                        } else {
                            ImGui::TextDisabled("Material: (none)");
                        }

                        // UV View button
                        if (ImGui::SmallButton("View UVs")) {
                            state.selectedMeshForUv = static_cast<int>(i);
                            state.showUvViewer = true;
                        }

                        ImGui::Unindent();
                        ImGui::PopID();

                        if (i < state.currentModel.meshes.size() - 1) {
                            ImGui::Spacing();
                        }
                    }
                    ImGui::EndChild();

                    // Show all / Hide all buttons
                    if (state.currentModel.meshes.size() > 1) {
                        if (ImGui::Button("Show All")) {
                            for (size_t i = 0; i < state.renderSettings.meshVisible.size(); i++) {
                                state.renderSettings.meshVisible[i] = 1;
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Hide All")) {
                            for (size_t i = 0; i < state.renderSettings.meshVisible.size(); i++) {
                                state.renderSettings.meshVisible[i] = 0;
                            }
                        }
                    }
                }

                // Collision shapes info
                if (!state.currentModel.collisionShapes.empty()) {
                    ImGui::Separator();
                    ImGui::Text("Collision Shapes: %zu", state.currentModel.collisionShapes.size());
                    for (const auto& shape : state.currentModel.collisionShapes) {
                        const char* typeStr = "Unknown";
                        switch (shape.type) {
                            case CollisionShapeType::Box: typeStr = "Box"; break;
                            case CollisionShapeType::Sphere: typeStr = "Sphere"; break;
                            case CollisionShapeType::Capsule: typeStr = "Capsule"; break;
                            case CollisionShapeType::Mesh: typeStr = "Mesh"; break;
                        }
                        ImGui::BulletText("%s: %s", shape.name.c_str(), typeStr);
                    }
                }

                // Skeleton bone list
                if (!state.currentModel.skeleton.bones.empty()) {
                    ImGui::Separator();
                    if (ImGui::TreeNode("Skeleton", "Skeleton (%zu bones)", state.currentModel.skeleton.bones.size())) {
                        // Calculate height based on number of bones (max 300)
                        float boneListHeight = std::min(300.0f, state.currentModel.skeleton.bones.size() * 20.0f + 20.0f);
                        ImGui::BeginChild("BoneList", ImVec2(0, boneListHeight), true);

                        for (size_t i = 0; i < state.currentModel.skeleton.bones.size(); i++) {
                            const auto& bone = state.currentModel.skeleton.bones[i];

                            // Color root bones differently
                            if (bone.parentIndex < 0) {
                                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "%s (root)", bone.name.c_str());
                            } else {
                                ImGui::Text("%s", bone.name.c_str());
                                ImGui::SameLine();
                                ImGui::TextDisabled("-> %s", bone.parentName.c_str());
                            }
                        }

                        ImGui::EndChild();
                        ImGui::TreePop();
                    }
                }
            }

            ImGui::End();
        }

        // MAO Viewer window
        if (state.showMaoViewer) {
            ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
            ImGui::Begin(("MAO Viewer - " + state.maoFileName).c_str(), &state.showMaoViewer);

            // Copy button
            if (ImGui::Button("Copy to Clipboard")) {
                ImGui::SetClipboardText(state.maoContent.c_str());
            }

            ImGui::Separator();

            // Scrollable text area
            ImGui::BeginChild("MaoContent", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted(state.maoContent.c_str());
            ImGui::EndChild();

            ImGui::End();
        }

        // UV Viewer window
        if (state.showUvViewer && state.hasModel && state.selectedMeshForUv >= 0 &&
            state.selectedMeshForUv < static_cast<int>(state.currentModel.meshes.size())) {

            const auto& mesh = state.currentModel.meshes[state.selectedMeshForUv];
            std::string title = "UV Viewer - " + (mesh.name.empty() ? "Mesh " + std::to_string(state.selectedMeshForUv) : mesh.name);

            ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
            ImGui::Begin(title.c_str(), &state.showUvViewer);

            ImVec2 canvasSize = ImGui::GetContentRegionAvail();
            float size = std::min(canvasSize.x, canvasSize.y - 20);
            if (size < 100) size = 100;

            ImVec2 canvasPos = ImGui::GetCursorScreenPos();
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            // Draw background
            drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + size, canvasPos.y + size),
                IM_COL32(40, 40, 40, 255));

            // Draw grid
            int gridLines = 8;
            for (int i = 0; i <= gridLines; i++) {
                float t = float(i) / gridLines;
                ImU32 col = (i == 0 || i == gridLines) ? IM_COL32(100, 100, 100, 255) : IM_COL32(60, 60, 60, 255);
                drawList->AddLine(
                    ImVec2(canvasPos.x + t * size, canvasPos.y),
                    ImVec2(canvasPos.x + t * size, canvasPos.y + size), col);
                drawList->AddLine(
                    ImVec2(canvasPos.x, canvasPos.y + t * size),
                    ImVec2(canvasPos.x + size, canvasPos.y + t * size), col);
            }

            // Draw UV triangles
            for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
                const auto& v0 = mesh.vertices[mesh.indices[i]];
                const auto& v1 = mesh.vertices[mesh.indices[i + 1]];
                const auto& v2 = mesh.vertices[mesh.indices[i + 2]];

                ImVec2 p0(canvasPos.x + v0.u * size, canvasPos.y + (1.0f - v0.v) * size);
                ImVec2 p1(canvasPos.x + v1.u * size, canvasPos.y + (1.0f - v1.v) * size);
                ImVec2 p2(canvasPos.x + v2.u * size, canvasPos.y + (1.0f - v2.v) * size);

                drawList->AddTriangle(p0, p1, p2, IM_COL32(0, 200, 255, 200), 1.0f);
            }

            // Reserve space for the canvas
            ImGui::Dummy(ImVec2(size, size));

            ImGui::Text("Triangles: %zu", mesh.indices.size() / 3);

            ImGui::End();
        }

        // Render
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render model in main area
        if (state.hasModel) {
            renderModel(state.currentModel, state.camera, state.renderSettings, display_w, display_h);
        } else {
            // Render empty scene with just grid/axes
            Model empty;
            renderModel(empty, state.camera, state.renderSettings, display_w, display_h);
        }

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}