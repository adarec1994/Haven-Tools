#include "Gff.h"
#include <fstream>
#include <cstring>
#include <cstdio>
#include <map>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <vector>
#include <filesystem>

static std::map<uint32_t, std::string> s_knownLabels;
static bool s_labelsLoaded = false;

static uint32_t hashString(const std::string& str) {
    uint32_t hash = 2166136261u;
    for (char c : str) {
        hash ^= static_cast<uint8_t>(std::tolower(c));
        hash *= 16777619u;
    }
    return hash;
}

static void loadHardcodedLabels() {
    const char* labels[] = {
        "Name", "Tag", "ResRef", "TemplateResRef", "Active", "ID", "Count", "Type",
        "Position_X", "Position_Y", "Position_Z",
        "Orientation_X", "Orientation_Y", "Orientation_Z", "Orientation_W",
        "Bearings_X", "Bearings_Y", "Bearings_Z",
        "Strength", "Dexterity", "Willpower", "Magic", "Cunning", "Constitution",
        "Health", "Mana", "Stamina", "Mana_Stamina",
        "Attack", "Defense", "Armor", "DamageScale", "SpellPower",
        "Level", "Experience", "Gold", "Silver", "Copper",
        "Regeneration_Health", "Regeneration_Stamina", "Regeneration_Mana",
        "Damage_Resistance_Fire", "Damage_Resistance_Cold",
        "Damage_Resistance_Electricity", "Damage_Resistance_Nature",
        "Damage_Resistance_Spirit", "Damage_Resistance_Physical",
        "Agent_ID", "Appearance_Type", "Gender", "Race", "Background",
        "Portrait", "Head_Morph", "Conversation", "Script",
        "Inventory", "ItemList", "Equip_ItemList",
        "Creature_Stats", "Creature_Type", "AI_BEHAVIOR",
        "Party_Rank", "Current_Strategy", "Approvel",
        "Area_ID", "Area_Name", "Objects", "Creatures", "Placeables", "Triggers",
        "Waypoints", "Stores", "Sounds", "Lights", "Cameras",
        "Variables", "Map_ID", "World_Map", "Note", "Trap_Data",
        "StackSize", "Cost", "BaseCost", "Plot", "Stolen", "Droppable",
        "Item_Material", "Item_Type", "Item_Icon",
        "GFF_ROOT", "SAVEGAME_PLAYERCHAR", "SAVEGAME_PLAYERCHAR_CHAR",
        "SAVEGAME_PARTYLIST", "SAVEGAME_CAMPAIGN_ID", "SAVEGAME_AREA_LIST",
        "SAVEGAME_WORLD_TIME", "SAVEGAME_GAMEMODE", "SAVEGAME_APPEARANCE",
        "ModelName", "TintFileName", "VFXName", "PhysicsName",
        "Body_Tint", "Face_Tint", "Hair_Tint", "Eyes_Tint", "Skin_Tint"
    };

    for (const char* label : labels) {
        std::string s(label);
        s_knownLabels[hashString(s)] = s;
    }
}

static bool tryLoadFile(const std::string& filename) {
    std::vector<std::string> searchPaths = {
        "src/dependencies/hashes/",
        "../src/dependencies/hashes/",
        "../../src/dependencies/hashes/",
        "../../../src/dependencies/hashes/",
        "dependencies/hashes/",
        "hashes/",
        "../hashes/",
        "./"
    };

    for (const auto& prefix : searchPaths) {
        std::string fullPath = prefix + filename;
        std::ifstream file(fullPath);

        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                if (line.empty()) continue;
                if (line.back() == '\r') line.pop_back();

                std::string key;
                size_t commaPos = line.find(',');
                if (commaPos != std::string::npos) {
                    key = line.substr(0, commaPos);
                } else {
                    std::stringstream ss(line);
                    ss >> key;
                }

                if (!key.empty()) {
                    uint32_t hash = hashString(key);
                    if (s_knownLabels.find(hash) == s_knownLabels.end()) {
                        s_knownLabels[hash] = key;
                    }
                }
            }
            return true;
        }
    }
    return false;
}

