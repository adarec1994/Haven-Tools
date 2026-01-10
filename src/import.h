#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <functional>
#include <filesystem>

namespace fs = std::filesystem;

struct ImportVertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
    float tx, ty, tz, tw;

    float boneWeights[4] = {0, 0, 0, 0};
    int boneIndices[4] = {0, 0, 0, 0};
};

struct ImportBone {
    std::string name;
    int index;
    int parentIndex;
    float translation[3];
    float rotation[4];
    float scale[3];
    float inverseBindMatrix[16];
};

struct DAOModelData {
    std::string name;

    struct MeshPart {
        std::string name;
        std::string materialName;
        std::vector<ImportVertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<int> bonesUsed;
        bool hasSkinning = false;
    };

    struct Material {
        std::string name;
        std::string diffuseMap;
        std::string normalMap;
        std::string specularMap;
    };

    struct Texture {
        std::string originalName;
        std::string ddsName;
        int width = 0, height = 0, channels = 0;
        std::vector<uint8_t> data;
    };

    struct Skeleton {
        std::vector<ImportBone> bones;
        bool hasSkeleton = false;
    };

    // Collision shapes imported from UE-named meshes
    enum class CollisionType { Box, Sphere, Capsule, Mesh };
    struct CollisionShape {
        std::string name;
        CollisionType type = CollisionType::Mesh;
        float posX = 0, posY = 0, posZ = 0;
        float rotX = 0, rotY = 0, rotZ = 0, rotW = 1;
        // Box dimensions (half-extents)
        float boxX = 1, boxY = 1, boxZ = 1;
        // Sphere/Capsule
        float radius = 1;
        float height = 2; // For capsule
        // Convex mesh data
        std::vector<float> meshVerts;
        std::vector<uint32_t> meshIndices;
    };

    std::vector<MeshPart> parts;
    std::vector<Material> materials;
    std::vector<Texture> textures;
    Skeleton skeleton;
    std::vector<CollisionShape> collisionShapes;
};

class DAOGraphicsTools {
public:
    DAOGraphicsTools();
    ~DAOGraphicsTools();

    bool Initialize();
    std::vector<uint8_t> ProcessMSH(const fs::path& xmlPath);
    std::vector<uint8_t> ProcessMMH(const fs::path& xmlPath);
    void Cleanup();

    fs::path GetWorkDir() const { return m_workDir; }
    fs::path GetMshDir() const { return m_mshDir; }
    fs::path GetMmhDir() const { return m_mmhDir; }

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

    using BackupCallback = std::function<bool(const std::string& erfName, const std::string& backupDir)>;
    using ProgressCallback = std::function<void(float progress, const std::string& status)>;

    void SetBackupCallback(BackupCallback cb) { m_backupCallback = cb; }
    void SetProgressCallback(ProgressCallback cb) { m_progressCallback = cb; }

    bool ImportToDirectory(const std::string& glbPath, const std::string& targetDir);

    static bool BackupExists(const std::string& erfPath);
    static std::string GetBackupDir();

private:
    bool LoadGLB(const std::string& path, DAOModelData& outData);
    bool WriteMSHXml(const fs::path& outputPath, const DAOModelData& model);
    bool WriteMMHXml(const fs::path& outputPath, const DAOModelData& model, const std::string& mshFilename);
    std::string GenerateMAO(const std::string& matName, const std::string& diffuse,
                            const std::string& normal, const std::string& specular);
    std::vector<uint8_t> GeneratePHY(const DAOModelData& model);
    bool RepackERF(const std::string& erfPath, const std::map<std::string, std::vector<uint8_t>>& newFiles);
    void ReportProgress(float progress, const std::string& status);

    DAOGraphicsTools m_tools;
    BackupCallback m_backupCallback;
    ProgressCallback m_progressCallback;
};