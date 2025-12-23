#include "model_loader.h"
#include <cstring>
#include <cmath>

// Convert half-float (16-bit) to float (32-bit)
float halfToFloat(uint16_t h) {
    uint32_t sign = (h >> 15) & 0x1;
    uint32_t exponent = (h >> 10) & 0x1F;
    uint32_t mantissa = h & 0x3FF;
    
    if (exponent == 0) {
        if (mantissa == 0) {
            // Zero
            uint32_t result = sign << 31;
            float f;
            std::memcpy(&f, &result, 4);
            return f;
        } else {
            // Denormalized
            while ((mantissa & 0x400) == 0) {
                mantissa <<= 1;
                exponent--;
            }
            exponent++;
            mantissa &= ~0x400;
        }
    } else if (exponent == 31) {
        // Inf/NaN
        uint32_t result = (sign << 31) | 0x7F800000 | (mantissa << 13);
        float f;
        std::memcpy(&f, &result, 4);
        return f;
    }
    
    exponent = exponent + (127 - 15);
    mantissa = mantissa << 13;
    
    uint32_t result = (sign << 31) | (exponent << 23) | mantissa;
    float f;
    std::memcpy(&f, &result, 4);
    return f;
}

// Read vertex data based on declaration type
void readDeclType(const std::vector<uint8_t>& data, uint32_t offset, uint32_t dataType, float* out) {
    auto readFloat = [&](uint32_t pos) -> float {
        if (pos + 4 > data.size()) return 0.0f;
        float val;
        std::memcpy(&val, &data[pos], 4);
        return val;
    };
    
    auto readShort = [&](uint32_t pos) -> int16_t {
        if (pos + 2 > data.size()) return 0;
        int16_t val;
        std::memcpy(&val, &data[pos], 2);
        return val;
    };
    
    auto readUShort = [&](uint32_t pos) -> uint16_t {
        if (pos + 2 > data.size()) return 0;
        uint16_t val;
        std::memcpy(&val, &data[pos], 2);
        return val;
    };
    
    auto readByte = [&](uint32_t pos) -> uint8_t {
        if (pos >= data.size()) return 0;
        return data[pos];
    };
    
    out[0] = out[1] = out[2] = out[3] = 0.0f;
    
    switch (dataType) {
        case VertexDeclType::FLOAT1:
            out[0] = readFloat(offset);
            break;
        case VertexDeclType::FLOAT2:
            out[0] = readFloat(offset);
            out[1] = readFloat(offset + 4);
            break;
        case VertexDeclType::FLOAT3:
            out[0] = readFloat(offset);
            out[1] = readFloat(offset + 4);
            out[2] = readFloat(offset + 8);
            break;
        case VertexDeclType::FLOAT4:
            out[0] = readFloat(offset);
            out[1] = readFloat(offset + 4);
            out[2] = readFloat(offset + 8);
            out[3] = readFloat(offset + 12);
            break;
        case VertexDeclType::COLOR:
        case VertexDeclType::UBYTE4:
            out[0] = readByte(offset) / 255.0f;
            out[1] = readByte(offset + 1) / 255.0f;
            out[2] = readByte(offset + 2) / 255.0f;
            out[3] = readByte(offset + 3) / 255.0f;
            break;
        case VertexDeclType::SHORT2:
            out[0] = static_cast<float>(readShort(offset));
            out[1] = static_cast<float>(readShort(offset + 2));
            break;
        case VertexDeclType::SHORT4:
            out[0] = static_cast<float>(readShort(offset));
            out[1] = static_cast<float>(readShort(offset + 2));
            out[2] = static_cast<float>(readShort(offset + 4));
            out[3] = static_cast<float>(readShort(offset + 6));
            break;
        case VertexDeclType::UBYTE4N:
            out[0] = readByte(offset) / 255.0f;
            out[1] = readByte(offset + 1) / 255.0f;
            out[2] = readByte(offset + 2) / 255.0f;
            out[3] = readByte(offset + 3) / 255.0f;
            break;
        case VertexDeclType::SHORT2N:
            out[0] = readShort(offset) / 32767.0f;
            out[1] = readShort(offset + 2) / 32767.0f;
            break;
        case VertexDeclType::SHORT4N:
            out[0] = readShort(offset) / 32767.0f;
            out[1] = readShort(offset + 2) / 32767.0f;
            out[2] = readShort(offset + 4) / 32767.0f;
            out[3] = readShort(offset + 6) / 32767.0f;
            break;
        case VertexDeclType::USHORT2N:
            out[0] = readUShort(offset) / 65535.0f;
            out[1] = readUShort(offset + 2) / 65535.0f;
            break;
        case VertexDeclType::USHORT4N:
            out[0] = readUShort(offset) / 65535.0f;
            out[1] = readUShort(offset + 2) / 65535.0f;
            out[2] = readUShort(offset + 4) / 65535.0f;
            out[3] = readUShort(offset + 6) / 65535.0f;
            break;
        case VertexDeclType::FLOAT16_2:
            out[0] = halfToFloat(readUShort(offset));
            out[1] = halfToFloat(readUShort(offset + 2));
            break;
        case VertexDeclType::FLOAT16_4:
            out[0] = halfToFloat(readUShort(offset));
            out[1] = halfToFloat(readUShort(offset + 2));
            out[2] = halfToFloat(readUShort(offset + 4));
            out[3] = halfToFloat(readUShort(offset + 6));
            break;
        default:
            break;
    }
}

