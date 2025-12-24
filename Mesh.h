#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <array>
#include <cmath>

struct Vertex {
    float x, y, z;          // Position
    float nx, ny, nz;       // Normal
    float u, v;             // TexCoord
};

// Material data parsed from MAO files
struct Material {
    std::string name;
    std::string maoContent;   // Raw MAO file content for viewing
    std::string diffuseMap;   // Path to diffuse texture
    std::string normalMap;    // Path to normal map
    std::string specularMap;  // Path to specular map
    std::string tintMap;      // Path to tint map

    // Material properties from MAO
    float specularPower = 50.0f;
    float opacity = 1.0f;

    // OpenGL texture ID (0 = not loaded)
    uint32_t diffuseTexId = 0;
    uint32_t normalTexId = 0;
    uint32_t specularTexId = 0;
};

// Collision shape types (from PHY files)
enum class CollisionShapeType {
    Box,      // boxs
    Sphere,   // sphs
    Capsule,  // caps
    Mesh      // mshs
};

struct CollisionShape {
    std::string name;
    CollisionShapeType type = CollisionShapeType::Box;

    // Position and rotation (quaternion)
    float posX = 0, posY = 0, posZ = 0;
    float rotX = 0, rotY = 0, rotZ = 0, rotW = 1;

    // Box dimensions (half-extents)
    float boxX = 1, boxY = 1, boxZ = 1;

    // Sphere/Capsule radius and height
    float radius = 1;
    float height = 2;

    // For mesh collision - vertices and indices
    std::vector<float> meshVerts;  // x,y,z triplets
    std::vector<uint32_t> meshIndices;
};

struct Bone {
    std::string name;
    std::string parentName;
    int parentIndex = -1;  // Index into skeleton's bones array, -1 = root

    // Local transform (relative to parent)
    float posX = 0, posY = 0, posZ = 0;
    float rotX = 0, rotY = 0, rotZ = 0, rotW = 1;

    // World transform (computed from hierarchy)
    float worldPosX = 0, worldPosY = 0, worldPosZ = 0;
    float worldRotX = 0, worldRotY = 0, worldRotZ = 0, worldRotW = 1;
};

struct Skeleton {
    std::vector<Bone> bones;

    // Find bone index by name, returns -1 if not found
    int findBone(const std::string& name) const {
        for (size_t i = 0; i < bones.size(); i++) {
            if (bones[i].name == name) return (int)i;
        }
        return -1;
    }
};

struct Mesh {
    std::string name;
    std::string materialName;  // Material reference (.mao file) - from MMH, not MSH
    int materialIndex = -1;    // Index into Model::materials, -1 = no material
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Bounding box
    float minX = 0, minY = 0, minZ = 0;
    float maxX = 0, maxY = 0, maxZ = 0;

    void calculateBounds() {
        if (vertices.empty()) return;

        minX = maxX = vertices[0].x;
        minY = maxY = vertices[0].y;
        minZ = maxZ = vertices[0].z;

        for (const auto& v : vertices) {
            if (v.x < minX) minX = v.x;
            if (v.x > maxX) maxX = v.x;
            if (v.y < minY) minY = v.y;
            if (v.y > maxY) maxY = v.y;
            if (v.z < minZ) minZ = v.z;
            if (v.z > maxZ) maxZ = v.z;
        }
    }

    std::array<float, 3> center() const {
        return { (minX + maxX) / 2.0f, (minY + maxY) / 2.0f, (minZ + maxZ) / 2.0f };
    }

    float radius() const {
        float dx = maxX - minX;
        float dy = maxY - minY;
        float dz = maxZ - minZ;
        return std::sqrt(dx*dx + dy*dy + dz*dz) / 2.0f;
    }
};

struct Model {
    std::string name;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;              // Loaded materials
    std::vector<CollisionShape> collisionShapes;  // From PHY file
    Skeleton skeleton;  // From MMH file

    void calculateBounds() {
        for (auto& mesh : meshes) {
            mesh.calculateBounds();
        }
    }

    // Find material index by name, returns -1 if not found
    int findMaterial(const std::string& name) const {
        for (size_t i = 0; i < materials.size(); i++) {
            if (materials[i].name == name) return (int)i;
        }
        return -1;
    }
};