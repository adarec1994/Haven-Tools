#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <map>
#include "Gff.h"
#include "erf.h"

struct TerrainVertex {
    float x, y, z;
    float u, v;
    float nx, ny, nz;
};

struct TerrainSector {
    int sectorIndex;
    int gridX, gridY;
    float offsetX, offsetY;
    float sectorSize;
    std::vector<TerrainVertex> vertices;
    std::vector<uint32_t> indices;
};

struct TerrainWorld {
    std::vector<TerrainSector> sectors;
    float totalWidth = 0;
    float totalHeight = 0;
    int sectorsX = 0;
    int sectorsY = 0;
    float sectorSize = 512.0f;
    float minX = 0, minY = 0, minZ = 0;
    float maxX = 0, maxY = 0, maxZ = 0;
};

class TerrainLoader {
public:
    TerrainLoader();
    ~TerrainLoader();
    bool loadFromERF(ERFFile& erf, const std::string& anyTmshName);
    bool loadSector(const std::vector<uint8_t>& data, int sectorIndex, TerrainSector& sector);
    const TerrainWorld& getTerrain() const { return m_terrain; }
    TerrainWorld& getTerrain() { return m_terrain; }
    bool isLoaded() const { return !m_terrain.sectors.empty(); }
    void clear();
private:
    TerrainWorld m_terrain;
    bool parseTMSH(const std::vector<uint8_t>& data, TerrainSector& sector);
    void computeNormals(TerrainSector& sector);
    std::vector<std::string> findAllTMSH(ERFFile& erf);
    int extractSectorIndex(const std::string& filename);
    void calculateSectorPosition(TerrainSector& sector, int totalSectors);
};

extern TerrainLoader g_terrainLoader;
bool isTerrain(const std::string& name);
void renderTerrain(const float* mvp);