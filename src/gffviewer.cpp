#include "GffViewer.h"
#include "Gff4FieldNames.h"
#include "erf.h"
#include "imgui.h"
#include "ImGuiFileDialog.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <set>
static bool containsCI(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    std::string h = haystack, n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    std::replace(h.begin(), h.end(), '_', ' ');
    std::replace(n.begin(), n.end(), '_', ' ');
    return h.find(n) != std::string::npos;
}

static std::string gff32ValuePreview(const GFF32::Structure& st) {
    if (st.hasField("Tag")) {
        const GFF32::Field* f = st.getField("Tag");
        if (f && (f->typeId == GFF32::TypeID::ExoString || f->typeId == GFF32::TypeID::ResRef)) {
            return "# " + std::get<std::string>(f->value);
        }
    }
    return "...";
}

static std::string gff4PrimitiveName(uint16_t typeId) {
    switch (typeId) {
        case 0: return "UINT8";
        case 1: return "INT8";
        case 2: return "UINT16";
        case 3: return "INT16";
        case 4: return "UINT32";
        case 5: return "INT32";
        case 6: return "UINT64";
        case 7: return "INT64";
        case 8: return "FLOAT32";
        case 9: return "FLOAT64";
        case 10: return "Vector3f";
        case 11: return "Vector2f";
        case 12: return "Vector4f";
        case 13: return "Quaternionf";
        case 14: return "ECString";
        case 15: return "Color4f";
        case 16: return "Matrix4x4f";
        case 17: return "TlkString";
        default: return "Type" + std::to_string(typeId);
    }
}

static std::string gff4TypeDesc(const GFFFile& gff, uint16_t typeId, uint16_t flags) {
    bool isList = (flags & FLAG_LIST) != 0;
    bool isStruct = (flags & FLAG_STRUCT) != 0;
    bool isRef = (flags & FLAG_REFERENCE) != 0;
    std::string inner;
    if (isStruct) {
        std::string fourcc;
        if (typeId < gff.structs().size())
            fourcc = gff.structs()[typeId].structType;
        else
            fourcc = "Struct" + std::to_string(typeId);
        inner = isRef ? ("*" + fourcc) : fourcc;
    } else {
        std::string name = gff4PrimitiveName(typeId);
        inner = isRef ? ("*" + name) : name;
    }
    if (isList) return "[" + inner + "]";
    return inner;
}

static std::string gff4StructTypeDesc(const GFFFile& gff, uint32_t structIndex, bool indirect) {
    if (structIndex < gff.structs().size()) {
        std::string fourcc = gff.structs()[structIndex].structType;
        return indirect ? ("*" + fourcc) : fourcc;
    }
    return "?";
}

static std::string readGffString(const GFFFile& gff, uint32_t address) {
    if (address == 0xFFFFFFFF) return "";
    const auto& cache = gff.stringCache();
    if (gff.isV41()) {
        if (address < cache.size()) return cache[address];
        return "";
    }
    const auto& raw = gff.rawData();
    uint32_t strPos = gff.dataOffset() + address;
    if (strPos + 4 > raw.size()) {
        return "";
    }
    uint32_t length = gff.readUInt32At(strPos);
    strPos += 4;
    std::string result;
    for (uint32_t i = 0; i < length && strPos + 2 <= raw.size(); i++) {
        uint16_t wc = gff.readUInt16At(strPos);
        strPos += 2;
        if (wc == 0) continue;
        if (wc < 0x80) result += static_cast<char>(wc);
        else if (wc < 0x800) {
            result += static_cast<char>(0xC0 | (wc >> 6));
            result += static_cast<char>(0x80 | (wc & 0x3F));
        } else {
            result += static_cast<char>(0xE0 | (wc >> 12));
            result += static_cast<char>(0x80 | ((wc >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (wc & 0x3F));
        }
    }
    return result;
}

static std::string gff4ReadFieldValueStr(const GFFFile& gff, const GFFField& field, uint32_t baseOffset) {
    bool isList = (field.flags & FLAG_LIST) != 0;
    bool isStruct = (field.flags & FLAG_STRUCT) != 0;
    bool isRef = (field.flags & FLAG_REFERENCE) != 0;
    if (isList || isStruct || isRef) {
        if (isRef && field.typeId == 17)
        return "...";
    }

    uint32_t dataPos = gff.dataOffset() + baseOffset + field.dataOffset;
    const auto& raw = gff.rawData();

    switch (field.typeId) {
        case 0: return std::to_string(gff.readUInt8At(dataPos));
        case 1: return std::to_string((int8_t)gff.readUInt8At(dataPos));
        case 2: return std::to_string(gff.readUInt16At(dataPos));
        case 3: return std::to_string(gff.readInt16At(dataPos));
        case 4: return std::to_string(gff.readUInt32At(dataPos));
        case 5: return std::to_string(gff.readInt32At(dataPos));
        case 6: {
            uint64_t val = 0;
            if (dataPos + 8 <= raw.size()) std::memcpy(&val, &raw[dataPos], 8);
            return std::to_string(val);
        }
        case 7: {
            int64_t val = 0;
            if (dataPos + 8 <= raw.size()) std::memcpy(&val, &raw[dataPos], 8);
            return std::to_string(val);
        }
        case 8: return std::to_string(gff.readFloatAt(dataPos));
        case 9: {
            double val = 0;
            if (dataPos + 8 <= raw.size()) std::memcpy(&val, &raw[dataPos], 8);
            return std::to_string(val);
        }
        case 10: {
            float x = gff.readFloatAt(dataPos);
            float y = gff.readFloatAt(dataPos + 4);
            float z = gff.readFloatAt(dataPos + 8);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3) << x << ", " << y << ", " << z;
            return oss.str();
        }
        case 11: {
            float x = gff.readFloatAt(dataPos);
            float y = gff.readFloatAt(dataPos + 4);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3) << x << ", " << y;
            return oss.str();
        }
        case 12: case 13: case 15: {
            float a = gff.readFloatAt(dataPos);
            float b = gff.readFloatAt(dataPos + 4);
            float c = gff.readFloatAt(dataPos + 8);
            float d = gff.readFloatAt(dataPos + 12);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3) << a << ", " << b << ", " << c << ", " << d;
            return oss.str();
        }
        case 14: {
            uint32_t address = gff.readUInt32At(dataPos);
            return readGffString(gff, address);
        }
        case 16: return "Matrix4x4";
        case 17: {
            uint32_t tlkId = gff.readUInt32At(dataPos);
            uint32_t address = gff.readUInt32At(dataPos + 4);
            std::string label = std::to_string(tlkId);
            std::string text;
            if (address != 0xFFFFFFFF && (address != 0 || gff.isV41()))
                text = readGffString(gff, address);
            if (text.empty() && GFF4TLK::isLoaded())
                text = GFF4TLK::lookup(tlkId);
            if (!text.empty()) {
                if (text.size() > 80) text = text.substr(0, 80) + "...";
                return label + ", " + text;
            }
            return label;
        }
        default: return "?";
    }
}

