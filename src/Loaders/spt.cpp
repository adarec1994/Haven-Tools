#include "spt.h"
#include "SpeedTree/SpeedTreeRT.h"

#include <fstream>
#include <cstring>
#include <cctype>
#include <utility>

#ifdef _WIN32
#include <windows.h>  // structured-exception guard around the SpeedTree engine
#endif

// The original spt_convert.exe exported branch + frond geometry only (no leaf
// card/mesh billboards), so that is the default to preserve the prior look.
// Set this to 1 to also emit leaf-card and leaf-mesh geometry.
#ifndef SPT_EXPORT_LEAF_GEOMETRY
#define SPT_EXPORT_LEAF_GEOMETRY 0
#endif

// SpeedTree is now compiled directly into the application (see src/SpeedTree,
// vendored from the open-source SpeedTree RT 4.1 SDK). The tree geometry is
// generated in-process: LoadTree -> Compute ->
// GetGeometry, then the resulting SGeometry is translated into our SptModel.
// No external SpeedTreeRT.dll / spt_convert.exe / temp files are involved.

static std::string g_sptError;
std::string getLastSptError() { return g_sptError; }

bool initSpeedTree() {
    return true;  // nothing to extract/spawn anymore
}

void shutdownSpeedTree() {
    // nothing to clean up anymore
}

namespace {

// Convert one SpeedTree triangle strip (array of vertex indices) into triangle-list
// indices. SpeedTree composite strips stitch sub-strips together with degenerate
// triangles, so we drop any triangle that has a repeated index.
void appendStripTriangles(const int* strip, int len, int maxVert, uint32_t base,
                          std::vector<uint32_t>& out) {
    for (int i = 0; i + 2 < len; ++i) {
        int a = strip[i], b = strip[i + 1], c = strip[i + 2];
        if (a == b || b == c || a == c) continue;  // degenerate stitch triangle
        if (a < 0 || b < 0 || c < 0 ||              // guard against corrupt indices
            a >= maxVert || b >= maxVert || c >= maxVert) continue;
        if (i & 1) std::swap(b, c);                // keep consistent winding
        out.push_back(base + static_cast<uint32_t>(a));
        out.push_back(base + static_cast<uint32_t>(b));
        out.push_back(base + static_cast<uint32_t>(c));
    }
}

// Branches and fronds share the same indexed-geometry layout. Extract the highest
// detail LOD (level 0) into a single submesh.
void extractIndexed(const CSpeedTreeRT::SGeometry::SIndexed& idx,
                    SptSubmeshType type, std::vector<SptSubmesh>& out) {
    const int lod = 0;  // 0 == highest detail
    if (idx.m_nNumLods <= 0 || idx.m_nNumVertices <= 0 || !idx.m_pCoords) return;
    if (!idx.m_pNumStrips || !idx.m_pStrips || !idx.m_pStripLengths) return;
    const int numStrips = idx.m_pNumStrips[lod];
    if (numStrips <= 0) return;
    const int* const* stripsLod = idx.m_pStrips[lod];
    const int* stripLensLod = idx.m_pStripLengths[lod];
    if (!stripsLod || !stripLensLod) return;  // per-LOD tables may be null

    SptSubmesh sm;
    sm.type = type;

    const int nv = idx.m_nNumVertices;
    sm.positions.assign(idx.m_pCoords, idx.m_pCoords + (size_t)nv * 3);
    sm.normals.assign((size_t)nv * 3, 0.0f);
    sm.texcoords.assign((size_t)nv * 2, 0.0f);
    if (idx.m_pNormals)
        std::memcpy(sm.normals.data(), idx.m_pNormals, (size_t)nv * 3 * sizeof(float));
    const float* uv = idx.m_pTexCoords[CSpeedTreeRT::TL_DIFFUSE];
    if (uv)
        std::memcpy(sm.texcoords.data(), uv, (size_t)nv * 2 * sizeof(float));

    for (int s = 0; s < numStrips; ++s) {
        const int sLen = stripLensLod[s];
        const int* strip = stripsLod[s];
        if (sLen > 2 && strip) appendStripTriangles(strip, sLen, nv, 0, sm.indices);
    }

    if (!sm.indices.empty()) out.push_back(std::move(sm));
}

#if SPT_EXPORT_LEAF_GEOMETRY
inline void pushVec3(std::vector<float>& dst, float x, float y, float z) {
    dst.push_back(x); dst.push_back(y); dst.push_back(z);
}

// Leaf clusters are either camera-cards (a quad) or full meshes. SpeedTree already
// bakes the card corner offsets / oriented mesh coords relative to the leaf center
// at Compute() time, so world position = leaf center + local coord for both.
void extractLeaves(CSpeedTreeRT::SGeometry& geo, std::vector<SptSubmesh>& out) {
    if (geo.m_nNumLeafLods <= 0 || !geo.m_pLeaves) return;
    const CSpeedTreeRT::SGeometry::SLeaf& lod = geo.m_pLeaves[0];  // highest detail
    if (lod.m_nNumLeaves <= 0 || !lod.m_pCards || !lod.m_pLeafCardIndices ||
        !lod.m_pCenterCoords)
        return;

    SptSubmesh cards;  cards.type  = SptSubmeshType::LeafCard;
    SptSubmesh meshes; meshes.type = SptSubmeshType::LeafMesh;

    for (int leaf = 0; leaf < lod.m_nNumLeaves; ++leaf) {
        const CSpeedTreeRT::SGeometry::SLeaf::SCard* card =
            lod.m_pCards + lod.m_pLeafCardIndices[leaf];
        const float* center = lod.m_pCenterCoords + leaf * 3;

        if (card->m_pMesh) {
            const CSpeedTreeRT::SGeometry::SLeaf::SMesh* mesh = card->m_pMesh;
            if (mesh->m_nNumVertices <= 0 || !mesh->m_pCoords || !mesh->m_pIndices)
                continue;
            const uint32_t base =
                static_cast<uint32_t>(meshes.positions.size() / 3);
            for (int v = 0; v < mesh->m_nNumVertices; ++v) {
                pushVec3(meshes.positions, center[0] + mesh->m_pCoords[v * 3 + 0],
                                           center[1] + mesh->m_pCoords[v * 3 + 1],
                                           center[2] + mesh->m_pCoords[v * 3 + 2]);
                if (mesh->m_pNormals)
                    pushVec3(meshes.normals, mesh->m_pNormals[v * 3 + 0],
                                             mesh->m_pNormals[v * 3 + 1],
                                             mesh->m_pNormals[v * 3 + 2]);
                else
                    pushVec3(meshes.normals, 0.0f, 0.0f, 1.0f);
                if (mesh->m_pTexCoords) {
                    meshes.texcoords.push_back(mesh->m_pTexCoords[v * 2 + 0]);
                    meshes.texcoords.push_back(mesh->m_pTexCoords[v * 2 + 1]);
                } else {
                    meshes.texcoords.push_back(0.0f);
                    meshes.texcoords.push_back(0.0f);
                }
            }
            for (int i = 0; i < mesh->m_nNumIndices; ++i)
                meshes.indices.push_back(base +
                    static_cast<uint32_t>(mesh->m_pIndices[i]));
        } else {
            if (!card->m_pCoords || !card->m_pTexCoords) continue;
            const uint32_t base = static_cast<uint32_t>(cards.positions.size() / 3);
            for (int c = 0; c < 4; ++c) {
                const float* off = card->m_pCoords + c * 4;  // (x,y,z,0)
                pushVec3(cards.positions, center[0] + off[0],
                                          center[1] + off[1],
                                          center[2] + off[2]);
                if (lod.m_pNormals)
                    pushVec3(cards.normals, lod.m_pNormals[leaf * 12 + c * 3 + 0],
                                            lod.m_pNormals[leaf * 12 + c * 3 + 1],
                                            lod.m_pNormals[leaf * 12 + c * 3 + 2]);
                else
                    pushVec3(cards.normals, 0.0f, 0.0f, 1.0f);
                cards.texcoords.push_back(card->m_pTexCoords[c * 2 + 0]);
                cards.texcoords.push_back(card->m_pTexCoords[c * 2 + 1]);
            }
            cards.indices.push_back(base + 0);
            cards.indices.push_back(base + 1);
            cards.indices.push_back(base + 2);
            cards.indices.push_back(base + 0);
            cards.indices.push_back(base + 2);
            cards.indices.push_back(base + 3);
        }
    }

    if (!cards.indices.empty())  out.push_back(std::move(cards));
    if (!meshes.indices.empty()) out.push_back(std::move(meshes));
}
#endif  // SPT_EXPORT_LEAF_GEOMETRY

}  // namespace

