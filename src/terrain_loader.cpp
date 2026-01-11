#include "terrain_loader.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

TerrainLoader g_terrainLoader;

bool isTerrain(const std::string& name) {
    if (name.size() < 5) return false;
    std::string ext = name.substr(name.size() - 5);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".tmsh";
}

void renderTerrain() {
}

TerrainLoader::TerrainLoader() {}
TerrainLoader::~TerrainLoader() {}

void TerrainLoader::clear() {
    m_terrain.sectors.clear();
    m_terrain.totalWidth = 0;
    m_terrain.totalHeight = 0;
    m_terrain.sectorsX = 0;
    m_terrain.sectorsY = 0;
}

std::vector<std::string> TerrainLoader::findAllTMSH(ERFFile& erf) {
    std::vector<std::string> tmshFiles;
    const auto& entries = erf.entries();
    for (const auto& entry : entries) {
        std::string name = entry.name;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (name.size() > 5 && name.substr(name.size() - 5) == ".tmsh") {
            tmshFiles.push_back(entry.name);
        }
    }
    std::sort(tmshFiles.begin(), tmshFiles.end());
    return tmshFiles;
}

int TerrainLoader::extractSectorIndex(const std::string& filename) {
    std::string name = filename;
    size_t lastSlash = name.find_last_of("/\\");
    if (lastSlash != std::string::npos) name = name.substr(lastSlash + 1);
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    std::string digits;
    for (char c : name) if (c >= '0' && c <= '9') digits += c;
    if (digits.empty()) return 0;
    return std::stoi(digits);
}

void TerrainLoader::calculateSectorPosition(TerrainSector& sector, int totalSectors) {
    int gridSize = (int)std::ceil(std::sqrt((double)totalSectors));
    sector.gridX = sector.sectorIndex % gridSize;
    sector.gridY = sector.sectorIndex / gridSize;
    sector.offsetX = sector.gridX * sector.sectorSize;
    sector.offsetY = sector.gridY * sector.sectorSize;
}

bool TerrainLoader::loadFromERF(ERFFile& erf, const std::string& anyTmshName) {
    clear();

    std::vector<std::string> tmshFiles = findAllTMSH(erf);
    std::cout << "[Terrain] Found " << tmshFiles.size() << " TMSH files" << std::endl;
    if (tmshFiles.empty()) return false;

    const std::string& tmshName = tmshFiles[0];

    const ERFEntry* entry = nullptr;
    for (const auto& e : erf.entries()) {
        if (e.name == tmshName) { entry = &e; break; }
    }
    if (!entry) return false;

    std::vector<uint8_t> data = erf.readEntry(*entry);
    if (data.empty()) return false;

    TerrainSector sector;
    sector.sectorIndex = 0;
    sector.sectorSize = 512.0f;
    sector.gridX = 0;
    sector.gridY = 0;
    sector.offsetX = 0;
    sector.offsetY = 0;

    if (loadSector(data, 0, sector)) {
        m_terrain.sectors.push_back(sector);

        m_terrain.minZ = 1e10f;
        m_terrain.maxZ = -1e10f;
        for (const auto& v : sector.vertices) {
            m_terrain.minZ = std::min(m_terrain.minZ, v.z);
            m_terrain.maxZ = std::max(m_terrain.maxZ, v.z);
        }

        m_terrain.totalWidth = 512;
        m_terrain.totalHeight = 512;
        std::cout << "[Terrain] Loaded sector: " << sector.vertices.size() << " vertices" << std::endl;
        std::cout << "[Terrain] Z range: [" << m_terrain.minZ << ", " << m_terrain.maxZ << "]" << std::endl;
        return true;
    }
    return false;
}

bool TerrainLoader::loadSector(const std::vector<uint8_t>& data, int sectorIndex, TerrainSector& sector) {
    if (data.size() < 28) return false;
    if (memcmp(data.data(), "GFF ", 4) != 0) return false;
    if (memcmp(data.data() + 4, "V4.0", 4) != 0) return false;
    return parseTMSH(data, sector);
}

bool TerrainLoader::parseTMSH(const std::vector<uint8_t>& data, TerrainSector& sector) {
    if (data.size() < 28) return false;

    uint32_t structCount = *reinterpret_cast<const uint32_t*>(&data[20]);
    uint32_t dataOffset = *reinterpret_cast<const uint32_t*>(&data[24]);

    std::cout << "[Terrain] GFF4: " << structCount << " structs, data at 0x" << std::hex << dataOffset << std::dec << std::endl;

    sector.vertices.clear();

    int bestCount = 0;
    uint32_t bestStart = 0;

    for (uint32_t start = dataOffset; start + 32 * 100 <= data.size(); start += 4) {
        int count = 0;
        uint32_t testOff = start;

        while (testOff + 32 <= data.size() && count < 50000) {
            float x = *reinterpret_cast<const float*>(&data[testOff]);
            float y = *reinterpret_cast<const float*>(&data[testOff + 4]);
            float z = *reinterpret_cast<const float*>(&data[testOff + 8]);

            bool validCoord = (x > -1000 && x < 1000 && y > -1000 && y < 1000 && z > -500 && z < 500);
            bool notAllZero = (x != 0.0f || y != 0.0f || z != 0.0f);

            if (validCoord && notAllZero) {
                count++;
                testOff += 32;
            } else if (validCoord && count > 0) {
                count++;
                testOff += 32;
            } else {
                break;
            }
        }

        if (count > bestCount) {
            bestCount = count;
            bestStart = start;
        }

        if (bestCount >= 1000) break;
    }

    if (bestCount >= 50) {
        std::cout << "[Terrain] Found VERT array at 0x" << std::hex << bestStart << std::dec << " with " << bestCount << " vertices" << std::endl;

        sector.vertices.reserve(bestCount);
        for (int i = 0; i < bestCount; i++) {
            uint32_t voff = bestStart + i * 32;
            if (voff + 12 > data.size()) break;

            TerrainVertex vert;
            vert.x = *reinterpret_cast<const float*>(&data[voff]);
            vert.y = *reinterpret_cast<const float*>(&data[voff + 4]);
            vert.z = *reinterpret_cast<const float*>(&data[voff + 8]);
            vert.u = 0;
            vert.v = 0;
            vert.nx = 0;
            vert.ny = 0;
            vert.nz = 1;
            sector.vertices.push_back(vert);
        }
    }

    std::cout << "[Terrain] Total vertices: " << sector.vertices.size() << std::endl;
    return !sector.vertices.empty();
}

void TerrainLoader::computeNormals(TerrainSector& sector) {
}

void TerrainLoader::createGLBuffers() {}
void TerrainLoader::destroyGLBuffers() {}