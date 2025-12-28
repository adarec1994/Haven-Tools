#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <map>
#include <unordered_map>
#include "Mesh.h"
#include "erf.h"
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
    float hairColor[3] = {0.4f, 0.25f, 0.15f};
    float ageAmount = 0.0f;
    int selectedTattoo = -1;
    void initMeshVisibility(size_t count) {
        meshVisible.assign(count, 1);
    }
};
struct MeshEntry {
    std::string mshFile;
    std::string mshName;
    int lod;
    std::string category;
    std::vector<std::string> animations;
};
struct CachedEntry {
    std::string name;
    size_t erfIdx;
    size_t entryIdx;
};
struct MeshBrowserState {
    std::vector<MeshEntry> allMeshes;
    std::vector<std::string> categories;
    int selectedCategory = 0;
    int selectedLod = 0;
    int selectedMeshIndex = -1;
    bool categorized = true;
    bool loaded = false;
    char meshFilter[64] = "";
};
struct AppState {
    bool showBrowser = true;
    bool showRenderSettings = false;
    bool showMaoViewer = false;
    bool showUvViewer = false;
    bool showAnimWindow = false;
    bool showMeshBrowser = false;
    std::string maoContent;
    std::string maoFileName;
    int selectedMeshForUv = -1;
    std::string selectedFolder;
    std::vector<std::string> erfFiles;
    std::vector<size_t> filteredErfIndices;
    std::map<std::string, std::vector<size_t>> erfsByName;
    std::string selectedErfName;
    std::vector<CachedEntry> mergedEntries;
    std::vector<int> filteredEntryIndices;
    std::string lastContentFilter;
    int selectedErfIndex = -1;
    std::unique_ptr<ERFFile> currentErf;
    int selectedEntryIndex = -1;
    std::string statusMessage;
    std::string extractPath;
    std::string lastDialogPath;
    char contentFilter[128] = "";
    CachedEntry pendingTextureExport;
    bool pendingTexExportPng = false;
    bool pendingTexExportDds = false;
    bool pendingTexDumpAll = false;
    bool pendingTexDumpPng = false;
    Model currentModel;
    bool hasModel = false;
    Camera camera;
    RenderSettings renderSettings;
    bool isPanning = false;
    double lastMouseX = 0;
    double lastMouseY = 0;
    std::vector<std::pair<std::string, std::string>> availableAnimFiles;
    std::vector<std::string> currentModelAnimations;
    int selectedAnimIndex = -1;
    Animation currentAnim;
    bool animPlaying = false;
    bool animLoop = true;
    float animTime = 0.0f;
    float animSpeed = 1.0f;
    std::vector<Bone> basePoseBones;
    char animFilter[64] = "";
    int selectedBoneIndex = -1;
    bool showTexturePreview = false;
    int previewTextureId = 0;
    std::string previewTextureName;
    int previewMeshIndex = -1;
    bool showUvOverlay = false;
    bool pendingExport = false;
    CachedEntry pendingExportEntry;
    MeshBrowserState meshBrowser;
    bool showHeadSelector = false;
    std::vector<std::string> availableHeads;
    std::vector<std::string> availableHeadNames;
    std::string pendingBodyMsh;
    CachedEntry pendingBodyEntry;
    int selectedHeadIndex = -1;
    std::vector<std::unique_ptr<ERFFile>> textureErfs;
    std::vector<std::string> textureErfPaths;
    std::vector<std::unique_ptr<ERFFile>> modelErfs;
    std::vector<std::string> modelErfPaths;
    std::vector<std::unique_ptr<ERFFile>> materialErfs;
    std::vector<std::string> materialErfPaths;
    bool textureErfsLoaded = false;
    bool modelErfsLoaded = false;
    bool materialErfsLoaded = false;
    std::map<std::string, std::vector<uint8_t>> meshCache;
    std::map<std::string, std::vector<uint8_t>> mmhCache;
    std::map<std::string, std::vector<uint8_t>> maoCache;
    std::map<std::string, std::vector<uint8_t>> textureCache;
    bool cacheBuilt = false;
    bool isPreloading = false;
    float preloadProgress = 0.0f;
    std::string preloadStatus;
    int mainTab = 0;
    struct CharacterDesigner {
        int race = 0;
        bool isMale = true;
        int equipTab = 0;
        int selectedHead = 0;
        int selectedHair = 0;
        int selectedArmor = 0;
        int selectedBoots = 0;
        int selectedGloves = 0;
        int selectedHelmet = -1;
        int selectedRobe = -1;
        int rememberedHair = 0;
        float ageAmount = 0.0f;
        int selectedTattoo = -1;
        float hairColor[3] = {0.3f, 0.2f, 0.1f};
        float skinColor[3] = {0.9f, 0.7f, 0.6f};
        std::vector<std::pair<std::string, std::string>> heads;
        std::vector<std::pair<std::string, std::string>> hairs;
        std::vector<std::pair<std::string, std::string>> armors;
        std::vector<std::pair<std::string, std::string>> boots;
        std::vector<std::pair<std::string, std::string>> gloves;
        std::vector<std::pair<std::string, std::string>> helmets;
        std::vector<std::pair<std::string, std::string>> robes;
        std::vector<std::pair<std::string, std::string>> tattoos;
        std::unordered_map<std::string, Model> partCache;
        std::string currentArmorPart;
        std::string currentBootsPart;
        std::string currentGlovesPart;
        std::string currentHeadPart;
        std::string currentHairPart;
        std::string currentHelmetPart;
        std::string currentEyesPart;
        std::string currentLashesPart;
        bool animsLoaded = false;
        bool needsRebuild = true;
        bool listsBuilt = false;
        std::string currentPrefix;
    } charDesigner;
};
std::string getExeDir();
void ensureExtractDir(const std::string& exeDir);
std::string versionToString(ERFVersion v);
bool isModelFile(const std::string& name);
bool isMaoFile(const std::string& name);
bool isPhyFile(const std::string& name);
bool isAnimFile(const std::string& name);
bool isMshFile(const std::string& name);
void dumpAllMshFileNames(const std::vector<std::string>& erfFiles);
bool isMshFile(const std::string& name);