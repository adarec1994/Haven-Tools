#include "terrain_loader.h"
#include "Shaders/d3d_context.h"
#include "Shaders/shader.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

TerrainLoader g_terrainLoader;

bool isTerrain(const std::string& name) {
    if (name.size() < 5) return false;
    std::string ext = name.substr(name.size() - 5);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".tmsh";
}

static bool isWaterFile(const std::string& name) {
    if (name.size() < 4) return false;
    std::string ext = name.substr(name.size() - 4);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".wat";
}

static bool isColwallFile(const std::string& name) {
    if (name.size() < 4) return false;
    std::string ext = name.substr(name.size() - 4);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".tcw";
}

static uint32_t ru32(const std::vector<uint8_t>& d, uint32_t off) {
    if ((size_t)off + 4 > d.size()) return 0;
    uint32_t v; memcpy(&v, &d[off], 4); return v;
}

static int32_t ri32(const std::vector<uint8_t>& d, uint32_t off) {
    if ((size_t)off + 4 > d.size()) return 0;
    int32_t v; memcpy(&v, &d[off], 4); return v;
}

static float rf32(const std::vector<uint8_t>& d, uint32_t off) {
    if ((size_t)off + 4 > d.size()) return 0;
    float v; memcpy(&v, &d[off], 4); return v;
}

static std::pair<uint32_t, uint32_t> readList(const std::vector<uint8_t>& data,
                                               uint32_t dataOffset, uint32_t fieldRawOffset) {
    if (fieldRawOffset == 0 || fieldRawOffset == 0xFFFFFFFF) return {0, 0};
    size_t absOff = (size_t)dataOffset + (size_t)fieldRawOffset;
    if (absOff + 4 > data.size()) return {0, 0};
    uint32_t count = ru32(data, (uint32_t)absOff);
    if (count == 0 || count > 500000) return {0, 0};
    return {count, (uint32_t)(absOff + 4)};
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

    auto drawBatch = [&](const std::vector<TerrainSimpleVertex>& triVerts,
                         float r, float g, float b, float a) {
        if (triVerts.empty()) return;
        DynamicVertexBuffer vb;
        if (!vb.create(d3d.device, (uint32_t)triVerts.size(), sizeof(TerrainSimpleVertex)))
            return;
        vb.update(d3d.context, triVerts.data(), (uint32_t)triVerts.size());

        CBSimple cb;
        memcpy(cb.modelViewProj, mvp, 64);
        cb.color[0] = r; cb.color[1] = g; cb.color[2] = b; cb.color[3] = a;
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
    };

    for (const auto& sector : terrain.sectors) {
        if (sector.vertices.empty() || sector.indices.empty()) continue;
        std::vector<TerrainSimpleVertex> triVerts;
        triVerts.reserve(sector.indices.size());
        for (uint32_t idx : sector.indices) {
            if (idx < sector.vertices.size()) {
                const auto& v = sector.vertices[idx];
                triVerts.push_back({v.x, v.y, v.z, v.nx, v.ny, v.nz});
            }
        }
        drawBatch(triVerts, 0.45f, 0.55f, 0.35f, 1.0f);
    }

    for (const auto& wm : terrain.water) {
        if (wm.vertices.empty() || wm.indices.empty()) continue;
        std::vector<TerrainSimpleVertex> triVerts;
        triVerts.reserve(wm.indices.size());
        for (uint32_t idx : wm.indices) {
            if (idx < wm.vertices.size()) {
                const auto& v = wm.vertices[idx];
                triVerts.push_back({v.x, v.y, v.z, v.nx, v.ny, v.nz});
            }
        }
        drawBatch(triVerts, 0.2f, 0.4f, 0.7f, 0.5f);
    }

    if (!terrain.collisionWalls.vertices.empty()) {
        const auto& cv = terrain.collisionWalls.vertices;
        uint32_t nVerts = (uint32_t)(cv.size() / 3);
        if (nVerts >= 2) {
            std::vector<TerrainSimpleVertex> lineVerts;
            float h = 2.0f;
            for (uint32_t i = 0; i + 1 < nVerts; i += 2) {
                float x0 = cv[i*3], y0 = cv[i*3+1], z0 = cv[i*3+2];
                float x1 = cv[(i+1)*3], y1 = cv[(i+1)*3+1], z1 = cv[(i+1)*3+2];
                lineVerts.push_back({x0, y0, z0, 0,0,1});
                lineVerts.push_back({x1, y1, z1, 0,0,1});
                lineVerts.push_back({x0, y0, z0+h, 0,0,1});
                lineVerts.push_back({x1, y1, z1, 0,0,1});
                lineVerts.push_back({x1, y1, z1+h, 0,0,1});
                lineVerts.push_back({x0, y0, z0+h, 0,0,1});
            }
            drawBatch(lineVerts, 0.8f, 0.2f, 0.2f, 0.6f);
        }
    }
}

