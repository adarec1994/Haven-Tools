#include "level_loader.h"
#include <iostream>
#include <cstring>
#include <algorithm>

static uint32_t readU32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24); }
static int32_t readI32(const uint8_t* p) { return (int32_t)readU32(p); }
static uint16_t readU16(const uint8_t* p) { return p[0] | (p[1] << 8); }
static float readF32(const uint8_t* p) { uint32_t v = readU32(p); float f; memcpy(&f, &v, 4); return f; }

struct GFF4Field {
    uint32_t labelHash;
    uint32_t typeAndFlags;
    uint32_t offset;
};

struct GFF4Struct {
    uint32_t type;
    uint32_t fieldCount;
    uint32_t dataOffset;
    std::vector<GFF4Field> fields;
};

static std::string readGFF4String(const uint8_t* data, size_t size, uint32_t offset) {
    if (offset >= size) return "";
    std::string result;
    while (offset < size && data[offset] != 0) {
        result += (char)data[offset++];
    }
    return result;
}

static std::string readGFF4StringUTF16(const uint8_t* data, size_t size, uint32_t offset) {
    if (offset + 4 > size) return "";
    uint32_t len = readU32(data + offset);
    offset += 4;
    std::string result;
    for (uint32_t i = 0; i < len && offset + 1 < size; i++) {
        uint16_t ch = readU16(data + offset);
        offset += 2;
        if (ch == 0) break;
        if (ch < 128) result += (char)ch;
    }
    return result;
}

static uint32_t hashLabel(const char* label) {
    uint32_t hash = 0;
    while (*label) {
        hash = (hash << 5) + hash + (uint8_t)(*label);
        label++;
    }
    return hash;
}

