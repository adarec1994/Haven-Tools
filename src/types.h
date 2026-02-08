#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <map>
#include <unordered_map>
#include "Mesh.h"
#include "erf.h"
#include "mor_loader.h"
#include "tnt_loader.h"
#include "GffViewer.h"

class ERFFile;
class GDAFile;

struct GDAEditorState {
    bool showWindow = false;
    std::string currentFile;
    GDAFile* editor = nullptr;
    int selectedRow = -1;
    char rowFilter[128] = "";
    bool showBackupDialog = false;
    bool showRestoreDialog = false;
    std::string statusMessage;
    std::vector<std::string> gdaFilesInErf;
    int selectedGdaInErf = -1;

    ~GDAEditorState();
};

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
        z = ty - dist;
        yaw = 3.14159f;
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
    bool useNormalMaps = true;
    bool useSpecularMaps = true;
    bool useTintMaps = true;
    std::vector<uint8_t> meshVisible;
    float hairColor[3] = {0.4f, 0.25f, 0.15f};
    float skinColor[3] = {1.0f, 1.0f, 1.0f};
    float eyeColor[3] = {0.4f, 0.3f, 0.2f};
    float ageAmount = 0.0f;
    float stubbleAmount[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float tattooAmount[3] = {0.0f, 0.0f, 0.0f};
    float tattooColor1[3] = {0.0f, 0.0f, 0.0f};
    float tattooColor2[3] = {0.0f, 0.0f, 0.0f};
    float tattooColor3[3] = {0.0f, 0.0f, 0.0f};
    float headZone1[3] = {1.0f, 1.0f, 1.0f};
    float headZone2[3] = {1.0f, 1.0f, 1.0f};
    float headZone3[3] = {1.0f, 1.0f, 1.0f};
    float armorZone1[3] = {1.0f, 1.0f, 1.0f};
    float armorZone2[3] = {1.0f, 1.0f, 1.0f};
    float armorZone3[3] = {1.0f, 1.0f, 1.0f};
    float clothesZone1[3] = {1.0f, 1.0f, 1.0f};
    float clothesZone2[3] = {1.0f, 1.0f, 1.0f};
    float clothesZone3[3] = {1.0f, 1.0f, 1.0f};
    float bootsZone1[3] = {1.0f, 1.0f, 1.0f};
    float bootsZone2[3] = {1.0f, 1.0f, 1.0f};
    float bootsZone3[3] = {1.0f, 1.0f, 1.0f};
    float glovesZone1[3] = {1.0f, 1.0f, 1.0f};
    float glovesZone2[3] = {1.0f, 1.0f, 1.0f};
    float glovesZone3[3] = {1.0f, 1.0f, 1.0f};
    float helmetZone1[3] = {1.0f, 1.0f, 1.0f};
    float helmetZone2[3] = {1.0f, 1.0f, 1.0f};
    float helmetZone3[3] = {1.0f, 1.0f, 1.0f};
    float tintZone1[3] = {1.0f, 1.0f, 1.0f};
    float tintZone2[3] = {1.0f, 1.0f, 1.0f};
    float tintZone3[3] = {1.0f, 1.0f, 1.0f};
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
    std::string source;
};
struct FSBSampleInfo {
    std::string name;
    uint32_t numSamples = 0;
    uint32_t compressedSize = 0;
    uint32_t mode = 0;
    uint32_t sampleRate = 0;
    uint16_t numChannels = 1;
    size_t dataOffset = 0;
    float duration = 0.0f;
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
    bool showTerrain = false;
    bool showBrowser = true;
    bool showRenderSettings = false;
    bool showMaoViewer = false;
    bool showUvViewer = false;
    bool showAnimWindow = false;
    bool showMeshBrowser = false;
    std::string lastRunVersion;
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
    std::vector<std::string> audioFiles;
    std::vector<std::string> voiceOverFiles;
    bool audioFilesLoaded = false;
    bool audioPlaying = false;
    std::string currentAudioPath;
    std::string currentAudioName;
    bool showAudioPlayer = false;
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
    TintCache tintCache;
    bool tintCacheLoaded = false;
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
        int selectedBeard = -1;
        int selectedArmor = 0;
        int selectedClothes = 0;
        int selectedBoots = 0;
        int selectedGloves = 0;
        int selectedHelmet = -1;
        int selectedRobe = -1;
        int rememberedHair = 0;
        float ageAmount = 0.0f;
        int selectedTattoo = -1;
        int armorStyle = 0;
        int clothesStyle = 0;
        int bootsStyle = 0;
        int glovesStyle = 0;
        int weaponStyle = 0;
        int selectedMainHandWeapon = -1;
        int selectedOffHandWeapon = -1;
        float hairColor[3] = {0.3f, 0.2f, 0.1f};
        float skinColor[3] = {0.9f, 0.7f, 0.6f};
        float eyeColor[3] = {0.4f, 0.3f, 0.2f};
        float stubbleAmount[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        float tattooAmount[3] = {0.0f, 0.0f, 0.0f};
        float tattooColor1[3] = {0.0f, 0.0f, 0.0f};
        float tattooColor2[3] = {0.0f, 0.0f, 0.0f};
        float tattooColor3[3] = {0.0f, 0.0f, 0.0f};
        float headTintZone1[3] = {1.0f, 1.0f, 1.0f};
        float headTintZone2[3] = {1.0f, 1.0f, 1.0f};
        float headTintZone3[3] = {1.0f, 1.0f, 1.0f};
        float armorTintZone1[3] = {1.0f, 1.0f, 1.0f};
        float armorTintZone2[3] = {1.0f, 1.0f, 1.0f};
        float armorTintZone3[3] = {1.0f, 1.0f, 1.0f};
        float clothesTintZone1[3] = {1.0f, 1.0f, 1.0f};
        float clothesTintZone2[3] = {1.0f, 1.0f, 1.0f};
        float clothesTintZone3[3] = {1.0f, 1.0f, 1.0f};
        float bootsTintZone1[3] = {1.0f, 1.0f, 1.0f};
        float bootsTintZone2[3] = {1.0f, 1.0f, 1.0f};
        float bootsTintZone3[3] = {1.0f, 1.0f, 1.0f};
        float glovesTintZone1[3] = {1.0f, 1.0f, 1.0f};
        float glovesTintZone2[3] = {1.0f, 1.0f, 1.0f};
        float glovesTintZone3[3] = {1.0f, 1.0f, 1.0f};
        float helmetTintZone1[3] = {1.0f, 1.0f, 1.0f};
        float helmetTintZone2[3] = {1.0f, 1.0f, 1.0f};
        float helmetTintZone3[3] = {1.0f, 1.0f, 1.0f};
        std::vector<std::pair<std::string, std::string>> heads;
        std::vector<std::pair<std::string, std::string>> hairs;
        std::vector<std::pair<std::string, std::string>> beards;
        std::vector<std::pair<std::string, std::string>> armors;
        std::vector<std::pair<std::string, std::string>> clothes;
        std::vector<std::pair<std::string, std::string>> boots;
        std::vector<std::pair<std::string, std::string>> gloves;
        std::vector<std::pair<std::string, std::string>> helmets;
        std::vector<std::pair<std::string, std::string>> robes;
        std::vector<std::pair<std::string, std::string>> tattoos;
        std::vector<std::pair<std::string, std::string>> swords;
        std::vector<std::pair<std::string, std::string>> greatswords;
        std::vector<std::pair<std::string, std::string>> daggers;
        std::vector<std::pair<std::string, std::string>> staves;
        std::vector<std::pair<std::string, std::string>> shields;
        std::vector<std::pair<std::string, std::string>> axes;
        std::vector<std::pair<std::string, std::string>> greataxes;
        std::vector<std::pair<std::string, std::string>> maces;
        std::vector<std::pair<std::string, std::string>> mauls;
        std::unordered_map<std::string, Model> partCache;
        std::string currentArmorPart;
        std::string currentClothesPart;
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
        std::vector<MorphPresetEntry> availableMorphPresets;
        int selectedMorphPreset = -1;
        MorphData morphData;
        bool morphLoaded = false;
        float faceMorphAmount = 1.0f;
        std::vector<Vertex> baseHeadVertices;
        std::vector<Vertex> baseEyesVertices;
        std::vector<Vertex> baseLashesVertices;
        int headMeshIndex = -1;
        int eyesMeshIndex = -1;
        int lashesMeshIndex = -1;
    } charDesigner;

    bool showFSBBrowser = false;
    std::string currentFSBPath;
    std::vector<FSBSampleInfo> currentFSBSamples;
    int selectedFSBSample = -1;
    char fsbSampleFilter[128] = "";

    GDAEditorState gdaEditor;
    GffViewerState gffViewer;
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