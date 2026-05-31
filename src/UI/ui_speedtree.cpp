#include "ui_internal.h"
#include "spt.h"
#include "renderer.h"
#include "terrain_loader.h"
#include "dds_loader.h"
#include <filesystem>

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
        // Save the offending .spt so the failure can be diagnosed offline.
        std::string dbgDir = tempDir + "haven_spt_failed\\";
        CreateDirectoryA(dbgDir.c_str(), NULL);
        std::string dbgName = name;
        for (auto& ch : dbgName) if (ch == '/' || ch == '\\') ch = '_';
        std::ofstream df(dbgDir + dbgName, std::ios::binary);
        if (df) df.write(reinterpret_cast<const char*>(sptData.data()), sptData.size());
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

    extractSptTextures(sptData, spt);

    std::string baseName = name;
    size_t dot = baseName.rfind('.');
    if (dot != std::string::npos) baseName = baseName.substr(0, dot);

    auto stripExt = [](const std::string& s) -> std::string {
        size_t d = s.rfind('.');
        return (d != std::string::npos) ? s.substr(0, d) : s;
    };

    // Search the .spt's own archive PLUS all the level's loaded archives for the
    // textures (env tree textures usually aren't in the .spt's own RIM). Original
    // assignment is kept: bark on branches, composite .dds on everything else.
    ensureBaseErfsLoaded(state);
    namespace fs = std::filesystem;
    std::vector<std::string> texPaths;
    texPaths.push_back(erfPath);
    {
        std::error_code ec;
        fs::path dir = fs::path(erfPath).parent_path();
        if (fs::is_directory(dir, ec))
            for (const auto& de : fs::directory_iterator(dir, ec)) {
                if (!de.is_regular_file()) continue;
                std::string fnl = de.path().filename().string();
                std::transform(fnl.begin(), fnl.end(), fnl.begin(), ::tolower);
                if (fnl.size() > 8 && fnl.substr(fnl.size() - 8) == ".gpu.rim")
                    texPaths.push_back(de.path().string());
            }
    }
    auto loadTexInto = [&](Material& mat, std::vector<std::string> names) {
        std::vector<std::string> cands;
        for (auto& nm : names) {
            if (nm.empty()) continue;
            std::string stem = stripExt(nm), nl = nm;
            std::transform(stem.begin(), stem.end(), stem.begin(), ::tolower);
            std::transform(nl.begin(), nl.end(), nl.begin(), ::tolower);
            cands.push_back(nl);
            cands.push_back(stem + ".dds");
            cands.push_back(stem + ".xds");
            cands.push_back(stem + ".tga");
        }
        if (cands.empty()) return;
        auto decodeInto = [&](const std::vector<uint8_t>& data) -> bool {
            if (data.empty()) return false;
            if (data.size() > 4 && data[0] == 'D' && data[1] == 'D' &&
                data[2] == 'S' && data[3] == ' ') {
                mat.diffuseTexId = createTextureFromDDS(data);
                decodeDDSToRGBA(data, mat.diffuseData, mat.diffuseWidth, mat.diffuseHeight);
                return mat.diffuseTexId != 0;
            }
            std::vector<uint8_t> rgba; int w = 0, h = 0;
            if (isXDS(data)) { if (!decodeXDSToRGBA(data, rgba, w, h)) return false; }
            else if (!decodeTGAToRGBA(data, rgba, w, h)) return false;
            mat.diffuseTexId = createTexture2D(rgba.data(), w, h);
            mat.diffuseData = std::move(rgba); mat.diffuseWidth = w; mat.diffuseHeight = h;
            return true;
        };
        auto matches = [&](const std::string& en) {
            std::string e = en;
            std::transform(e.begin(), e.end(), e.begin(), ::tolower);
            for (const auto& c : cands) if (e == c) return true;
            return false;
        };
        for (const auto& ap : texPaths) {
            ERFFile erf;
            if (!erf.open(ap)) continue;
            for (const auto& te : erf.entries())
                if (matches(te.name) && decodeInto(erf.readEntry(te))) return;
        }
        auto coll = [&](const std::vector<std::unique_ptr<ERFFile>>& c) -> bool {
            for (const auto& e : c)
                for (const auto& te : e->entries())
                    if (matches(te.name) && decodeInto(e->readEntry(te))) return true;
            return false;
        };
        if (coll(state.textureErfs)) return;
        if (coll(state.modelErfs)) return;
        if (coll(state.materialErfs)) return;
        if (state.currentErf)
            for (const auto& te : state.currentErf->entries())
                if (matches(te.name) && decodeInto(state.currentErf->readEntry(te))) return;
    };

    Material branchMat;
    std::string branchKey = spt.branchTexture.empty() ? baseName : stripExt(spt.branchTexture);
    branchMat.name = branchKey;
    branchMat.diffuseMap = branchKey;
    branchMat.opacity = 1.0f;
    loadTexInto(branchMat, {spt.branchTexture, branchKey});

    Material ddsMat;
    std::string ddsKey = baseName + "_diffuse";
    ddsMat.name = ddsKey;
    ddsMat.diffuseMap = ddsKey;
    ddsMat.opacity = 1.0f;
    loadTexInto(ddsMat, {spt.compositeTexture, ddsKey, baseName});

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