static std::string gff4StructPreview(const GFFFile& gff, uint32_t structIndex, uint32_t baseOffset) {
    if (structIndex >= gff.structs().size()) return "?";
    const GFFStruct& st = gff.structs()[structIndex];
    std::string result;
    int totalLen = 0;
    for (size_t i = 0; i < st.fields.size(); ++i) {
        const GFFField& field = st.fields[i];
        if (!result.empty()) { result += ", "; totalLen += 2; }
        std::string val = gff4ReadFieldValueStr(gff, field, baseOffset);
        totalLen += (int)val.size();
        if (totalLen > 100) { result += "..."; break; }
        result += val;
        if (val == "...") break;
    }
    return result;
}

static std::string gff4ListPreview(size_t count) {
    if (count == 0) return "(no items)";
    if (count == 1) return "(1 item)";
    return "(" + std::to_string(count) + " items)";
}

static void buildFullTree(GffViewerState& state);
static void requestCacheBuild(GffViewerState& state);

bool loadGffData(GffViewerState& state, const std::vector<uint8_t>& data,
                 const std::string& fileName, const std::string& erfSource, size_t erfEntryIndex) {
    state.clear();
    state.fileName = fileName;
    state.erfSource = erfSource;
    state.erfEntryIndex = erfEntryIndex;
    if (data.empty()) { state.statusMessage = "Empty data"; return false; }

    bool parsed = false;
    if (GFF32::GFF32File::isGFF32(data)) {
        state.gff32 = std::make_unique<GFF32::GFF32File>();
        if (state.gff32->load(data)) {
            state.loadedFormat = GffViewerState::Format::GFF32;
            state.statusMessage = "Loaded GFF 3.2: " + state.gff32->fileType();
            parsed = true;
        } else {
            state.gff32.reset();
        }
    }
    if (!parsed && data.size() >= 4 && data[0] == 'G' && data[1] == 'F' && data[2] == 'F' && data[3] == ' ') {
        state.gff4 = std::make_unique<GFFFile>();
        if (state.gff4->load(data)) {
            state.loadedFormat = GffViewerState::Format::GFF4;
            state.statusMessage = "Loaded GFF 4";
            if (!GFF4TLK::isLoaded() && !state.gamePath.empty()) {
                int tlkCount = GFF4TLK::loadAllFromPath(state.gamePath);
                if (tlkCount > 0)
                    state.tlkStatus = "Loaded " + std::to_string(GFF4TLK::count()) + " strings from " + std::to_string(tlkCount) + " TLK files";
            }
            parsed = true;
        } else {
            state.gff4.reset();
        }
    }

    state.showWindow = false;
    state.bgLoading.store(true);
    state.bgStatusMessage = "Building tree for " + fileName + "...";
    state.stopBgThread();
    state.bgThread = std::thread([&state]() {
        try {
            buildFullTree(state);
            rebuildGffTree(state);
        } catch (const std::exception& e) {
            state.statusMessage = std::string("Error: ") + e.what();
        } catch (...) {
            state.statusMessage = "Error building tree";
        }
        state.bgLoading.store(false);
        state.showWindow = true;
    });
    return true;
}

static void buildTreeFromGff32Struct(GffViewerState& state, const GFF32::Structure& st,
                                      const std::string& basePath, int depth, bool forceExpand = false);
static void buildTreeFromGff4Struct(GffViewerState& state, uint32_t structIndex,
                                     uint32_t baseOffset, int depth, const std::string& basePath, bool forceExpand = false,
                                     std::set<std::pair<uint32_t,uint32_t>>* visited = nullptr);
void rebuildGffTree(GffViewerState& state) {
    state.visibleIndices.clear();
    if (!state.cacheReady || state.fullTree.empty()) return;
    if (state.expandedPaths.empty()) state.expandedPaths.insert("");
    int skipAboveDepth = -1;
    for (int i = 0; i < (int)state.fullTree.size(); ++i) {
        const auto& node = state.fullTree[i];
        if (skipAboveDepth >= 0 && node.depth > skipAboveDepth) continue;
        skipAboveDepth = -1;
        state.visibleIndices.push_back(i);
        if (node.isExpandable && state.expandedPaths.count(node.path) == 0)
            skipAboveDepth = node.depth;
    }
}

static void buildFullTree(GffViewerState& state) {
    state.fullTree.clear();
    std::set<std::string> savedPaths = state.expandedPaths;
    std::vector<GffViewerState::TreeNode> savedFlat = std::move(state.flattenedTree);
    state.flattenedTree.clear();

    if (state.loadedFormat == GffViewerState::Format::GFF32 && state.gff32 && state.gff32->root()) {
        GffViewerState::TreeNode rootNode;
        rootNode.label = state.gff32->fileType() + " " + state.gff32->fileVersion();
        rootNode.typeName = "Root"; rootNode.value = ""; rootNode.depth = 0;
        rootNode.isExpandable = true; rootNode.isExpanded = true;
        rootNode.childCount = state.gff32->root()->fieldCount(); rootNode.path = "";
        state.flattenedTree.push_back(rootNode);
        buildTreeFromGff32Struct(state, *state.gff32->root(), "", 1, true);
    }
    else if (state.loadedFormat == GffViewerState::Format::GFF4 && state.gff4 && state.gff4->isLoaded()) {
        const auto& hdr = state.gff4->header();
        char fileType[5]={0}, fileVer[5]={0}, platform[5]={0};
        std::memcpy(fileType, &hdr.fileType, 4);
        std::memcpy(fileVer, &hdr.fileVersion, 4);
        std::memcpy(platform, &hdr.platform, 4);
        std::string version = (hdr.version == 0x56342E30) ? "V4.0" : ((hdr.version == 0x56342E31) ? "V4.1" : "V4.?");
        GffViewerState::TreeNode rootNode;
        rootNode.numericLabel = 0;
        rootNode.label = std::string("GFF  ") + version + " " + fileType + " " + fileVer + " " + platform;
        rootNode.typeName = !state.gff4->structs().empty() ? std::string(state.gff4->structs()[0].structType) : "?";
        rootNode.value = !state.gff4->structs().empty() ? gff4StructPreview(*state.gff4, 0, 0) : "";
        rootNode.depth = 0; rootNode.isExpandable = !state.gff4->structs().empty();
        rootNode.isExpanded = true;
        rootNode.childCount = state.gff4->structs().empty() ? 0 : state.gff4->structs()[0].fields.size();
        rootNode.path = ""; rootNode.structIndex = 0; rootNode.baseOffset = 0; rootNode.isListItem = false;
        state.flattenedTree.push_back(rootNode);
        if (!state.gff4->structs().empty())
            buildTreeFromGff4Struct(state, 0, 0, 1, "", true);
    }
    state.fullTree = std::move(state.flattenedTree);
    state.flattenedTree = std::move(savedFlat);
    state.expandedPaths = savedPaths;
    for (auto& node : state.fullTree) node.buildSearchKeys();
    state.cacheReady = true;
}

