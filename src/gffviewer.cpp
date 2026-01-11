#include "GffViewer.h"
#include "imgui.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
static bool containsCI(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    std::string h = haystack, n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    return h.find(n) != std::string::npos;
}
bool loadGffData(GffViewerState& state, const std::vector<uint8_t>& data,
                 const std::string& fileName, const std::string& erfSource) {
    state.clear();
    state.fileName = fileName;
    state.erfSource = erfSource;
    if (data.empty()) {
        state.statusMessage = "Empty data";
        return false;
    }
    if (GFF32::GFF32File::isGFF32(data)) {
        state.gff32 = std::make_unique<GFF32::GFF32File>();
        if (state.gff32->load(data)) {
            state.loadedFormat = GffViewerState::Format::GFF32;
            state.statusMessage = "Loaded GFF 3.2: " + state.gff32->fileType();
            rebuildGffTree(state);
            return true;
        }
        state.gff32.reset();
    }
    if (data.size() >= 4 && data[0] == 'G' && data[1] == 'F' && data[2] == 'F' && data[3] == ' ') {
        state.gff4 = std::make_unique<GFFFile>();
        if (state.gff4->load(data)) {
            state.loadedFormat = GffViewerState::Format::GFF4;
            state.statusMessage = "Loaded GFF 4";
            rebuildGffTree(state);
            return true;
        }
        state.gff4.reset();
    }
    state.statusMessage = "Failed to parse as GFF";
    return false;
}
static void buildTreeFromGff32Struct(GffViewerState& state, const GFF32::Structure& st,
                                      const std::string& basePath, int depth);
static void buildTreeFromGff4Struct(GffViewerState& state, uint32_t structIndex,
                                     uint32_t baseOffset, int depth, const std::string& basePath);
