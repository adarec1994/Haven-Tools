#include "gda.h"
#include "Gff.h"
#include <algorithm>
#include <cstring>
#include <iostream>

static uint32_t s_crc32Table[256];
static bool s_crc32Init = false;

static void initCrc32Table() {
    if (s_crc32Init) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
        s_crc32Table[i] = crc;
    }
    s_crc32Init = true;
}

uint32_t GDAFile::hashColumnName(const std::string& name) {
    initCrc32Table();
    uint32_t crc = 0xFFFFFFFF;
    for (char c : name) {
        wchar_t wc = static_cast<wchar_t>(c >= 'A' && c <= 'Z' ? c + 32 : c);
        uint8_t lo = wc & 0xFF;
        uint8_t hi = (wc >> 8) & 0xFF;
        crc = (crc >> 8) ^ s_crc32Table[(crc ^ lo) & 0xFF];
        crc = (crc >> 8) ^ s_crc32Table[(crc ^ hi) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

const std::map<uint32_t, std::string> GDAFile::s_knownColumns = {
    {hashColumnName("ID"), "ID"},
    {hashColumnName("LABEL"), "LABEL"},
    {hashColumnName("MODELTYPE"), "MODELTYPE"},
    {hashColumnName("MODELSUBTYPE"), "MODELSUBTYPE"},
    {hashColumnName("MODELVARIATION"), "MODELVARIATION"},
    {hashColumnName("ICONNAME"), "ICONNAME"},
    {hashColumnName("DEFAULTMATERIAL"), "DEFAULTMATERIAL"},
    {hashColumnName("NAME"), "NAME"},
    {hashColumnName("DESCRIPTION"), "DESCRIPTION"},
    {hashColumnName("RESREF"), "RESREF"},
    {hashColumnName("TAG"), "TAG"},
    {hashColumnName("ENABLED"), "ENABLED"},
    {hashColumnName("STRINGID"), "STRINGID"},
    {hashColumnName("COST"), "COST"},
    {hashColumnName("VALUE"), "VALUE"},
    {hashColumnName("COMMENT"), "COMMENT"},
    {hashColumnName("SCRIPT"), "SCRIPT"},
    {hashColumnName("MODEL"), "MODEL"},
    {hashColumnName("TEXTURE"), "TEXTURE"},
    {hashColumnName("MATERIAL"), "MATERIAL"},
    {hashColumnName("APPEARANCE"), "APPEARANCE"},
    {hashColumnName("NAMESTRINGID"), "NAMESTRINGID"},
    {hashColumnName("DESCSTRINGID"), "DESCSTRINGID"},
    {hashColumnName("PREFIX"), "PREFIX"},
    {hashColumnName("PACKAGE"), "PACKAGE"},
    {hashColumnName("PARENTFOLDER"), "PARENTFOLDER"},
};

GDAFile::GDAFile() {}
GDAFile::~GDAFile() {}

int GDAFile::findColumn(const std::string& name) const {
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    for (size_t i = 0; i < m_columns.size(); i++) {
        std::string colLower = m_columns[i].name;
        std::transform(colLower.begin(), colLower.end(), colLower.begin(), ::tolower);
        if (colLower == lowerName) return static_cast<int>(i);
    }
    return -1;
}

bool GDAFile::load(const std::vector<uint8_t>& data, const std::string& name) {
    m_loaded = false;
    m_modified = false;
    m_name = name;
    m_columns.clear();
    m_rows.clear();

    if (!parseGDA(data)) return false;
    m_loaded = true;
    return true;
}

std::string GDAFile::readString(const std::vector<uint8_t>& data, uint32_t offset) {
    if (offset == 0xFFFFFFFF || offset >= data.size()) return "****";

    if (offset + 4 > data.size()) return "****";
    uint32_t len = *reinterpret_cast<const uint32_t*>(&data[offset]);
    if (len == 0 || len > 10000) return "****";

    offset += 4;
    std::string result;
    for (uint32_t i = 0; i < len && offset + i * 2 + 1 < data.size(); i++) {
        uint16_t wc = *reinterpret_cast<const uint16_t*>(&data[offset + i * 2]);
        if (wc == 0) break;
        if (wc < 128) result += static_cast<char>(wc);
        else result += '?';
    }
    return result.empty() ? "****" : result;
}

bool GDAFile::parseGDA(const std::vector<uint8_t>& data) {
    GFFFile gff;
    if (!gff.load(data)) return false;

    const auto& structs = gff.structs();
    if (structs.size() < 3) return false;

    // Find struct indices
    int gtopIdx = -1, colmIdx = -1, rowsIdx = -1;
    for (size_t i = 0; i < structs.size(); i++) {
        std::string stype(structs[i].structType);
        if (stype == "gtop") gtopIdx = (int)i;
        else if (stype == "colm") colmIdx = (int)i;
        else if (stype == "rows") rowsIdx = (int)i;
    }

    if (gtopIdx < 0 || colmIdx < 0 || rowsIdx < 0) return false;

    uint32_t dataOffset = gff.dataOffset();
    const auto& gtopStruct = structs[gtopIdx];
    const auto& colmStruct = structs[colmIdx];
    const auto& rowsStruct = structs[rowsIdx];

    // gtop struct has 2 fields:
    // Field 0 (type=colmIdx, flags=0xc000): list of columns
    // Field 1 (type=rowsIdx, flags=0xc000): list of rows

    if (gtopStruct.fields.size() < 2) return false;

    // Get column list
    uint32_t colmListRefPos = dataOffset + gtopStruct.fields[0].dataOffset;
    int32_t colmListOffset = gff.readInt32At(colmListRefPos);
    if (colmListOffset < 0) return false;

    uint32_t colmListPos = dataOffset + colmListOffset;
    uint32_t columnCount = gff.readUInt32At(colmListPos);
    uint32_t colmDataStart = colmListPos + 4;
    uint32_t colmStructSize = colmStruct.structSize;  // 8 bytes: hash (4) + type (1) + padding (3)

    // Get row list
    uint32_t rowsListRefPos = dataOffset + gtopStruct.fields[1].dataOffset;
    int32_t rowsListOffset = gff.readInt32At(rowsListRefPos);
    if (rowsListOffset < 0) return false;

    uint32_t rowsListPos = dataOffset + rowsListOffset;
    uint32_t rowCount = gff.readUInt32At(rowsListPos);
    uint32_t rowDataStart = rowsListPos + 4;
    uint32_t rowSize = rowsStruct.structSize;

    // Build column info from rows struct fields (they have the offsets)
    // and from colm list (they have the hashes and types)
    struct ColInfo {
        uint32_t hash;
        uint8_t gdaType;  // 0=string, 1=int, 2=float, 3=bool, 4=resource
        uint16_t gffTypeId;
        uint32_t offset;
        std::string name;
    };
    std::vector<ColInfo> colInfos;

    // Read column metadata from colm list
    for (uint32_t i = 0; i < columnCount && i < rowsStruct.fields.size(); i++) {
        ColInfo ci;

        // From colm struct: hash and GDA type
        uint32_t colmOffset = colmDataStart + i * colmStructSize;
        ci.hash = gff.readUInt32At(colmOffset);
        ci.gdaType = gff.readUInt8At(colmOffset + 4);

        // From rows struct field: GFF type and offset within row
        ci.gffTypeId = rowsStruct.fields[i].typeId;
        ci.offset = rowsStruct.fields[i].dataOffset;

        // Look up column name
        auto it = s_knownColumns.find(ci.hash);
        if (it != s_knownColumns.end()) {
            ci.name = it->second;
        } else {
            ci.name = "COL_" + std::to_string(ci.hash);
        }

        colInfos.push_back(ci);
    }

    // Build columns
    for (const auto& ci : colInfos) {
        GDAColumn col;
        col.hash = ci.hash;
        col.name = ci.name;
        switch (ci.gdaType) {
            case 0: col.type = GDAType::String; break;
            case 1: col.type = GDAType::Int; break;
            case 2: col.type = GDAType::Float; break;
            case 3: col.type = GDAType::Bool; break;
            case 4: col.type = GDAType::Resource; break;
            default: col.type = GDAType::Int; break;
        }
        m_columns.push_back(col);
    }

    // Read rows
    for (uint32_t r = 0; r < rowCount; r++) {
        GDARow row;
        uint32_t rowOff = rowDataStart + r * rowSize;

        for (size_t c = 0; c < colInfos.size(); c++) {
            const auto& ci = colInfos[c];
            uint32_t valOff = rowOff + ci.offset;

            if (valOff + 4 > data.size()) {
                row.values.push_back(std::string("****"));
                continue;
            }

            // Use GDA type for interpretation
            switch (ci.gdaType) {
                case 0: // String
                case 4: // Resource (also a string)
                {
                    int32_t strOff = gff.readInt32At(valOff);
                    if (strOff < 0 || strOff == (int32_t)0xFFFFFFFF) {
                        row.values.push_back(std::string("****"));
                    } else {
                        std::string str = readString(data, dataOffset + strOff);
                        row.values.push_back(str);
                    }
                    break;
                }
                case 1: // Int
                {
                    // Check GFF type for size
                    switch (ci.gffTypeId) {
                        case 0: row.values.push_back(static_cast<int32_t>(gff.readUInt8At(valOff))); break;
                        case 1: row.values.push_back(static_cast<int32_t>(gff.readAt<int8_t>(valOff))); break;
                        case 2: row.values.push_back(static_cast<int32_t>(gff.readUInt16At(valOff))); break;
                        case 3: row.values.push_back(static_cast<int32_t>(gff.readInt16At(valOff))); break;
                        case 4: row.values.push_back(static_cast<int32_t>(gff.readUInt32At(valOff))); break;
                        case 5: row.values.push_back(gff.readInt32At(valOff)); break;
                        default: row.values.push_back(gff.readInt32At(valOff)); break;
                    }
                    break;
                }
                case 2: // Float
                {
                    if (ci.gffTypeId == 9) {
                        row.values.push_back(static_cast<float>(gff.readAt<double>(valOff)));
                    } else {
                        row.values.push_back(gff.readFloatAt(valOff));
                    }
                    break;
                }
                case 3: // Bool
                {
                    if (ci.gffTypeId == 0) {
                        row.values.push_back(gff.readUInt8At(valOff) != 0);
                    } else {
                        row.values.push_back(gff.readInt32At(valOff) != 0);
                    }
                    break;
                }
                default:
                    row.values.push_back(gff.readInt32At(valOff));
                    break;
            }
        }

        m_rows.push_back(row);
    }

    return true;
}