TerrainLoader::TerrainLoader() {}
TerrainLoader::~TerrainLoader() {}

void TerrainLoader::clear() {
    m_terrain.sectors.clear();
    m_terrain.water.clear();
    m_terrain.collisionWalls.vertices.clear();
    m_terrain.minX = m_terrain.minY = m_terrain.minZ = 0;
    m_terrain.maxX = m_terrain.maxY = m_terrain.maxZ = 0;
}

void TerrainLoader::computeBounds() {
    m_terrain.minX = m_terrain.minY = m_terrain.minZ = 1e10f;
    m_terrain.maxX = m_terrain.maxY = m_terrain.maxZ = -1e10f;
    for (const auto& s : m_terrain.sectors) {
        m_terrain.minX = std::min(m_terrain.minX, s.minX);
        m_terrain.minY = std::min(m_terrain.minY, s.minY);
        m_terrain.minZ = std::min(m_terrain.minZ, s.minZ);
        m_terrain.maxX = std::max(m_terrain.maxX, s.maxX);
        m_terrain.maxY = std::max(m_terrain.maxY, s.maxY);
        m_terrain.maxZ = std::max(m_terrain.maxZ, s.maxZ);
    }
}

bool TerrainLoader::loadFromERF(ERFFile& erf, const std::string& anyTmshName) {
    clear();

    std::vector<std::string> tmshFiles, watFiles, tcwFiles;
    for (const auto& e : erf.entries()) {
        std::string name = e.name;
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (isTerrain(lower)) tmshFiles.push_back(name);
        else if (isWaterFile(lower)) watFiles.push_back(name);
        else if (isColwallFile(lower)) tcwFiles.push_back(name);
    }
    std::sort(tmshFiles.begin(), tmshFiles.end());
    std::sort(watFiles.begin(), watFiles.end());

    for (const auto& fn : tmshFiles) {
        const ERFEntry* entry = nullptr;
        for (const auto& e : erf.entries())
            if (e.name == fn) { entry = &e; break; }
        if (!entry) continue;

        std::vector<uint8_t> data = erf.readEntry(*entry);
        if (data.empty()) continue;

        TerrainSector sector;
        if (parseTMSH(data, sector))
            m_terrain.sectors.push_back(std::move(sector));
    }

    for (const auto& fn : watFiles) {
        const ERFEntry* entry = nullptr;
        for (const auto& e : erf.entries())
            if (e.name == fn) { entry = &e; break; }
        if (!entry) continue;

        std::vector<uint8_t> data = erf.readEntry(*entry);
        if (data.empty()) continue;

        WaterMesh wm;
        if (parseWater(data, wm))
            m_terrain.water.push_back(std::move(wm));
    }

    for (const auto& fn : tcwFiles) {
        const ERFEntry* entry = nullptr;
        for (const auto& e : erf.entries())
            if (e.name == fn) { entry = &e; break; }
        if (!entry) continue;

        std::vector<uint8_t> data = erf.readEntry(*entry);
        if (data.empty()) continue;

        parseColwall(data, m_terrain.collisionWalls);
    }

    computeBounds();

    return !m_terrain.sectors.empty();
}

