#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>

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

class DAOImporter {
public:
    DAOImporter();
    ~DAOImporter();

    bool ImportToDirectory(const std::string& glbPath, const std::string& targetDir);
    bool ConvertAndAddToERF(const std::string& glbPath, const std::string& erfPath);

private:
    bool LoadGLB(const std::string& path, DAOModelData& outData);

    std::vector<uint8_t> GenerateMMH(const DAOModelData& model, const std::string& mshFilename);
    std::vector<uint8_t> GenerateMSH(const DAOModelData& model);
    std::string GenerateMAO(const std::string& materialName, const std::string& diffuse, const std::string& normal, const std::string& specular);

    bool RepackERF(const std::string& erfPath, const std::map<std::string, std::vector<uint8_t>>& newFiles);
};