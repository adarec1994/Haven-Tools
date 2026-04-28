#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <variant>
#include <filesystem>

namespace fs = std::filesystem;

enum class GDAType {
    String = 0,
    Int = 1,
    Float = 2,
    Bool = 3,
    Resource = 4
};

struct GDAColumn {
    uint32_t hash;
    GDAType type;
    std::string name;
};

using GDAValue = std::variant<std::string, int32_t, float, bool>;

struct GDARow {
    std::vector<GDAValue> values;
};

class GDAFile {
public:
    GDAFile();
    ~GDAFile();

    bool load(const std::vector<uint8_t>& data, const std::string& name = "");

    const std::vector<GDAColumn>& columns() const { return m_columns; }
    const std::vector<GDARow>& rows() const { return m_rows; }
    std::vector<GDARow>& rows() { return m_rows; }

    bool isLoaded() const { return m_loaded; }
    bool isModified() const { return m_modified; }
    void setModified(bool m) { m_modified = m; }

    const std::string& name() const { return m_name; }

    int findColumn(const std::string& name) const;

    static uint32_t hashColumnName(const std::string& name);

private:
    bool parseGDA(const std::vector<uint8_t>& data);
    std::string readString(const std::vector<uint8_t>& data, uint32_t offset);

    std::vector<GDAColumn> m_columns;
    std::vector<GDARow> m_rows;
    std::string m_name;
    bool m_loaded = false;
    bool m_modified = false;

    static const std::map<uint32_t, std::string> s_knownColumns;
};