static void requestCacheBuild(GffViewerState& state) {
    state.cacheReady = false;
    state.fullTree.clear();
    state.filteredIndices.clear();
    buildFullTree(state);
}

static void refilterTree(GffViewerState& state) {
    state.filteredIndices.clear();
    std::string needle = state.searchFilter;
    if (needle.empty()) return;
    std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
    std::replace(needle.begin(), needle.end(), '_', ' ');
    bool isGff4 = state.loadedFormat == GffViewerState::Format::GFF4;
    int col = state.filterColumn;
    if (!isGff4 && col > 0) col++;
    for (int i = 0; i < (int)state.fullTree.size(); ++i) {
        const auto& node = state.fullTree[i];
        bool matches = false;
        switch (col) {
            case 0: matches = node.searchAll.find(needle) != std::string::npos; break;
            case 1: matches = node.searchIndex.find(needle) != std::string::npos; break;
            case 2: matches = node.searchLabel.find(needle) != std::string::npos; break;
            case 3: matches = node.searchType.find(needle) != std::string::npos; break;
            case 4: matches = node.searchValue.find(needle) != std::string::npos; break;
        }
        if (matches) state.filteredIndices.push_back(i);
    }
}

static void buildTreeFromGff32Struct(GffViewerState& state, const GFF32::Structure& st,
                                      const std::string& basePath, int depth, bool forceExpand) {
    if (depth > 100 || state.flattenedTree.size() > 500000) return;
    for (const auto& label : st.fieldOrder) {
        auto it = st.fields.find(label);
        if (it == st.fields.end()) continue;
        const GFF32::Field& field = it->second;
        std::string path = basePath.empty() ? label : basePath + "." + label;
        GffViewerState::TreeNode node;
        node.label = label;
        node.depth = depth;
        node.path = path;
        bool isStruct = field.typeId == GFF32::TypeID::Structure;
        bool isList = field.typeId == GFF32::TypeID::List;
        bool isLocString = field.typeId == GFF32::TypeID::ExoLocString;
        if (isStruct) {
            auto ptr = std::get<GFF32::StructurePtr>(field.value);
            node.typeName = "Structure:" + std::to_string(ptr ? ptr->structId : -1);
            node.isExpandable = ptr && ptr->fieldCount() > 0;
            node.childCount = ptr ? ptr->fieldCount() : 0;
            node.value = ptr ? gff32ValuePreview(*ptr) : "(null)";
        } else if (isList) {
            auto ptr = std::get<GFF32::ListPtr>(field.value);
            node.typeName = "List";
            node.isExpandable = ptr && !ptr->empty();
            node.childCount = ptr ? ptr->size() : 0;
            node.value = ptr ? ("(" + std::to_string(ptr->size()) + " items)") : "(empty)";
        } else if (isLocString) {
            auto& loc = std::get<GFF32::ExoLocString>(field.value);
            node.typeName = "ExoLocString";
            node.isExpandable = !loc.strings.empty();
            node.childCount = loc.strings.size();
            node.value = std::to_string(loc.stringref);
        } else {
            node.typeName = field.getTypeName();
            node.isExpandable = false;
            node.childCount = 0;
            node.value = field.getDisplayValue();
        }
        node.isExpanded = forceExpand || state.expandedPaths.count(path) > 0;
        state.flattenedTree.push_back(node);
        if (node.isExpanded) {
            if (isStruct) {
                auto ptr = std::get<GFF32::StructurePtr>(field.value);
                if (ptr) buildTreeFromGff32Struct(state, *ptr, path, depth + 1, forceExpand);
            } else if (isList) {
                auto ptr = std::get<GFF32::ListPtr>(field.value);
                if (ptr) {
                    for (size_t i = 0; i < ptr->size(); ++i) {
                        std::string itemPath = path + "[" + std::to_string(i) + "]";
                        const GFF32::Structure& item = (*ptr)[i];
                        GffViewerState::TreeNode itemNode;
                        itemNode.label = std::to_string(i);
                        itemNode.typeName = "Structure:" + std::to_string(item.structId);
                        itemNode.value = gff32ValuePreview(item);
                        itemNode.depth = depth + 1;
                        itemNode.path = itemPath;
                        itemNode.isExpandable = item.fieldCount() > 0;
                        itemNode.childCount = item.fieldCount();
                        itemNode.isExpanded = forceExpand || state.expandedPaths.count(itemPath) > 0;
                        state.flattenedTree.push_back(itemNode);
                        if (itemNode.isExpanded)
                            buildTreeFromGff32Struct(state, item, itemPath, depth + 2, forceExpand);
                    }
                }
            } else if (isLocString) {
                auto& loc = std::get<GFF32::ExoLocString>(field.value);
                for (size_t i = 0; i < loc.strings.size(); ++i) {
                    std::string itemPath = path + "[" + std::to_string(i) + "]";
                    const auto& ls = loc.strings[i];
                    GffViewerState::TreeNode lsNode;
                    lsNode.label = std::to_string(i);
                    std::string lang;
                    switch (ls.language) {
                        case 0: lang = "English"; break;
                        case 1: lang = "French"; break;
                        case 2: lang = "German"; break;
                        case 3: lang = "Italian"; break;
                        case 4: lang = "Spanish"; break;
                        case 5: lang = "Polish"; break;
                        default: lang = "Lang" + std::to_string(ls.language); break;
                    }
                    lsNode.typeName = lang + (ls.gender ? " (F)" : " (M)");
                    lsNode.value = ls.text;
                    lsNode.depth = depth + 1;
                    lsNode.path = itemPath;
                    lsNode.isExpandable = false;
                    lsNode.childCount = 0;
                    lsNode.isExpanded = false;
                    state.flattenedTree.push_back(lsNode);
                }
            }
        }
    }
}

