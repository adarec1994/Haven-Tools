#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <functional>

struct DAOModelData {
    std::string name;

    struct Vertex {
        float x, y, z;
        float nx, ny, nz;
        float u, v;
    };

    struct MeshPart {
        std::string materialName;
        std::vector<Vertex> vertices;
        std::vector<uint16_t> indices;
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

// Callback types
using BackupConfirmCallback = std::function<bool(const std::string& erfName, const std::string& backupDir)>;
using ProgressCallback = std::function<void(float progress, const std::string& status)>;

class DAOImporter {
public:
    DAOImporter();
    ~DAOImporter();

    // Set callback for backup confirmation dialog
    // Called when no backup exists - return true to create backup, false to skip
    void SetBackupConfirmCallback(BackupConfirmCallback callback) { m_backupCallback = callback; }

    // Set callback for progress updates
    // Called with progress (0.0-1.0) and status message
    void SetProgressCallback(ProgressCallback callback) { m_progressCallback = callback; }

    // Import GLB to Dragon Age directory
    bool ImportToDirectory(const std::string& glbPath, const std::string& targetDir);

    // Convert GLB and add all assets to a single ERF
    bool ConvertAndAddToERF(const std::string& glbPath, const std::string& erfPath);

    // Check if backup exists for an ERF
    static bool BackupExists(const std::string& erfPath);

    // Get backup directory path
    static std::string GetBackupDir();

private:
    bool LoadGLB(const std::string& path, DAOModelData& outData);

    std::vector<uint8_t> GenerateMMH(const DAOModelData& model, const std::string& mshFilename);
    std::vector<uint8_t> GenerateMSH(const DAOModelData& model);
    std::string GenerateMAO(const std::string& materialName, const std::string& diffuse, const std::string& normal, const std::string& specular);

    bool RepackERF(const std::string& erfPath, const std::map<std::string, std::vector<uint8_t>>& newFiles);

    void ReportProgress(float progress, const std::string& status);

    BackupConfirmCallback m_backupCallback;
    ProgressCallback m_progressCallback;
};