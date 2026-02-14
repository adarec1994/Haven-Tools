#include "spt.h"
#include "dependencies/speedtree/spt_convert.h"
#include "dependencies/speedtree/speedtree_dll.h"

#include <fstream>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cstdlib>
#include <unistd.h>
#endif

#pragma pack(push, 1)
static const uint32_t SPTM_MAGIC   = 0x4D545053;
static const uint32_t SPTM_VERSION = 2;

struct SptmFileHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t numSubmeshes;
    float    boundMin[3];
    float    boundMax[3];
};

struct SptmSubmeshHeader {
    uint32_t type;
    uint32_t numVertices;
    uint32_t numIndices;
};
#pragma pack(pop)

static std::string g_exePath;
static std::string g_dllPath;
static std::string g_tempDir;

static bool writeFile(const std::string& path, const unsigned char* data, unsigned int size) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(data), size);
    return f.good();
}

bool initSpeedTree() {
#ifdef _WIN32
    char tmpBuf[MAX_PATH];
    GetTempPathA(MAX_PATH, tmpBuf);
    g_tempDir = std::string(tmpBuf) + "haven_speedtree\\";
    CreateDirectoryA(g_tempDir.c_str(), NULL);
#else
    g_tempDir = "/tmp/haven_speedtree/";
    mkdir(g_tempDir.c_str(), 0755);
#endif

    g_exePath = g_tempDir + "spt_convert.exe";
    g_dllPath = g_tempDir + "SpeedTreeRT.dll";

    if (!writeFile(g_exePath, spt_convert_exe_data, spt_convert_exe_size)) {
        fprintf(stderr, "[SPT] Failed to extract spt_convert.exe\n");
        return false;
    }

    if (!writeFile(g_dllPath, speedtree_dll_data, speedtree_dll_size)) {
        fprintf(stderr, "[SPT] Failed to extract SpeedTreeRT.dll\n");
        return false;
    }

    return true;
}

void shutdownSpeedTree() {
    if (!g_exePath.empty()) {
#ifdef _WIN32
        DeleteFileA(g_exePath.c_str());
        DeleteFileA(g_dllPath.c_str());
        RemoveDirectoryA(g_tempDir.c_str());
#else
        remove(g_exePath.c_str());
        remove(g_dllPath.c_str());
        rmdir(g_tempDir.c_str());
#endif
    }
    g_exePath.clear();
    g_dllPath.clear();
    g_tempDir.clear();
}

static bool runConverter(const std::string& inputPath, const std::string& outputPath) {
    std::string cmd = "\"" + g_exePath + "\" \"" + inputPath + "\" \"" + outputPath + "\"";

#ifdef _WIN32
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};

    std::string cmdLine = cmd;
    if (!CreateProcessA(NULL, &cmdLine[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        fprintf(stderr, "[SPT] CreateProcess failed: %lu\n", GetLastError());
        return false;
    }
    WaitForSingleObject(pi.hProcess, 30000);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode == 0;
#else
    return system(cmd.c_str()) == 0;
#endif
}

static bool readSptmesh(const std::string& path, SptModel& model) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    SptmFileHeader hdr;
    f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (!f || hdr.magic != SPTM_MAGIC || hdr.version != SPTM_VERSION) return false;

    memcpy(model.boundMin, hdr.boundMin, sizeof(hdr.boundMin));
    memcpy(model.boundMax, hdr.boundMax, sizeof(hdr.boundMax));
    model.submeshes.resize(hdr.numSubmeshes);

    for (uint32_t i = 0; i < hdr.numSubmeshes; ++i) {
        SptmSubmeshHeader sh;
        f.read(reinterpret_cast<char*>(&sh), sizeof(sh));
        if (!f) return false;

        auto& sm = model.submeshes[i];
        sm.type = static_cast<SptSubmeshType>(sh.type);

        sm.positions.resize(sh.numVertices * 3);
        sm.normals.resize(sh.numVertices * 3);
        sm.texcoords.resize(sh.numVertices * 2);
        sm.indices.resize(sh.numIndices);

        f.read(reinterpret_cast<char*>(sm.positions.data()), sh.numVertices * 3 * sizeof(float));
        f.read(reinterpret_cast<char*>(sm.normals.data()),   sh.numVertices * 3 * sizeof(float));
        f.read(reinterpret_cast<char*>(sm.texcoords.data()), sh.numVertices * 2 * sizeof(float));
        f.read(reinterpret_cast<char*>(sm.indices.data()),   sh.numIndices * sizeof(uint32_t));

        if (!f) return false;
    }

    return true;
}