bool loadMSH(const std::vector<uint8_t>& data, Model& outModel) {
    GFFFile gff;
    if (!gff.load(data)) {
        return false;
    }
    
    // Check if it's a valid MSH file
    // MSH magic might be different, let's just proceed if GFF loaded
    
    outModel.meshes.clear();
    outModel.name = "Model";
    
    // Get vertex and index buffer offsets from root struct (struct 0)
    uint32_t vertexBufferOffset = gff.getListDataOffset(0, GFFFieldID::VERTEX_BUFFER, 0);
    uint32_t indexBufferOffset = gff.getListDataOffset(0, GFFFieldID::INDEX_BUFFER, 0);
    
    // Get mesh chunks
    std::vector<GFFStructRef> meshChunks = gff.readStructList(0, GFFFieldID::MESH_CHUNKS, 0);
    
    if (meshChunks.empty()) {
        return false;
    }
    
    for (const auto& chunkRef : meshChunks) {
        Mesh mesh;
        
        // Read chunk name - try both NAME (2) and alternate
        const GFFField* nameField = gff.findField(chunkRef.structIndex, GFFFieldID::NAME);
        if (nameField && nameField->typeId == 14) {
            mesh.name = gff.readStringByLabel(chunkRef.structIndex, GFFFieldID::NAME, chunkRef.offset);
        }
        if (mesh.name.empty()) {
            mesh.name = "chunk_" + std::to_string(outModel.meshes.size());
        }
        
        // Read mesh parameters
        uint32_t vertexSize = gff.readUInt32ByLabel(chunkRef.structIndex, GFFFieldID::VERTEX_SIZE, chunkRef.offset);
        uint32_t vertexCount = gff.readUInt32ByLabel(chunkRef.structIndex, GFFFieldID::VERTEX_COUNT, chunkRef.offset);
        uint32_t indexCount = gff.readUInt32ByLabel(chunkRef.structIndex, GFFFieldID::INDEX_COUNT, chunkRef.offset);
        uint32_t indexFormat = gff.readUInt32ByLabel(chunkRef.structIndex, GFFFieldID::INDEX_FORMAT, chunkRef.offset);
        uint32_t vertexOffset = gff.readUInt32ByLabel(chunkRef.structIndex, GFFFieldID::VERTEX_OFFSET, chunkRef.offset);
        uint32_t indexOffset = gff.readUInt32ByLabel(chunkRef.structIndex, GFFFieldID::INDEX_OFFSET, chunkRef.offset);
        
        if (vertexCount == 0 || indexCount == 0 || vertexSize == 0) {
            continue;
        }
        
        // Read vertex declarator to find position/normal/texcoord streams
        std::vector<GFFStructRef> declList = gff.readStructList(chunkRef.structIndex, GFFFieldID::VERTEX_DECLARATOR, chunkRef.offset);
        
        // Find position, normal, and texcoord streams
        VertexStreamDesc posStream = {0, 0, 0, 0, 0};
        VertexStreamDesc normalStream = {0, 0, 0, 0, 0};
        VertexStreamDesc texcoordStream = {0, 0, 0, 0, 0};
        bool hasPos = false, hasNormal = false, hasTexcoord = false;
        
        for (const auto& declRef : declList) {
            uint32_t usage = gff.readUInt32ByLabel(declRef.structIndex, GFFFieldID::DECL_USAGE, declRef.offset);
            
            VertexStreamDesc desc;
            desc.stream = gff.readUInt32ByLabel(declRef.structIndex, GFFFieldID::DECL_STREAM, declRef.offset);
            desc.offset = gff.readUInt32ByLabel(declRef.structIndex, GFFFieldID::DECL_OFFSET, declRef.offset);
            desc.dataType = gff.readUInt32ByLabel(declRef.structIndex, GFFFieldID::DECL_DATATYPE, declRef.offset);
            desc.usage = usage;
            desc.usageIndex = gff.readUInt32ByLabel(declRef.structIndex, GFFFieldID::DECL_USAGE_INDEX, declRef.offset);
            
            if (usage == VertexUsage::POSITION && !hasPos) {
                posStream = desc;
                hasPos = true;
            } else if (usage == VertexUsage::NORMAL && !hasNormal) {
                normalStream = desc;
                hasNormal = true;
            } else if (usage == VertexUsage::TEXCOORD && !hasTexcoord) {
                texcoordStream = desc;
                hasTexcoord = true;
            }
        }
        
        if (!hasPos) {
            continue; // Need at least position data
        }
        
        // Calculate base offsets
        // Formula: DataOffset + VertexBufferOffset + 4 (skip count) + VertexOffset + StreamOffset
        uint32_t vertexDataBase = gff.dataOffset() + vertexBufferOffset + 4 + vertexOffset;
        uint32_t indexDataBase = gff.dataOffset() + indexBufferOffset + 4;
        
        // Adjust index offset based on format
        if (indexFormat == 0) {
            indexDataBase += indexOffset * 2; // 16-bit indices
        } else {
            indexDataBase += indexOffset * 4; // 32-bit indices
        }
        
        // Read vertices
        mesh.vertices.resize(vertexCount);
        
        for (uint32_t i = 0; i < vertexCount; i++) {
            uint32_t baseOff = vertexDataBase + i * vertexSize;
            float vals[4];
            
            // Position
            readDeclType(gff.rawData(), baseOff + posStream.offset, posStream.dataType, vals);
            mesh.vertices[i].x = vals[0];
            mesh.vertices[i].y = vals[1];
            mesh.vertices[i].z = vals[2];
            
            // Normal
            if (hasNormal) {
                readDeclType(gff.rawData(), baseOff + normalStream.offset, normalStream.dataType, vals);
                mesh.vertices[i].nx = vals[0];
                mesh.vertices[i].ny = vals[1];
                mesh.vertices[i].nz = vals[2];
            } else {
                mesh.vertices[i].nx = 0;
                mesh.vertices[i].ny = 1;
                mesh.vertices[i].nz = 0;
            }
            
            // Texcoord
            if (hasTexcoord) {
                readDeclType(gff.rawData(), baseOff + texcoordStream.offset, texcoordStream.dataType, vals);
                mesh.vertices[i].u = vals[0];
                mesh.vertices[i].v = 1.0f - vals[1]; // Flip V
            } else {
                mesh.vertices[i].u = 0;
                mesh.vertices[i].v = 0;
            }
        }
        
        // Read indices
        mesh.indices.resize(indexCount);
        
        for (uint32_t i = 0; i < indexCount; i++) {
            if (indexFormat == 0) {
                // 16-bit indices
                uint16_t idx;
                std::memcpy(&idx, &gff.rawData()[indexDataBase + i * 2], 2);
                mesh.indices[i] = idx;
            } else {
                // 32-bit indices
                uint32_t idx;
                std::memcpy(&idx, &gff.rawData()[indexDataBase + i * 4], 4);
                mesh.indices[i] = idx;
            }
        }
        
        mesh.calculateBounds();
        outModel.meshes.push_back(mesh);
    }
    
    if (outModel.meshes.empty()) {
        return false;
    }
    
    return true;
}