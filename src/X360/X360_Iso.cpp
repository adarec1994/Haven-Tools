#include "X360_Iso.h"

#include <algorithm>
#include <cstring>

namespace X360 {

namespace {
    constexpr uint64_t XGD1_OFFSET     = 0x18300000ull;  // original Xbox
    constexpr uint64_t XGD2_OFFSET     = 0x0FD90000ull;  // XGD2 / "Global"
    constexpr uint64_t XGD3_OFFSET     = 0x02080000ull;  // XGD3
    constexpr uint64_t HEADER_OFFSET   = 0x10000ull;     // volume descriptor
    constexpr uint32_t SECTOR_SIZE     = 2048;
    constexpr uint64_t FILETIME_SIZE   = 8;
    constexpr uint64_t UNUSED_SIZE     = 0x7c8;
    constexpr uint8_t  ATTR_DIR        = 0x10;
    constexpr uint8_t  ATTR_NOR        = 0x80;
    static constexpr char SIGNATURE[20] = {
        'M','I','C','R','O','S','O','F','T','*',
        'X','B','O','X','*','M','E','D','I','A'
    };

    Iso* g_currentIso = nullptr;

    inline uint16_t rd16(const uint8_t* p) {
        return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    }
    inline uint32_t rd32(const uint8_t* p) {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
               ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }

    std::string toLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        return s;
    }
}

Iso* Iso::getCurrent() { return g_currentIso; }

Iso::Iso() : m_baseOffset(0) {}

Iso::~Iso() { close(); }

void Iso::close() {
    if (g_currentIso == this) g_currentIso = nullptr;
    if (m_file.is_open()) m_file.close();
    m_path.clear();
    m_baseOffset = 0;
    m_files.clear();
}

bool Iso::open(const std::string& isoPath) {
    close();

    m_file.open(isoPath, std::ios::binary);
    if (!m_file.is_open()) return false;
    m_path = isoPath;

    if (!detectLayout()) {
        close();
        return false;
    }

    uint32_t rootSector = 0, rootSize = 0;
    if (!parseVolumeDescriptor(rootSector, rootSize)) {
        close();
        return false;
    }

    if (rootSector == 0 || rootSize == 0) {
        // Empty image. Not strictly an error — successful open with zero files.
        g_currentIso = this;
        return true;
    }

    walkDirectory(rootSector, rootSize, std::string());

    g_currentIso = this;
    return true;
}

bool Iso::detectLayout() {
    static const uint64_t candidates[] = { 0ull, XGD3_OFFSET, XGD2_OFFSET, XGD1_OFFSET };
    char buf[20];
    for (uint64_t base : candidates) {
        m_file.clear();
        m_file.seekg((std::streamoff)(base + HEADER_OFFSET), std::ios::beg);
        if (!m_file.good()) continue;
        m_file.read(buf, 20);
        if (m_file.gcount() != 20) continue;
        if (std::memcmp(buf, SIGNATURE, 20) == 0) {
            m_baseOffset = base;
            return true;
        }
    }
    return false;
}

bool Iso::parseVolumeDescriptor(uint32_t& outRootSector, uint32_t& outRootSize) {
    // Volume descriptor layout (after the 20-byte leading signature, which
    // detectLayout() has already verified):
    //   uint32 root_dir_sector
    //   uint32 root_dir_size  (bytes)
    //   uint64 filetime
    //   bytes  unused[0x7c8]
    //   bytes  signature[20]   (must match again)
    m_file.clear();
    m_file.seekg((std::streamoff)(m_baseOffset + HEADER_OFFSET + 20), std::ios::beg);
    if (!m_file.good()) return false;

    uint8_t hdr[8];
    m_file.read(reinterpret_cast<char*>(hdr), 8);
    if (m_file.gcount() != 8) return false;
    outRootSector = rd32(hdr);
    outRootSize   = rd32(hdr + 4);

    m_file.seekg((std::streamoff)(FILETIME_SIZE + UNUSED_SIZE), std::ios::cur);
    if (!m_file.good()) return false;

    char tail[20];
    m_file.read(tail, 20);
    if (m_file.gcount() != 20) return false;
    if (std::memcmp(tail, SIGNATURE, 20) != 0) return false;

    return true;
}