bool loadAREFile(const std::vector<uint8_t>& data, LevelArea& outArea) {
    if (data.size() < 64) return false;
    
    const uint8_t* p = data.data();
    
    if (memcmp(p, "ARE ", 4) != 0) {
        std::cout << "[ARE] Not an ARE file" << std::endl;
        return false;
    }
    
    // ARE V3.28 header
    uint32_t topLevelFieldCount = readU32(p + 0x0C);
    uint32_t structOffset = readU32(p + 0x10);
    uint32_t structCount = readU32(p + 0x14);
    uint32_t fieldOffset = readU32(p + 0x18);
    uint32_t fieldCount = readU32(p + 0x1C);
    uint32_t labelOffset = readU32(p + 0x20);
    uint32_t labelCount = readU32(p + 0x24);
    uint32_t fieldDataOffset = readU32(p + 0x28);
    uint32_t fieldDataSize = readU32(p + 0x2C);
    uint32_t fieldIndicesOffset = readU32(p + 0x30);
    uint32_t fieldIndicesSize = readU32(p + 0x34);
    uint32_t listIndicesOffset = readU32(p + 0x38);
    uint32_t listIndicesSize = readU32(p + 0x3C);
    
    std::cout << "[ARE] structs=" << structCount << " fields=" << fieldCount 
              << " labels=" << labelCount << " topFields=" << topLevelFieldCount << std::endl;
    
    // Read all labels (16 bytes each)
    std::vector<std::string> labels;
    for (uint32_t i = 0; i < labelCount && labelOffset + (i+1)*16 <= data.size(); i++) {
        char label[17] = {0};
        memcpy(label, p + labelOffset + i*16, 16);
        labels.push_back(label);
    }
    
    // Helper to get label name
    auto getLabel = [&](uint32_t idx) -> std::string {
        if (idx < labels.size()) return labels[idx];
        return "";
    };
    
    // Top-level fields at offset 0x40 (each 12 bytes: labelIdx, type, dataOffset)
    uint32_t topFieldsOffset = 0x40;
    
    struct FieldEntry {
        uint32_t labelIdx;
        uint32_t type;
        uint32_t dataOffset;
        std::string label;
    };
    
    std::vector<FieldEntry> topFields;
    for (uint32_t i = 0; i < topLevelFieldCount && topFieldsOffset + 12 <= data.size(); i++) {
        FieldEntry fe;
        fe.labelIdx = readU32(p + topFieldsOffset);
        fe.type = readU32(p + topFieldsOffset + 4);
        fe.dataOffset = readU32(p + topFieldsOffset + 8);
        fe.label = getLabel(fe.labelIdx);
        topFields.push_back(fe);
        topFieldsOffset += 12;
    }
    
    // Helper to read list from field data
    auto readListIndices = [&](uint32_t offset) -> std::vector<uint32_t> {
        std::vector<uint32_t> result;
        if (listIndicesOffset == 0xFFFFFFFF) {
            // List data is inline in fieldData
            uint32_t absOff = fieldDataOffset + offset;
            if (absOff + 4 > data.size()) return result;
            uint32_t count = readU32(p + absOff);
            absOff += 4;
            for (uint32_t i = 0; i < count && absOff + 4 <= data.size(); i++) {
                result.push_back(readU32(p + absOff));
                absOff += 4;
            }
        } else {
            // Use list indices table
            uint32_t absOff = listIndicesOffset + offset;
            if (absOff + 4 > data.size()) return result;
            uint32_t count = readU32(p + absOff);
            absOff += 4;
            for (uint32_t i = 0; i < count && absOff + 4 <= data.size(); i++) {
                result.push_back(readU32(p + absOff));
                absOff += 4;
            }
        }
        return result;
    };
    
    // Helper to read struct's fields
    auto readStructFields = [&](uint32_t structIdx) -> std::vector<FieldEntry> {
        std::vector<FieldEntry> fields;
        if (structIdx >= structCount) return fields;
        uint32_t soff = structOffset + structIdx * 12;
        if (soff + 12 > data.size()) return fields;
        
        uint32_t structType = readU32(p + soff);
        uint32_t dataOrIdx = readU32(p + soff + 4);
        uint32_t numFields = readU32(p + soff + 8);
        
        if (numFields == 1) {
            // Single field - dataOrIdx is field index directly
            if (dataOrIdx < fieldCount) {
                uint32_t foff = fieldOffset + dataOrIdx * 12;
                if (foff + 12 <= data.size()) {
                    FieldEntry fe;
                    fe.labelIdx = readU32(p + foff);
                    fe.type = readU32(p + foff + 4);
                    fe.dataOffset = readU32(p + foff + 8);
                    fe.label = getLabel(fe.labelIdx);
                    fields.push_back(fe);
                }
            }
        } else if (numFields > 1) {
            // Multiple fields - dataOrIdx is byte offset into fieldIndices
            uint32_t fidxOff = fieldIndicesOffset + dataOrIdx;
            for (uint32_t i = 0; i < numFields && fidxOff + 4 <= data.size(); i++) {
                uint32_t fieldIdx = readU32(p + fidxOff);
                fidxOff += 4;
                if (fieldIdx < fieldCount) {
                    uint32_t foff = fieldOffset + fieldIdx * 12;
                    if (foff + 12 <= data.size()) {
                        FieldEntry fe;
                        fe.labelIdx = readU32(p + foff);
                        fe.type = readU32(p + foff + 4);
                        fe.dataOffset = readU32(p + foff + 8);
                        fe.label = getLabel(fe.labelIdx);
                        fields.push_back(fe);
                    }
                }
            }
        }
        return fields;
    };
    
    // Helper to read ResRef (type 11)
    auto readResRef = [&](uint32_t offset) -> std::string {
        uint32_t absOff = fieldDataOffset + offset;
        if (absOff + 1 > data.size()) return "";
        uint8_t len = p[absOff];
        if (absOff + 1 + len > data.size()) return "";
        return std::string((char*)(p + absOff + 1), len);
    };
    
    // Helper to read float (type 8)
    auto readFloatField = [&](uint32_t offset) -> float {
        uint32_t absOff = fieldDataOffset + offset;
        if (absOff + 4 > data.size()) return 0.0f;
        return readF32(p + absOff);
    };
    
    // Parse object from struct
    auto parseObject = [&](uint32_t structIdx) -> LevelObject {
        LevelObject obj;
        auto fields = readStructFields(structIdx);
        for (const auto& f : fields) {
            if (f.label == "TemplateResRef") {
                obj.templateResRef = readResRef(f.dataOffset);
            } else if (f.label == "XPosition") {
                obj.posX = readFloatField(f.dataOffset);
            } else if (f.label == "YPosition") {
                obj.posY = readFloatField(f.dataOffset);
            } else if (f.label == "ZPosition") {
                obj.posZ = readFloatField(f.dataOffset);
            } else if (f.label == "XOrientation") {
                obj.rotX = readFloatField(f.dataOffset);
            } else if (f.label == "YOrientation") {
                obj.rotY = readFloatField(f.dataOffset);
            } else if (f.label == "ZOrientation") {
                obj.rotZ = readFloatField(f.dataOffset);
            } else if (f.label == "WOrientation") {
                obj.rotW = readFloatField(f.dataOffset);
            } else if (f.label == "Active") {
                obj.active = (f.dataOffset != 0);
            }
        }
        return obj;
    };
    
    // Process top-level fields
    for (const auto& tf : topFields) {
        // Type 15 = List
        if (tf.type == 15) {
            auto structIndices = readListIndices(tf.dataOffset);
            
            std::vector<LevelObject> objects;
            for (uint32_t si : structIndices) {
                auto obj = parseObject(si);
                if (!obj.templateResRef.empty()) {
                    objects.push_back(obj);
                }
            }
            
            if (tf.label == "CreatureList") {
                outArea.creatures = objects;
                std::cout << "[ARE] Loaded " << objects.size() << " creatures" << std::endl;
            } else if (tf.label == "PlaceableList") {
                outArea.placeables = objects;
                std::cout << "[ARE] Loaded " << objects.size() << " placeables" << std::endl;
            } else if (tf.label == "TriggerList") {
                outArea.triggers = objects;
            } else if (tf.label == "WaypointList") {
                outArea.waypoints = objects;
            } else if (tf.label == "SoundList") {
                outArea.sounds = objects;
            } else if (tf.label == "StoreList") {
                outArea.stores = objects;
            } else if (tf.label == "ItemList") {
                outArea.items = objects;
            } else if (tf.label == "StageList") {
                outArea.stages = objects;
            }
            
            if (!objects.empty() && !tf.label.empty()) {
                std::cout << "[ARE] " << tf.label << ": " << objects.size() << " objects" << std::endl;
            }
        }
    }
    
    return true;
}