static void buildTreeFromGff4Struct(GffViewerState& state, uint32_t structIndex,
                                     uint32_t baseOffset, int depth, const std::string& basePath, bool forceExpand,
                                     std::set<std::pair<uint32_t,uint32_t>>* visited) {
    if (depth > 100 || state.flattenedTree.size() > 500000) return;
    if (!state.gff4 || structIndex >= state.gff4->structs().size()) return;

    std::set<std::pair<uint32_t,uint32_t>> localVisited;
    if (!visited) visited = &localVisited;
    auto key = std::make_pair(structIndex, baseOffset);
    if (visited->count(key)) {
        return;
    }
    visited->insert(key);

    const GFFStruct& st = state.gff4->structs()[structIndex];
    for (size_t i = 0; i < st.fields.size(); ++i) {
        const GFFField& field = st.fields[i];
        std::string path = basePath.empty() ? std::to_string(field.label) : basePath + "." + std::to_string(field.label);
        GffViewerState::TreeNode node;
        node.numericLabel = field.label;
        node.label = getGFF4FieldName(field.label);
        node.typeName = gff4TypeDesc(*state.gff4, field.typeId, field.flags);
        node.depth = depth;
        node.path = path;
        node.structIndex = structIndex;
        node.fieldIndex = static_cast<uint32_t>(i);
        node.baseOffset = baseOffset;
        node.isListItem = false;

        bool isList = (field.flags & FLAG_LIST) != 0;
        bool isStruct = (field.flags & FLAG_STRUCT) != 0;
        bool isRef = (field.flags & FLAG_REFERENCE) != 0;

        if (isList && (isStruct || isRef)) {
            node.isExpandable = true;
            auto items = state.gff4->readStructList(structIndex, field.label, baseOffset);
            node.childCount = items.size();
            node.value = gff4ListPreview(items.size());
        } else if (isList && !isStruct && !isRef) {
            auto [count, dataStart] = state.gff4->readPrimitiveListInfo(structIndex, field.label, baseOffset);
            node.childCount = count;
            node.isExpandable = count > 0;
            node.value = gff4ListPreview(count);
        } else if (isRef && !isStruct && field.typeId <= 17) {

            uint32_t ptrPos = state.gff4->dataOffset() + baseOffset + field.dataOffset;
            const auto& raw = state.gff4->rawData();
            if (ptrPos + 4 <= raw.size()) {
                uint32_t ptr = state.gff4->readUInt32At(ptrPos);
                if (ptr == 0xFFFFFFFF) {
                    node.value = "null";
                } else {
                    GFFField deref = field;
                    deref.flags &= ~FLAG_REFERENCE;
                    if (field.typeId == 14) {

                        node.value = gff4ReadFieldValueStr(*state.gff4, deref, baseOffset);
                    } else {

                        deref.dataOffset = 0;
                        node.value = gff4ReadFieldValueStr(*state.gff4, deref, ptr);
                    }
                }
            } else {
                node.value = "?";
            }
            node.isExpandable = false;
            node.childCount = 0;
        } else if (isRef && !isStruct) {
            auto ref = state.gff4->readStructRef(structIndex, field.label, baseOffset);
            if (ref.structIndex < state.gff4->structs().size()) {
                node.isExpandable = true;
                node.childCount = state.gff4->structs()[ref.structIndex].fields.size();
                node.typeName = gff4StructTypeDesc(*state.gff4, ref.structIndex, true);
                node.value = gff4StructPreview(*state.gff4, ref.structIndex, ref.offset);
            } else {
                node.isExpandable = false;
                node.childCount = 0;
                node.value = "...";
            }
        } else if (isStruct && isRef) {
            node.isExpandable = true;
            auto ref = state.gff4->readStructRef(structIndex, field.label, baseOffset);
            node.childCount = (ref.structIndex < state.gff4->structs().size())
                ? state.gff4->structs()[ref.structIndex].fields.size() : 0;
            node.value = (ref.structIndex < state.gff4->structs().size())
                ? gff4StructPreview(*state.gff4, ref.structIndex, ref.offset) : "?";
        } else if (isStruct && !isRef) {
            node.isExpandable = true;
            uint32_t embStructOffset = baseOffset + field.dataOffset;
            uint32_t embStructIdx = field.typeId;
            if (embStructIdx < state.gff4->structs().size()) {
                node.childCount = state.gff4->structs()[embStructIdx].fields.size();
                node.value = gff4StructPreview(*state.gff4, embStructIdx, embStructOffset);
            } else {
                node.childCount = 0; node.value = "?";
            }
        } else {
            node.isExpandable = false;
            node.childCount = 0;
            node.value = gff4ReadFieldValueStr(*state.gff4, field, baseOffset);
        }

        node.isExpanded = forceExpand || state.expandedPaths.count(path) > 0;
        state.flattenedTree.push_back(node);

        if (node.isExpanded && node.isExpandable) {
            if (isList && (isStruct || isRef)) {
                auto items = state.gff4->readStructList(structIndex, field.label, baseOffset);
                bool elemIndirect = isRef;
                for (size_t j = 0; j < items.size(); ++j) {
                    std::string itemPath = path + "[" + std::to_string(j) + "]";
                    GffViewerState::TreeNode itemNode;
                    itemNode.numericLabel = static_cast<uint32_t>(j);
                    itemNode.label = "";
                    itemNode.isListItem = true;
                    if (items[j].structIndex < state.gff4->structs().size()) {
                        itemNode.typeName = gff4StructTypeDesc(*state.gff4, items[j].structIndex, elemIndirect);
                        itemNode.childCount = state.gff4->structs()[items[j].structIndex].fields.size();
                        itemNode.value = gff4StructPreview(*state.gff4, items[j].structIndex, items[j].offset);
                    } else {
                        itemNode.typeName = "?";
                        itemNode.childCount = 0;
                        itemNode.value = "?";
                    }
                    itemNode.depth = depth + 1;
                    itemNode.path = itemPath;
                    itemNode.structIndex = items[j].structIndex;
                    itemNode.baseOffset = items[j].offset;
                    itemNode.isExpandable = itemNode.childCount > 0;
                    itemNode.isExpanded = forceExpand || state.expandedPaths.count(itemPath) > 0;
                    state.flattenedTree.push_back(itemNode);
                    if (itemNode.isExpanded) {
                        buildTreeFromGff4Struct(state, items[j].structIndex, items[j].offset, depth + 2, itemPath, forceExpand, visited);
                    }
                }
            } else if (isList && !isStruct && !isRef) {
                auto [count, dataStart] = state.gff4->readPrimitiveListInfo(structIndex, field.label, baseOffset);
                uint32_t itemSize = GFFFile::primitiveTypeSize(field.typeId);
                if (count > 100000) count = 100000;
                for (uint32_t j = 0; j < count; ++j) {
                    std::string itemPath = path + "[" + std::to_string(j) + "]";
                    GffViewerState::TreeNode itemNode;
                    itemNode.numericLabel = j;
                    itemNode.label = "";
                    itemNode.isListItem = true;
                    itemNode.typeName = gff4PrimitiveName(field.typeId);
                    itemNode.depth = depth + 1;
                    itemNode.path = itemPath;
                    itemNode.isExpandable = false;
                    itemNode.childCount = 0;
                    GFFField fakeField = field;
                    fakeField.flags = 0;
                    fakeField.dataOffset = (dataStart + j * itemSize) - state.gff4->dataOffset();
                    itemNode.value = gff4ReadFieldValueStr(*state.gff4, fakeField, 0);
                    state.flattenedTree.push_back(itemNode);
                }
            } else if (isRef && !isStruct) {
                auto ref = state.gff4->readStructRef(structIndex, field.label, baseOffset);
                if (ref.structIndex < state.gff4->structs().size()) {
                    buildTreeFromGff4Struct(state, ref.structIndex, ref.offset, depth + 1, path, forceExpand, visited);
                }
            } else if (isStruct && isRef) {
                auto ref = state.gff4->readStructRef(structIndex, field.label, baseOffset);
                if (ref.structIndex < state.gff4->structs().size()) {
                    buildTreeFromGff4Struct(state, ref.structIndex, ref.offset, depth + 1, path, forceExpand, visited);
                }
            } else if (isStruct && !isRef) {
                uint32_t embStructOffset = baseOffset + field.dataOffset;
                uint32_t embStructIdx = field.typeId;
                if (embStructIdx < state.gff4->structs().size()) {
                    buildTreeFromGff4Struct(state, embStructIdx, embStructOffset, depth + 1, path, forceExpand, visited);
                }
            }
        }
    }
}