bool TerrainLoader::parseTMSH(const std::vector<uint8_t>& data, TerrainSector& sector) {
    if (data.size() < 28) return false;
    if (memcmp(data.data(), "GFF ", 4) != 0 || memcmp(data.data() + 4, "V4.0", 4) != 0)
        return false;

    uint32_t structCount = ru32(data, 20);
    uint32_t dataOffset = ru32(data, 24);

    uint32_t vertSize = 32, edgeSize = 16, faceSize = 12;
    uint32_t off = 28;
    for (uint32_t i = 0; i < structCount && off + 16 <= data.size(); i++) {
        char fourcc[5]; memcpy(fourcc, &data[off], 4); fourcc[4] = 0;
        uint32_t ss = ru32(data, off + 12);
        if (strncmp(fourcc, "VERT", 4) == 0) vertSize = ss;
        else if (strncmp(fourcc, "EDGE", 4) == 0) edgeSize = ss;
        else if (strncmp(fourcc, "FACE", 4) == 0) faceSize = ss;
        off += 16;
    }

    sector.sectorId = ri32(data, dataOffset + 0);

    uint32_t rawVertOff = ru32(data, dataOffset + 20);
    uint32_t rawEdgeOff = ru32(data, dataOffset + 24);
    uint32_t rawFaceOff = ru32(data, dataOffset + 28);

    auto [vertCount, vertStart] = readList(data, dataOffset, rawVertOff);
    auto [edgeCount, edgeStart] = readList(data, dataOffset, rawEdgeOff);
    auto [faceCount, faceStart] = readList(data, dataOffset, rawFaceOff);

    if (vertCount == 0 || faceCount == 0) return false;

    std::unordered_map<uint32_t, uint32_t> vertIdToIndex;
    sector.vertices.resize(vertCount);
    sector.minX = sector.minY = sector.minZ = 1e10f;
    sector.maxX = sector.maxY = sector.maxZ = -1e10f;

    for (uint32_t i = 0; i < vertCount; i++) {
        size_t voff = (size_t)vertStart + (size_t)i * vertSize;
        if (voff + 20 > data.size()) break;

        float x = rf32(data, (uint32_t)(voff + 0));
        float y = rf32(data, (uint32_t)(voff + 4));
        float z = rf32(data, (uint32_t)(voff + 8));
        uint32_t id = ru32(data, (uint32_t)(voff + 16));

        sector.vertices[i] = {x, y, z, 0, 0, 1, 0, 0};
        vertIdToIndex[id] = i;

        sector.minX = std::min(sector.minX, x); sector.maxX = std::max(sector.maxX, x);
        sector.minY = std::min(sector.minY, y); sector.maxY = std::max(sector.maxY, y);
        sector.minZ = std::min(sector.minZ, z); sector.maxZ = std::max(sector.maxZ, z);
    }

    std::unordered_map<uint32_t, uint32_t> edgeIdToVertIdx;
    for (uint32_t i = 0; i < edgeCount; i++) {
        size_t eoff = (size_t)edgeStart + (size_t)i * edgeSize;
        if (eoff + 8 > data.size()) break;

        uint32_t edgeId = ru32(data, (uint32_t)(eoff + 0));
        uint32_t startVertId = ru32(data, (uint32_t)(eoff + 4));

        auto it = vertIdToIndex.find(startVertId);
        if (it != vertIdToIndex.end())
            edgeIdToVertIdx[edgeId] = it->second;
    }

    sector.indices.reserve(faceCount * 3);

    for (uint32_t i = 0; i < faceCount; i++) {
        size_t foff = (size_t)faceStart + (size_t)i * faceSize;
        if (foff + 8 > data.size()) break;

        uint32_t edgeListRaw = ru32(data, (uint32_t)(foff + 4));
        if (edgeListRaw == 0 || edgeListRaw == 0xFFFFFFFF) continue;
        size_t edgeListAbs = (size_t)dataOffset + (size_t)edgeListRaw;
        if (edgeListAbs + 16 > data.size()) continue;

        uint32_t numEdges = ru32(data, (uint32_t)edgeListAbs);
        if (numEdges != 3) continue;

        for (uint32_t e = 0; e < 3; e++) {
            uint32_t edgeId = ru32(data, (uint32_t)(edgeListAbs + 4 + e * 4));
            auto it = edgeIdToVertIdx.find(edgeId);
            sector.indices.push_back(it != edgeIdToVertIdx.end() ? it->second : 0);
        }
    }

    computeNormals(sector);

    return !sector.vertices.empty();
}

