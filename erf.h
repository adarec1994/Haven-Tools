#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <fstream>

struct ERFEntry {
    std::string name;
    uint64_t name_hash;
    uint32_t type_hash;
    uint64_t offset;
    uint32_t packed_length;
    uint32_t length;
    uint32_t resid;
    uint16_t restype;
};

enum class ERFVersion {
    Unknown,
    V1_0,
    V1_1,
    V2_0,
    V2_2,
    V3_0
};

class ERFFile {
public:
    ERFFile();
    ~ERFFile();

    bool open(const std::string& path);
    void close();

    const std::vector<ERFEntry>& entries() const { return m_entries; }
    ERFVersion version() const { return m_version; }
    const std::string& path() const { return m_path; }
    std::string filename() const;

    bool extractEntry(const ERFEntry& entry, const std::string& destPath);
    std::vector<uint8_t> readEntry(const ERFEntry& entry);

    uint32_t encryption() const { return m_encryption; }
    uint32_t compression() const { return m_compression; }

private:
    bool parseV1();
    bool parseV2_0();
    bool parseV2_2();
    bool parseV3_0();

    std::string m_path;
    std::ifstream m_file;
    ERFVersion m_version;
    std::vector<ERFEntry> m_entries;
    uint32_t m_encryption;
    uint32_t m_compression;
};

std::vector<std::string> scanForERFFiles(const std::string& rootPath);