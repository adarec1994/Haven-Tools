#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include "Mesh.h"
#include "erf.h"

// Forward declarations
class ERFFile;

struct Camera {
    float x = 0.0f, y = 0.0f, z = 5.0f;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float moveSpeed = 5.0f;
    float lookSensitivity = 0.003f;

    void setPosition(float px, float py, float pz) {
        x = px;
        y = py;
        z = pz;
    }

    void lookAt(float tx, float ty, float tz, float dist) {
        x = tx;
        y = tz + dist * 0.5f;
        z = ty + dist;
        yaw = 0.0f;
        pitch = -0.2f;
        moveSpeed = dist * 0.5f;
        if (moveSpeed < 1.0f) moveSpeed = 1.0f;
    }

    void getForward(float& fx, float& fy, float& fz) const {
        fx = -std::sin(yaw) * std::cos(pitch);
        fy = std::sin(pitch);
        fz = -std::cos(yaw) * std::cos(pitch);
    }

    void getRight(float& rx, float& ry, float& rz) const {
        rx = std::cos(yaw);
        ry = 0.0f;
        rz = -std::sin(yaw);
    }

    void moveForward(float amount) {
        float fx, fy, fz;
        getForward(fx, fy, fz);
        x += fx * amount;
        y += fy * amount;
        z += fz * amount;
    }

    void moveRight(float amount) {
        float rx, ry, rz;
        getRight(rx, ry, rz);
        x += rx * amount;
        z += rz * amount;
    }

    void moveUp(float amount) {
        y += amount;
    }

    void rotate(float deltaYaw, float deltaPitch) {
        yaw += deltaYaw;
        pitch += deltaPitch;
        if (pitch > 1.5f) pitch = 1.5f;
        if (pitch < -1.5f) pitch = -1.5f;
    }
};

struct RenderSettings {
    bool wireframe = false;
    bool showAxes = true;
    bool showGrid = true;
    bool showCollision = true;
    bool collisionWireframe = true;
    bool showSkeleton = true;
    bool showBoneNames = false;
    bool showTextures = true;
    std::vector<uint8_t> meshVisible;

    void initMeshVisibility(size_t count) {
        meshVisible.resize(count, 1);
    }
};

struct AppState {
    // UI state
    bool showBrowser = true;
    bool showRenderSettings = false;
    bool showMaoViewer = false;
    bool showUvViewer = false;
    bool showAnimWindow = false;

    // MAO viewer
    std::string maoContent;
    std::string maoFileName;

    // UV viewer
    int selectedMeshForUv = -1;

    // ERF browser
    std::string selectedFolder;
    std::vector<std::string> erfFiles;
    int selectedErfIndex = -1;
    std::unique_ptr<ERFFile> currentErf;
    int selectedEntryIndex = -1;
    std::string statusMessage;
    std::string extractPath;

    // Model state
    Model currentModel;
    bool hasModel = false;

    // Camera and rendering
    Camera camera;
    RenderSettings renderSettings;

    // Mouse state
    bool isPanning = false;
    double lastMouseX = 0;
    double lastMouseY = 0;

    // Animation
    std::vector<std::pair<std::string, std::string>> availableAnimFiles;
    int selectedAnimIndex = -1;
    Animation currentAnim;
    bool animPlaying = false;
    float animTime = 0.0f;
    float animSpeed = 1.0f;
    std::vector<Bone> basePoseBones;
    char animFilter[64] = "";
};

// Utility functions
std::string getExeDir();
void ensureExtractDir(const std::string& exeDir);
std::string versionToString(ERFVersion v);

// File type detection
bool isModelFile(const std::string& name);
bool isMaoFile(const std::string& name);
bool isPhyFile(const std::string& name);
bool isAnimFile(const std::string& name);