#pragma once
#include "Mesh.h"
#include <string>
#include <vector>
#include <cstdint>

struct RMLPropInstance {
    std::string modelName;
    std::string modelFile;
    float posX = 0, posY = 0, posZ = 0;
    float orientX = 0, orientY = 0, orientZ = 0, orientW = 1;
    float scale = 1.0f;
    int32_t modelId = -1;
};

struct RMLSptInstance {
    float posX = 0, posY = 0, posZ = 0;
    float orientX = 0, orientY = 0, orientZ = 0, orientW = 1;
    int32_t treeId = -1;
    float scale = 1.0f;
};

struct RMLData {
    std::vector<RMLPropInstance> props;
    std::vector<RMLSptInstance> sptInstances;
    float roomPosX = 0, roomPosY = 0, roomPosZ = 0;
};


bool parseRML(const std::vector<uint8_t>& data, RMLData& outData);


void transformModelVertices(Model& model, float px, float py, float pz,
                           float qx, float qy, float qz, float qw,
                           float scale);