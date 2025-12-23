#pragma once
#include "Mesh.h"
#include "Gff.h"
#include <vector>
#include <cstdint>

// Vertex stream descriptor
struct VertexStreamDesc {
    uint32_t stream;
    uint32_t offset;
    uint32_t dataType;
    uint32_t usage;
    uint32_t usageIndex;
};

// Load a model from MSH file data
bool loadMSH(const std::vector<uint8_t>& data, Model& outModel);

// Helper to convert half-float to float
float halfToFloat(uint16_t h);