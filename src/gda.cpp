#include "gda.h"
#include "Gff.h"
#include <fstream>
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
        char lc = (c >= 'A' && c <= 'Z') ? (c + 32) : c;
        crc = (crc >> 8) ^ s_crc32Table[(crc ^ lc) & 0xFF];
        crc = (crc >> 8) ^ s_crc32Table[(crc ^ 0) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

int GDATable::findColumn(const std::string& name) const {
    std::string lowerName = name;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    for (size_t i = 0; i < columns.size(); i++) {
        std::string colLower = columns[i].name;
        std::transform(colLower.begin(), colLower.end(), colLower.begin(), ::tolower);
        if (colLower == lowerName) return static_cast<int>(i);
    }
    return -1;
}

GDAValue GDATable::getValue(int rowIndex, int colIndex) const {
    if (rowIndex < 0 || rowIndex >= (int)rows.size()) return GDAValue{};
    if (colIndex < 0 || colIndex >= (int)rows[rowIndex].values.size()) return GDAValue{};
    return rows[rowIndex].values[colIndex];
}

GDAValue GDATable::getValue(int rowIndex, const std::string& colName) const {
    int colIdx = findColumn(colName);
    if (colIdx < 0) return GDAValue{};
    return getValue(rowIndex, colIdx);
}

bool GDATable::setValue(int rowIndex, int colIndex, const GDAValue& value) {
    if (rowIndex < 0 || rowIndex >= (int)rows.size()) return false;
    if (colIndex < 0 || colIndex >= (int)rows[rowIndex].values.size()) return false;
    rows[rowIndex].values[colIndex] = value;
    return true;
}

bool GDATable::setValue(int rowIndex, const std::string& colName, const GDAValue& value) {
    int colIdx = findColumn(colName);
    if (colIdx < 0) return false;
    return setValue(rowIndex, colIdx, value);
}

int GDATable::findRowById(int32_t id) const {
    for (size_t i = 0; i < rows.size(); i++) {
        if (rows[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

int GDATable::addRow(int32_t id) {
    if (findRowById(id) >= 0) return -1;
    GDARow row;
    row.id = id;
    for (const auto& col : columns) {
        switch (col.type) {
            case GDAColumnType::Int:
            case GDAColumnType::Bool:
                row.values.push_back(int32_t(0));
                break;
            case GDAColumnType::Float:
                row.values.push_back(0.0f);
                break;
            case GDAColumnType::String:
            case GDAColumnType::Resource:
                row.values.push_back(std::string("****"));
                break;
            default:
                row.values.push_back(std::string("****"));
                break;
        }
    }
    rows.push_back(row);
    return static_cast<int>(rows.size() - 1);
}

bool GDATable::removeRow(int rowIndex) {
    if (rowIndex < 0 || rowIndex >= (int)rows.size()) return false;
    rows.erase(rows.begin() + rowIndex);
    return true;
}

int32_t GDATable::getNextAvailableId() const {
    int32_t maxId = 0;
    for (const auto& row : rows) {
        if (row.id > maxId) maxId = row.id;
    }
    return maxId + 1;
}

GDAFile::GDAFile() : m_loaded(false), m_modified(false) {}
GDAFile::~GDAFile() {}

bool GDAFile::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    size_t size = file.tellg();
    file.seekg(0);
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    m_table.sourceFile = path;
    m_table.name = fs::path(path).stem().string();
    return load(data, m_table.name);
}

bool GDAFile::load(const std::vector<uint8_t>& data, const std::string& name) {
    m_loaded = false;
    m_modified = false;
    if (!name.empty()) m_table.name = name;
    m_rawData = data;
    if (!parseGFF(data)) return false;
    m_loaded = true;
    return true;
}

bool GDAFile::parseGFF(const std::vector<uint8_t>& data) {
    GFFFile gff;
    if (!gff.load(data)) return false;

    m_table.columns.clear();
    m_table.rows.clear();

    const auto& structs = gff.structs();
    if (structs.empty()) return false;

    int colmIdx = -1;
    int rowsIdx = -1;

    for (size_t i = 0; i < structs.size(); i++) {
        std::string stype(structs[i].structType);
        if (stype == "COLM" || stype == "colm") colmIdx = (int)i;
        else if (stype == "ROWS" || stype == "rows") rowsIdx = (int)i;
    }

    if (colmIdx < 0) return false;

    static const std::map<uint32_t, std::string> knownColumns = {
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
        {hashColumnName("APPEARANCE"), "APPEARANCE"}
    };

    const auto& colmStruct = structs[colmIdx];
    for (const auto& field : colmStruct.fields) {
        GDAColumn col;
        col.hash = field.label;
        col.flags = field.flags;
        col.offset = field.dataOffset;

        switch (field.typeId) {
            case 5: col.type = GDAColumnType::Int; col.size = 4; break;
            case 8: col.type = GDAColumnType::Float; col.size = 4; break;
            case 10:
            case 11: col.type = GDAColumnType::String; col.size = 4; break;
            case 12: col.type = GDAColumnType::Resource; col.size = 4; break;
            default: col.type = GDAColumnType::Int; col.size = 4; break;
        }

        auto it = knownColumns.find(field.label);
        if (it != knownColumns.end()) {
            col.name = it->second;
        } else {
            col.name = "COL_" + std::to_string(field.label);
        }

        m_table.columns.push_back(col);
    }

    if (rowsIdx < 0 || structs.size() < 1) return m_table.columns.size() > 0;

    const auto& rootStruct = structs[0];
    uint32_t rowStructSize = (rowsIdx >= 0) ? structs[rowsIdx].structSize : 0;

    for (const auto& field : rootStruct.fields) {
        if (field.flags & FLAG_LIST) {
            uint32_t listOff = gff.dataOffset() + field.dataOffset;
            if (listOff + 4 > data.size()) continue;

            uint32_t rowCount = gff.readUInt32At(listOff);
            uint32_t rowDataStart = listOff + 4;

            for (uint32_t r = 0; r < rowCount; r++) {
                GDARow row;
                uint32_t rowOff = rowDataStart + r * rowStructSize;

                for (const auto& col : m_table.columns) {
                    uint32_t valOff = rowOff + col.offset;

                    if (valOff + 4 > data.size()) {
                        row.values.push_back(std::string("****"));
                        continue;
                    }

                    if (col.name == "ID") {
                        row.id = gff.readInt32At(valOff);
                        continue;
                    }

                    switch (col.type) {
                        case GDAColumnType::Int:
                            row.values.push_back(gff.readInt32At(valOff));
                            break;
                        case GDAColumnType::Float:
                            row.values.push_back(gff.readFloatAt(valOff));
                            break;
                        case GDAColumnType::String:
                        case GDAColumnType::Resource: {
                            int32_t strOff = gff.readInt32At(valOff);
                            if (strOff == -1) {
                                row.values.push_back(std::string("****"));
                            } else {
                                uint32_t absOff = gff.dataOffset() + strOff;
                                std::string str;
                                while (absOff < data.size() && data[absOff] != 0) {
                                    str += static_cast<char>(data[absOff]);
                                    absOff++;
                                }
                                row.values.push_back(str.empty() ? "****" : str);
                            }
                            break;
                        }
                        case GDAColumnType::Bool:
                            row.values.push_back(gff.readInt32At(valOff) != 0);
                            break;
                        default:
                            row.values.push_back(std::string("****"));
                            break;
                    }
                }

                m_table.rows.push_back(row);
            }
            break;
        }
    }

    auto it = std::find_if(m_table.columns.begin(), m_table.columns.end(),
        [](const GDAColumn& c) { return c.name == "ID"; });
    if (it != m_table.columns.end()) {
        m_table.columns.erase(it);
    }

    return true;
}

bool GDAFile::save(const std::string& path) {
    std::vector<uint8_t> data = saveToMemory();
    if (data.empty()) return false;
    std::ofstream file(path, std::ios::binary);
    if (!file) return false;
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    m_modified = false;
    return file.good();
}

std::vector<uint8_t> GDAFile::saveToMemory() {
    return m_rawData;
}

std::vector<uint8_t> GDAFile::buildGFF() {
    return m_rawData;
}

bool GDAFile::createBackup(const std::string& gdaPath, const std::string& backupDir) {
    if (!fs::exists(gdaPath)) return false;
    fs::create_directories(backupDir);
    std::string backupPath = getBackupPath(gdaPath, backupDir);
    if (fs::exists(backupPath)) return true;
    try {
        fs::copy_file(gdaPath, backupPath);
        return true;
    } catch (...) {
        return false;
    }
}

bool GDAFile::restoreBackup(const std::string& gdaPath, const std::string& backupDir) {
    std::string backupPath = getBackupPath(gdaPath, backupDir);
    if (!fs::exists(backupPath)) return false;
    try {
        fs::copy_file(backupPath, gdaPath, fs::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

bool GDAFile::backupExists(const std::string& gdaPath, const std::string& backupDir) {
    return fs::exists(getBackupPath(gdaPath, backupDir));
}

std::string GDAFile::getBackupPath(const std::string& gdaPath, const std::string& backupDir) {
    std::string filename = fs::path(gdaPath).filename().string();
    return (fs::path(backupDir) / (filename + ".backup")).string();
}

std::vector<std::string> getItemVariationTypes() {
    return {
        "mace_variation",
        "greatsword_variation",
        "gloves_heavy_variation",
        "gloves_medium_variation",
        "gloves_light_variation",
        "gloves_massive_variation",
        "boots_massive_variation",
        "boots_heavy_variation",
        "boots_light_variation",
        "boots_medium_variation",
        "helmet_mage_variation",
        "helmet_massive_variation",
        "helmet_heavy_variation",
        "helmet_medium_variation",
        "helmet_light_variation",
        "armor_massive_variation",
        "armor_medium_variation",
        "armor_light_variation",
        "armor_heavy_variation",
        "longsword_variation",
        "staff_variation",
        "wand_variation",
        "dagger_variation",
        "waraxe_variation",
        "battleaxe_variation",
        "maul_variation",
        "lround_shield_variation",
        "sround_shield_variation",
        "kite_shield_variation",
        "tower_shield_variation",
        "crossbow_variation",
        "shortbow_variation",
        "longbow_variation",
        "bolt_variation",
        "arrow_variation",
        "clothing_variation"
    };
}

ItemVariation parseItemVariationRow(const GDATable& table, int rowIndex) {
    ItemVariation var;
    if (rowIndex < 0 || rowIndex >= (int)table.rows.size()) return var;
    var.id = table.rows[rowIndex].id;

    auto getStr = [&](const std::string& col) -> std::string {
        GDAValue val = table.getValue(rowIndex, col);
        if (std::holds_alternative<std::string>(val)) {
            return std::get<std::string>(val);
        }
        return "";
    };

    auto getInt = [&](const std::string& col) -> int32_t {
        GDAValue val = table.getValue(rowIndex, col);
        if (std::holds_alternative<int32_t>(val)) {
            return std::get<int32_t>(val);
        }
        return 0;
    };

    var.label = getStr("LABEL");
    var.modelType = getStr("MODELTYPE");
    var.modelSubType = getStr("MODELSUBTYPE");
    var.modelVariation = getStr("MODELVARIATION");
    var.iconName = getStr("ICONNAME");
    var.defaultMaterial = getInt("DEFAULTMATERIAL");

    return var;
}

bool createItemVariationRow(GDATable& table, const ItemVariation& variation) {
    int rowIdx = table.addRow(variation.id);
    if (rowIdx < 0) return false;
    table.setValue(rowIdx, "LABEL", variation.label);
    table.setValue(rowIdx, "MODELTYPE", variation.modelType);
    table.setValue(rowIdx, "MODELSUBTYPE", variation.modelSubType);
    table.setValue(rowIdx, "MODELVARIATION", variation.modelVariation);
    table.setValue(rowIdx, "ICONNAME", variation.iconName);
    table.setValue(rowIdx, "DEFAULTMATERIAL", variation.defaultMaterial);
    return true;
}