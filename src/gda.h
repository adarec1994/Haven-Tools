#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <variant>
#include <filesystem>
#include <memory>

namespace fs = std::filesystem;

enum class GDAColumnType {
    Int,
    Float,
    String,
    Resource,
    Bool
};

using GDAValue = std::variant<int32_t, float, std::string, bool>;

struct GDAColumn {
    std::string name;
    uint32_t hash;
    GDAColumnType type;
    uint16_t flags;
    uint32_t offset;
    uint32_t size;
};

struct GDARow {
    int32_t id;
    std::vector<GDAValue> values;
};

struct GDATable {
    std::string name;
    std::string sourceFile;
    std::vector<GDAColumn> columns;
    std::vector<GDARow> rows;

    int findColumn(const std::string& name) const;
    GDAValue getValue(int rowIndex, int colIndex) const;
    GDAValue getValue(int rowIndex, const std::string& colName) const;
    bool setValue(int rowIndex, int colIndex, const GDAValue& value);
    bool setValue(int rowIndex, const std::string& colName, const GDAValue& value);
    int findRowById(int32_t id) const;
    int addRow(int32_t id);
    bool removeRow(int rowIndex);
    int32_t getNextAvailableId() const;
};

class GDAFile {
public:
    GDAFile();
    ~GDAFile();

    bool load(const std::string& path);
    bool load(const std::vector<uint8_t>& data, const std::string& name = "");
    bool save(const std::string& path);
    std::vector<uint8_t> saveToMemory();

    GDATable& table() { return m_table; }
    const GDATable& table() const { return m_table; }

    bool isLoaded() const { return m_loaded; }
    bool isModified() const { return m_modified; }
    void setModified(bool modified) { m_modified = modified; }

    static bool createBackup(const std::string& gdaPath, const std::string& backupDir);
    static bool restoreBackup(const std::string& gdaPath, const std::string& backupDir);
    static bool backupExists(const std::string& gdaPath, const std::string& backupDir);
    static std::string getBackupPath(const std::string& gdaPath, const std::string& backupDir);

private:
    bool parseGFF(const std::vector<uint8_t>& data);
    std::vector<uint8_t> buildGFF();
    static uint32_t hashColumnName(const std::string& name);

    GDATable m_table;
    bool m_loaded;
    bool m_modified;
    std::vector<uint8_t> m_rawData;
};

struct ItemVariation {
    int32_t id;
    std::string label;
    std::string modelType;
    std::string modelSubType;
    std::string modelVariation;
    std::string iconName;
    int32_t defaultMaterial;
};

std::vector<std::string> getItemVariationTypes();
ItemVariation parseItemVariationRow(const GDATable& table, int rowIndex);
bool createItemVariationRow(GDATable& table, const ItemVariation& variation);