#include "erf.h"
#include "fnv.h"
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <map>

namespace fs = std::filesystem;

template<typename T>
T readLE(std::istream& s) {
    T val = 0;
    s.read(reinterpret_cast<char*>(&val), sizeof(T));
    return val;
}

std::string readString(std::istream& s, size_t len) {
    std::string result(len, '\0');
    s.read(&result[0], len);
    return result;
}

std::u16string readU16String(std::istream& s, size_t byteLen) {
    std::u16string result(byteLen / 2, u'\0');
    s.read(reinterpret_cast<char*>(&result[0]), byteLen);
    return result;
}

std::string u16ToUtf8(const std::u16string& u16) {
    std::string result;
    for (char16_t c : u16) {
        if (c == 0) break;
        if (c < 0x80) {
            result += static_cast<char>(c);
        } else if (c < 0x800) {
            result += static_cast<char>(0xC0 | (c >> 6));
            result += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            result += static_cast<char>(0xE0 | (c >> 12));
            result += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            result += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return result;
}

ERFFile::ERFFile() : m_version(ERFVersion::Unknown), m_encryption(0), m_compression(0) {}

ERFFile::~ERFFile() {
    close();
}

std::string ERFFile::filename() const {
    return fs::path(m_path).filename().string();
}

bool ERFFile::open(const std::string& path) {
    close();

    m_file.open(path, std::ios::binary);
    if (!m_file) return false;

    m_path = path;

    char magic[16];
    m_file.read(magic, 16);

    if (std::memcmp(magic, "ERF ", 4) == 0 ||
        std::memcmp(magic, "MOD ", 4) == 0 ||
        std::memcmp(magic, "SAV ", 4) == 0 ||
        std::memcmp(magic, "HAK ", 4) == 0) {
        if (std::memcmp(magic + 4, "V1.0", 4) == 0) {
            m_version = ERFVersion::V1_0;
            return parseV1();
        } else if (std::memcmp(magic + 4, "V1.1", 4) == 0) {
            m_version = ERFVersion::V1_1;
            return parseV1();
        }
    }

    std::u16string ver16(8, u'\0');
    std::memcpy(&ver16[0], magic, 16);
    std::string verStr = u16ToUtf8(ver16);

    if (verStr == "ERF V2.0") {
        m_version = ERFVersion::V2_0;
        return parseV2_0();
    } else if (verStr == "ERF V2.2") {
        m_version = ERFVersion::V2_2;
        return parseV2_2();
    } else if (verStr == "ERF V3.0") {
        m_version = ERFVersion::V3_0;
        return parseV3_0();
    }

    close();
    return false;
}

void ERFFile::close() {
    if (m_file.is_open()) {
        m_file.close();
    }
    m_entries.clear();
    m_version = ERFVersion::Unknown;
    m_path.clear();
    m_encryption = 0;
    m_compression = 0;
}

bool ERFFile::parseV1() {
    m_file.seekg(8);

    uint32_t langCount = readLE<uint32_t>(m_file);
    uint32_t langSize = readLE<uint32_t>(m_file);
    uint32_t entryCount = readLE<uint32_t>(m_file);
    uint32_t langOffset = readLE<uint32_t>(m_file);
    uint32_t keyOffset = readLE<uint32_t>(m_file);
    uint32_t resOffset = readLE<uint32_t>(m_file);

    m_file.seekg(keyOffset);

    size_t nameLen = (m_version == ERFVersion::V1_0) ? 16 : 32;

    struct KeyEntry {
        std::string resref;
        uint32_t resid;
        uint16_t restype;
    };

    std::vector<KeyEntry> keys(entryCount);
    for (uint32_t i = 0; i < entryCount; i++) {
        std::string name = readString(m_file, nameLen);
        size_t nullPos = name.find('\0');
        if (nullPos != std::string::npos) name.resize(nullPos);
        keys[i].resref = name;
        keys[i].resid = readLE<uint32_t>(m_file);
        keys[i].restype = readLE<uint16_t>(m_file);
        m_file.seekg(2, std::ios::cur);
    }

    m_file.seekg(resOffset);

    m_entries.resize(entryCount);
    for (uint32_t i = 0; i < entryCount; i++) {
        uint32_t offset = readLE<uint32_t>(m_file);
        uint32_t length = readLE<uint32_t>(m_file);

        m_entries[i].name = keys[i].resref;
        m_entries[i].name_hash = fnv64(keys[i].resref);
        m_entries[i].type_hash = keys[i].restype;
        m_entries[i].offset = offset;
        m_entries[i].packed_length = length;
        m_entries[i].length = length;
        m_entries[i].resid = keys[i].resid;
        m_entries[i].restype = keys[i].restype;
    }

    return true;
}

bool ERFFile::parseV2_0() {
    uint32_t fileCount = readLE<uint32_t>(m_file);
    uint32_t year = readLE<uint32_t>(m_file);
    uint32_t day = readLE<uint32_t>(m_file);
    uint32_t unknown = readLE<uint32_t>(m_file);

    m_entries.resize(fileCount);
    for (uint32_t i = 0; i < fileCount; i++) {
        std::u16string name16 = readU16String(m_file, 64);
        m_entries[i].name = u16ToUtf8(name16);
        m_entries[i].offset = readLE<uint32_t>(m_file);
        m_entries[i].packed_length = readLE<uint32_t>(m_file);
        m_entries[i].length = m_entries[i].packed_length;
        m_entries[i].name_hash = fnv64(m_entries[i].name);
        m_entries[i].type_hash = 0;
        m_entries[i].resid = i;
        m_entries[i].restype = 0;
    }

    return true;
}

bool ERFFile::parseV2_2() {
    uint32_t fileCount = readLE<uint32_t>(m_file);
    uint32_t year = readLE<uint32_t>(m_file);
    uint32_t day = readLE<uint32_t>(m_file);
    uint32_t unknown = readLE<uint32_t>(m_file);
    uint32_t flags = readLE<uint32_t>(m_file);
    uint32_t moduleId = readLE<uint32_t>(m_file);

    m_file.seekg(16, std::ios::cur);

    m_encryption = (flags >> 4) & 0xF;
    m_compression = (flags >> 29) & 0x7;

    m_entries.resize(fileCount);
    for (uint32_t i = 0; i < fileCount; i++) {
        std::u16string name16 = readU16String(m_file, 64);
        m_entries[i].name = u16ToUtf8(name16);
        m_entries[i].offset = readLE<uint32_t>(m_file);
        m_entries[i].packed_length = readLE<uint32_t>(m_file);
        m_entries[i].length = readLE<uint32_t>(m_file);
        m_entries[i].name_hash = fnv64(m_entries[i].name);
        m_entries[i].type_hash = 0;
        m_entries[i].resid = i;
        m_entries[i].restype = 0;
    }

    return true;
}

bool ERFFile::parseV3_0() {
    uint32_t stringTableSize = readLE<uint32_t>(m_file);
    uint32_t fileCount = readLE<uint32_t>(m_file);
    uint32_t flags = readLE<uint32_t>(m_file);
    uint32_t moduleId = readLE<uint32_t>(m_file);

    m_file.seekg(16, std::ios::cur);

    m_encryption = (flags >> 4) & 0xF;
    m_compression = (flags >> 29) & 0x7;

    std::map<uint32_t, std::string> names;
    if (stringTableSize > 0) {
        std::vector<char> stringTable(stringTableSize);
        m_file.read(stringTable.data(), stringTableSize);

        uint32_t offset = 0;
        size_t start = 0;
        for (size_t i = 0; i < stringTableSize; i++) {
            if (stringTable[i] == '\0') {
                if (i > start) {
                    names[offset] = std::string(&stringTable[start], i - start);
                }
                offset = static_cast<uint32_t>(i + 1);
                start = i + 1;
            }
        }
    }

    m_entries.resize(fileCount);
    for (uint32_t i = 0; i < fileCount; i++) {
        int32_t nameOffset = readLE<int32_t>(m_file);
        uint64_t nameHash = readLE<uint64_t>(m_file);
        uint32_t typeHash = readLE<uint32_t>(m_file);
        uint32_t offset = readLE<uint32_t>(m_file);
        uint32_t packLen = readLE<uint32_t>(m_file);
        uint32_t unpackLen = readLE<uint32_t>(m_file);

        std::string name;
        if (nameOffset != -1) {
            auto it = names.find(static_cast<uint32_t>(nameOffset));
            if (it != names.end()) {
                name = it->second;
            }
        }

        if (name.empty()) {
            std::stringstream ss;
            ss << "[" << std::hex << std::setfill('0') << std::setw(16) << nameHash
               << "].[" << std::setw(8) << typeHash << "]";
            name = ss.str();
        }

        m_entries[i].name = name;
        m_entries[i].name_hash = nameHash;
        m_entries[i].type_hash = typeHash;
        m_entries[i].offset = offset;
        m_entries[i].packed_length = packLen;
        m_entries[i].length = unpackLen;
        m_entries[i].resid = i;
        m_entries[i].restype = 0;
    }

    return true;
}

std::vector<uint8_t> ERFFile::readEntry(const ERFEntry& entry) {
    if (!m_file.is_open()) return {};

    m_file.clear();
    m_file.seekg(entry.offset);

    std::vector<uint8_t> data(entry.packed_length);
    m_file.read(reinterpret_cast<char*>(data.data()), entry.packed_length);
    return data;
}

bool ERFFile::extractEntry(const ERFEntry& entry, const std::string& destPath) {
    std::vector<uint8_t> data = readEntry(entry);
    if (data.empty() && entry.packed_length > 0) return false;

    std::ofstream out(destPath, std::ios::binary);
    if (!out) return false;

    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    return out.good();
}

bool ERFFile::replaceEntry(size_t entryIndex, const std::vector<uint8_t>& newData) {
    if (entryIndex >= m_entries.size() || !m_file.is_open()) return false;

    // Read entire file into memory
    m_file.clear();
    m_file.seekg(0, std::ios::end);
    size_t fileSize = static_cast<size_t>(m_file.tellg());
    m_file.seekg(0);
    std::vector<uint8_t> buf(fileSize);
    m_file.read(reinterpret_cast<char*>(buf.data()), fileSize);

    ERFEntry& target = m_entries[entryIndex];
    uint64_t oldOff = target.offset;
    uint32_t oldLen = target.packed_length;
    uint32_t newLen = static_cast<uint32_t>(newData.size());
    int64_t diff = static_cast<int64_t>(newLen) - static_cast<int64_t>(oldLen);

    // Splice: [before entry] + [new data] + [after entry]
    std::vector<uint8_t> newBuf;
    newBuf.reserve(static_cast<size_t>(static_cast<int64_t>(fileSize) + diff));
    newBuf.insert(newBuf.end(), buf.begin(), buf.begin() + oldOff);
    newBuf.insert(newBuf.end(), newData.begin(), newData.end());
    if (oldOff + oldLen < fileSize)
        newBuf.insert(newBuf.end(), buf.begin() + oldOff + oldLen, buf.end());

    // Update in-memory entries
    target.packed_length = newLen;
    target.length = newLen;
    for (size_t i = 0; i < m_entries.size(); i++) {
        if (i != entryIndex && m_entries[i].offset > oldOff)
            m_entries[i].offset = static_cast<uint64_t>(static_cast<int64_t>(m_entries[i].offset) + diff);
    }

    // Patch TOC in buffer based on version
    auto writeU32 = [&](size_t pos, uint32_t val) {
        if (pos + 4 <= newBuf.size())
            std::memcpy(&newBuf[pos], &val, 4);
    };

    switch (m_version) {
    case ERFVersion::V1_0:
    case ERFVersion::V1_1: {
        uint32_t resOffset;
        std::memcpy(&resOffset, &newBuf[28], 4);
        for (size_t i = 0; i < m_entries.size(); i++) {
            writeU32(resOffset + i * 8,     static_cast<uint32_t>(m_entries[i].offset));
            writeU32(resOffset + i * 8 + 4, m_entries[i].packed_length);
        }
        break;
    }
    case ERFVersion::V2_0: {
        for (size_t i = 0; i < m_entries.size(); i++) {
            writeU32(32 + i * 72 + 64, static_cast<uint32_t>(m_entries[i].offset));
            writeU32(32 + i * 72 + 68, m_entries[i].packed_length);
        }
        break;
    }
    case ERFVersion::V2_2: {
        for (size_t i = 0; i < m_entries.size(); i++) {
            writeU32(56 + i * 76 + 64, static_cast<uint32_t>(m_entries[i].offset));
            writeU32(56 + i * 76 + 68, m_entries[i].packed_length);
            writeU32(56 + i * 76 + 72, m_entries[i].length);
        }
        break;
    }
    case ERFVersion::V3_0: {
        uint32_t stSize;
        std::memcpy(&stSize, &newBuf[16], 4);
        size_t tocStart = 48 + stSize;
        for (size_t i = 0; i < m_entries.size(); i++) {
            writeU32(tocStart + i * 28 + 16, static_cast<uint32_t>(m_entries[i].offset));
            writeU32(tocStart + i * 28 + 20, m_entries[i].packed_length);
            writeU32(tocStart + i * 28 + 24, m_entries[i].length);
        }
        break;
    }
    default:
        return false;
    }

    // Write back and reopen
    m_file.close();
    {
        std::ofstream out(m_path, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(reinterpret_cast<const char*>(newBuf.data()), newBuf.size());
        if (!out.good()) return false;
    }
    m_file.open(m_path, std::ios::binary);
    return m_file.is_open();
}

std::vector<std::string> scanForERFFiles(const std::string& rootPath) {
    std::vector<std::string> result;

    try {
        for (const auto& entry : fs::recursive_directory_iterator(rootPath, fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".erf" || ext == ".mod" || ext == ".sav" || ext == ".hak") {
                    result.push_back(entry.path().string());
                }
            }
        }
    } catch (...) {}

    std::sort(result.begin(), result.end());
    return result;
}