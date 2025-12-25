#include "Gff.h"
#include <fstream>
#include <cstring>

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

    // Check magic "GFF " = 0x20464647
    if (m_header.magic != 0x20464647) {
        return false;
    }

    return true;
}

bool GFFFile::parseStructs() {
    if (m_data.size() < 28) return false;

    m_structs.resize(m_header.structCount);

    // Struct definitions start at offset 28
    uint32_t pos = 28;

    for (uint32_t i = 0; i < m_header.structCount; i++) {
        std::memcpy(m_structs[i].structType, &m_data[pos], 4);
        m_structs[i].structType[4] = '\0';
        m_structs[i].fieldCount = readAt<uint32_t>(pos + 4);
        m_structs[i].fieldOffset = readAt<uint32_t>(pos + 8);
        m_structs[i].structSize = readAt<uint32_t>(pos + 12);
        pos += 16;
    }

    // Read fields for each struct
    for (uint32_t i = 0; i < m_header.structCount; i++) {
        uint32_t fieldPos = m_structs[i].fieldOffset;
        m_structs[i].fields.resize(m_structs[i].fieldCount);

        for (uint32_t j = 0; j < m_structs[i].fieldCount; j++) {
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
    // "MMH " = 0x204D484D
    return m_header.fileType == 0x204D484D;
}

bool GFFFile::isMSH() const {
    // "MESH" = 0x4853454D
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

    // Type 14 is ECString
    if (field->typeId != 14) return "";

    uint32_t dataPos = m_header.dataOffset + field->dataOffset + baseOffset;
    int32_t strOffset = readAt<int32_t>(dataPos);

    if (strOffset < 0) return ""; // Null reference

    uint32_t strPos = m_header.dataOffset + strOffset;
    uint32_t length = readAt<uint32_t>(strPos);
    strPos += 4;

    std::string result;
    result.reserve(length);

    for (uint32_t i = 0; i < length && strPos + 1 < m_data.size(); i++) {
        char c = static_cast<char>(m_data[strPos]);
        strPos += 2; // Skip second byte (wchar)
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

    // For a single reference (not a list), read the reference data directly
    if (isRef && !isList) {
        uint32_t dataPos = m_header.dataOffset + field->dataOffset + baseOffset;

        // Read the reference: structIndex (2 bytes) + flags (2 bytes) + offset (4 bytes)
        uint16_t refStructIdx = readAt<uint16_t>(dataPos);
        uint16_t refFlags = readAt<uint16_t>(dataPos + 2);
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

    if (ref < 0) return result; // Null reference

    uint32_t listPos = m_header.dataOffset + ref;
    uint32_t listCount = readAt<uint32_t>(listPos);
    listPos += 4;

    if (isList && isStruct && !isRef) {
        // Struct list - items are sequential in memory
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
        // Ref struct list - each item is an offset
        for (uint32_t i = 0; i < listCount; i++) {
            uint32_t itemOffset = readAt<uint32_t>(listPos);
            listPos += 4;

            GFFStructRef sr;
            sr.structIndex = field->typeId;
            sr.offset = itemOffset;
            result.push_back(sr);
        }
    }
    else if (isList && isRef && !isStruct) {
        // Generic list with references
        for (uint32_t i = 0; i < listCount; i++) {
            uint16_t structRef = readAt<uint16_t>(listPos);
            listPos += 2;
            uint16_t fieldFlags = readAt<uint16_t>(listPos);
            listPos += 2;
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

    uint32_t dataPos = m_header.dataOffset + field->dataOffset + baseOffset;
    int32_t ref = readAt<int32_t>(dataPos);

    if (ref < 0) return 0;

    return ref; // Return offset relative to data section
}