void GFFFile::initLabelCache() {
    if (s_labelsLoaded) return;
    s_labelsLoaded = true;

    loadHardcodedLabels();

    tryLoadFile("StatPropertyNames.txt");
    tryLoadFile("StatPropertyNames2.txt");
    tryLoadFile("ResRefNames.txt");
    tryLoadFile("ResRefNames2.txt");

    if (s_knownLabels.find(hashString("Name")) == s_knownLabels.end()) s_knownLabels[hashString("Name")] = "Name";
    if (s_knownLabels.find(hashString("Tag")) == s_knownLabels.end()) s_knownLabels[hashString("Tag")] = "Tag";
    if (s_knownLabels.find(hashString("ResRef")) == s_knownLabels.end()) s_knownLabels[hashString("ResRef")] = "ResRef";
}

std::string GFFFile::getLabel(uint32_t hash) {
    if (!s_labelsLoaded) {
        initLabelCache();
    }

    auto it = s_knownLabels.find(hash);
    if (it != s_knownLabels.end()) {
        return it->second;
    }
    std::stringstream ss;
    ss << "0x" << std::hex << std::uppercase << hash;
    return ss.str();
}

GFFFile::GFFFile() : m_loaded(false) {
    std::memset(&m_header, 0, sizeof(m_header));
}

GFFFile::~GFFFile() {
    close();
}

bool GFFFile::load(const std::string& path) {
    close();

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;

    size_t size = file.tellg();
    file.seekg(0);

    m_data.resize(size);
    file.read(reinterpret_cast<char*>(m_data.data()), size);
    file.close();

    if (!parseHeader()) {
        close();
        return false;
    }

    if (!parseStructs()) {
        close();
        return false;
    }

    m_loaded = true;
    return true;
}

bool GFFFile::load(const std::vector<uint8_t>& data) {
    close();

    m_data = data;

    if (!parseHeader()) {
        close();
        return false;
    }

    if (!parseStructs()) {
        close();
        return false;
    }

    m_loaded = true;
    return true;
}

void GFFFile::close() {
    m_data.clear();
    m_structs.clear();
    m_loaded = false;
    std::memset(&m_header, 0, sizeof(m_header));
}

bool GFFFile::parseHeader() {
    if (m_data.size() < 28) return false;

    m_header.magic = readAt<uint32_t>(0);
    m_header.version = readAt<uint32_t>(4);
    m_header.platform = readAt<uint32_t>(8);
    m_header.fileType = readAt<uint32_t>(12);
    m_header.fileVersion = readAt<uint32_t>(16);
    m_header.structCount = readAt<uint32_t>(20);

    if (m_header.magic != 0x20464647) {
        return false;
    }

    char ver[5] = {};
    std::memcpy(ver, &m_header.version, 4);
    m_header.isV41 = (std::string(ver) >= "V4.1");

    if (m_header.isV41) {
        if (m_data.size() < 36) return false;
        m_header.stringCount = readAt<uint32_t>(24);
        m_header.stringOffset = readAt<uint32_t>(28);
        m_header.dataOffset = readAt<uint32_t>(32);
        uint32_t strStart = m_header.stringOffset;
        uint32_t strEnd = m_header.dataOffset;
        if (strEnd > strStart && strEnd <= m_data.size()) {
            std::string current;
            for (uint32_t pos = strStart; pos < strEnd; pos++) {
                uint8_t c = m_data[pos];
                if (c == 0) {
                    m_stringCache.push_back(current);
                    current.clear();
                    if (m_stringCache.size() >= m_header.stringCount) break;
                } else {
                    current += static_cast<char>(c);
                }
            }
        }
    } else {
        m_header.stringCount = 0;
        m_header.stringOffset = 0;
        m_header.dataOffset = readAt<uint32_t>(24);
    }

    return true;
}