bool TerrainLoader::parseWater(const std::vector<uint8_t>& data, WaterMesh& mesh) {
    if (data.size() < 28) return false;
    if (memcmp(data.data(), "GFF ", 4) != 0 || memcmp(data.data() + 4, "V4.0", 4) != 0)
        return false;

    uint32_t structCount = ru32(data, 20);
    uint32_t dataOffset = ru32(data, 24);

    uint32_t vertStructSize = 64;
    uint32_t off = 28;
    for (uint32_t i = 0; i < structCount && off + 16 <= data.size(); i++) {
        char fourcc[5]; memcpy(fourcc, &data[off], 4); fourcc[4] = 0;
        if (strncmp(fourcc, "VERT", 4) == 0) vertStructSize = ru32(data, off + 12);
        off += 16;
    }

    mesh.waterId = ri32(data, dataOffset + 0);

    auto [idxCount, idxStart] = readList(data, dataOffset, ru32(data, dataOffset + 8));
    auto [vertCount, vertStart] = readList(data, dataOffset, ru32(data, dataOffset + 12));

    if (vertCount == 0) return false;

    mesh.vertices.resize(vertCount);
    for (uint32_t i = 0; i < vertCount; i++) {
        size_t voff = (size_t)vertStart + (size_t)i * vertStructSize;
        if (voff + 64 > data.size()) break;
        WaterVertex& wv = mesh.vertices[i];
        wv.x = rf32(data, (uint32_t)(voff + 0));  wv.y = rf32(data, (uint32_t)(voff + 4));  wv.z = rf32(data, (uint32_t)(voff + 8));
        wv.nx = rf32(data, (uint32_t)(voff + 16)); wv.ny = rf32(data, (uint32_t)(voff + 20)); wv.nz = rf32(data, (uint32_t)(voff + 24));
        wv.u = rf32(data, (uint32_t)(voff + 32));  wv.v = rf32(data, (uint32_t)(voff + 36));
        wv.r = rf32(data, (uint32_t)(voff + 48));  wv.g = rf32(data, (uint32_t)(voff + 52));
        wv.b = rf32(data, (uint32_t)(voff + 56));  wv.a = rf32(data, (uint32_t)(voff + 60));
    }

    mesh.indices.resize(idxCount);
    for (uint32_t i = 0; i < idxCount; i++) {
        size_t ioff = (size_t)idxStart + (size_t)i * 4;
        if (ioff + 4 > data.size()) break;
        mesh.indices[i] = ru32(data, (uint32_t)ioff);
    }

    return true;
}

bool TerrainLoader::parseColwall(const std::vector<uint8_t>& data, CollisionWall& walls) {
    if (data.size() < 28) return false;
    if (memcmp(data.data(), "GFF ", 4) != 0 || memcmp(data.data() + 4, "V4.0", 4) != 0)
        return false;

    uint32_t dataOffset = ru32(data, 24);

    uint32_t v2Raw = ru32(data, dataOffset + 4);
    if (v2Raw == 0xFFFFFFFF) return false;

    auto [floatCount, floatStart] = readList(data, dataOffset, v2Raw);
    if (floatCount < 6) return false;

    walls.vertices.resize(floatCount);
    for (uint32_t i = 0; i < floatCount; i++) {
        size_t foff = (size_t)floatStart + (size_t)i * 4;
        if (foff + 4 > data.size()) break;
        walls.vertices[i] = rf32(data, (uint32_t)foff);
    }

    return true;
}

void TerrainLoader::computeNormals(TerrainSector& sector) {
    size_t nv = sector.vertices.size();
    size_t ni = sector.indices.size();
    TerrainVertex* verts = sector.vertices.data();
    const uint32_t* idx = sector.indices.data();

    for (size_t i = 0; i < nv; i++) {
        verts[i].nx = verts[i].ny = verts[i].nz = 0;
    }

    for (size_t i = 0; i + 2 < ni; i += 3) {
        uint32_t i0 = idx[i], i1 = idx[i+1], i2 = idx[i+2];
        if (i0 >= nv || i1 >= nv || i2 >= nv) continue;

        float e1x = verts[i1].x-verts[i0].x, e1y = verts[i1].y-verts[i0].y, e1z = verts[i1].z-verts[i0].z;
        float e2x = verts[i2].x-verts[i0].x, e2y = verts[i2].y-verts[i0].y, e2z = verts[i2].z-verts[i0].z;
        float nx = e1y*e2z - e1z*e2y;
        float ny = e1z*e2x - e1x*e2z;
        float nz = e1x*e2y - e1y*e2x;

        verts[i0].nx += nx; verts[i0].ny += ny; verts[i0].nz += nz;
        verts[i1].nx += nx; verts[i1].ny += ny; verts[i1].nz += nz;
        verts[i2].nx += nx; verts[i2].ny += ny; verts[i2].nz += nz;
    }

    for (size_t i = 0; i < nv; i++) {
        float len = std::sqrt(verts[i].nx*verts[i].nx + verts[i].ny*verts[i].ny + verts[i].nz*verts[i].nz);
        if (len > 1e-8f) { verts[i].nx /= len; verts[i].ny /= len; verts[i].nz /= len; }
        else { verts[i].nx = 0; verts[i].ny = 0; verts[i].nz = 1; }
    }
}

