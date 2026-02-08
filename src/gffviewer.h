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
        uint32_t numericLabel = 0;  // raw GFF4 field label ID for Index column
        bool isListItem = false;    // true for list items (show index, not label ID)
    };
    std::vector<TreeNode> flattenedTree;
    std::set<std::string> expandedPaths;
    char searchFilter[256] = {0};
    bool searchInValues = false;
    int selectedNodeIndex = -1;
    std::string statusMessage;
    void clear() {
        gff32.reset();
        gff4.reset();
        loadedFormat = Format::None;
        flattenedTree.clear();
        expandedPaths.clear();
        selectedNodeIndex = -1;
        searchFilter[0] = '\0';
        statusMessage.clear();
    }
    bool isLoaded() const {
        return loadedFormat != Format::None;
    }
};
bool loadGffData(GffViewerState& state, const std::vector<uint8_t>& data, 
                 const std::string& fileName, const std::string& erfSource = "");
void rebuildGffTree(GffViewerState& state);
void drawGffViewerWindow(GffViewerState& state);