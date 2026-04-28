#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <map>
#include "erf.h"

struct TerrainVertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
};

struct TerrainSector {
    int sectorId = 0;
    std::vector<TerrainVertex> vertices;
    std::vector<uint32_t> indices;
    float minX = 0, minY = 0, minZ = 0;
    float maxX = 0, maxY = 0, maxZ = 0;
};

struct WaterVertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
    float r, g, b, a;
};

struct WaterMesh {
    int waterId = 0;
    std::vector<WaterVertex> vertices;
    std::vector<uint32_t> indices;
};

struct CollisionWall {
    std::vector<float> vertices;
};

struct TerrainWorld {
    std::vector<TerrainSector> sectors;
    std::vector<WaterMesh> water;
    CollisionWall collisionWalls;
    float minX = 0, minY = 0, minZ = 0;
    float maxX = 0, maxY = 0, maxZ = 0;
};

class TerrainLoader {
public:
    TerrainLoader();
    ~TerrainLoader();
    bool loadFromERF(ERFFile& erf, const std::string& anyTmshName);
    const TerrainWorld& getTerrain() const { return m_terrain; }
    TerrainWorld& getTerrain() { return m_terrain; }
    bool isLoaded() const { return !m_terrain.sectors.empty(); }
    void clear();
    std::vector<uint8_t> generateHeightmap(int& outW, int& outH, int maxRes = 512);
private:
    TerrainWorld m_terrain;
    bool parseTMSH(const std::vector<uint8_t>& data, TerrainSector& sector);
    bool parseWater(const std::vector<uint8_t>& data, WaterMesh& mesh);
    bool parseColwall(const std::vector<uint8_t>& data, CollisionWall& walls);
    void computeNormals(TerrainSector& sector);
    void computeBounds();
};

extern TerrainLoader g_terrainLoader;
bool isTerrain(const std::string& name);
void renderTerrain(const float* mvp);