std::vector<uint8_t> TerrainLoader::generateHeightmap(int& outW, int& outH, int maxRes) {
    if (m_terrain.sectors.empty()) { outW = outH = 0; return {}; }

    float spanX = m_terrain.maxX - m_terrain.minX;
    float spanY = m_terrain.maxY - m_terrain.minY;
    if (spanX < 1e-6f || spanY < 1e-6f) { outW = outH = 0; return {}; }

    float aspect = spanX / spanY;
    if (aspect >= 1.0f) {
        outW = maxRes;
        outH = std::max(1, (int)(maxRes / aspect));
    } else {
        outH = maxRes;
        outW = std::max(1, (int)(maxRes * aspect));
    }

    std::vector<float> zBuf(outW * outH, -1e30f);

    auto worldToPixel = [&](float wx, float wy, int& px, int& py) {
        px = (int)((wx - m_terrain.minX) / spanX * (outW - 1));
        py = (int)((wy - m_terrain.minY) / spanY * (outH - 1));
        py = (outH - 1) - py;
        if (px < 0) px = 0; if (px >= outW) px = outW - 1;
        if (py < 0) py = 0; if (py >= outH) py = outH - 1;
    };

    for (const auto& sec : m_terrain.sectors) {
        for (size_t i = 0; i + 2 < sec.indices.size(); i += 3) {
            const auto& v0 = sec.vertices[sec.indices[i]];
            const auto& v1 = sec.vertices[sec.indices[i+1]];
            const auto& v2 = sec.vertices[sec.indices[i+2]];

            int px0, py0, px1, py1, px2, py2;
            worldToPixel(v0.x, v0.y, px0, py0);
            worldToPixel(v1.x, v1.y, px1, py1);
            worldToPixel(v2.x, v2.y, px2, py2);

            int minPx = std::min({px0, px1, px2});
            int maxPx = std::max({px0, px1, px2});
            int minPy = std::min({py0, py1, py2});
            int maxPy = std::max({py0, py1, py2});

            for (int py = minPy; py <= maxPy; py++) {
                for (int px = minPx; px <= maxPx; px++) {
                    float dx0 = (float)(px1 - px0), dy0 = (float)(py1 - py0);
                    float dx1 = (float)(px2 - px0), dy1 = (float)(py2 - py0);
                    float dx2 = (float)(px  - px0), dy2 = (float)(py  - py0);

                    float d00 = dx0*dx0 + dy0*dy0;
                    float d01 = dx0*dx1 + dy0*dy1;
                    float d11 = dx1*dx1 + dy1*dy1;
                    float d20 = dx2*dx0 + dy2*dy0;
                    float d21 = dx2*dx1 + dy2*dy1;

                    float denom = d00*d11 - d01*d01;
                    if (std::abs(denom) < 1e-10f) continue;
                    float u = (d11*d20 - d01*d21) / denom;
                    float v = (d00*d21 - d01*d20) / denom;

                    if (u >= -0.001f && v >= -0.001f && u + v <= 1.002f) {
                        float z = v0.z * (1.0f - u - v) + v1.z * u + v2.z * v;
                        if (z > zBuf[py * outW + px])
                            zBuf[py * outW + px] = z;
                    }
                }
            }
        }
    }

    float minZ = m_terrain.minZ;
    float maxZ = m_terrain.maxZ;
    float rangeZ = maxZ - minZ;
    if (rangeZ < 1e-6f) rangeZ = 1.0f;

    std::vector<uint8_t> rgba(outW * outH * 4);
    for (int i = 0; i < outW * outH; i++) {
        if (zBuf[i] < -1e20f) {
            rgba[i*4+0] = 20; rgba[i*4+1] = 20; rgba[i*4+2] = 25; rgba[i*4+3] = 255;
        } else {
            float t = (zBuf[i] - minZ) / rangeZ;
            if (t < 0) t = 0; if (t > 1) t = 1;

            uint8_t r, g, b;
            if (t < 0.15f) {
                float s = t / 0.15f;
                r = (uint8_t)(30 + 50*s); g = (uint8_t)(60 + 60*s); b = (uint8_t)(30 + 30*s);
            } else if (t < 0.4f) {
                float s = (t - 0.15f) / 0.25f;
                r = (uint8_t)(80 + 60*s); g = (uint8_t)(120 + 40*s); b = (uint8_t)(60 - 10*s);
            } else if (t < 0.7f) {
                float s = (t - 0.4f) / 0.3f;
                r = (uint8_t)(140 + 40*s); g = (uint8_t)(160 - 30*s); b = (uint8_t)(50 + 20*s);
            } else {
                float s = (t - 0.7f) / 0.3f;
                r = (uint8_t)(180 + 60*s); g = (uint8_t)(130 + 110*s); b = (uint8_t)(70 + 170*s);
            }
            rgba[i*4+0] = r; rgba[i*4+1] = g; rgba[i*4+2] = b; rgba[i*4+3] = 255;
        }
    }

    return rgba;
}