// Runs the SpeedTree engine behind a structured-exception guard. A malformed or
// version-incompatible .spt that makes the engine touch bad memory becomes a
// clean failure (CT_FAULT) instead of crashing the app. This frame holds no C++
// objects with destructors, so __try/__except is well-formed under /EHsc.
enum { CT_OK = 0, CT_FAIL = 1, CT_FAULT = 2 };
static int computeTreeGeometry(CSpeedTreeRT& tree, const unsigned char* data,
                               unsigned int size, CSpeedTreeRT::SGeometry& geo,
                               float bounds[6], CSpeedTreeRT::SMapBank& bank) {
#ifdef _WIN32
    __try {
#endif
        if (!tree.LoadTree(data, size)) return CT_FAIL;
        if (!tree.Compute(nullptr, 1, true)) return CT_FAIL;
        tree.SetLodLevel(1.0f);
        tree.GetGeometry(geo, SpeedTree_AllGeometry);
        tree.GetBoundingBox(bounds);
        tree.GetMapBank(bank);  // composite filename lives here, not in the .spt bytes
        return CT_OK;
#ifdef _WIN32
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return CT_FAULT;
    }
#endif
}

bool loadSptModel(const std::string& sptPath, SptModel& outModel) {
    g_sptError.clear();
    std::ifstream f(sptPath, std::ios::binary);
    if (!f) { g_sptError = "cannot open temp .spt file"; return false; }
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(f)),
                                     std::istreambuf_iterator<char>());
    if (data.empty()) { g_sptError = "spt data is empty"; return false; }

    // Heap-allocate the engine: on a structured-exception fault its internal
    // state may be corrupt, so we must NOT run ~CSpeedTreeRT (it would crash
    // freeing half-built data). geo is a borrowed view (trivial destructor) and
    // is safe on the stack.
    CSpeedTreeRT* tree = new CSpeedTreeRT();
    CSpeedTreeRT::SGeometry geo;
    CSpeedTreeRT::SMapBank bank;
    float b[6] = {0, 0, 0, 0, 0, 0};
    int rc = computeTreeGeometry(*tree, data.data(),
                                 static_cast<unsigned int>(data.size()), geo, b, bank);
    if (rc != CT_OK) {
        const char* err = CSpeedTreeRT::GetCurrentError();
        if (rc == CT_FAULT)
            g_sptError = "engine access violation (incompatible .spt)";
        else
            g_sptError = (err && err[0]) ? err : "LoadTree/Compute failed";
        if (rc == CT_FAIL) delete tree;  // clean failure: state valid, free it
        // CT_FAULT: intentionally leak the corrupt engine rather than risk a
        // secondary crash in its destructor. Rare path; the user can reload.
        return false;
    }

    outModel = SptModel{};
    extractIndexed(geo.m_sBranches, SptSubmeshType::Branch, outModel.submeshes);
    extractIndexed(geo.m_sFronds, SptSubmeshType::Frond, outModel.submeshes);
