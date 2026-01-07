#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <functional>
#include <filesystem>

namespace fs = std::filesystem;

struct DAOModelData {
    std::string name;

    struct Vertex {
        float x, y, z;
        float nx, ny, nz;
        float u, v;
        float tx, ty, tz, tw;  // tangent (w is handedness, typically 1.0 or -1.0)
    };

    struct MeshPart {
        std::string name;
        std::string materialName;
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    struct Texture {
        std::string originalName;
        std::string ddsName;
        int width = 0;
        int height = 0;
        int channels = 0;
        std::vector<uint8_t> data;
    };

    struct Material {
        std::string name;
        std::string diffuseMap;
        std::string normalMap;
        std::string specularMap;
    };

    std::vector<MeshPart> parts;
    std::vector<Texture> textures;
    std::vector<Material> materials;
};

using BackupConfirmCallback = std::function<bool(const std::string& erfName, const std::string& backupDir)>;
using ProgressCallback = std::function<void(float progress, const std::string& status)>;

class DAOGraphicsTools {
public:
    DAOGraphicsTools();
    ~DAOGraphicsTools();

    bool Initialize();
    bool IsReady() const { return m_initialized; }
    const fs::path& GetWorkDir() const { return m_workDir; }

    std::vector<uint8_t> ProcessMSH(const fs::path& xmlPath);
    std::vector<uint8_t> ProcessMMH(const fs::path& xmlPath);

    void Cleanup();

private:
    bool ExtractTools();
    bool RunProcessor(const fs::path& exePath, const fs::path& xmlPath);
    bool RunProcessorWithCmd(const fs::path& exePath, const std::string& cmdLine);
    std::vector<uint8_t> ReadBinaryFile(const fs::path& path);

    fs::path m_workDir;
    fs::path m_mshDir;
    fs::path m_mmhDir;
    fs::path m_aniDir;
    bool m_initialized = false;
};

class DAOImporter {
public:
    DAOImporter();
    ~DAOImporter();

    void SetBackupConfirmCallback(BackupConfirmCallback callback) { m_backupCallback = callback; }
    void SetProgressCallback(ProgressCallback callback) { m_progressCallback = callback; }

    bool ImportToDirectory(const std::string& glbPath, const std::string& targetDir);

    static bool BackupExists(const std::string& erfPath);
    static std::string GetBackupDir();

private:
    bool LoadGLB(const std::string& path, DAOModelData& outData);

    bool WriteMSHXml(const fs::path& outputPath, const DAOModelData& model);
    bool WriteMMHXml(const fs::path& outputPath, const DAOModelData& model, const std::string& mshFilename);
    std::string GenerateMAO(const std::string& materialName, const std::string& diffuse, const std::string& normal, const std::string& specular);

    bool RepackERF(const std::string& erfPath, const std::map<std::string, std::vector<uint8_t>>& newFiles);

    void ReportProgress(float progress, const std::string& status);

    DAOGraphicsTools m_tools;
    BackupConfirmCallback m_backupCallback;
    ProgressCallback m_progressCallback;
};