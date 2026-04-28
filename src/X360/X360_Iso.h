// Xbox 360 / original Xbox ISO (XGD / GDFX / XDVDFS) reader.
//
// Detects XGD1 / XGD2 / XGD3 / raw layouts by probing for the
// "MICROSOFT*XBOX*MEDIA" signature at file offset 0x10000 + base, with base
// drawn from {0, 0x18300000, 0x0FD90000, 0x02080000}.
//
// Exposes a flat list of files (directories are walked away) addressable by
// forward-slashed virtual path. The most recently opened instance registers
// itself as "current" so ERFFile::open() can transparently route paths
// beginning with "iso://" through the ISO instead of the OS filesystem.

#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace X360 {

class Iso {
public:
    struct FileEntry {
        std::string path;     // forward-slashed virtual path, e.g. "data/foo.erf"
        uint32_t sector;      // start sector inside the ISO (multiply by 2048, add base)
        uint64_t size;        // file size in bytes
    };

    Iso();
    ~Iso();

    Iso(const Iso&) = delete;
    Iso& operator=(const Iso&) = delete;

    bool open(const std::string& isoPath);
    void close();
    bool isOpen() const { return m_file.is_open(); }

    const std::string& path() const { return m_path; }
    const std::vector<FileEntry>& files() const { return m_files; }

    // Locate a file by virtual path. Comparison is case-insensitive (ISO
    // filenames are stored as ASCII but their case can vary across releases).
    const FileEntry* find(const std::string& virtualPath) const;

    // Read an entire file out of the ISO. Returns empty vector on failure.
    std::vector<uint8_t> readFile(const std::string& virtualPath);
    std::vector<uint8_t> readFile(const FileEntry& entry);

    // Return ERFs (.erf, .lvl) inside the ISO, formatted as "iso://<path>"
    // for handing to ERFFile::open() / scanForERFFiles consumers.
    std::vector<std::string> listErfsAsVirtualPaths() const;

    // Whichever Iso was opened most recently - for ERFFile::open() to find
    // when given an "iso://" path. Returns nullptr when no ISO is open.
    static Iso* getCurrent();

private:
    bool detectLayout();
    bool parseVolumeDescriptor(uint32_t& outRootSector, uint32_t& outRootSize);
    void walkDirectory(uint32_t startSector, uint32_t dirSize, const std::string& parentPath);

    std::string m_path;
    std::ifstream m_file;
    uint64_t m_baseOffset;       // adds onto every read; identifies XGD layout
    std::vector<FileEntry> m_files;
};

} // namespace X360
