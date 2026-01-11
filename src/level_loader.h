#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>

struct LevelObject {
    std::string templateResRef;
    std::string name;
    std::string type;
    float posX = 0, posY = 0, posZ = 0;
    float rotX = 0, rotY = 0, rotZ = 0, rotW = 1.0f;
    bool active = true;
    int importance = 0;
};

struct LevelArea {
    std::string name;
    std::string areaName;
    std::vector<LevelObject> creatures;
    std::vector<LevelObject> placeables;
    std::vector<LevelObject> triggers;
    std::vector<LevelObject> waypoints;
    std::vector<LevelObject> sounds;
    std::vector<LevelObject> stores;
    std::vector<LevelObject> items;
    std::vector<LevelObject> stages;
};

struct TerrainVertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
    uint8_t blendWeights[4];
    uint8_t blendIndices[4];
};

struct TerrainSector {
    int sectorX = 0;
    int sectorY = 0;
    float offsetX = 0, offsetY = 0;
    float sectorSize = 0;
    int gridWidth = 0;
    int gridHeight = 0;
    std::vector<TerrainVertex> vertices;
    std::vector<uint32_t> indices;
};

struct LevelTerrain {
    std::string name;
    std::vector<TerrainSector> sectors;
    float minX = 0, minY = 0, minZ = 0;
    float maxX = 0, maxY = 0, maxZ = 0;
};

struct LevelData {
    std::string name;
    LevelArea area;
    LevelTerrain terrain;
    bool hasArea = false;
    bool hasTerrain = false;
};

bool loadAREFile(const std::vector<uint8_t>& data, LevelArea& outArea);
bool loadTMSHFile(const std::vector<uint8_t>& data, TerrainSector& outSector);
bool loadLevelHeader(const std::vector<uint8_t>& data, LevelData& outLevel);