void Iso::walkDirectory(uint32_t startSector, uint32_t dirSize,
                        const std::string& parentPath) {
    if (dirSize == 0) return;

    // Pull the entire directory region into memory. Directory regions are
    // bounded (KB-scale even for large discs) so a single allocation is fine.
    std::vector<uint8_t> data(dirSize);
    m_file.clear();
    m_file.seekg((std::streamoff)(m_baseOffset + (uint64_t)startSector * SECTOR_SIZE),
                 std::ios::beg);
    if (!m_file.good()) return;
    m_file.read(reinterpret_cast<char*>(data.data()), dirSize);
    if ((uint32_t)m_file.gcount() != dirSize) return;

    // Linear walk. Entries are 4-byte-aligned within the directory; trailing
    // padding bytes are 0xff. Some images set the first 16 bits of an unused
    // entry slot to 0xffff as an end marker — treat that as "skip to next
    // sector boundary" rather than abort, in case there are more entries
    // packed after.
    uint32_t off = 0;
    while (off + 14 <= dirSize) {
        // Skip 0xff padding bytes between entries.
        while (off + 14 <= dirSize && data[off] == 0xff && data[off + 1] == 0xff) {
            // Advance to the next sector boundary; entries don't straddle them.
            uint32_t next = (off + SECTOR_SIZE) & ~(SECTOR_SIZE - 1);
            if (next <= off) { off = dirSize; break; }
            off = next;
        }
        if (off + 14 > dirSize) break;

        uint16_t leftPtr   = rd16(data.data() + off + 0);
        // uint16_t rightPtr  = rd16(data.data() + off + 2);  // tree hint, unused for linear walk
        uint32_t sector    = rd32(data.data() + off + 4);
        uint32_t fileSize  = rd32(data.data() + off + 8);
        uint8_t  attrs     = data[off + 12];
        uint8_t  nameLen   = data[off + 13];

        // 0xffff in either subtree pointer marks an unused slot: skip to
        // next sector boundary and keep scanning.
        if (leftPtr == 0xffff || nameLen == 0 || off + 14u + nameLen > dirSize) {
            uint32_t next = (off + SECTOR_SIZE) & ~(SECTOR_SIZE - 1);
            if (next <= off) break;
            off = next;
            continue;
        }

        std::string name(reinterpret_cast<const char*>(data.data() + off + 14), nameLen);
        std::string fullPath = parentPath.empty() ? name : (parentPath + "/" + name);

        if (attrs & ATTR_DIR) {
            walkDirectory(sector, fileSize, fullPath);
            // walkDirectory leaves the file position somewhere inside the
            // recursed directory; nothing else relies on it here, but reset
            // anyway so future calls (e.g. readFile) don't trip on it.
            m_file.clear();
        } else {
            FileEntry e;
            e.path = fullPath;
            e.sector = sector;
            e.size = fileSize;
            m_files.push_back(std::move(e));
        }

        // Advance past this entry, padded to a 4-byte boundary.
        uint32_t entrySize = 14u + nameLen;
        uint32_t aligned = (entrySize + 3u) & ~3u;
        off += aligned;
    }
}

const Iso::FileEntry* Iso::find(const std::string& virtualPath) const {
    std::string needle = toLower(virtualPath);
    // Normalise backslashes that callers may have introduced.
    std::replace(needle.begin(), needle.end(), '\\', '/');
    for (const auto& e : m_files) {
        if (toLower(e.path) == needle) return &e;
    }
    return nullptr;
}

std::vector<uint8_t> Iso::readFile(const FileEntry& entry) {
    std::vector<uint8_t> out;
    if (!m_file.is_open()) return out;
    out.resize(entry.size);
    m_file.clear();
    m_file.seekg((std::streamoff)(m_baseOffset + (uint64_t)entry.sector * SECTOR_SIZE),
                 std::ios::beg);
    if (!m_file.good()) { out.clear(); return out; }
    m_file.read(reinterpret_cast<char*>(out.data()), (std::streamsize)entry.size);
    if ((uint64_t)m_file.gcount() != entry.size) out.clear();
    return out;
}

std::vector<uint8_t> Iso::readFile(const std::string& virtualPath) {
    const FileEntry* e = find(virtualPath);
    if (!e) return {};
    return readFile(*e);
}

std::vector<std::string> Iso::listErfsAsVirtualPaths() const {
    std::vector<std::string> out;
    out.reserve(m_files.size());
    for (const auto& e : m_files) {
        if (e.path.size() < 4) continue;
        std::string ext = toLower(e.path.substr(e.path.size() - 4));
        if (ext == ".erf" || ext == ".lvl" || ext == ".rim" ||
            ext == ".arl" || ext == ".opf") {
            out.push_back("iso://" + e.path);
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace X360
