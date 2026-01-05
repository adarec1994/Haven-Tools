#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include "erf.h"

// Intermediate structure to hold mesh data extracted from GLB
// Decouples the GLB loader from the DAO writer logic
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

    std::vector<MeshPart> parts;
    std::vector<std::string> texturePaths;
};

class DAOImporter {
public:
    DAOImporter();
    ~DAOImporter();

    // Main entry point: Converts GLB to DAO formats and adds to ERF
    // Returns true on success
    bool ImportToDirectory(const std::string& glbPath, const std::string& targetDir);
    bool ConvertAndAddToERF(const std::string& glbPath, const std::string& erfPath);

private:
    // Loads GLB and populates DAOModelData
    bool LoadGLB(const std::string& path, DAOModelData& outData);

    // Generators for specific DAO formats
    std::vector<uint8_t> GenerateMMH(const DAOModelData& model, const std::string& mshFilename);
    std::vector<uint8_t> GenerateMSH(const DAOModelData& model);

    // Updated signature to match import.cpp
    std::string GenerateMAO(const std::string& materialName, const std::string& diffuse, const std::string& normal, const std::string& specular);

    // Re-packs the ERF with new files
    bool RepackERF(const std::string& erfPath, const std::map<std::string, std::vector<uint8_t>>& newFiles);
};