#include "terrain_loader.h"
#include "Shaders/d3d_context.h"
#include "Shaders/shader.h"
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

struct TerrainSimpleVertex {
    float x, y, z;
    float nx, ny, nz;
};

void renderTerrain(const float* mvp) {
    if (!g_terrainLoader.isLoaded()) return;
    if (!shadersAvailable()) return;

    D3DContext& d3d = getD3DContext();
    auto& shader = getSimpleShader();
    if (!shader.valid) return;

    const TerrainWorld& terrain = g_terrainLoader.getTerrain();

    for (const auto& sector : terrain.sectors) {
        if (sector.vertices.empty()) continue;

        std::vector<TerrainSimpleVertex> triVerts;

        if (!sector.indices.empty()) {
            triVerts.reserve(sector.indices.size());
            for (uint32_t idx : sector.indices) {
                if (idx < sector.vertices.size()) {
                    const auto& v = sector.vertices[idx];
                    triVerts.push_back({ v.x, v.y, v.z, v.nx, v.ny, v.nz });
                }
            }
        } else {
            uint32_t vertCount = (uint32_t)sector.vertices.size();
            uint32_t gridN = (uint32_t)std::round(std::sqrt((double)vertCount));

            if (gridN > 1 && gridN * gridN == vertCount) {
                triVerts.reserve((gridN - 1) * (gridN - 1) * 6);
                for (uint32_t row = 0; row + 1 < gridN; row++) {
                    for (uint32_t col = 0; col + 1 < gridN; col++) {
                        uint32_t i00 = row * gridN + col;
                        uint32_t i10 = row * gridN + col + 1;
                        uint32_t i01 = (row + 1) * gridN + col;
                        uint32_t i11 = (row + 1) * gridN + col + 1;

                        const auto& v00 = sector.vertices[i00];
                        const auto& v10 = sector.vertices[i10];
                        const auto& v01 = sector.vertices[i01];
                        const auto& v11 = sector.vertices[i11];

                        triVerts.push_back({ v00.x, v00.y, v00.z, v00.nx, v00.ny, v00.nz });
                        triVerts.push_back({ v10.x, v10.y, v10.z, v10.nx, v10.ny, v10.nz });
                        triVerts.push_back({ v01.x, v01.y, v01.z, v01.nx, v01.ny, v01.nz });

                        triVerts.push_back({ v10.x, v10.y, v10.z, v10.nx, v10.ny, v10.nz });
                        triVerts.push_back({ v11.x, v11.y, v11.z, v11.nx, v11.ny, v11.nz });
                        triVerts.push_back({ v01.x, v01.y, v01.z, v01.nx, v01.ny, v01.nz });
                    }
                }
            } else {
                uint32_t triCount = vertCount / 3;
                triVerts.reserve(triCount * 3);
                for (uint32_t i = 0; i + 2 < vertCount; i += 3) {
                    const auto& v0 = sector.vertices[i];
                    const auto& v1 = sector.vertices[i + 1];
                    const auto& v2 = sector.vertices[i + 2];
                    triVerts.push_back({ v0.x, v0.y, v0.z, v0.nx, v0.ny, v0.nz });
                    triVerts.push_back({ v1.x, v1.y, v1.z, v1.nx, v1.ny, v1.nz });
                    triVerts.push_back({ v2.x, v2.y, v2.z, v2.nx, v2.ny, v2.nz });
                }
            }
        }

        if (triVerts.empty()) continue;

        DynamicVertexBuffer vb;
        if (!vb.create(d3d.device, (uint32_t)triVerts.size(), sizeof(TerrainSimpleVertex)))
            continue;

        vb.update(d3d.context, triVerts.data(), (uint32_t)triVerts.size());

        CBSimple cb;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                cb.modelViewProj[j * 4 + i] = mvp[i * 4 + j];
        cb.color[0] = 0.45f; cb.color[1] = 0.55f;
        cb.color[2] = 0.35f; cb.color[3] = 1.0f;
        updateSimpleCB(cb);

        d3d.context->IASetInputLayout(shader.inputLayout);
        UINT stride = sizeof(TerrainSimpleVertex), offset = 0;
        d3d.context->IASetVertexBuffers(0, 1, &vb.buffer, &stride, &offset);
        d3d.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3d.context->VSSetShader(shader.vs, nullptr, 0);
        d3d.context->PSSetShader(shader.ps, nullptr, 0);
        ID3D11Buffer* cbs[] = { getSimpleCB() };
        d3d.context->VSSetConstantBuffers(0, 1, cbs);
        d3d.context->PSSetConstantBuffers(0, 1, cbs);

        d3d.context->Draw((UINT)triVerts.size(), 0);

        vb.destroy();
    }
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
        if (m_terrain.minZ < 1e9f) {
            std::cout << "[Terrain] Z range: [" << m_terrain.minZ << ", " << m_terrain.maxZ << "]" << std::endl;
        }
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

    std::cout << "[Terrain] GFF4 TMSH: " << structCount << " structs" << std::endl;

    struct GffStruct {
        char type[5];
        uint32_t fieldCount;
        uint32_t fieldOffset;
        uint32_t size;
    };

    std::vector<GffStruct> structs;
    uint32_t off = 28;
    for (uint32_t i = 0; i < structCount && off + 16 <= data.size(); i++) {
        GffStruct s;
        memcpy(s.type, &data[off], 4);
        s.type[4] = 0;
        s.fieldCount = *reinterpret_cast<const uint32_t*>(&data[off + 4]);
        s.fieldOffset = *reinterpret_cast<const uint32_t*>(&data[off + 8]);
        s.size = *reinterpret_cast<const uint32_t*>(&data[off + 12]);
        structs.push_back(s);
        off += 16;
    }

    int vertStructIdx = -1;
    for (size_t i = 0; i < structs.size(); i++) {
        if (strncmp(structs[i].type, "VERT", 4) == 0) {
            vertStructIdx = (int)i;
            std::cout << "[Terrain] Found VERT struct at index " << i << ", size=" << structs[i].size << std::endl;
            break;
        }
    }

    sector.vertices.clear();

    uint32_t vertListCount = 0;
    uint32_t vertListOffset = 0;
    uint32_t vertSize = (vertStructIdx >= 0) ? structs[vertStructIdx].size : 32;

    for (uint32_t searchOff = dataOffset; searchOff + 8 <= data.size(); searchOff += 4) {
        uint32_t count = *reinterpret_cast<const uint32_t*>(&data[searchOff]);
        uint32_t listOff = *reinterpret_cast<const uint32_t*>(&data[searchOff + 4]);

        if (count > 100 && count < 100000 && listOff > dataOffset && listOff + count * vertSize <= data.size()) {
            int validCount = 0;
            int nonZeroCount = 0;

            for (uint32_t i = 0; i < std::min(count, 100u); i++) {
                uint32_t voff = listOff + i * vertSize;
                if (voff + 12 > data.size()) break;

                float x = *reinterpret_cast<const float*>(&data[voff]);
                float y = *reinterpret_cast<const float*>(&data[voff + 4]);
                float z = *reinterpret_cast<const float*>(&data[voff + 8]);

                if (x > -1000 && x < 1000 && y > -1000 && y < 1000 && z > -500 && z < 500) {
                    validCount++;
                    if (x != 0 || y != 0 || z != 0) nonZeroCount++;
                }
            }

            if (validCount >= 50 && nonZeroCount >= 10 && count > vertListCount) {
                vertListCount = count;
                vertListOffset = listOff;
                std::cout << "[Terrain] Found VERT list: count=" << count << " at 0x" << std::hex << listOff << std::dec << std::endl;
            }
        }
    }

    if (vertListCount > 0 && vertListOffset > 0) {
        sector.vertices.reserve(vertListCount);

        for (uint32_t i = 0; i < vertListCount; i++) {
            uint32_t voff = vertListOffset + i * vertSize;
            if (voff + 12 > data.size()) break;

            TerrainVertex v;
            v.x = *reinterpret_cast<const float*>(&data[voff]);
            v.y = *reinterpret_cast<const float*>(&data[voff + 4]);
            v.z = *reinterpret_cast<const float*>(&data[voff + 8]);
            v.u = 0; v.v = 0;
            v.nx = 0; v.ny = 0; v.nz = 1;

            sector.vertices.push_back(v);
        }
    }

    if (sector.vertices.empty()) {
        std::cout << "[Terrain] No VERT list found, scanning for BLPG..." << std::endl;

        for (uint32_t start = dataOffset; start + 32 * 50 <= data.size(); start += 4) {
            int count = 0;
            uint32_t testOff = start;

            while (testOff + 32 <= data.size() && count < 50000) {
                uint32_t flag = *reinterpret_cast<const uint32_t*>(&data[testOff + 4]);
                float x = *reinterpret_cast<const float*>(&data[testOff + 16]);
                float y = *reinterpret_cast<const float*>(&data[testOff + 20]);
                float z = *reinterpret_cast<const float*>(&data[testOff + 24]);
                float pad = *reinterpret_cast<const float*>(&data[testOff + 28]);

                if (flag == 1 && x > -1000 && x < 1000 && y > -1000 && y < 1000 &&
                    z > -500 && z < 500 && pad == 0.0f) {
                    count++;
                    testOff += 32;
                } else {
                    break;
                }
            }

            if (count >= 50) {
                std::cout << "[Terrain] Found BLPG at 0x" << std::hex << start << std::dec << " with " << count << " vertices" << std::endl;

                sector.vertices.reserve(count);
                for (int i = 0; i < count; i++) {
                    uint32_t voff = start + i * 32;
                    TerrainVertex v;
                    v.x = *reinterpret_cast<const float*>(&data[voff + 16]);
                    v.y = *reinterpret_cast<const float*>(&data[voff + 20]);
                    v.z = *reinterpret_cast<const float*>(&data[voff + 24]);
                    v.u = 0; v.v = 0;
                    v.nx = 0; v.ny = 0; v.nz = 1;
                    sector.vertices.push_back(v);
                }
                break;
            }
        }
    }

    std::cout << "[Terrain] Total vertices: " << sector.vertices.size() << std::endl;
    return !sector.vertices.empty();
}

void TerrainLoader::computeNormals(TerrainSector& sector) {
}