bool GFFFile::parseStructs() {
    if (m_data.size() < 28) return false;

    m_structs.resize(m_header.structCount);

    size_t structPos = m_header.isV41 ? 36 : 28;

    for (uint32_t i = 0; i < m_header.structCount; i++) {
        if (structPos + 16 > m_data.size()) return false;
        std::memcpy(m_structs[i].structType, &m_data[structPos], 4);
        m_structs[i].structType[4] = '\0';
        m_structs[i].fieldCount = readAt<uint32_t>(structPos + 4);
        m_structs[i].fieldOffset = readAt<uint32_t>(structPos + 8);
        m_structs[i].structSize = readAt<uint32_t>(structPos + 12);
        structPos += 16;
    }

    for (uint32_t i = 0; i < m_header.structCount; i++) {
        uint32_t fieldPos = m_structs[i].fieldOffset;
        m_structs[i].fields.resize(m_structs[i].fieldCount);

        for (uint32_t j = 0; j < m_structs[i].fieldCount; j++) {
            if (fieldPos + 12 > m_data.size()) break;
            m_structs[i].fields[j].label = readAt<uint32_t>(fieldPos);
            m_structs[i].fields[j].typeId = readAt<uint16_t>(fieldPos + 4);
            m_structs[i].fields[j].flags = readAt<uint16_t>(fieldPos + 6);
            m_structs[i].fields[j].dataOffset = readAt<uint32_t>(fieldPos + 8);
            fieldPos += 12;
        }
    }

    return true;
}

bool GFFFile::isMMH() const {
    return m_header.fileType == 0x204D484D;
}

bool GFFFile::isMSH() const {
    return m_header.fileType == 0x4853454D;
}

const GFFField* GFFFile::findField(const GFFStruct& st, uint32_t label) const {
    for (const auto& field : st.fields) {
        if (field.label == label) {
            return &field;
        }
    }
    return nullptr;
}

const GFFField* GFFFile::findField(uint32_t structIndex, uint32_t label) const {
    if (structIndex >= m_structs.size()) return nullptr;
    return findField(m_structs[structIndex], label);
}