void rebuildGffTree(GffViewerState& state) {
    state.flattenedTree.clear();
    if (state.loadedFormat == GffViewerState::Format::GFF32 && state.gff32 && state.gff32->root()) {
        GffViewerState::TreeNode rootNode;
        rootNode.label = state.gff32->fileType() + " " + state.gff32->fileVersion();
        rootNode.typeName = "Root";
        rootNode.value = "";
        rootNode.depth = 0;
        rootNode.isExpandable = true;
        rootNode.isExpanded = state.expandedPaths.count("") > 0 || state.expandedPaths.empty();
        rootNode.childCount = state.gff32->root()->fieldCount();
        rootNode.path = "";
        state.flattenedTree.push_back(rootNode);
        if (state.expandedPaths.empty()) {
            state.expandedPaths.insert("");
        }
        if (rootNode.isExpanded) {
            buildTreeFromGff32Struct(state, *state.gff32->root(), "", 1);
        }
    }
    else if (state.loadedFormat == GffViewerState::Format::GFF4 && state.gff4 && state.gff4->isLoaded()) {
        GffViewerState::TreeNode rootNode;
        if (state.gff4->isMMH()) {
            rootNode.label = "MMH (Model Hierarchy)";
        } else if (state.gff4->isMSH()) {
            rootNode.label = "MSH (Mesh Data)";
        } else {
            rootNode.label = "GFF4";
        }
        rootNode.typeName = "Root";
        rootNode.value = std::to_string(state.gff4->structs().size()) + " structs";
        rootNode.depth = 0;
        rootNode.isExpandable = !state.gff4->structs().empty();
        rootNode.isExpanded = state.expandedPaths.count("") > 0 || state.expandedPaths.empty();
        rootNode.childCount = state.gff4->structs().empty() ? 0 : state.gff4->structs()[0].fields.size();
        rootNode.path = "";
        rootNode.structIndex = 0;
        rootNode.baseOffset = 0;
        state.flattenedTree.push_back(rootNode);
        if (state.expandedPaths.empty()) {
            state.expandedPaths.insert("");
        }
        if (rootNode.isExpanded && !state.gff4->structs().empty()) {
            buildTreeFromGff4Struct(state, 0, 0, 1, "");
        }
    }
}
static void buildTreeFromGff32Struct(GffViewerState& state, const GFF32::Structure& st,
                                      const std::string& basePath, int depth) {
    for (const auto& label : st.fieldOrder) {
        auto it = st.fields.find(label);
        if (it == st.fields.end()) continue;
        const GFF32::Field& field = it->second;
        std::string path = basePath.empty() ? label : basePath + "." + label;
        GffViewerState::TreeNode node;
        node.label = label;
        node.typeName = field.getTypeName();
        node.depth = depth;
        node.path = path;
        bool isStruct = field.typeId == GFF32::TypeID::Structure;
        bool isList = field.typeId == GFF32::TypeID::List;
        if (isStruct) {
            auto ptr = std::get<GFF32::StructurePtr>(field.value);
            node.isExpandable = ptr && ptr->fieldCount() > 0;
            node.childCount = ptr ? ptr->fieldCount() : 0;
            node.value = ptr ? ("Struct:" + std::to_string(ptr->structId)) : "(null)";
        } else if (isList) {
            auto ptr = std::get<GFF32::ListPtr>(field.value);
            node.isExpandable = ptr && !ptr->empty();
            node.childCount = ptr ? ptr->size() : 0;
            node.value = ptr ? ("List[" + std::to_string(ptr->size()) + "]") : "(empty)";
        } else {
            node.isExpandable = false;
            node.childCount = 0;
            node.value = field.getDisplayValue();
        }
        node.isExpanded = state.expandedPaths.count(path) > 0;
        state.flattenedTree.push_back(node);
        if (node.isExpanded) {
            if (isStruct) {
                auto ptr = std::get<GFF32::StructurePtr>(field.value);
                if (ptr) {
                    buildTreeFromGff32Struct(state, *ptr, path, depth + 1);
                }
            } else if (isList) {
                auto ptr = std::get<GFF32::ListPtr>(field.value);
                if (ptr) {
                    for (size_t i = 0; i < ptr->size(); ++i) {
                        std::string itemPath = path + "[" + std::to_string(i) + "]";
                        const GFF32::Structure& item = (*ptr)[i];
                        GffViewerState::TreeNode itemNode;
                        itemNode.label = "[" + std::to_string(i) + "]";
                        itemNode.typeName = "Struct:" + std::to_string(item.structId);
                        itemNode.value = std::to_string(item.fieldCount()) + " fields";
                        itemNode.depth = depth + 1;
                        itemNode.path = itemPath;
                        itemNode.isExpandable = item.fieldCount() > 0;
                        itemNode.childCount = item.fieldCount();
                        itemNode.isExpanded = state.expandedPaths.count(itemPath) > 0;
                        state.flattenedTree.push_back(itemNode);
                        if (itemNode.isExpanded) {
                            buildTreeFromGff32Struct(state, item, itemPath, depth + 2);
                        }
                    }
                }
            }
        }
    }
}
static std::string gff4TypeName(uint16_t typeId, uint16_t flags) {
    bool isList = (flags & FLAG_LIST) != 0;
    bool isStruct = (flags & FLAG_STRUCT) != 0;
    bool isRef = (flags & FLAG_REFERENCE) != 0;
    std::string name;
    if (isStruct) {
        name = "Struct";
    } else {
        switch (typeId) {
            case 0: name = "UINT8"; break;
            case 1: name = "INT8"; break;
            case 2: name = "UINT16"; break;
            case 3: name = "INT16"; break;
            case 4: name = "UINT32"; break;
            case 5: name = "INT32"; break;
            case 6: name = "UINT64"; break;
            case 7: name = "INT64"; break;
            case 8: name = "FLOAT32"; break;
            case 9: name = "FLOAT64"; break;
            case 10: name = "Vector3"; break;
            case 11: name = "Vector4"; break;
            case 12: name = "Quaternion"; break;
            case 13: name = "String"; break;
            case 14: name = "String"; break;
            case 15: name = "Color4"; break;
            case 16: name = "Matrix4x4"; break;
            case 17: name = "TlkString"; break;
            default: name = "Type" + std::to_string(typeId); break;
        }
    }
    if (isList) name = "List<" + name + ">";
    if (isRef) name += "&";
    return name;
}
static void buildTreeFromGff4Struct(GffViewerState& state, uint32_t structIndex,
                                     uint32_t baseOffset, int depth, const std::string& basePath) {
    if (!state.gff4 || structIndex >= state.gff4->structs().size()) return;
    const GFFStruct& st = state.gff4->structs()[structIndex];
    for (size_t i = 0; i < st.fields.size(); ++i) {
        const GFFField& field = st.fields[i];
        std::string label = std::to_string(field.label);  
        std::string path = basePath.empty() ? label : basePath + "." + label;
        GffViewerState::TreeNode node;
        node.label = label;
        node.typeName = gff4TypeName(field.typeId, field.flags);
        node.depth = depth;
        node.path = path;
        node.structIndex = structIndex;
        node.fieldIndex = static_cast<uint32_t>(i);
        node.baseOffset = baseOffset;
        bool isList = (field.flags & FLAG_LIST) != 0;
        bool isStruct = (field.flags & FLAG_STRUCT) != 0;
        bool isRef = (field.flags & FLAG_REFERENCE) != 0;
        if (isStruct || isList) {
            node.isExpandable = true;
            if (isList) {
                auto items = state.gff4->readStructList(structIndex, field.label, baseOffset);
                node.childCount = items.size();
                node.value = "List[" + std::to_string(items.size()) + "]";
            } else if (isRef) {
                auto ref = state.gff4->readStructRef(structIndex, field.label, baseOffset);
                node.childCount = 1;
                node.value = "Ref -> Struct " + std::to_string(ref.structIndex);
            } else {
                node.childCount = 1;
                node.value = "Embedded";
            }
        } else {
            node.isExpandable = false;
            node.childCount = 0;
            switch (field.typeId) {
                case 4: 
                    node.value = std::to_string(state.gff4->readUInt32ByLabel(structIndex, field.label, baseOffset));
                    break;
                case 5: 
                    node.value = std::to_string(state.gff4->readInt32ByLabel(structIndex, field.label, baseOffset));
                    break;
                case 8: 
                    node.value = std::to_string(state.gff4->readFloatByLabel(structIndex, field.label, baseOffset));
                    break;
                case 13: 
                case 14: 
                    node.value = "\"" + state.gff4->readStringByLabel(structIndex, field.label, baseOffset) + "\"";
                    break;
                default:
                    node.value = "(type " + std::to_string(field.typeId) + ")";
                    break;
            }
        }
        node.isExpanded = state.expandedPaths.count(path) > 0;
        state.flattenedTree.push_back(node);
        if (node.isExpanded && node.isExpandable) {
            if (isList) {
                auto items = state.gff4->readStructList(structIndex, field.label, baseOffset);
                for (size_t j = 0; j < items.size(); ++j) {
                    std::string itemPath = path + "[" + std::to_string(j) + "]";
                    GffViewerState::TreeNode itemNode;
                    itemNode.label = "[" + std::to_string(j) + "]";
                    itemNode.typeName = "Struct";
                    itemNode.depth = depth + 1;
                    itemNode.path = itemPath;
                    itemNode.structIndex = items[j].structIndex;
                    itemNode.baseOffset = items[j].offset;
                    itemNode.isExpandable = true;
                    itemNode.childCount = state.gff4->structs()[items[j].structIndex].fields.size();
                    itemNode.value = std::string(state.gff4->structs()[items[j].structIndex].structType);
                    itemNode.isExpanded = state.expandedPaths.count(itemPath) > 0;
                    state.flattenedTree.push_back(itemNode);
                    if (itemNode.isExpanded) {
                        buildTreeFromGff4Struct(state, items[j].structIndex, items[j].offset, depth + 2, itemPath);
                    }
                }
            } else if (isRef) {
                auto ref = state.gff4->readStructRef(structIndex, field.label, baseOffset);
                if (ref.structIndex < state.gff4->structs().size()) {
                    buildTreeFromGff4Struct(state, ref.structIndex, ref.offset, depth + 1, path);
                }
            }
        }
    }
}
void drawGffViewerWindow(GffViewerState& state) {
    if (!state.showWindow) return;
    ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
    std::string title = "GFF Viewer";
    if (!state.fileName.empty()) {
        title += " - " + state.fileName;
    }
    title += "###GffViewer";
    if (!ImGui::Begin(title.c_str(), &state.showWindow)) {
        ImGui::End();
        return;
    }
    if (state.isLoaded()) {
        ImGui::BeginChild("TreeView", ImVec2(0, 0), true);
        std::string filter = state.searchFilter;
        for (size_t i = 0; i < state.flattenedTree.size(); ++i) {
            const auto& node = state.flattenedTree[i];
            if (!filter.empty()) {
                bool matches = containsCI(node.label, filter);
                if (state.searchInValues && !matches) {
                    matches = containsCI(node.value, filter);
                }
                if (!matches) continue;
            }
            float indent = node.depth * 20.0f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
            if (node.isExpandable) {
                ImGui::PushID(static_cast<int>(i));
                bool wasExpanded = node.isExpanded;
                if (ImGui::SmallButton(wasExpanded ? "-" : "+")) {
                    if (wasExpanded) {
                        state.expandedPaths.erase(node.path);
                    } else {
                        state.expandedPaths.insert(node.path);
                    }
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
            ImVec4 color = ImVec4(1, 1, 1, 1);
            if (node.typeName.find("List") != std::string::npos) {
                color = ImVec4(1.0f, 0.8f, 0.4f, 1.0f);
            } else if (node.typeName.find("Struct") != std::string::npos) {
                color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f);
            } else if (node.typeName.find("String") != std::string::npos ||
                       node.typeName == "ResRef" || node.typeName == "ExoString") {
                color = ImVec4(1.0f, 0.6f, 0.6f, 1.0f);
            } else if (node.typeName.find("INT") != std::string::npos ||
                       node.typeName.find("DWORD") != std::string::npos ||
                       node.typeName.find("WORD") != std::string::npos ||
                       node.typeName.find("BYTE") != std::string::npos) {
                color = ImVec4(0.6f, 0.8f, 1.0f, 1.0f);
            } else if (node.typeName.find("FLOAT") != std::string::npos ||
                       node.typeName.find("DOUBLE") != std::string::npos) {
                color = ImVec4(0.8f, 1.0f, 0.6f, 1.0f);
            }
            char label[512];
            snprintf(label, sizeof(label), "%s##%zu", node.label.c_str(), i);
            bool selected = (static_cast<int>(i) == state.selectedNodeIndex);
            if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                state.selectedNodeIndex = static_cast<int>(i);
                if (ImGui::IsMouseDoubleClicked(0) && node.isExpandable) {
                    if (node.isExpanded) {
                        state.expandedPaths.erase(node.path);
                    } else {
                        state.expandedPaths.insert(node.path);
                    }
                    rebuildGffTree(state);
                    ImGui::EndChild();
                    ImGui::End();
                    return;
                }
            }
            ImGui::SameLine(300);
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", node.typeName.c_str());
            ImGui::SameLine(420);
            ImGui::TextColored(color, "%s", node.value.c_str());
        }
        ImGui::EndChild();
    } else {
        ImGui::TextDisabled("No GFF file loaded");
    }
    ImGui::End();
}