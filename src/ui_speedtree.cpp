#include "ui_internal.h"
#include "spt.h"
#include "renderer.h"
#include "terrain_loader.h"
#include "dds_loader.h"

bool loadSptFromData(AppState& state, const std::vector<uint8_t>& sptData,
                     const std::string& name, const std::string& erfPath) {
    std::string tempDir;
#ifdef _WIN32
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    tempDir = tmp;
#else
    tempDir = "/tmp/";
#endif
    std::string tempSpt = tempDir + "haven_temp.spt";
    {
        std::ofstream f(tempSpt, std::ios::binary);
        f.write(reinterpret_cast<const char*>(sptData.data()), sptData.size());
    }

    SptModel spt;
    if (!loadSptModel(tempSpt, spt)) {
#ifdef _WIN32
        DeleteFileA(tempSpt.c_str());
#else
        remove(tempSpt.c_str());
#endif
        return false;
    }
#ifdef _WIN32
    DeleteFileA(tempSpt.c_str());
#else
    remove(tempSpt.c_str());
#endif

    for (const auto& mat : state.currentModel.materials) {
        if (mat.diffuseTexId != 0) destroyTexture(mat.diffuseTexId);
        if (mat.normalTexId != 0) destroyTexture(mat.normalTexId);
        if (mat.specularTexId != 0) destroyTexture(mat.specularTexId);
        if (mat.tintTexId != 0) destroyTexture(mat.tintTexId);
    }
    destroyLevelBuffers();
    state.currentModel = Model();
    state.currentModel.name = name;

    // Extract texture names from raw SPT data
    extractSptTextures(sptData, spt);

    // Strip extension for base name
    std::string baseName = name;
    size_t dot = baseName.rfind('.');
    if (dot != std::string::npos) baseName = baseName.substr(0, dot);

    // Helper: strip extension from texture filename
    auto stripExt = [](const std::string& s) -> std::string {
        size_t d = s.rfind('.');
        return (d != std::string::npos) ? s.substr(0, d) : s;
    };

    // Create materials: Branch = TGA, Frond+Leaf = _diffuse.dds
    // Branch material (TGA)
    Material branchMat;
    std::string branchKey = spt.branchTexture.empty() ? baseName : stripExt(spt.branchTexture);
    branchMat.name = branchKey;
    branchMat.diffuseMap = branchKey;
    branchMat.opacity = 1.0f;

    // Frond + Leaf material (_diffuse.dds with alpha)
    Material ddsMat;
    std::string ddsKey = baseName + "_diffuse";
    ddsMat.name = ddsKey;
    ddsMat.diffuseMap = ddsKey;
    ddsMat.opacity = 1.0f;

    // Load textures from ERF
    ERFFile texErf;
    if (texErf.open(erfPath)) {
        for (const auto& entry : texErf.entries()) {
            std::string entryLower = entry.name;
            std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);

            // Branch TGA
            if (branchMat.diffuseTexId == 0) {
                std::string keyLower = branchKey;
                std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);
                if (entryLower == keyLower + ".tga") {
                    auto tgaData = texErf.readEntry(entry);
                    if (!tgaData.empty()) {
                        std::vector<uint8_t> rgba; int w, h;
                        if (decodeTGAToRGBA(tgaData, rgba, w, h)) {
                            branchMat.diffuseTexId = createTexture2D(rgba.data(), w, h);
                            branchMat.diffuseData = std::move(rgba);
                            branchMat.diffuseWidth = w;
                            branchMat.diffuseHeight = h;
                        }
                    }
                }
            }

            // Frond+Leaf DDS
            if (ddsMat.diffuseTexId == 0) {
                std::string keyLower = ddsKey;
                std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);
                if (entryLower == keyLower + ".dds" || entryLower == keyLower) {
                    auto ddsData = texErf.readEntry(entry);
                    if (!ddsData.empty()) {
                        ddsMat.diffuseTexId = createTextureFromDDS(ddsData);
                        decodeDDSToRGBA(ddsData, ddsMat.diffuseData, ddsMat.diffuseWidth, ddsMat.diffuseHeight);
                    }
                }
            }
        }
    }

    int branchMatIdx = (int)state.currentModel.materials.size();
    state.currentModel.materials.push_back(std::move(branchMat));
    int ddsMatIdx = (int)state.currentModel.materials.size();
    state.currentModel.materials.push_back(std::move(ddsMat));

    for (const auto& sm : spt.submeshes) {
        Mesh mesh;
        const char* typeNames[] = {"Branch", "Frond", "LeafCard", "LeafMesh"};
        mesh.name = typeNames[(int)sm.type];
        if (sm.type == SptSubmeshType::Branch) {
            mesh.materialIndex = branchMatIdx;
            mesh.materialName = branchKey;
            mesh.alphaTest = false;
        } else {
            mesh.materialIndex = ddsMatIdx;
            mesh.materialName = ddsKey;
            mesh.alphaTest = true;
        }

        uint32_t nv = sm.vertexCount();
        mesh.vertices.resize(nv);
        for (uint32_t i = 0; i < nv; i++) {
            auto& v = mesh.vertices[i];
            v.x  = sm.positions[i*3+0];
            v.y  = sm.positions[i*3+1];
            v.z  = sm.positions[i*3+2];
            v.nx = sm.normals[i*3+0];
            v.ny = sm.normals[i*3+1];
            v.nz = sm.normals[i*3+2];
            v.u  = sm.texcoords[i*2+0];
            v.v  = sm.texcoords[i*2+1];
        }
        mesh.indices = sm.indices;
        mesh.calculateBounds();
        state.currentModel.meshes.push_back(std::move(mesh));
    }

    state.hasModel = true;
    state.currentModel.calculateBounds();
    state.currentModelAnimations.clear();

    if (!state.currentModel.meshes.empty()) {
        float minX = 1e30f, maxX = -1e30f;
        float minY = 1e30f, maxY = -1e30f;
        float minZ = 1e30f, maxZ = -1e30f;
        for (const auto& m : state.currentModel.meshes) {
            if (m.minX < minX) minX = m.minX;
            if (m.maxX > maxX) maxX = m.maxX;
            if (m.minY < minY) minY = m.minY;
            if (m.maxY > maxY) maxY = m.maxY;
            if (m.minZ < minZ) minZ = m.minZ;
            if (m.maxZ > maxZ) maxZ = m.maxZ;
        }
        float cx = (minX+maxX)/2, cy = (minY+maxY)/2, cz = (minZ+maxZ)/2;
        float dx = maxX-minX, dy = maxY-minY, dz = maxZ-minZ;
        float r = std::sqrt(dx*dx+dy*dy+dz*dz)/2;
        if (r < 0.5f) r = 0.5f;
        state.camera.lookAt(cx, cy, cz, r * 2.0f);
        state.camera.moveSpeed = r * 0.05f;
    }

    return true;
}