std::string GFFFile::readStringByLabel(uint32_t structIndex, uint32_t label, uint32_t baseOffset) {
    if (structIndex >= m_structs.size()) return "";

    const GFFField* field = findField(structIndex, label);
    if (!field) return "";

    if (field->typeId != 14 && field->typeId != 10 && field->typeId != 11) return "";

    uint32_t dataPos = m_header.dataOffset + field->dataOffset + baseOffset;
    uint32_t address = readAt<uint32_t>(dataPos);

    if (address == 0xFFFFFFFF) return "";

    if (field->typeId == 14 && m_header.isV41) {
        if (address < m_stringCache.size()) return m_stringCache[address];
        return "";
    }

    uint32_t strPos = m_header.dataOffset + address;
    if (strPos + 4 > m_data.size()) return "";
    uint32_t length = readAt<uint32_t>(strPos);
    strPos += 4;

    std::string result;
    result.reserve(length);

    if (field->typeId == 14) {
        for (uint32_t i = 0; i < length && strPos + 2 <= m_data.size(); i++) {
            uint16_t wc = readAt<uint16_t>(strPos);
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
    } else {
        for (uint32_t i = 0; i < length && strPos < m_data.size(); i++) {
            char c = static_cast<char>(m_data[strPos]);
            strPos += 1;
            if (c != '\0') result += c;
        }
    }

    return result;
}

int32_t GFFFile::readInt32ByLabel(uint32_t structIndex, uint32_t label, uint32_t baseOffset) {
    if (structIndex >= m_structs.size()) return 0;
    const GFFField* field = findField(structIndex, label);
    if (!field) return 0;
    uint32_t dataPos = m_header.dataOffset + field->dataOffset + baseOffset;
    return readAt<int32_t>(dataPos);
}

uint32_t GFFFile::readUInt32ByLabel(uint32_t structIndex, uint32_t label, uint32_t baseOffset) {
    if (structIndex >= m_structs.size()) return 0;
    const GFFField* field = findField(structIndex, label);
    if (!field) return 0;
    uint32_t dataPos = m_header.dataOffset + field->dataOffset + baseOffset;
    return readAt<uint32_t>(dataPos);
}

float GFFFile::readFloatByLabel(uint32_t structIndex, uint32_t label, uint32_t baseOffset) {
    if (structIndex >= m_structs.size()) return 0.0f;
    const GFFField* field = findField(structIndex, label);
    if (!field) return 0.0f;
    uint32_t dataPos = m_header.dataOffset + field->dataOffset + baseOffset;
    return readAt<float>(dataPos);
}

GFFStructRef GFFFile::readStructRef(uint32_t structIndex, uint32_t label, uint32_t baseOffset) {
    GFFStructRef result = {0, 0};
    if (structIndex >= m_structs.size()) return result;

    const GFFField* field = findField(structIndex, label);
    if (!field) return result;

    bool isRef = (field->flags & FLAG_REFERENCE) != 0;
    bool isList = (field->flags & FLAG_LIST) != 0;

    if (isRef && !isList) {
        uint32_t dataPos = m_header.dataOffset + field->dataOffset + baseOffset;
        uint16_t refStructIdx = readAt<uint16_t>(dataPos);
        uint32_t refOffset = readAt<uint32_t>(dataPos + 4);
        result.structIndex = refStructIdx;
        result.offset = refOffset;
    }
    return result;
}

std::vector<GFFStructRef> GFFFile::readStructList(uint32_t structIndex, uint32_t label, uint32_t baseOffset) {
    std::vector<GFFStructRef> result;
    if (structIndex >= m_structs.size()) return result;

    const GFFField* field = findField(structIndex, label);
    if (!field) return result;

    bool isList = (field->flags & FLAG_LIST) != 0;
    bool isStruct = (field->flags & FLAG_STRUCT) != 0;
    bool isRef = (field->flags & FLAG_REFERENCE) != 0;

    uint32_t dataPos = m_header.dataOffset + field->dataOffset + baseOffset;
    int32_t ref = readAt<int32_t>(dataPos);
    if (ref < 0) return result;

    uint32_t listPos = m_header.dataOffset + ref;
    if (listPos + 4 > m_data.size()) return result;
    uint32_t listCount = readAt<uint32_t>(listPos);
    listPos += 4;

    if (isList && isStruct && !isRef) {
        if (field->typeId >= m_structs.size()) return result;
        uint32_t structSize = m_structs[field->typeId].structSize;
        uint32_t itemOffset = ref + 4;
        for (uint32_t i = 0; i < listCount; i++) {
            GFFStructRef sr;
            sr.structIndex = field->typeId;
            sr.offset = itemOffset;
            result.push_back(sr);
            itemOffset += structSize;
        }
    }
    else if (isList && isStruct && isRef) {
        for (uint32_t i = 0; i < listCount; i++) {
            if (listPos + 4 > m_data.size()) break;
            uint32_t itemOffset = readAt<uint32_t>(listPos);
            listPos += 4;
            GFFStructRef sr;
            sr.structIndex = field->typeId;
            sr.offset = itemOffset;
            result.push_back(sr);
        }
    }
    else if (isList && isRef && !isStruct) {
        for (uint32_t i = 0; i < listCount; i++) {
            if (listPos + 8 > m_data.size()) break;
            uint16_t structRef = readAt<uint16_t>(listPos);
            listPos += 4;
            uint32_t fieldOffset = readAt<uint32_t>(listPos);
            listPos += 4;
            GFFStructRef sr;
            sr.structIndex = structRef;
            sr.offset = fieldOffset;
            result.push_back(sr);
        }
    }
    return result;
}

uint32_t GFFFile::getListDataOffset(uint32_t structIndex, uint32_t label, uint32_t baseOffset) {
    if (structIndex >= m_structs.size()) return 0;
    const GFFField* field = findField(structIndex, label);
    if (!field) return 0;
    return readAt<int32_t>(m_header.dataOffset + field->dataOffset + baseOffset);
}

std::string GFFFile::readRawString(size_t offset) const {
    if (offset + 4 > m_data.size()) return "";
    uint32_t len = readAt<uint32_t>(offset);
    if (offset + 4 + len > m_data.size()) return "";
    return std::string((const char*)m_data.data() + offset + 4, len);
}

std::string GFFFile::readLocString(size_t offset) const {
    if (offset + 12 > m_data.size()) return "";
    int32_t strRef = readAt<int32_t>(offset + 4);
    uint32_t count = readAt<uint32_t>(offset + 8);

    if (count > 0) {
        size_t current = offset + 12;
        if (current + 8 <= m_data.size()) {
            uint32_t len = readAt<uint32_t>(current + 4);
            if (current + 8 + len <= m_data.size()) {
                return std::string((const char*)m_data.data() + current + 8, len);
            }
        }
    }
    if (strRef != -1) return "StrRef:" + std::to_string(strRef);
    return "";
}

std::string GFFFile::getFieldDisplayValue(const GFFField& field) const {
    size_t dataPos = m_header.dataOffset + field.dataOffset;

    if (field.flags & FLAG_LIST) return "(List)";
    if (field.flags & FLAG_STRUCT) return "(Struct)";
    if ((field.flags & FLAG_REFERENCE) && field.typeId > 17) return "(Reference)";

    if ((field.flags & FLAG_REFERENCE) && field.typeId != 14) {
        uint32_t ptr = readAt<uint32_t>(dataPos);
        if (ptr == 0xFFFFFFFF) return "null";
        dataPos = m_header.dataOffset + ptr;
    }

    switch (field.typeId) {
        case 0: return std::to_string(readAt<uint8_t>(dataPos));
        case 1: return std::to_string(readAt<int8_t>(dataPos));
        case 2: return std::to_string(readAt<uint16_t>(dataPos));
        case 3: return std::to_string(readAt<int16_t>(dataPos));
        case 4: return std::to_string(readAt<uint32_t>(dataPos));
        case 5: return std::to_string(readAt<int32_t>(dataPos));
        case 6: return std::to_string(readAt<uint64_t>(dataPos));
        case 7: return std::to_string(readAt<int64_t>(dataPos));
        case 8: return std::to_string(readAt<float>(dataPos));
        case 9: return std::to_string(readAt<double>(dataPos));
        case 10:
        case 11:
        {
            int32_t relOffset = readAt<int32_t>(dataPos);
            if (relOffset < 0) return "";
            return readRawString(m_header.dataOffset + relOffset);
        }
        case 14:
        {
            uint32_t address = readAt<uint32_t>(dataPos);
            if (address == 0xFFFFFFFF) return "";
            if (m_header.isV41) {
                if (address < m_stringCache.size()) return m_stringCache[address];
                return "";
            }
            uint32_t strPos = m_header.dataOffset + address;
            if (strPos + 4 > m_data.size()) return "";
            uint32_t length = readAt<uint32_t>(strPos);
            strPos += 4;
            std::string result;
            result.reserve(length);
            for (uint32_t i = 0; i < length && strPos + 2 <= m_data.size(); i++) {
                uint16_t wc = readAt<uint16_t>(strPos);
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
        case 12:
        {
            int32_t relOffset = readAt<int32_t>(dataPos);
            if (relOffset < 0) return "";
            return readLocString(m_header.dataOffset + relOffset);
        }
        case 13: return "(Binary)";
        case 17:
        {
            uint32_t tlkId = readAt<uint32_t>(dataPos);
            uint32_t address = readAt<uint32_t>(dataPos + 4);
            std::string label = std::to_string(tlkId);
            std::string text;
            if (address != 0xFFFFFFFF && (address != 0 || m_header.isV41)) {
                if (m_header.isV41) {
                    if (address < m_stringCache.size()) text = m_stringCache[address];
                } else {
                    uint32_t strPos = m_header.dataOffset + address;
                    if (strPos + 4 <= m_data.size()) {
                        uint32_t length = readAt<uint32_t>(strPos);
                        strPos += 4;
                        for (uint32_t i = 0; i < length && strPos + 2 <= m_data.size(); i++) {
                            uint16_t wc = readAt<uint16_t>(strPos);
                            strPos += 2;
                            if (wc == 0) continue;
                            if (wc < 0x80) text += static_cast<char>(wc);
                            else if (wc < 0x800) {
                                text += static_cast<char>(0xC0 | (wc >> 6));
                                text += static_cast<char>(0x80 | (wc & 0x3F));
                            } else {
                                text += static_cast<char>(0xE0 | (wc >> 12));
                                text += static_cast<char>(0x80 | ((wc >> 6) & 0x3F));
                                text += static_cast<char>(0x80 | (wc & 0x3F));
                            }
                        }
                    }
                }
            }
            if (text.empty() && GFF4TLK::isLoaded())
                text = GFF4TLK::lookup(tlkId);
            if (!text.empty()) return label + ", " + text;
            return label;
        }
        default: return "???";
    }
}

void GFFFile::walk(GFF4Visitor visitor) const {
    if (!m_loaded || m_structs.empty()) return;
    walkStruct(0, visitor, "", 0);
}

void GFFFile::walkStruct(uint32_t structIdx, GFF4Visitor visitor, const std::string& basePath, int depth) const {
    if (structIdx >= m_structs.size()) return;
    const GFFStruct& st = m_structs[structIdx];

    for (const auto& field : st.fields) {
        std::string labelName = getLabel(field.label);
        std::string currentPath = basePath.empty() ? labelName : basePath + "." + labelName;
        std::string valStr = getFieldDisplayValue(field);

        std::string typeName = "Unknown";
        bool isComplex = false;

        if (field.flags & FLAG_LIST) { typeName = "List"; isComplex = true; }
        else if (field.flags & FLAG_STRUCT) { typeName = "Struct"; isComplex = true; }
        else if ((field.flags & FLAG_REFERENCE) && field.typeId > 17) { typeName = "Reference"; isComplex = true; }
        else {
            switch(field.typeId) {
                case 0: typeName = "BYTE"; break;
                case 4: typeName = "DWORD"; break;
                case 5: typeName = "INT"; break;
                case 8: typeName = "FLOAT"; break;
                case 10: typeName = "STRING"; break;
                case 11: typeName = "RESREF"; break;
                default: typeName = "Type_" + std::to_string(field.typeId); break;
            }
        }

        visitor(currentPath, labelName, typeName, valStr, depth, isComplex);

        if (field.flags & FLAG_STRUCT && !(field.flags & FLAG_LIST) && !(field.flags & FLAG_REFERENCE)) {
            walkStruct(field.typeId, visitor, currentPath, depth + 1);
        }
        else if (field.flags & FLAG_LIST) {
            uint32_t listOffset = m_header.dataOffset + readAt<uint32_t>(m_header.dataOffset + field.dataOffset);
            if (listOffset + 4 <= m_data.size()) {
                uint32_t count = readAt<uint32_t>(listOffset);
                for(uint32_t k=0; k<count; ++k) {
                    if (listOffset + 4 + (k*4) + 4 > m_data.size()) break;
                    uint32_t itemStructIdx = readAt<uint32_t>(listOffset + 4 + (k*4));
                    std::string itemPath = currentPath + "[" + std::to_string(k) + "]";
                    visitor(itemPath, std::to_string(k), "Struct", "", depth + 1, true);
                    walkStruct(itemStructIdx, visitor, itemPath, depth + 2);
                }
            }
        }
    }
}

std::pair<uint32_t, uint32_t> GFFFile::readPrimitiveListInfo(uint32_t structIndex, uint32_t label, uint32_t baseOffset) {
    const GFFField* field = findField(structIndex, label);
    if (!field) return {0, 0};
    if (!(field->flags & FLAG_LIST)) return {0, 0};
    uint32_t dataPos = m_header.dataOffset + baseOffset + field->dataOffset;
    if (dataPos + 4 > m_data.size()) return {0, 0};
    int32_t ref = readAt<int32_t>(dataPos);
    if (ref < 0) return {0, 0};
    uint32_t listPos = m_header.dataOffset + ref;
    if (listPos + 4 > m_data.size()) return {0, 0};
    uint32_t count = readAt<uint32_t>(listPos);
    return {count, listPos + 4};
}

uint32_t GFFFile::primitiveTypeSize(uint16_t typeId) {
    switch (typeId) {
        case 0: case 1: return 1;
        case 2: case 3: return 2;
        case 4: case 5: case 8: case 14: return 4;
        case 6: case 7: case 9: return 8;
        case 10: return 12;
        case 11: return 8;
        case 12: case 13: case 15: return 16;
        case 16: return 64;
        case 17: return 8;
        default: return 4;
    }
}

static std::vector<uint16_t> utf8ToUtf16(const std::string& str) {
    std::vector<uint16_t> result;
    size_t i = 0;
    while (i < str.size()) {
        uint32_t cp = 0;
        uint8_t c = str[i];
        if (c < 0x80) { cp = c; i += 1; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; if (i + 1 < str.size()) cp = (cp << 6) | (str[i+1] & 0x3F); i += 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; if (i + 2 < str.size()) { cp = (cp << 6) | (str[i+1] & 0x3F); cp = (cp << 6) | (str[i+2] & 0x3F); } i += 3; }
        else { cp = c & 0x07; if (i + 3 < str.size()) { cp = (cp << 6) | (str[i+1] & 0x3F); cp = (cp << 6) | (str[i+2] & 0x3F); cp = (cp << 6) | (str[i+3] & 0x3F); } i += 4; }
        if (cp <= 0xFFFF) result.push_back(static_cast<uint16_t>(cp));
        else { cp -= 0x10000; result.push_back(static_cast<uint16_t>(0xD800 + (cp >> 10))); result.push_back(static_cast<uint16_t>(0xDC00 + (cp & 0x3FF))); }
    }
    return result;
}

bool GFFFile::writeECString(uint32_t fieldDataPos, const std::string& newStr) {
    if (fieldDataPos + 4 > m_data.size()) return false;
    uint32_t address = readUInt32At(fieldDataPos);
    auto wchars = utf8ToUtf16(newStr);
    uint32_t newLen = static_cast<uint32_t>(wchars.size());
    if (address != 0xFFFFFFFF) {
        uint32_t strPos = m_header.dataOffset + address;
        if (strPos + 4 <= m_data.size()) {
            uint32_t oldLen = readUInt32At(strPos);
            if (newLen <= oldLen) {
                writeAt(strPos, newLen);
                for (uint32_t i = 0; i < newLen; i++)
                    writeAt(strPos + 4 + i * 2, wchars[i]);
                for (uint32_t i = newLen; i < oldLen; i++)
                    writeAt<uint16_t>(strPos + 4 + i * 2, 0);
                return true;
            }
        }
    }
    uint32_t newAddress = static_cast<uint32_t>(m_data.size()) - m_header.dataOffset;
    m_data.resize(m_data.size() + 4 + newLen * 2);
    uint32_t newStrPos = m_header.dataOffset + newAddress;
    writeAt(newStrPos, newLen);
    for (uint32_t i = 0; i < newLen; i++)
        writeAt(newStrPos + 4 + i * 2, wchars[i]);
    writeAt(fieldDataPos, newAddress);
    return true;
}

bool GFFFile::save(const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    f.write(reinterpret_cast<const char*>(m_data.data()), m_data.size());
    return f.good();
}

namespace GFF4TLK {
    static std::unordered_map<uint32_t, std::string> s_strings;
    static bool s_loaded = false;

    static std::string readECString(const GFFFile& gff, uint32_t dataPos) {
        const auto& raw = gff.rawData();
        int32_t relOffset = gff.readInt32At(dataPos);
        if (relOffset < 0) return "";
        uint32_t strPos = gff.dataOffset() + relOffset;
        if (strPos + 4 > raw.size()) return "";
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

    static bool loadV02(GFFFile& gff) {
        auto items = gff.readStructList(0, 19001, 0);
        for (auto& item : items) {
            uint32_t id = gff.readUInt32ByLabel(item.structIndex, 19002, item.offset);
            const GFFField* strField = gff.findField(item.structIndex, 19003);
            if (strField) {
                uint32_t dataPos = gff.dataOffset() + item.offset + strField->dataOffset;
                s_strings[id] = readECString(gff, dataPos);
            }
        }
        return true;
    }

    static std::string huffmanDecompress(uint32_t bitStart,
                                          const std::vector<int32_t>& tree,
                                          const std::vector<uint32_t>& data) {
        if (tree.empty() || data.empty()) return "";
        uint32_t index = bitStart >> 5;
        uint32_t shift = bitStart & 0x1F;
        if (index >= data.size()) return "";
        uint32_t n = data[index] >> shift;
        std::string result;
        while (true) {
            int32_t e = (int32_t)(tree.size() / 2) - 1;
            while (e >= 0) {
                if ((uint32_t)e * 2 + 1 >= tree.size()) return result;
                e = tree[e * 2 + (n & 1)];
                if (shift < 31) {
                    n >>= 1;
                    shift++;
                } else {
                    index++;
                    if (index >= data.size()) return result;
                    n = data[index];
                    shift = 0;
                }
            }
            if (e == -1) break;
            uint16_t wc = (uint16_t)(-(e) - 1);
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

    static bool loadV05(GFFFile& gff) {
        auto [treeCount, treeStart] = gff.readPrimitiveListInfo(0, 19007, 0);
        auto [dataCount, dataStart] = gff.readPrimitiveListInfo(0, 19008, 0);
        if (treeCount == 0 || dataCount == 0) return false;
        std::vector<int32_t> tree(treeCount);
        for (uint32_t i = 0; i < treeCount; i++)
            tree[i] = gff.readAt<int32_t>(treeStart + i * 4);
        std::vector<uint32_t> data(dataCount);
        for (uint32_t i = 0; i < dataCount; i++)
            data[i] = gff.readAt<uint32_t>(dataStart + i * 4);
        auto entries = gff.readStructList(0, 19006, 0);
        for (auto& entry : entries) {
            uint32_t id = gff.readUInt32ByLabel(entry.structIndex, 19004, entry.offset);
            uint32_t bitOffset = gff.readUInt32ByLabel(entry.structIndex, 19005, entry.offset);
            s_strings[id] = huffmanDecompress(bitOffset, tree, data);
        }
        return true;
    }

    bool loadFromData(const std::vector<uint8_t>& data) {
        GFFFile gff;
        if (!gff.load(data)) return false;
        uint32_t fv = gff.header().fileVersion;
        char ver[5] = {};
        std::memcpy(ver, &fv, 4);
        bool ok = false;
        if (std::string(ver) == "V0.2") {
            ok = loadV02(gff);
        } else if (std::string(ver) == "V0.5") {
            ok = loadV05(gff);
        }
        if (ok) s_loaded = true;
        return ok;
    }

    bool loadFromFile(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        f.seekg(0, std::ios::end);
        size_t sz = f.tellg();
        f.seekg(0, std::ios::beg);
        std::vector<uint8_t> data(sz);
        f.read(reinterpret_cast<char*>(data.data()), sz);
        return loadFromData(data);
    }

    int loadAllFromPath(const std::string& gamePath) {
        clear();
        namespace fs = std::filesystem;
        int fileCount = 0;
        fs::path base(gamePath);
        if (!fs::exists(base)) return 0;
        try {
            for (auto& entry : fs::recursive_directory_iterator(base, fs::directory_options::skip_permission_denied)) {
                if (!entry.is_regular_file()) continue;
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext != ".tlk") continue;
                size_t before = s_strings.size();
                if (loadFromFile(entry.path().string())) {
                    if (s_strings.size() > before) fileCount++;
                }
            }
        } catch (...) {}
        return fileCount;
    }

    void clear() {
        s_strings.clear();
        s_loaded = false;
    }

    bool isLoaded() { return s_loaded; }

    std::string lookup(uint32_t id) {
        auto it = s_strings.find(id);
        if (it != s_strings.end()) return it->second;
        return "";
    }

    size_t count() { return s_strings.size(); }
}