bool loadSptModel(const std::string& sptPath, SptModel& outModel) {
    if (g_exePath.empty()) {
        fprintf(stderr, "[SPT] Not initialized - call initSpeedTree() first\n");
        return false;
    }

    std::string meshPath = g_tempDir + "temp_output.sptmesh";

    if (!runConverter(sptPath, meshPath)) {
        fprintf(stderr, "[SPT] Conversion failed for: %s\n", sptPath.c_str());
        return false;
    }

    bool ok = readSptmesh(meshPath, outModel);

#ifdef _WIN32
    DeleteFileA(meshPath.c_str());
#else
    remove(meshPath.c_str());
#endif

    return ok;
}

void extractSptTextures(const std::vector<uint8_t>& rawData, SptModel& model) {
    // Scan raw SPT binary for embedded texture filenames (.tga and .dds)
    // They appear as null-terminated ASCII strings
    std::vector<std::string> tgaFiles, ddsFiles;
    size_t n = rawData.size();

    for (size_t i = 0; i + 4 < n; ) {
        // Look for printable string runs ending with .tga or .dds
        if (rawData[i] >= 0x20 && rawData[i] <= 0x7e) {
            size_t start = i;
            while (i < n && rawData[i] >= 0x20 && rawData[i] <= 0x7e) i++;
            size_t len = i - start;
            if (len >= 5) {  // minimum: "a.tga" or "a.dds"
                std::string s(reinterpret_cast<const char*>(&rawData[start]), len);
                std::string lower = s;
                for (auto& c : lower) c = (char)tolower((unsigned char)c);
                if (lower.size() > 4) {
                    std::string ext = lower.substr(lower.size() - 4);
                    if (ext == ".tga") {
                        // Deduplicate
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

    // Categorize:
    // - First .tga is typically the branch/bark texture (at offset ~0x20)
    // - .tga files with common frond prefixes are frond textures
    // - _Diffuse.dds is the leaf composite
    // - Normal maps (*_n.tga) are skipped for diffuse assignment
    for (size_t i = 0; i < tgaFiles.size(); i++) {
        const auto& name = tgaFiles[i];
        std::string lower = name;
        for (auto& c : lower) c = (char)tolower((unsigned char)c);
        // Skip normal maps
        if (lower.size() > 6 && lower.substr(lower.size() - 6) == "_n.tga") continue;
        if (model.branchTexture.empty()) {
            model.branchTexture = name;  // First non-normal .tga = branch
        } else {
            // Remaining .tga files are frond textures
            bool dup = false;
            for (const auto& ft : model.frondTextures)
                if (ft == name) { dup = true; break; }
            if (!dup) model.frondTextures.push_back(name);
        }
    }

    for (const auto& name : ddsFiles) {
        std::string lower = name;
        for (auto& c : lower) c = (char)tolower((unsigned char)c);
        if (lower.find("diffuse") != std::string::npos || lower.find("_diffuse") != std::string::npos) {
            model.compositeTexture = name;
            break;
        }
    }
    // If no diffuse DDS found, use first DDS
    if (model.compositeTexture.empty() && !ddsFiles.empty())
        model.compositeTexture = ddsFiles[0];

    fprintf(stderr, "[SPT] Textures: branch='%s' fronds=%zu composite='%s'\n",
            model.branchTexture.c_str(), model.frondTextures.size(),
            model.compositeTexture.c_str());
}