bool loadTMSHFile(const std::vector<uint8_t>& data, TerrainSector& outSector) {
    if (data.size() < 32) return false;
    
    const uint8_t* p = data.data();
    if (memcmp(p, "GFF V4.0PC  TRN ", 16) != 0) {
        std::cout << "[TMSH] Not a terrain file" << std::endl;
        return false;
    }
    
    // Parse GFF V4 header
    uint32_t structCount = readU32(p + 0x18);
    uint32_t dataOffset = readU32(p + 0x1C);
    
    std::cout << "[TMSH] GFF V4 terrain file, structs: " << structCount << std::endl;
    
    // Find SECT struct which contains terrain sector data
    // GFF V4 struct table starts after header at offset 0x20
    uint32_t structTableOffset = 0x20;
    
    struct StructDef {
        char type[4];
        uint32_t fieldCount;
        uint32_t fieldsOffset;
        uint32_t fieldsSize;
    };
    
    for (uint32_t i = 0; i < structCount && structTableOffset + 16 <= data.size(); i++) {
        char type[5] = {0};
        memcpy(type, p + structTableOffset, 4);
        uint32_t fc = readU32(p + structTableOffset + 4);
        uint32_t fo = readU32(p + structTableOffset + 8);
        uint32_t fs = readU32(p + structTableOffset + 12);
        
        std::cout << "[TMSH] Struct " << i << ": " << type << " fields=" << fc << " offset=" << fo << std::endl;
        
        if (strcmp(type, "SECT") == 0) {
            // This is a sector struct - parse terrain data
            std::cout << "[TMSH] Found SECT struct" << std::endl;
        }
        
        structTableOffset += 16;
    }
    
    // For now, try to find raw vertex data
    // Terrain heightmap is typically encoded as bytes in a grid pattern
    // Look for the data section after struct table
    uint32_t dataStart = structTableOffset;
    
    // Skip to actual terrain data - look for patterns
    // The heightmap data appears to be encoded with 4-byte entries
    // Format seems to be: flags(1) + layer(1) + height_low(1) + height_high(1)
    
    // Find VERT section by looking for pattern
    size_t vertStart = 0;
    size_t vertCount = 0;
    
    for (size_t i = structTableOffset; i + 4 < data.size(); i++) {
        // Look for start of height data (pattern: repeating 4-byte structures)
        if (p[i] == 0x01 && p[i+1] == 0x23 && p[i+2] == 0xff) {
            // Found start of height data
            vertStart = i;
            break;
        }
    }
    
    if (vertStart > 0) {
        std::cout << "[TMSH] Found vertex data at offset 0x" << std::hex << vertStart << std::dec << std::endl;
        
        // Parse heightmap - assume 512x512 grid initially
        int gridSize = 512;
        outSector.gridWidth = gridSize;
        outSector.gridHeight = gridSize;
        outSector.sectorSize = 256.0f; // meters
        
        // Each vertex is 4 bytes
        for (int y = 0; y < gridSize && vertStart + (y * gridSize + gridSize) * 4 <= data.size(); y++) {
            for (int x = 0; x < gridSize; x++) {
                size_t idx = vertStart + (y * gridSize + x) * 4;
                if (idx + 4 > data.size()) break;
                
                uint8_t flags = p[idx];
                uint8_t layer = p[idx + 1];
                uint8_t heightLow = p[idx + 2];
                uint8_t heightHigh = p[idx + 3];
                
                // Decode height (seems to be inverted)
                float height = (float)(255 - heightLow) + (float)heightHigh * 0.1f;
                
                TerrainVertex v;
                v.x = (float)x * (outSector.sectorSize / gridSize);
                v.y = (float)y * (outSector.sectorSize / gridSize);
                v.z = height * 0.5f; // Scale factor
                v.nx = 0; v.ny = 0; v.nz = 1;
                v.u = (float)x / gridSize;
                v.v = (float)y / gridSize;
                
                outSector.vertices.push_back(v);
            }
        }
        
        // Generate indices for triangle grid
        for (int y = 0; y < gridSize - 1; y++) {
            for (int x = 0; x < gridSize - 1; x++) {
                uint32_t i0 = y * gridSize + x;
                uint32_t i1 = y * gridSize + x + 1;
                uint32_t i2 = (y + 1) * gridSize + x;
                uint32_t i3 = (y + 1) * gridSize + x + 1;
                
                outSector.indices.push_back(i0);
                outSector.indices.push_back(i1);
                outSector.indices.push_back(i2);
                
                outSector.indices.push_back(i1);
                outSector.indices.push_back(i3);
                outSector.indices.push_back(i2);
            }
        }
        
        std::cout << "[TMSH] Generated " << outSector.vertices.size() << " vertices, " 
                  << outSector.indices.size() / 3 << " triangles" << std::endl;
        
        return true;
    }
    
    return false;
}

bool loadLevelHeader(const std::vector<uint8_t>& data, LevelData& outLevel) {
    if (data.size() < 32) return false;
    
    const uint8_t* p = data.data();
    if (memcmp(p, "GFF V4.0PC  TRN ", 16) != 0) {
        return false;
    }
    
    std::cout << "[LVL] Parsing level header" << std::endl;
    
    // Parse struct table to find AREA references
    uint32_t structCount = readU32(p + 0x18);
    uint32_t structTableOffset = 0x20;
    
    for (uint32_t i = 0; i < structCount && structTableOffset + 16 <= data.size(); i++) {
        char type[5] = {0};
        memcpy(type, p + structTableOffset, 4);
        
        if (strcmp(type, "AREA") == 0) {
            std::cout << "[LVL] Found AREA struct" << std::endl;
            outLevel.hasArea = true;
        } else if (strcmp(type, "MESH") == 0) {
            std::cout << "[LVL] Found MESH struct" << std::endl;
            outLevel.hasTerrain = true;
        }
        
        structTableOffset += 16;
    }
    
    return true;
}