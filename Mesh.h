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

struct Mesh {
    std::string name;
    std::string materialName;  // Material reference
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Bounding box for camera positioning
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

// Collision shape types
enum class CollisionShapeType {
    Box,
    Sphere,
    Capsule,
    Cylinder,
    Mesh
};

struct CollisionShape {
    std::string name;
    CollisionShapeType type = CollisionShapeType::Box;

    // Position and rotation
    float posX = 0, posY = 0, posZ = 0;
    float rotX = 0, rotY = 0, rotZ = 0, rotW = 1;  // Quaternion

    // Dimensions (usage depends on type)
    float dimX = 1, dimY = 1, dimZ = 1;  // Box half-extents
    float radius = 1;                      // Sphere/Capsule/Cylinder radius
    float height = 1;                      // Capsule/Cylinder height

    // For mesh collision
    Mesh mesh;
};

struct Model {
    std::string name;
    std::vector<Mesh> meshes;
    std::vector<CollisionShape> collisionShapes;

    void calculateBounds() {
        for (auto& mesh : meshes) {
            mesh.calculateBounds();
        }
    }
};