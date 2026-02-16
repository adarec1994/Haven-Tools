#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <array>
#include <cmath>
constexpr int MAX_BONES_PER_VERTEX = 4;
struct Vertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
    float boneWeights[MAX_BONES_PER_VERTEX] = {0, 0, 0, 0};
    int boneIndices[MAX_BONES_PER_VERTEX] = {-1, -1, -1, -1};
};
struct Material {
    std::string name;
    std::string maoSource;
    std::string maoContent;
    std::string diffuseMap;
    std::string normalMap;
    std::string specularMap;
    std::string tintMap;
    std::string ageDiffuseMap;
    std::string ageNormalMap;
    std::string tattooMap;
    std::string browStubbleMap;
    std::string browStubbleNormalMap;
    float specularPower = 50.0f;
    float opacity = 1.0f;
    uint32_t diffuseTexId = 0;
    uint32_t normalTexId = 0;
    uint32_t specularTexId = 0;
    uint32_t tintTexId = 0;
    uint32_t ageDiffuseTexId = 0;
    uint32_t ageNormalTexId = 0;
    uint32_t tattooTexId = 0;
    uint32_t browStubbleTexId = 0;
    uint32_t browStubbleNormalTexId = 0;
    std::vector<uint8_t> diffuseData;
    int diffuseWidth = 0, diffuseHeight = 0;
    std::vector<uint8_t> normalData;
    int normalWidth = 0, normalHeight = 0;
    std::vector<uint8_t> specularData;
    int specularWidth = 0, specularHeight = 0;
    std::vector<uint8_t> tintData;
    int tintWidth = 0, tintHeight = 0;

    bool isTerrain = false;
    std::string paletteMap;
    std::string palNormalMap;
    std::string maskVMap;
    std::string maskAMap;
    std::string maskA2Map;
    std::string reliefMap;
    uint32_t paletteTexId = 0;
    uint32_t palNormalTexId = 0;
    uint32_t maskVTexId = 0;
    uint32_t maskATexId = 0;
    uint32_t maskA2TexId = 0;
    uint32_t reliefTexId = 0;
    float palDim[4] = {0.5f, 0.25f, 2.0f, 4.0f};     // cellW, cellH, numCols, numRows
    float palParam[4] = {0.0625f, 0.03125f, 0.375f, 0.1875f}; // padX, padY, usableW, usableH
    float uvScales[8] = {24.0f, 24.0f, 24.0f, 24.0f, 24.0f, 24.0f, 24.0f, 24.0f};
    float reliefScales[8] = {};

    bool isWater = false;
    float waveParams[12] = {};    // 3 waves: dirX, dirY, uvScale, 0 (from mat_vVSHWaterParams)
    float waterColor[4] = {1.0f, 0.5f, 0.5f, 0.0f};  // xyz = normal blend weights per layer (mat_vPSHWaterParams[0])
    float waterVisual[4] = {1.25f, 1.75f, 500.0f, 0.5f}; // fresnelPow, specIntensity, specPower, extra (mat_vPSHWaterParams[1])
    std::string waterNormalMap;     // mat_tNormalMap
    std::string waterDecalMap;      // mml_tDecal (foam/flow texture for flowing water)
    std::string waterMaskMap;       // mml_tWaterMask
};
enum class CollisionShapeType {
    Box,
    Sphere,
    Capsule,
    Mesh
};
struct CollisionShape {
    std::string name;
    std::string boneName;
    int boneIndex = -1;
    CollisionShapeType type = CollisionShapeType::Box;
    float localPosX = 0, localPosY = 0, localPosZ = 0;
    float localRotX = 0, localRotY = 0, localRotZ = 0, localRotW = 1;
    float posX = 0, posY = 0, posZ = 0;
    float rotX = 0, rotY = 0, rotZ = 0, rotW = 1;
    float boxX = 1, boxY = 1, boxZ = 1;
    float radius = 1;
    float height = 2;
    std::vector<float> meshVerts;
    std::vector<uint32_t> meshIndices;
    bool meshVertsWorldSpace = false;
};
struct Bone {
    std::string name;
    std::string parentName;
    int parentIndex = -1;
    float posX = 0, posY = 0, posZ = 0;
    float rotX = 0, rotY = 0, rotZ = 0, rotW = 1;
    float worldPosX = 0, worldPosY = 0, worldPosZ = 0;
    float worldRotX = 0, worldRotY = 0, worldRotZ = 0, worldRotW = 1;
    float invBindPosX = 0, invBindPosY = 0, invBindPosZ = 0;
    float invBindRotX = 0, invBindRotY = 0, invBindRotZ = 0, invBindRotW = 1;
};
struct Skeleton {
    std::vector<Bone> bones;
    int findBone(const std::string& name) const {
        for (size_t i = 0; i < bones.size(); i++) {
            if (bones[i].name == name) return (int)i;
        }
        return -1;
    }
};
struct Mesh {
    std::string name;
    std::string materialName;
    int materialIndex = -1;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<int> bonesUsed;
    bool hasSkinning = false;
    float minX = 0, minY = 0, minZ = 0;
    float maxX = 0, maxY = 0, maxZ = 0;
    std::vector<int> skinningBoneMap;
    bool skinningCacheBuilt = false;
    bool skipInvBind = false;
    bool alphaTest = false;
    std::string parentBoneName;
    float localPosX = 0, localPosY = 0, localPosZ = 0;
    float localRotX = 0, localRotY = 0, localRotZ = 0, localRotW = 1;
    void calculateBounds() {
        if (vertices.empty()) return;
        minX = maxX = vertices[0].x;
        minY = maxY = vertices[0].y;
        minZ = maxZ = vertices[0].z;
        for (const auto& v : vertices) {
            if (v.x < minX) minX = v.x;
            if (v.x > maxX) maxX = v.x;
            if (v.y < minY) minY = v.y;
            if (v.y > maxY) maxY = v.y;
            if (v.z < minZ) minZ = v.z;
            if (v.z > maxZ) maxZ = v.z;
        }
    }
    std::array<float, 3> center() const {
        return { (minX + maxX) / 2.0f, (minY + maxY) / 2.0f, (minZ + maxZ) / 2.0f };
    }
    float radius() const {
        float dx = maxX - minX;
        float dy = maxY - minY;
        float dz = maxZ - minZ;
        return std::sqrt(dx*dx + dy*dy + dz*dz) / 2.0f;
    }
};
struct Model {
    std::string name;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<CollisionShape> collisionShapes;
    Skeleton skeleton;
    std::vector<std::string> boneIndexArray;
    void calculateBounds() {
        for (auto& mesh : meshes) {
            mesh.calculateBounds();
        }
    }
    int findMaterial(const std::string& name) const {
        for (size_t i = 0; i < materials.size(); i++) {
            if (materials[i].name == name) return (int)i;
        }
        return -1;
    }
};
struct AnimKeyframe {
    float time;
    float x, y, z, w;
};
struct AnimTrack {
    std::string boneName;
    int boneIndex = -1;
    bool isRotation = false;
    bool isTranslation = false;
    std::vector<AnimKeyframe> keyframes;
};
struct Animation {
    std::string name;
    std::string filename;
    float duration = 0.0f;
    float frameRate = 30.0f;
    std::vector<AnimTrack> tracks;
};
bool loadMSH(const std::vector<uint8_t>& data, Model& outModel);