static std::vector<float> parseFloats(const std::string& str) {
    std::vector<float> result;
    std::istringstream iss(str);
    std::string token;
    while (std::getline(iss, token, ',')) {
        try { result.push_back(std::stof(token)); } catch (...) { result.push_back(0.0f); }
    }
    return result;
}

bool applyGffEdit(GffViewerState& state, const GffViewerState::TreeNode& node, const char* newValue, const char* newValue2) {
    if (state.loadedFormat == GffViewerState::Format::GFF4 && state.gff4) {
        const auto& structs = state.gff4->structs();
        if (node.structIndex >= structs.size()) return false;
        if (node.fieldIndex >= structs[node.structIndex].fields.size()) return false;
        const GFFField& field = structs[node.structIndex].fields[node.fieldIndex];
        if ((field.flags & (FLAG_LIST | FLAG_STRUCT)) != 0) return false;

        uint32_t dataPos = state.gff4->dataOffset() + node.baseOffset + field.dataOffset;
        bool isRef = (field.flags & FLAG_REFERENCE) != 0;
        if (isRef && field.typeId <= 17 && field.typeId != 14) {
            if (dataPos + 4 > state.gff4->rawData().size()) return false;
            uint32_t ptr = state.gff4->readUInt32At(dataPos);
            if (ptr == 0xFFFFFFFF) return false;
            if (field.typeId == 14)
                dataPos = state.gff4->dataOffset() + node.baseOffset + field.dataOffset;
            else
                dataPos = state.gff4->dataOffset() + ptr;
        }

        std::string val(newValue);
        switch (field.typeId) {
            case 0: { uint8_t v = static_cast<uint8_t>(std::stoul(val)); state.gff4->writeAt(dataPos, v); break; }
            case 1: { int8_t v = static_cast<int8_t>(std::stoi(val)); state.gff4->writeAt(dataPos, v); break; }
            case 2: { uint16_t v = static_cast<uint16_t>(std::stoul(val)); state.gff4->writeAt(dataPos, v); break; }
            case 3: { int16_t v = static_cast<int16_t>(std::stoi(val)); state.gff4->writeAt(dataPos, v); break; }
            case 4: { uint32_t v = static_cast<uint32_t>(std::stoul(val)); state.gff4->writeAt(dataPos, v); break; }
            case 5: { int32_t v = static_cast<int32_t>(std::stoi(val)); state.gff4->writeAt(dataPos, v); break; }
            case 6: { uint64_t v = std::stoull(val); state.gff4->writeAt(dataPos, v); break; }
            case 7: { int64_t v = std::stoll(val); state.gff4->writeAt(dataPos, v); break; }
            case 8: { float v = std::stof(val); state.gff4->writeAt(dataPos, v); break; }
            case 9: { double v = std::stod(val); state.gff4->writeAt(dataPos, v); break; }
            case 10: {
                auto f = parseFloats(val);
                while (f.size() < 3) f.push_back(0.0f);
                for (int i = 0; i < 3; i++) state.gff4->writeAt(dataPos + i * 4, f[i]);
                break;
            }
            case 11: {
                auto f = parseFloats(val);
                while (f.size() < 2) f.push_back(0.0f);
                for (int i = 0; i < 2; i++) state.gff4->writeAt(dataPos + i * 4, f[i]);
                break;
            }
            case 12: case 13: case 15: {
                auto f = parseFloats(val);
                while (f.size() < 4) f.push_back(0.0f);
                for (int i = 0; i < 4; i++) state.gff4->writeAt(dataPos + i * 4, f[i]);
                break;
            }
            case 14: {
                state.gff4->writeECString(dataPos, val);
                break;
            }
            case 16: {
                auto f = parseFloats(val);
                while (f.size() < 16) f.push_back(0.0f);
                for (int i = 0; i < 16; i++) state.gff4->writeAt(dataPos + i * 4, f[i]);
                break;
            }
            case 17: {
                uint32_t tlkId = static_cast<uint32_t>(std::stoul(val));
                state.gff4->writeAt(dataPos, tlkId);
                if (newValue2 && newValue2[0] != '\0') {
                    state.gff4->writeECString(dataPos + 4, std::string(newValue2));
                }
                break;
            }
            default: return false;
        }
        state.hasUnsavedChanges = true;
        return true;
    }
    else if (state.loadedFormat == GffViewerState::Format::GFF32 && state.gff32 && state.gff32->root()) {
        std::string path = node.path;
        std::vector<std::string> parts;
        std::istringstream iss(path);
        std::string part;
        while (std::getline(iss, part, '.')) parts.push_back(part);

        GFF32::Structure* current = state.gff32->root();
        for (size_t i = 0; i < parts.size() - 1; i++) {
            std::string& p = parts[i];
            size_t bk = p.find('[');
            if (bk != std::string::npos) {
                std::string fieldName = p.substr(0, bk);
                size_t idx = std::stoul(p.substr(bk + 1, p.size() - bk - 2));
                GFF32::Field* f = current->getField(fieldName);
                if (!f) return false;
                auto ptr = std::get<GFF32::ListPtr>(f->value);
                if (!ptr || idx >= ptr->size()) return false;
                current = &(*ptr)[idx];
            } else {
                GFF32::Field* f = current->getField(p);
                if (!f) return false;
                if (f->typeId == GFF32::TypeID::Structure) {
                    auto ptr = std::get<GFF32::StructurePtr>(f->value);
                    if (!ptr) return false;
                    current = ptr.get();
                }
            }
        }

        std::string lastPart = parts.back();
        size_t bk = lastPart.find('[');
        if (bk != std::string::npos) {
            std::string fieldName = lastPart.substr(0, bk);
            size_t idx = std::stoul(lastPart.substr(bk + 1, lastPart.size() - bk - 2));
            GFF32::Field* f = current->getField(fieldName);
            if (!f) return false;
            if (f->typeId == GFF32::TypeID::ExoLocString) {
                auto& loc = std::get<GFF32::ExoLocString>(f->value);
                if (idx >= loc.strings.size()) return false;
                loc.strings[idx].text = std::string(newValue);
                state.hasUnsavedChanges = true;
                return true;
            }
            return false;
        }

        GFF32::Field* f = current->getField(lastPart);
        if (!f) return false;

        std::string val(newValue);
        switch (f->typeId) {
            case GFF32::TypeID::BYTE: f->value = static_cast<uint8_t>(std::stoul(val)); break;
            case GFF32::TypeID::CHAR: f->value = static_cast<int8_t>(std::stoi(val)); break;
            case GFF32::TypeID::WORD: f->value = static_cast<uint16_t>(std::stoul(val)); break;
            case GFF32::TypeID::SHORT: f->value = static_cast<int16_t>(std::stoi(val)); break;
            case GFF32::TypeID::DWORD: f->value = static_cast<uint32_t>(std::stoul(val)); break;
            case GFF32::TypeID::INT: f->value = static_cast<int32_t>(std::stoi(val)); break;
            case GFF32::TypeID::DWORD64: f->value = static_cast<uint64_t>(std::stoull(val)); break;
            case GFF32::TypeID::INT64: f->value = static_cast<int64_t>(std::stoll(val)); break;
            case GFF32::TypeID::FLOAT: f->value = std::stof(val); break;
            case GFF32::TypeID::DOUBLE: f->value = std::stod(val); break;
            case GFF32::TypeID::ExoString: case GFF32::TypeID::ResRef: f->value = val; break;
            case GFF32::TypeID::ExoLocString: {
                auto& loc = std::get<GFF32::ExoLocString>(f->value);
                loc.stringref = static_cast<int32_t>(std::stoi(val));
                break;
            }
            default: return false;
        }
        state.hasUnsavedChanges = true;
        return true;
    }
    return false;
}

