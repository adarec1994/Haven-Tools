#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <functional>

// Represents the raw data extracted from the GLB file
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

// Callback types for UI integration
using BackupConfirmCallback = std::function<bool(const std::string& erfName, const std::string& backupDir)>;
using ProgressCallback = std::function<void(float progress, const std::string& status)>;

class DAOImporter {
public:
    DAOImporter();
    ~DAOImporter();

    // Set callbacks
    void SetBackupConfirmCallback(BackupConfirmCallback callback) { m_backupCallback = callback; }
    void SetProgressCallback(ProgressCallback callback) { m_progressCallback = callback; }

    // Mode 1: loose files in override directory
    bool ImportToDirectory(const std::string& glbPath, const std::string& targetDir);

    // Mode 2: Pack everything into a single ERF
    bool ConvertAndAddToERF(const std::string& glbPath, const std::string& erfPath);

    // Utilities
    static bool BackupExists(const std::string& erfPath);
    static std::string GetBackupDir();

private:
    // Core GLB Loader
    bool LoadGLB(const std::string& path, DAOModelData& outData);

    // MSH Generation Pipeline (GLB -> XML -> Binary)
    std::string GenerateMSH_XML(const DAOModelData& model);
    std::vector<uint8_t> ConvertXMLToMSH(const std::string& xmlContent);
    std::vector<uint8_t> GenerateMSH(const DAOModelData& model);

    // Helpers
    std::vector<uint8_t> GenerateMMH(const DAOModelData& model, const std::string& mshFilename);
    std::string GenerateMAO(const std::string& materialName, const std::string& diffuse, const std::string& normal, const std::string& specular);
    bool RepackERF(const std::string& erfPath, const std::map<std::string, std::vector<uint8_t>>& newFiles);
    void ReportProgress(float progress, const std::string& status);

    BackupConfirmCallback m_backupCallback;
    ProgressCallback m_progressCallback;
};