#pragma once
#include "Gff32.h"
#include "Gff.h"
#include <string>
#include <vector>
#include <memory>
#include <set>
struct GffViewerState {
    bool showWindow = false;
    std::string fileName;
    std::string erfSource;
    std::unique_ptr<GFF32::GFF32File> gff32;
    std::unique_ptr<GFFFile> gff4;
    enum class Format { None, GFF32, GFF4 } loadedFormat = Format::None;
    struct TreeNode {
        std::string label;
        std::string typeName;
        std::string value;
        int depth = 0;
        bool isExpandable = false;
        bool isExpanded = false;
        size_t childCount = 0;
        std::string path;
        uint32_t structIndex = 0;
        uint32_t fieldIndex = 0;
        uint32_t baseOffset = 0;
        uint32_t numericLabel = 0;
        bool isListItem = false;
    };
    std::vector<TreeNode> flattenedTree;
    std::set<std::string> expandedPaths;
    char searchFilter[256] = {0};
    int filterColumn = 0;
    bool searchInValues = false;
    std::string lastFilterText;
    int selectedNodeIndex = -1;
    std::string statusMessage;
    std::string tlkPath;
    std::string tlkStatus;
    std::string gamePath;


    bool hasUnsavedChanges = false;
    int editingNodeIndex = -1;
    char editBuffer[4096] = {0};
    char editBuffer2[4096] = {0};
    bool showSaveConfirm = false;
    std::string lastSavePath;

    void clear() {
        gff32.reset();
        gff4.reset();
        loadedFormat = Format::None;
        flattenedTree.clear();
        expandedPaths.clear();
        selectedNodeIndex = -1;
        searchFilter[0] = '\0';
        lastFilterText.clear();
        statusMessage.clear();
        hasUnsavedChanges = false;
        editingNodeIndex = -1;
        editBuffer[0] = '\0';
        editBuffer2[0] = '\0';
        showSaveConfirm = false;
    }
    bool isLoaded() const {
        return loadedFormat != Format::None;
    }
};

bool loadGffData(GffViewerState& state, const std::vector<uint8_t>& data,
                 const std::string& fileName, const std::string& erfSource = "");
void rebuildGffTree(GffViewerState& state);
void drawGffViewerWindow(GffViewerState& state);


bool applyGffEdit(GffViewerState& state, int nodeIndex, const char* newValue, const char* newValue2 = nullptr);
bool saveGffFile(GffViewerState& state, const std::string& path);