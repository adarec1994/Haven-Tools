#include "Gff.h"
#include <fstream>
#include <cstring>
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
    m_header.dataOffset = readAt<uint32_t>(24);

    if (m_header.magic != 0x20464647) {
        return false;
    }

    return true;
}

bool GFFFile::parseStructs() {
    if (m_data.size() < 28) return false;

    m_structs.resize(m_header.structCount);

    size_t structPos = sizeof(GFFHeader);

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
    int32_t strOffset = readAt<int32_t>(dataPos);

    if (strOffset < 0) return "";

    uint32_t strPos = m_header.dataOffset + strOffset;
    if (strPos + 4 > m_data.size()) return "";
    uint32_t length = readAt<uint32_t>(strPos);
    strPos += 4;

    std::string result;
    result.reserve(length);

    for (uint32_t i = 0; i < length && strPos + 1 < m_data.size(); i++) {
        char c = static_cast<char>(m_data[strPos]);
        strPos += 1;
        if (c != '\0') result += c;
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
    if (field.flags & FLAG_REFERENCE) return "(Reference)";

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
        case 12:
        {
            int32_t relOffset = readAt<int32_t>(dataPos);
            if (relOffset < 0) return "";
            return readLocString(m_header.dataOffset + relOffset);
        }
        case 13: return "(Binary)";
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
        else if (field.flags & FLAG_REFERENCE) { typeName = "Reference"; isComplex = true; }
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