void drawGffViewerWindow(GffViewerState& state) {
    if (!state.showWindow) return;
    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
    std::string title = "GFF Viewer";
    if (!state.fileName.empty()) title += " - " + state.fileName;
    if (state.hasUnsavedChanges) title += " *";
    title += "###GffViewer";
    if (!ImGui::Begin(title.c_str(), &state.showWindow)) { ImGui::End(); return; }
    if (!state.isLoaded()) {
        ImGui::TextDisabled("No GFF file loaded");
        ImGui::End();
        return;
    }
    if (ImGui::Button(GFF4TLK::isLoaded() ? "TLK Loaded" : "Load TLK")) {
        IGFD::FileDialogConfig config;
        config.path = state.tlkPath.empty() ? "." : state.tlkPath;
        ImGuiFileDialog::Instance()->OpenDialog("LoadTLK", "Select TLK File", ".tlk", config);
    }
    if (GFF4TLK::isLoaded()) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%zu strings)", GFF4TLK::count());
        ImGui::SameLine();
        if (ImGui::SmallButton("Unload")) {
            GFF4TLK::clear();
            state.tlkStatus.clear();

            rebuildGffTree(state);
            requestCacheBuild(state);
        }
    }
    if (!state.tlkStatus.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "%s", state.tlkStatus.c_str());
    }
    if (ImGuiFileDialog::Instance()->Display("LoadTLK", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
            state.tlkPath = path.substr(0, path.find_last_of("/\\") + 1);
            if (GFF4TLK::loadFromFile(path)) {
                state.tlkStatus = "Loaded " + std::to_string(GFF4TLK::count()) + " strings";

                rebuildGffTree(state);
            requestCacheBuild(state);
            } else {
                state.tlkStatus = "Failed to load TLK";
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    bool isGff4 = state.loadedFormat == GffViewerState::Format::GFF4;
    const char* filterOptions = isGff4 ? "All\0Index\0Label\0Type\0Value\0" : "All\0Label\0Type\0Value\0";
    ImGui::SetNextItemWidth(80.0f);
    ImGui::Combo("##FilterCol", &state.filterColumn, filterOptions);
    ImGui::SameLine();

    std::string filter = state.searchFilter;
    bool filtering = !filter.empty() && state.cacheReady;
    float filterWidth = filtering ? ImGui::GetContentRegionAvail().x - 100.0f : -1.0f;
    ImGui::SetNextItemWidth(filterWidth);
    ImGui::InputTextWithHint("##Filter", "Filter...", state.searchFilter, sizeof(state.searchFilter));

    filter = state.searchFilter;
    filtering = !filter.empty() && state.cacheReady;
    if (state.cacheReady && (filter != state.lastFilterText || state.filterColumn != state.lastFilterColumn)) {
        state.lastFilterText = filter;
        state.lastFilterColumn = state.filterColumn;
        refilterTree(state);
    }
    if (filtering) {
        ImGui::SameLine();
        ImGui::TextDisabled("%d hits", (int)state.filteredIndices.size());
    }

    bool hasEditableSelection = false;
    const GffViewerState::TreeNode* selectedNode = nullptr;
    if (state.selectedNodeIndex >= 0 && state.selectedNodeIndex < (int)state.fullTree.size()) {
        selectedNode = &state.fullTree[state.selectedNodeIndex];
        hasEditableSelection = !selectedNode->isExpandable && !selectedNode->isListItem;
        if (hasEditableSelection && selectedNode) {
            if (selectedNode->typeName == "Root" || selectedNode->typeName == "List" ||
                selectedNode->typeName.find("Structure") != std::string::npos)
                hasEditableSelection = false;
        }
    }
    float editHeight = hasEditableSelection ? 100.0f : 0.0f;
    if (hasEditableSelection && selectedNode && selectedNode->typeName.find("TlkString") != std::string::npos)
        editHeight = 130.0f;
    float bottomBarHeight = 32.0f;

    ImGui::BeginChild("TreeView", ImVec2(0, -(editHeight + bottomBarHeight)), true);
    int numCols = isGff4 ? 4 : 3;

    ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg;

    if (ImGui::BeginTable("##GffTree", numCols, tableFlags)) {
        if (isGff4) {
            ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        } else {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        }
        ImGui::TableHeadersRow();

        if (filtering) {
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(state.filteredIndices.size()));
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    const auto& node = state.fullTree[state.filteredIndices[row]];
                    size_t i = static_cast<size_t>(row);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    if (isGff4) {
                        ImGui::Text("%u", node.numericLabel);
                        ImGui::TableNextColumn();
                    }
                    ImGui::Dummy(ImVec2(20, 0));
                    ImGui::SameLine();
                    char labelBuf[512];
                    snprintf(labelBuf, sizeof(labelBuf), "%s##f%zu", node.label.c_str(), i);
                    bool selected = (state.filteredIndices[row] == state.selectedNodeIndex);
                    if (ImGui::Selectable(labelBuf, selected, ImGuiSelectableFlags_SpanAllColumns))
                        state.selectedNodeIndex = state.filteredIndices[row];
                    ImGui::TableNextColumn();
                    ImVec4 typeColor(0.5f, 0.5f, 0.5f, 1.0f);
                    if (node.typeName.find("[") != std::string::npos || node.typeName == "List")
                        typeColor = ImVec4(0.9f, 0.7f, 0.3f, 1.0f);
                    else if (node.typeName.find("Structure") != std::string::npos || node.typeName == "Root")
                        typeColor = ImVec4(0.5f, 0.7f, 0.5f, 1.0f);
                    else if (isGff4 && node.isExpandable)
                        typeColor = ImVec4(0.5f, 0.7f, 0.5f, 1.0f);
                    ImGui::TextColored(typeColor, "%s", node.typeName.c_str());
                    ImGui::TableNextColumn();
                    ImVec4 valColor(1, 1, 1, 1);
                    bool isListNode = node.typeName.find("[") != std::string::npos || node.typeName == "List";
                    bool isStructNode = node.isExpandable && !isListNode;
                    if (isListNode)
                        valColor = ImVec4(1.0f, 0.8f, 0.4f, 1.0f);
                    else if (isStructNode)
                        valColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
                    else if (node.typeName.find("String") != std::string::npos ||
                             node.typeName == "ResRef" || node.typeName == "ExoString" ||
                             node.typeName.find("ECString") != std::string::npos)
                        valColor = ImVec4(1.0f, 0.6f, 0.6f, 1.0f);
                    else if (node.typeName.find("INT") != std::string::npos ||
                             node.typeName.find("DWORD") != std::string::npos ||
                             node.typeName.find("WORD") != std::string::npos ||
                             node.typeName.find("BYTE") != std::string::npos ||
                             node.typeName.find("UINT") != std::string::npos)
                        valColor = ImVec4(0.6f, 0.8f, 1.0f, 1.0f);
                    else if (node.typeName.find("FLOAT") != std::string::npos ||
                             node.typeName.find("DOUBLE") != std::string::npos ||
                             node.typeName.find("Vector") != std::string::npos ||
                             node.typeName.find("Quaternion") != std::string::npos ||
                             node.typeName.find("Color") != std::string::npos)
                        valColor = ImVec4(0.8f, 1.0f, 0.6f, 1.0f);
                    else if (node.typeName.find("TlkString") != std::string::npos)
                        valColor = ImVec4(0.8f, 0.6f, 1.0f, 1.0f);
                    ImGui::TextColored(valColor, "%s", node.value.c_str());
                }
            }
        } else {
            for (size_t i = 0; i < state.visibleIndices.size(); ++i) {
                const auto& node = state.fullTree[state.visibleIndices[i]];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                float indent;
                if (isGff4) {
                    ImGui::Text("%u", node.numericLabel);
                    ImGui::TableNextColumn();
                    indent = node.depth * 16.0f;
                } else {
                    indent = node.depth * 16.0f;
                }

                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
                if (node.isExpandable) {
                    bool isExp = state.expandedPaths.count(node.path) > 0;
                    ImGui::PushID(static_cast<int>(i));
                    if (ImGui::SmallButton(isExp ? "-" : "+")) {
                        if (isExp) state.expandedPaths.erase(node.path);
                        else state.expandedPaths.insert(node.path);
                        rebuildGffTree(state);
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopID();
                    ImGui::SameLine();
                } else {
                    ImGui::Dummy(ImVec2(20, 0));
                    ImGui::SameLine();
                }

                char labelBuf[512];
                snprintf(labelBuf, sizeof(labelBuf), "%s##%zu", node.label.c_str(), i);
                bool selected = (static_cast<int>(state.visibleIndices[i]) == state.selectedNodeIndex);
                if (ImGui::Selectable(labelBuf, selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                    state.selectedNodeIndex = static_cast<int>(state.visibleIndices[i]);
                    if (ImGui::IsMouseDoubleClicked(0) && node.isExpandable) {
                        bool isExp = state.expandedPaths.count(node.path) > 0;
                        if (isExp) state.expandedPaths.erase(node.path);
                        else state.expandedPaths.insert(node.path);
                        rebuildGffTree(state);
                        ImGui::EndTable();
                        ImGui::EndChild();
                        ImGui::End();
                        return;
                    }
                }

                ImGui::TableNextColumn();
                ImVec4 typeColor(0.5f, 0.5f, 0.5f, 1.0f);
                if (node.typeName.find("[") != std::string::npos || node.typeName == "List")
                    typeColor = ImVec4(0.9f, 0.7f, 0.3f, 1.0f);
                else if (node.typeName.find("Structure") != std::string::npos || node.typeName == "Root")
                    typeColor = ImVec4(0.5f, 0.7f, 0.5f, 1.0f);
                else if (isGff4 && node.isExpandable)
                    typeColor = ImVec4(0.5f, 0.7f, 0.5f, 1.0f);
                ImGui::TextColored(typeColor, "%s", node.typeName.c_str());

                ImGui::TableNextColumn();
                ImVec4 valColor(1, 1, 1, 1);
                bool isListNode = node.typeName.find("[") != std::string::npos || node.typeName == "List";
                bool isStructNode = node.isExpandable && !isListNode;
                if (isListNode)
                    valColor = ImVec4(1.0f, 0.8f, 0.4f, 1.0f);
                else if (isStructNode)
                    valColor = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
                else if (node.typeName.find("String") != std::string::npos ||
                         node.typeName == "ResRef" || node.typeName == "ExoString" ||
                         node.typeName.find("ECString") != std::string::npos)
                    valColor = ImVec4(1.0f, 0.6f, 0.6f, 1.0f);
                else if (node.typeName.find("INT") != std::string::npos ||
                         node.typeName.find("DWORD") != std::string::npos ||
                         node.typeName.find("WORD") != std::string::npos ||
                         node.typeName.find("BYTE") != std::string::npos ||
                         node.typeName.find("UINT") != std::string::npos)
                    valColor = ImVec4(0.6f, 0.8f, 1.0f, 1.0f);
                else if (node.typeName.find("FLOAT") != std::string::npos ||
                         node.typeName.find("DOUBLE") != std::string::npos ||
                         node.typeName.find("Vector") != std::string::npos ||
                         node.typeName.find("Quaternion") != std::string::npos ||
                         node.typeName.find("Color") != std::string::npos)
                    valColor = ImVec4(0.8f, 1.0f, 0.6f, 1.0f);
                else if (node.typeName.find("TlkString") != std::string::npos)
                    valColor = ImVec4(0.8f, 0.6f, 1.0f, 1.0f);
                ImGui::TextColored(valColor, "%s", node.value.c_str());
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    selectedNode = nullptr;
    hasEditableSelection = false;
    if (state.selectedNodeIndex >= 0 && state.selectedNodeIndex < (int)state.fullTree.size()) {
        selectedNode = &state.fullTree[state.selectedNodeIndex];
        hasEditableSelection = !selectedNode->isExpandable && !selectedNode->isListItem;
        if (hasEditableSelection && selectedNode) {
            if (selectedNode->typeName == "Root" || selectedNode->typeName == "List" ||
                selectedNode->typeName.find("Structure") != std::string::npos)
                hasEditableSelection = false;
        }
    }

    if (hasEditableSelection && selectedNode) {
        if (state.editingNodeIndex != state.selectedNodeIndex) {
            if (state.editingNodeIndex >= 0 && !state.lastEditPath.empty()) {
                const GffViewerState::TreeNode* oldNode = nullptr;
                for (auto& n : state.fullTree) { if (n.path == state.lastEditPath) { oldNode = &n; break; } }
                if (!oldNode) {
                    for (auto& n : state.fullTree) { if (n.path == state.lastEditPath) { oldNode = &n; break; } }
                }
                if (oldNode) {
                    bool wasTlk = oldNode->typeName.find("TlkString") != std::string::npos;
                    try {
                        if (applyGffEdit(state, *oldNode, state.editBuffer, wasTlk ? state.editBuffer2 : nullptr)) {
                            for (auto& n : state.fullTree) {
                                if (n.path == state.lastEditPath) {
                                    std::string newVal = state.editBuffer;
                                    if (wasTlk && state.editBuffer2[0])
                                        newVal += ", " + std::string(state.editBuffer2);
                                    n.value = newVal;
                                    n.buildSearchKeys();
                                    break;
                                }
                            }
                            rebuildGffTree(state);
                            if (filtering) refilterTree(state);
                        }
                    } catch (...) {}
                }
            }
            state.editingNodeIndex = state.selectedNodeIndex;
            state.lastEditPath = selectedNode->path;
            std::string val = selectedNode->value;
            if (selectedNode->typeName.find("TlkString") != std::string::npos) {
                size_t comma = val.find(',');
                if (comma != std::string::npos) {
                    strncpy(state.editBuffer, val.substr(0, comma).c_str(), sizeof(state.editBuffer) - 1);
                    std::string rest = val.substr(comma + 1);
                    while (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
                    strncpy(state.editBuffer2, rest.c_str(), sizeof(state.editBuffer2) - 1);
                } else {
                    strncpy(state.editBuffer, val.c_str(), sizeof(state.editBuffer) - 1);
                    state.editBuffer2[0] = '\0';
                }
            } else {
                strncpy(state.editBuffer, val.c_str(), sizeof(state.editBuffer) - 1);
                state.editBuffer2[0] = '\0';
            }
        }
        ImGui::Separator();
        ImGui::Text("Edit: %s (%s)", selectedNode->label.c_str(), selectedNode->typeName.c_str());
        bool isTlk = selectedNode->typeName.find("TlkString") != std::string::npos;
        if (isTlk) {
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputText("TLK ID", state.editBuffer, sizeof(state.editBuffer));
            ImGui::InputTextMultiline("##EditText", state.editBuffer2, sizeof(state.editBuffer2),
                ImVec2(-1, ImGui::GetContentRegionAvail().y - 30));
        } else {
            ImGui::InputTextMultiline("##EditVal", state.editBuffer, sizeof(state.editBuffer),
                ImVec2(-1, ImGui::GetContentRegionAvail().y - 30));
        }
    }

    bool didPushSaveColor = state.hasUnsavedChanges;
    if (didPushSaveColor) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    }
    float btnWidth = 80.0f;
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - btnWidth - 10.0f);
    std::string saveBtnLabel = state.hasUnsavedChanges ? "Save *" : "Save";
    if (ImGui::Button(saveBtnLabel.c_str(), ImVec2(btnWidth, 0))) {
        if (state.editingNodeIndex >= 0 && !state.lastEditPath.empty()) {
            const GffViewerState::TreeNode* curNode = nullptr;
            for (auto& n : state.fullTree) { if (n.path == state.lastEditPath) { curNode = &n; break; } }
            if (curNode) {
                bool wasTlk = curNode->typeName.find("TlkString") != std::string::npos;
                try { applyGffEdit(state, *curNode, state.editBuffer, wasTlk ? state.editBuffer2 : nullptr); } catch (...) {}
            }
        }
        if (state.hasUnsavedChanges) {
            IGFD::FileDialogConfig config;
            std::string defaultPath = ".";
            if (!state.overridePath.empty()) {
                std::filesystem::create_directories(state.overridePath);
                defaultPath = state.overridePath;
            } else if (!state.gamePath.empty()) {
                std::filesystem::path overridePath = std::filesystem::path(state.gamePath) / "packages" / "core" / "override";
                std::filesystem::create_directories(overridePath);
                defaultPath = overridePath.string();
            }
            config.path = defaultPath;
            config.fileName = state.fileName;
            ImGuiFileDialog::Instance()->OpenDialog("SaveGFF", "Save GFF File", ".*", config);
        }
    }
    if (didPushSaveColor)
        ImGui::PopStyleColor(2);

    if (ImGuiFileDialog::Instance()->Display("SaveGFF", ImGuiWindowFlags_NoCollapse, ImVec2(500, 400))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string savePath = ImGuiFileDialog::Instance()->GetFilePathName();
            std::vector<uint8_t> gffBytes;
            if (state.loadedFormat == GffViewerState::Format::GFF4 && state.gff4) {
                gffBytes = state.gff4->rawData();
            } else if (state.loadedFormat == GffViewerState::Format::GFF32 && state.gff32) {
                gffBytes = state.gff32->save();
            }
            if (!gffBytes.empty()) {
                std::ofstream out(savePath, std::ios::binary | std::ios::trunc);
                if (out) {
                    out.write(reinterpret_cast<const char*>(gffBytes.data()), gffBytes.size());
                    if (out.good()) {
                        state.hasUnsavedChanges = false;
                        state.statusMessage = "Saved: " + savePath;
                    } else {
                        state.statusMessage = "Write error: " + savePath;
                    }
                } else {
                    state.statusMessage = "Cannot open: " + savePath;
                }
            } else {
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::End();
}

void drawGffLoadingOverlay(GffViewerState& state) {
    if (!state.bgLoading.load()) return;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.3f));
    ImGui::Begin("##GffInputBlock", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::End();
    ImGui::PopStyleColor();

    ImVec2 center(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::Begin("##GffLoading", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    const char* spinChars = "|/-\\";
    int idx = (int)(ImGui::GetTime() * 8.0f) % 4;
    ImGui::Text("  %c  Loading %s...", spinChars[idx], state.fileName.c_str());

    ImGui::End();
}