#pragma once
#include "Mesh.h"
#include "Gff.h"
#include <vector>
#include <cstdint>

struct VertexStreamDesc {
    uint32_t stream;
    uint32_t offset;
    uint32_t dataType;
    uint32_t usage;
    uint32_t usageIndex;
};

bool loadMSH(const std::vector<uint8_t>& data, Model& outModel);

float halfToFloat(uint16_t h);

void readDeclType(const std::vector<uint8_t>& data, uint32_t offset, uint32_t dataType, float* out);