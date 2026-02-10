#pragma once
#include "Gff32.h"
#include "Gff.h"
#include <string>
#include <vector>
#include <memory>
#include <set>
#include <algorithm>
#include <atomic>
#include <thread>
#include <mutex>
struct GffViewerState {
    bool showWindow = false;
    std::string fileName;
    std::string erfSource;
    size_t erfEntryIndex = 0;
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
        std::string searchAll;
        std::string searchLabel;
        std::string searchType;
        std::string searchValue;
        std::string searchIndex;
        void buildSearchKeys() {
            auto toLow = [](std::string s) {
                std::transform(s.begin(), s.end(), s.begin(), ::tolower);
                std::replace(s.begin(), s.end(), '_', ' ');
                return s;
            };
            searchLabel = toLow(label);
            searchType = toLow(typeName);
            searchValue = toLow(value);
            searchIndex = std::to_string(numericLabel);
            searchAll = searchLabel + " " + searchType + " " + searchValue + " " + searchIndex;
        }
    };
    std::vector<TreeNode> fullTree;
    std::vector<TreeNode> flattenedTree;
    std::vector<int> visibleIndices;
    std::vector<int> filteredIndices;
    bool cacheReady = false;

    std::atomic<bool> bgLoading{false};
    std::string bgStatusMessage;
    std::thread bgThread;

    std::set<std::string> expandedPaths;
    char searchFilter[256] = {0};
    int filterColumn = 0;
    int lastFilterColumn = -1;
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
    std::string lastEditPath;

    void stopBgThread() {
        if (bgThread.joinable()) bgThread.join();
    }
    void clear() {
        stopBgThread();
        gff32.reset();
        gff4.reset();
        loadedFormat = Format::None;
        flattenedTree.clear();
        fullTree.clear();
        visibleIndices.clear();
        filteredIndices.clear();
        cacheReady = false;
        bgLoading.store(false);
        expandedPaths.clear();
        selectedNodeIndex = -1;
        searchFilter[0] = '\0';
        lastFilterText.clear();
        lastFilterColumn = -1;
        statusMessage.clear();
        hasUnsavedChanges = false;
        editingNodeIndex = -1;
        editBuffer[0] = '\0';
        editBuffer2[0] = '\0';
        lastEditPath.clear();
    }
    ~GffViewerState() { stopBgThread(); }
    bool isLoaded() const {
        return loadedFormat != Format::None;
    }
};

bool loadGffData(GffViewerState& state, const std::vector<uint8_t>& data,
                 const std::string& fileName, const std::string& erfSource = "",
                 size_t erfEntryIndex = 0);
void rebuildGffTree(GffViewerState& state);
void drawGffViewerWindow(GffViewerState& state);
void drawGffLoadingOverlay(GffViewerState& state);

bool applyGffEdit(GffViewerState& state, const GffViewerState::TreeNode& node, const char* newValue, const char* newValue2 = nullptr);