#if SPT_EXPORT_LEAF_GEOMETRY
    extractLeaves(geo, outModel.submeshes);
#endif

    // The composite (foliage) texture filename is only in the map bank, not in the
    // .spt's raw bytes, so capture it here (borrowed c_str(), copy before delete).
    if (bank.m_pCompositeMaps && bank.m_pCompositeMaps[CSpeedTreeRT::TL_DIFFUSE] &&
        bank.m_pCompositeMaps[CSpeedTreeRT::TL_DIFFUSE][0])
        outModel.compositeTexture = bank.m_pCompositeMaps[CSpeedTreeRT::TL_DIFFUSE];

    outModel.boundMin[0] = b[0]; outModel.boundMin[1] = b[1]; outModel.boundMin[2] = b[2];
    outModel.boundMax[0] = b[3]; outModel.boundMax[1] = b[4]; outModel.boundMax[2] = b[5];

    delete tree;  // geometry already copied out; safe to free
    if (outModel.submeshes.empty()) {
        g_sptError = "no branch/frond geometry produced";
        return false;
    }
    return true;
}

void extractSptTextures(const std::vector<uint8_t>& rawData, SptModel& model) {
    std::vector<std::string> tgaFiles, ddsFiles;
    size_t n = rawData.size();

    for (size_t i = 0; i + 4 < n; ) {
        if (rawData[i] >= 0x20 && rawData[i] <= 0x7e) {
            size_t start = i;
            while (i < n && rawData[i] >= 0x20 && rawData[i] <= 0x7e) i++;
            size_t len = i - start;
            if (len >= 5) {
                std::string s(reinterpret_cast<const char*>(&rawData[start]), len);
                std::string lower = s;
                for (auto& c : lower) c = (char)tolower((unsigned char)c);
                if (lower.size() > 4) {
                    std::string ext = lower.substr(lower.size() - 4);
                    if (ext == ".tga") {
                        bool found = false;
                        for (const auto& existing : tgaFiles)
                            if (existing == s) { found = true; break; }
                        if (!found) tgaFiles.push_back(s);
                    } else if (ext == ".dds") {
                        bool found = false;
                        for (const auto& existing : ddsFiles)
                            if (existing == s) { found = true; break; }
                        if (!found) ddsFiles.push_back(s);
                    }
                }
            }
        } else {
            i++;
        }
    }

    for (size_t i = 0; i < tgaFiles.size(); i++) {
        const auto& name = tgaFiles[i];
        std::string lower = name;
        for (auto& c : lower) c = (char)tolower((unsigned char)c);
        if (lower.size() > 6 && lower.substr(lower.size() - 6) == "_n.tga") continue;
        if (model.branchTexture.empty()) {
            model.branchTexture = name;
        } else {
            bool dup = false;
            for (const auto& ft : model.frondTextures)
                if (ft == name) { dup = true; break; }
            if (!dup) model.frondTextures.push_back(name);
        }
    }

    // Keep the authoritative map-bank composite name (set by loadSptModel) if we
    // have it; only fall back to scraping .dds names from the bytes otherwise.
    if (model.compositeTexture.empty()) {
        for (const auto& name : ddsFiles) {
            std::string lower = name;
            for (auto& c : lower) c = (char)tolower((unsigned char)c);
            if (lower.find("diffuse") != std::string::npos || lower.find("_diffuse") != std::string::npos) {
                model.compositeTexture = name;
                break;
            }
        }
        if (model.compositeTexture.empty() && !ddsFiles.empty())
            model.compositeTexture = ddsFiles[0];
    }
}
