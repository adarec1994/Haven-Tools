#pragma once
#include <string>
#include <vector>
#include <cstdint>

enum class SptSubmeshType : uint32_t {
    Branch   = 0,
    Frond    = 1,
    LeafCard = 2,
    LeafMesh = 3
};

struct SptSubmesh {
    SptSubmeshType        type;
    std::vector<float>    positions;
    std::vector<float>    normals;
    std::vector<float>    texcoords;
    std::vector<uint32_t> indices;
    uint32_t vertexCount() const { return (uint32_t)(positions.size() / 3); }
    uint32_t indexCount()  const { return (uint32_t)indices.size(); }
};

struct SptModel {
    std::vector<SptSubmesh> submeshes;
    float boundMin[3];
    float boundMax[3];
    // Texture filenames extracted from raw SPT data
    std::string branchTexture;       // e.g. "newbark.tga"
    std::vector<std::string> frondTextures;  // e.g. "FR_c_Redwood.tga"
    std::string compositeTexture;    // e.g. "tre_c_confsap_Diffuse.dds"
};

bool initSpeedTree();
void shutdownSpeedTree();
bool loadSptModel(const std::string& sptPath, SptModel& outModel);
void extractSptTextures(const std::vector<uint8_t>& rawData, SptModel& model);