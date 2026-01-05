#include "import.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <cerrno>
#include <chrono>
#include <ctime>
#include <map>
#include <set>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

namespace fs = std::filesystem;

// ============================================================================
// GFF Builder - Exact format matching Dragon Age Origins
//
// Key insight: For struct lists, child struct DATA is stored inline in the data block
// at offsets calculated from the struct list reference. Each child struct's field
// dataOffset values are RELATIVE to that child's position in the inline data.
// ============================================================================
class GFFBuilder {
public:
    struct Field {
        uint32_t label;
        uint16_t typeId;
        uint16_t flags;
        uint32_t dataOrOffset;
    };

    struct Struct {
        char type[4];
        std::vector<Field> fields;
        uint32_t structSize;  // = fieldCount * 12 for struct list navigation
    };

    GFFBuilder(const char* fileType) {
        memset(m_fileType, ' ', 4);
        memcpy(m_fileType, fileType, std::min((size_t)4, strlen(fileType)));
    }

    uint32_t AddStruct(const char* type) {
        Struct s;
        memset(s.type, ' ', 4);
        memcpy(s.type, type, std::min((size_t)4, strlen(type)));
        s.structSize = 0;
        m_structs.push_back(s);
        return (uint32_t)m_structs.size() - 1;
    }

    // For root struct fields - absolute offset into data block
    void AddUInt32Field(uint32_t structIdx, uint32_t label, uint32_t value) {
        m_queuedUInt32s.push_back({structIdx, (uint32_t)m_structs[structIdx].fields.size(), value});
        Field f;
        f.label = label;
        f.typeId = 4;  // UINT32
        f.flags = 0;
        f.dataOrOffset = 0;
        m_structs[structIdx].fields.push_back(f);
    }

    void AddStringField(uint32_t structIdx, uint32_t label, const std::string& str) {
        m_queuedStrings.push_back({structIdx, (uint32_t)m_structs[structIdx].fields.size(), str});
        Field f;
        f.label = label;
        f.typeId = 14;  // ECString
        f.flags = 0;
        f.dataOrOffset = 0;
        m_structs[structIdx].fields.push_back(f);
    }

    void AddStructListField(uint32_t structIdx, uint32_t label, uint32_t childStructType, const std::vector<uint32_t>& childIndices) {
        m_queuedStructLists.push_back({structIdx, (uint32_t)m_structs[structIdx].fields.size(), childStructType, childIndices});
        Field f;
        f.label = label;
        f.typeId = (uint16_t)childStructType;
        f.flags = 0x8000 | 0x4000;  // List | Struct
        f.dataOrOffset = 0;
        m_structs[structIdx].fields.push_back(f);
    }

    void AddBinaryField(uint32_t structIdx, uint32_t label, const std::vector<uint8_t>& data) {
        m_queuedBinary.push_back({structIdx, (uint32_t)m_structs[structIdx].fields.size(), data});
        Field f;
        f.label = label;
        f.typeId = 0;  // UINT8
        f.flags = 0x8000;  // List
        f.dataOrOffset = 0;
        m_structs[structIdx].fields.push_back(f);
    }

    std::vector<uint8_t> Build() {
        std::cout << "[GFF] Building GFF file..." << std::endl;
        std::cout << "[GFF] FileType: " << std::string(m_fileType, 4) << std::endl;
        std::cout << "[GFF] Struct count: " << m_structs.size() << std::endl;

        // Calculate struct sizes (fieldCount * 12 for inline data layout)
        for (size_t i = 0; i < m_structs.size(); i++) {
            // For struct lists, structSize determines how much inline data each child has
            // We use 4 bytes per field for simple types
            uint32_t dataSize = 0;
            for (const auto& f : m_structs[i].fields) {
                dataSize += 4;  // Each field contributes 4 bytes of inline data
            }
            m_structs[i].structSize = dataSize;
            std::cout << "[GFF] Struct " << i << " type=" << std::string(m_structs[i].type, 4)
                      << " fields=" << m_structs[i].fields.size()
                      << " structSize=" << m_structs[i].structSize << std::endl;
        }

        // Data block
        std::vector<uint8_t> dataBlock;

        auto appendToData = [&](const void* d, size_t len) -> uint32_t {
            uint32_t off = (uint32_t)dataBlock.size();
            const uint8_t* p = (const uint8_t*)d;
            dataBlock.insert(dataBlock.end(), p, p + len);
            return off;
        };

        auto writeU32ToData = [&](uint32_t v) -> uint32_t {
            return appendToData(&v, 4);
        };

        // Process strings first - store actual string data
        // stringDataOffsets = where the string content is stored (length + UTF16 chars)
        std::map<std::pair<uint32_t,uint32_t>, uint32_t> stringDataOffsets;
        for (auto& q : m_queuedStrings) {
            uint32_t strLen = (uint32_t)q.str.length();
            uint32_t off = appendToData(&strLen, 4);
            for (char c : q.str) {
                uint8_t bytes[2] = {(uint8_t)c, 0};
                dataBlock.insert(dataBlock.end(), bytes, bytes + 2);
            }
            stringDataOffsets[{q.structIdx, q.fieldIdx}] = off;
            std::cout << "[GFF] String data stored at " << off << ": \"" << q.str << "\"" << std::endl;
        }

        // Now create string references for root struct strings
        // The field's dataOffset points to where the reference is stored
        // The reference itself is the offset to the string data
        std::map<std::pair<uint32_t,uint32_t>, uint32_t> stringRefOffsets;
        for (auto& q : m_queuedStrings) {
            // Check if this is a root struct string (not in a struct list)
            bool isChildStruct = false;
            for (const auto& sl : m_queuedStructLists) {
                for (uint32_t idx : sl.childIndices) {
                    if (idx == q.structIdx) {
                        isChildStruct = true;
                        break;
                    }
                }
                if (isChildStruct) break;
            }

            if (!isChildStruct) {
                // Root struct string: write the reference (offset to string data)
                uint32_t strDataOff = stringDataOffsets[{q.structIdx, q.fieldIdx}];
                uint32_t refOff = writeU32ToData(strDataOff);
                stringRefOffsets[{q.structIdx, q.fieldIdx}] = refOff;
                std::cout << "[GFF] String ref at " << refOff << " -> data at " << strDataOff << std::endl;
            }
        }

        // Process binary data (they're referenced by offset)
        std::map<std::pair<uint32_t,uint32_t>, uint32_t> binaryOffsets;
        for (auto& q : m_queuedBinary) {
            uint32_t count = (uint32_t)q.data.size();
            uint32_t off = appendToData(&count, 4);
            appendToData(q.data.data(), q.data.size());
            binaryOffsets[{q.structIdx, q.fieldIdx}] = off;
            std::cout << "[GFF] Binary stored at " << off << ", size=" << count << std::endl;
        }

        // Process UINT32s for root structs (absolute offsets)
        std::map<std::pair<uint32_t,uint32_t>, uint32_t> uint32Offsets;
        for (auto& q : m_queuedUInt32s) {
            // Check if this struct is used in a struct list (has a parent)
            bool isChildStruct = false;
            for (const auto& sl : m_queuedStructLists) {
                for (uint32_t idx : sl.childIndices) {
                    if (idx == q.structIdx) {
                        isChildStruct = true;
                        break;
                    }
                }
                if (isChildStruct) break;
            }

            if (!isChildStruct) {
                // Root struct - store in data block with absolute offset
                uint32_t off = writeU32ToData(q.value);
                uint32Offsets[{q.structIdx, q.fieldIdx}] = off;
            }
        }

        // Process struct lists - need to handle nesting
        // First, identify which struct lists are nested (parent is a child of another struct list)
        std::set<std::pair<uint32_t,uint32_t>> nestedStructLists;
        std::map<std::pair<uint32_t,uint32_t>, uint32_t> structListOffsets;

        for (const auto& q : m_queuedStructLists) {
            // Check if this struct list's parent struct is a child of another struct list
            for (const auto& parentSL : m_queuedStructLists) {
                for (uint32_t idx : parentSL.childIndices) {
                    if (idx == q.structIdx) {
                        nestedStructLists.insert({q.structIdx, q.fieldIdx});
                        break;
                    }
                }
            }
        }

        // Process nested struct lists FIRST - they need to be written before their parents
        for (auto& q : m_queuedStructLists) {
            auto key = std::make_pair(q.structIdx, q.fieldIdx);
            if (nestedStructLists.count(key)) {
                // This is a nested struct list - write its data now
                uint32_t count = (uint32_t)q.childIndices.size();
                uint32_t listStart = writeU32ToData(count);

                std::cout << "[GFF] Nested StructList at " << listStart << " count=" << count << " (struct " << q.structIdx << " field " << q.fieldIdx << ")" << std::endl;

                // Write inline data for each child struct
                for (size_t i = 0; i < q.childIndices.size(); i++) {
                    uint32_t childIdx = q.childIndices[i];
                    const auto& childStruct = m_structs[childIdx];

                    std::cout << "[GFF]   Nested child " << i << " (struct " << childIdx << ") data at " << dataBlock.size() << std::endl;

                    // Write each field's data inline
                    for (size_t fi = 0; fi < childStruct.fields.size(); fi++) {
                        bool found = false;
                        for (const auto& uq : m_queuedUInt32s) {
                            if (uq.structIdx == childIdx && uq.fieldIdx == fi) {
                                writeU32ToData(uq.value);
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            writeU32ToData(0);
                        }
                    }
                }

                structListOffsets[key] = listStart;
            }
        }

        // Now process top-level struct lists
        for (auto& q : m_queuedStructLists) {
            auto key = std::make_pair(q.structIdx, q.fieldIdx);
            if (!nestedStructLists.count(key)) {
                // This is a top-level struct list
                uint32_t count = (uint32_t)q.childIndices.size();
                uint32_t listStart = writeU32ToData(count);

                std::cout << "[GFF] StructList at " << listStart << " count=" << count << std::endl;

                // Write inline data for each child struct
                for (size_t i = 0; i < q.childIndices.size(); i++) {
                    uint32_t childIdx = q.childIndices[i];
                    const auto& childStruct = m_structs[childIdx];

                    uint32_t childDataStart = (uint32_t)dataBlock.size();
                    std::cout << "[GFF]   Child " << i << " (struct " << childIdx << ") data at " << childDataStart << std::endl;

                    // Write each field's data inline
                    for (size_t fi = 0; fi < childStruct.fields.size(); fi++) {
                        const auto& field = childStruct.fields[fi];

                        // Check if this is a queued uint32
                        bool found = false;
                        for (const auto& uq : m_queuedUInt32s) {
                            if (uq.structIdx == childIdx && uq.fieldIdx == fi) {
                                writeU32ToData(uq.value);
                                found = true;
                                break;
                            }
                        }

                        // Check if this is a queued string (write offset to string DATA)
                        if (!found) {
                            for (const auto& sq : m_queuedStrings) {
                                if (sq.structIdx == childIdx && sq.fieldIdx == fi) {
                                    uint32_t strDataOff = stringDataOffsets[{childIdx, (uint32_t)fi}];
                                    writeU32ToData(strDataOff);  // Write reference to string data
                                    found = true;
                                    break;
                                }
                            }
                        }

                        // Check if nested struct list - write reference to pre-written list data
                        if (!found) {
                            for (const auto& slq : m_queuedStructLists) {
                                if (slq.structIdx == childIdx && slq.fieldIdx == fi) {
                                    auto nestedKey = std::make_pair(childIdx, (uint32_t)fi);
                                    if (structListOffsets.count(nestedKey)) {
                                        writeU32ToData(structListOffsets[nestedKey]);
                                        std::cout << "[GFF]     Nested list ref at field " << fi << " -> " << structListOffsets[nestedKey] << std::endl;
                                    } else {
                                        writeU32ToData(0);
                                    }
                                    found = true;
                                    break;
                                }
                            }
                        }

                        if (!found) {
                            // Default: write 0
                            writeU32ToData(0);
                        }
                    }
                }

                structListOffsets[key] = listStart;
            }
        }

        // Now set field dataOffset values
        // For root struct fields: absolute offset into data block
        // For child struct fields: relative offset within inline data (0, 4, 8, ...)
        for (size_t si = 0; si < m_structs.size(); si++) {
            bool isChildStruct = false;
            for (const auto& sl : m_queuedStructLists) {
                for (uint32_t idx : sl.childIndices) {
                    if (idx == si) {
                        isChildStruct = true;
                        break;
                    }
                }
                if (isChildStruct) break;
            }

            for (size_t fi = 0; fi < m_structs[si].fields.size(); fi++) {
                auto& field = m_structs[si].fields[fi];

                if (isChildStruct) {
                    // Child struct: relative offset (0, 4, 8, ...)
                    field.dataOrOffset = (uint32_t)(fi * 4);
                } else {
                    // Root struct: look up absolute offset
                    auto key = std::make_pair((uint32_t)si, (uint32_t)fi);

                    if (stringRefOffsets.count(key)) {
                        // String field: dataOffset points to where the reference is stored
                        field.dataOrOffset = stringRefOffsets[key];
                    } else if (binaryOffsets.count(key)) {
                        field.dataOrOffset = binaryOffsets[key];
                    } else if (uint32Offsets.count(key)) {
                        field.dataOrOffset = uint32Offsets[key];
                    } else if (structListOffsets.count(key)) {
                        field.dataOrOffset = structListOffsets[key];
                    }
                }
            }
        }

        // Build the final file
        std::vector<uint8_t> buffer;

        auto writeBytes = [&](const void* data, size_t len) {
            const uint8_t* p = (const uint8_t*)data;
            buffer.insert(buffer.end(), p, p + len);
        };
        auto writeU32 = [&](uint32_t v) { writeBytes(&v, 4); };
        auto writeU16 = [&](uint16_t v) { writeBytes(&v, 2); };

        // Header (28 bytes)
        writeBytes("GFF ", 4);
        writeBytes("V4.0", 4);
        writeBytes("PC  ", 4);
        writeBytes(m_fileType, 4);
        writeBytes("V1.0", 4);
        writeU32((uint32_t)m_structs.size());

        size_t dataOffsetPos = buffer.size();
        writeU32(0);  // Placeholder

        // Struct array (16 bytes each)
        std::vector<size_t> fieldOffsetPositions;
        for (size_t i = 0; i < m_structs.size(); i++) {
            const auto& s = m_structs[i];
            writeBytes(s.type, 4);
            writeU32((uint32_t)s.fields.size());
            fieldOffsetPositions.push_back(buffer.size());
            writeU32(0);  // FieldOffset placeholder
            writeU32(s.structSize);
        }

        // Field array (12 bytes each)
        for (size_t i = 0; i < m_structs.size(); i++) {
            uint32_t fieldOffset = (uint32_t)buffer.size();
            memcpy(&buffer[fieldOffsetPositions[i]], &fieldOffset, 4);

            std::cout << "[GFF] Struct " << i << " fields at offset " << fieldOffset << std::endl;

            for (size_t j = 0; j < m_structs[i].fields.size(); j++) {
                const auto& f = m_structs[i].fields[j];
                writeU32(f.label);
                writeU16(f.typeId);
                writeU16(f.flags);
                writeU32(f.dataOrOffset);

                std::cout << "[GFF]   Field " << j << ": label=" << f.label
                          << " type=" << f.typeId << " flags=0x" << std::hex << f.flags << std::dec
                          << " dataOffset=" << f.dataOrOffset << std::endl;
            }
        }

        // Update DataOffset in header
        uint32_t dataOffset = (uint32_t)buffer.size();
        memcpy(&buffer[dataOffsetPos], &dataOffset, 4);

        std::cout << "[GFF] DataOffset=" << dataOffset << std::endl;

        // Append data block
        buffer.insert(buffer.end(), dataBlock.begin(), dataBlock.end());

        std::cout << "[GFF] Final size=" << buffer.size() << " (data block=" << dataBlock.size() << ")" << std::endl;

        return buffer;
    }

private:
    char m_fileType[4];
    struct QueuedString { uint32_t structIdx; uint32_t fieldIdx; std::string str; };
    struct QueuedStructList { uint32_t structIdx; uint32_t fieldIdx; uint32_t childType; std::vector<uint32_t> childIndices; };
    struct QueuedBinary { uint32_t structIdx; uint32_t fieldIdx; std::vector<uint8_t> data; };
    struct QueuedUInt32 { uint32_t structIdx; uint32_t fieldIdx; uint32_t value; };
    std::vector<Struct> m_structs;
    std::vector<QueuedString> m_queuedStrings;
    std::vector<QueuedStructList> m_queuedStructLists;
    std::vector<QueuedBinary> m_queuedBinary;
    std::vector<QueuedUInt32> m_queuedUInt32s;
};

// ============================================================================
// DAOImporter Implementation
// ============================================================================

DAOImporter::DAOImporter() : m_backupCallback(nullptr), m_progressCallback(nullptr) {}
DAOImporter::~DAOImporter() {}

bool DAOImporter::BackupExists(const std::string& erfPath) {
    fs::path backupDir = fs::current_path() / "backups";
    std::string erfName = fs::path(erfPath).filename().string();
    return fs::exists(backupDir / (erfName + ".bak"));
}

std::string DAOImporter::GetBackupDir() {
    return (fs::current_path() / "backups").string();
}

static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static std::string CleanName(const std::string& input) {
    std::string result = input;
    size_t lastSlash = result.find_last_of("/\\");
    if (lastSlash != std::string::npos) result = result.substr(lastSlash + 1);
    size_t lastDot = result.find_last_of('.');
    if (lastDot != std::string::npos) result = result.substr(0, lastDot);
    return ToLower(result);
}

static std::string FindErfPath(const fs::path& root, const std::string& filename) {
    if (fs::exists(root / filename)) return (root / filename).string();
    try {
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (entry.is_regular_file() && ToLower(entry.path().filename().string()) == ToLower(filename)) {
                return entry.path().string();
            }
        }
    } catch (...) {}
    return "";
}

static std::vector<uint8_t> ConvertToDDS(const std::vector<uint8_t>& imageData, int width, int height, int channels) {
    std::vector<uint8_t> dds;

    auto writeU32 = [&](uint32_t v) {
        dds.push_back(v & 0xFF);
        dds.push_back((v >> 8) & 0xFF);
        dds.push_back((v >> 16) & 0xFF);
        dds.push_back((v >> 24) & 0xFF);
    };

    // DDS Magic
    dds.push_back('D'); dds.push_back('D'); dds.push_back('S'); dds.push_back(' ');

    // DDS Header
    writeU32(124);              // Header size
    writeU32(0x1 | 0x2 | 0x4 | 0x1000); // Flags
    writeU32(height);
    writeU32(width);
    writeU32(width * 4);        // Pitch
    writeU32(0);                // Depth
    writeU32(1);                // MipMapCount
    for (int i = 0; i < 11; ++i) writeU32(0); // Reserved

    // Pixel format
    writeU32(32);               // Size
    writeU32(0x41);             // Flags (DDPF_RGB | DDPF_ALPHAPIXELS)
    writeU32(0);                // FourCC
    writeU32(32);               // RGBBitCount
    writeU32(0x00FF0000);       // RBitMask
    writeU32(0x0000FF00);       // GBitMask
    writeU32(0x000000FF);       // BBitMask
    writeU32(0xFF000000);       // ABitMask

    writeU32(0x1000);           // Caps
    for (int i = 0; i < 4; ++i) writeU32(0); // Caps2-4

    // Pixel data (BGRA)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int srcIdx = (y * width + x) * channels;
            uint8_t r = imageData[srcIdx];
            uint8_t g = (channels > 1) ? imageData[srcIdx + 1] : r;
            uint8_t b = (channels > 2) ? imageData[srcIdx + 2] : r;
            uint8_t a = (channels > 3) ? imageData[srcIdx + 3] : 255;
            dds.push_back(b);
            dds.push_back(g);
            dds.push_back(r);
            dds.push_back(a);
        }
    }

    return dds;
}

void DAOImporter::ReportProgress(float progress, const std::string& status) {
    if (m_progressCallback) {
        m_progressCallback(progress, status);
    }
}

bool DAOImporter::ImportToDirectory(const std::string& glbPath, const std::string& targetDir) {
    ReportProgress(0.0f, "Loading GLB...");
    std::cout << "[Import] Processing: " << fs::path(glbPath).filename().string() << std::endl;

    DAOModelData modelData;
    if (!LoadGLB(glbPath, modelData)) return false;

    ReportProgress(0.1f, "Locating ERF files...");

    const fs::path baseDir(targetDir);
    fs::path corePath = baseDir / "packages" / "core" / "data";
    fs::path texturePath = baseDir / "packages" / "core" / "textures" / "high";

    auto resolvePath = [&](const std::string& filename) -> std::string {
        if (fs::exists(corePath / filename)) return (corePath / filename).string();
        return FindErfPath(baseDir, filename);
    };

    std::string meshErf = resolvePath("modelmeshdata.erf");
    std::string hierErf = resolvePath("modelhierarchies.erf");
    std::string matErf  = resolvePath("materialobjects.erf");
    std::string texErf;
    if (fs::exists(texturePath / "texturepack.erf")) {
        texErf = (texturePath / "texturepack.erf").string();
    } else {
        texErf = FindErfPath(baseDir, "texturepack.erf");
    }

    if (meshErf.empty() || hierErf.empty() || matErf.empty()) {
        std::cerr << "[Import] Error: Required ERFs not found." << std::endl;
        return false;
    }

    std::cout << "[Import] ERF locations:" << std::endl;
    std::cout << "  mesh: " << meshErf << std::endl;
    std::cout << "  hier: " << hierErf << std::endl;
    std::cout << "  mat:  " << matErf << std::endl;
    std::cout << "  tex:  " << (texErf.empty() ? "NOT FOUND" : texErf) << std::endl;

    ReportProgress(0.2f, "Generating MSH...");
    std::string baseName = ToLower(fs::path(glbPath).stem().string());
    std::map<std::string, std::vector<uint8_t>> meshFiles, hierFiles, matFiles, texFiles;

    std::string mshFile = baseName + ".msh";
    meshFiles[mshFile] = GenerateMSH(modelData);
    std::cout << "  + Generated: " << mshFile << " (" << meshFiles[mshFile].size() << " bytes)" << std::endl;

    ReportProgress(0.3f, "Generating MMH...");
    std::string mmhFile = baseName + ".mmh";
    hierFiles[mmhFile] = GenerateMMH(modelData, mshFile);
    std::cout << "  + Generated: " << mmhFile << " (" << hierFiles[mmhFile].size() << " bytes)" << std::endl;

    ReportProgress(0.4f, "Converting textures...");
    for (const auto& tex : modelData.textures) {
        if (tex.width > 0 && tex.height > 0 && !tex.data.empty() && !tex.ddsName.empty()) {
            std::vector<uint8_t> ddsData = ConvertToDDS(tex.data, tex.width, tex.height, tex.channels);
            texFiles[tex.ddsName] = std::move(ddsData);
            std::cout << "  + Generated texture: " << tex.ddsName << " (" << tex.width << "x" << tex.height << ")" << std::endl;
        }
    }

    ReportProgress(0.5f, "Generating MAO files...");
    for (const auto& mat : modelData.materials) {
        std::string maoFile = mat.name + ".mao";
        std::string xml = GenerateMAO(mat.name, mat.diffuseMap, mat.normalMap, mat.specularMap);
        matFiles[maoFile].assign(xml.begin(), xml.end());
        std::cout << "  + Generated: " << maoFile << std::endl;
    }

    std::cout << "\n[Import] Updating ERF files..." << std::endl;

    ReportProgress(0.6f, "Updating modelmeshdata.erf...");
    bool ok1 = RepackERF(meshErf, meshFiles);

    ReportProgress(0.7f, "Updating modelhierarchies.erf...");
    bool ok2 = RepackERF(hierErf, hierFiles);

    ReportProgress(0.8f, "Updating materialobjects.erf...");
    bool ok3 = RepackERF(matErf, matFiles);

    bool ok4 = true;
    if (!texErf.empty() && !texFiles.empty()) {
        ReportProgress(0.9f, "Updating texturepack.erf...");
        ok4 = RepackERF(texErf, texFiles);
    }

    ReportProgress(1.0f, ok1 && ok2 && ok3 && ok4 ? "Import complete!" : "Import failed!");
    std::cout << "\n[Import] " << (ok1 && ok2 && ok3 && ok4 ? "SUCCESS" : "FAILED") << std::endl;
    return ok1 && ok2 && ok3 && ok4;
}

bool DAOImporter::LoadGLB(const std::string& path, DAOModelData& outData) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    if (!loader.LoadBinaryFromFile(&model, &err, &warn, path)) {
        std::cerr << "GLB Load Error: " << err << std::endl;
        return false;
    }
    outData.name = ToLower(fs::path(path).stem().string());

    // Extract images first
    for (size_t i = 0; i < model.images.size(); ++i) {
        const auto& img = model.images[i];
        if (img.width > 0 && img.height > 0 && !img.image.empty()) {
            DAOModelData::Texture tex;
            tex.originalName = img.uri.empty() ? (img.name.empty() ? "texture_" + std::to_string(i) : img.name) : img.uri;
            tex.ddsName = ""; // Will be set by material
            tex.width = img.width;
            tex.height = img.height;
            tex.channels = img.component;
            tex.data = img.image;
            outData.textures.push_back(tex);
        }
    }

    auto getTextureImageIndex = [&](int textureIndex) -> int {
        if (textureIndex < 0 || textureIndex >= (int)model.textures.size()) return -1;
        return model.textures[textureIndex].source;
    };

    // Process materials
    for (size_t i = 0; i < model.materials.size(); ++i) {
        const auto& gltfMat = model.materials[i];
        DAOModelData::Material mat;
        mat.name = CleanName(gltfMat.name.empty() ? "material_" + std::to_string(i) : gltfMat.name);

        mat.diffuseMap = mat.name + "_d.dds";
        mat.normalMap = mat.name + "_n.dds";
        mat.specularMap = mat.name + "_spec.dds";

        int diffuseIdx = getTextureImageIndex(gltfMat.pbrMetallicRoughness.baseColorTexture.index);
        int normalIdx = getTextureImageIndex(gltfMat.normalTexture.index);
        int specIdx = getTextureImageIndex(gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index);
        if (specIdx < 0) specIdx = getTextureImageIndex(gltfMat.occlusionTexture.index);

        if (diffuseIdx >= 0 && diffuseIdx < (int)outData.textures.size()) {
            outData.textures[diffuseIdx].ddsName = mat.diffuseMap;
        } else {
            mat.diffuseMap = "default_d.dds";
        }
        if (normalIdx >= 0 && normalIdx < (int)outData.textures.size()) {
            outData.textures[normalIdx].ddsName = mat.normalMap;
        } else {
            mat.normalMap = "default_n.dds";
        }
        if (specIdx >= 0 && specIdx < (int)outData.textures.size()) {
            outData.textures[specIdx].ddsName = mat.specularMap;
        } else {
            mat.specularMap = "default_spec.dds";
        }

        outData.materials.push_back(mat);
    }

    if (outData.materials.empty()) {
        DAOModelData::Material defaultMat;
        defaultMat.name = outData.name;
        defaultMat.diffuseMap = "default_d.dds";
        defaultMat.normalMap = "default_n.dds";
        defaultMat.specularMap = "default_spec.dds";
        outData.materials.push_back(defaultMat);
    }

    // Process meshes
    for (const auto& mesh : model.meshes) {
        for (const auto& primitive : mesh.primitives) {
            DAOModelData::MeshPart part;

            if (primitive.material >= 0 && primitive.material < (int)outData.materials.size()) {
                part.materialName = outData.materials[primitive.material].name;
            } else if (!outData.materials.empty()) {
                part.materialName = outData.materials[0].name;
            }

            if (primitive.attributes.count("POSITION")) {
                const auto& acc = model.accessors[primitive.attributes.at("POSITION")];
                const auto& bv = model.bufferViews[acc.bufferView];
                const auto& buf = model.buffers[bv.buffer];
                const float* p = (const float*)&buf.data[bv.byteOffset + acc.byteOffset];
                for (size_t i = 0; i < acc.count; ++i) {
                    DAOModelData::Vertex v = {};
                    v.x = p[i*3]; v.y = p[i*3+1]; v.z = p[i*3+2];
                    part.vertices.push_back(v);
                }
            }
            if (primitive.attributes.count("NORMAL")) {
                const auto& acc = model.accessors[primitive.attributes.at("NORMAL")];
                const auto& bv = model.bufferViews[acc.bufferView];
                const auto& buf = model.buffers[bv.buffer];
                const float* n = (const float*)&buf.data[bv.byteOffset + acc.byteOffset];
                for (size_t i = 0; i < part.vertices.size(); ++i) {
                    part.vertices[i].nx = n[i*3]; part.vertices[i].ny = n[i*3+1]; part.vertices[i].nz = n[i*3+2];
                }
            }
            if (primitive.attributes.count("TEXCOORD_0")) {
                const auto& acc = model.accessors[primitive.attributes.at("TEXCOORD_0")];
                const auto& bv = model.bufferViews[acc.bufferView];
                const auto& buf = model.buffers[bv.buffer];
                const float* uv = (const float*)&buf.data[bv.byteOffset + acc.byteOffset];
                for (size_t i = 0; i < part.vertices.size(); ++i) {
                    part.vertices[i].u = uv[i*2]; part.vertices[i].v = uv[i*2+1];
                }
            }
            if (primitive.indices >= 0) {
                const auto& acc = model.accessors[primitive.indices];
                const auto& bv = model.bufferViews[acc.bufferView];
                const auto& buf = model.buffers[bv.buffer];
                if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    const uint16_t* idx = (const uint16_t*)&buf.data[bv.byteOffset + acc.byteOffset];
                    for(size_t k=0; k<acc.count; ++k) part.indices.push_back(idx[k]);
                } else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    const uint32_t* idx = (const uint32_t*)&buf.data[bv.byteOffset + acc.byteOffset];
                    for(size_t k=0; k<acc.count; ++k) part.indices.push_back((uint16_t)idx[k]);
                } else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    const uint8_t* idx = (const uint8_t*)&buf.data[bv.byteOffset + acc.byteOffset];
                    for(size_t k=0; k<acc.count; ++k) part.indices.push_back((uint16_t)idx[k]);
                }
            }
            outData.parts.push_back(part);
        }
    }

    return true;
}

std::vector<uint8_t> DAOImporter::GenerateMMH(const DAOModelData& model, const std::string& mshFilename) {
    GFFBuilder gff("MMH ");

    // Struct 0: Root "MMH "
    uint32_t root = gff.AddStruct("MMH ");

    // Struct 1: Mesh header "mshh"
    uint32_t mshh = gff.AddStruct("mshh");

    // Root fields
    gff.AddStringField(root, 6000, model.name);  // Name

    std::string mshBase = mshFilename;
    if (mshBase.find('.') != std::string::npos) mshBase = mshBase.substr(0, mshBase.find_last_of('.'));
    gff.AddStringField(root, 6005, mshBase);  // ModelHierarchyModelDataName

    // Mshh fields
    gff.AddStringField(mshh, 6000, model.name);  // Name
    gff.AddStringField(mshh, 6006, model.name);  // MeshGroupName

    std::string mat = model.name;
    if (!model.parts.empty() && !model.parts[0].materialName.empty()) {
        mat = model.parts[0].materialName;
    } else if (!model.materials.empty()) {
        mat = model.materials[0].name;
    }

    gff.AddStringField(mshh, 6001, mat);  // DefaultMaterialObject

    // Add mesh header list to root (struct index 1)
    gff.AddStructListField(root, 6999, 1, {mshh});

    return gff.Build();
}

std::vector<uint8_t> DAOImporter::GenerateMSH(const DAOModelData& model) {
    std::cout << "[MSH] Generating MSH for: " << model.name << std::endl;

    GFFBuilder gff("MSH ");

    // Collect all vertex and index data
    std::vector<float> vBuf;
    std::vector<uint16_t> iBuf;
    uint32_t idxOff = 0;

    for (const auto& part : model.parts) {
        for (const auto& v : part.vertices) {
            vBuf.push_back(v.x); vBuf.push_back(v.y); vBuf.push_back(v.z);
            vBuf.push_back(v.nx); vBuf.push_back(v.ny); vBuf.push_back(v.nz);
            vBuf.push_back(v.u); vBuf.push_back(v.v);
        }
        for (auto i : part.indices) iBuf.push_back(i + (uint16_t)idxOff);
        idxOff += (uint32_t)part.vertices.size();
    }

    uint32_t vCount = (uint32_t)(vBuf.size() / 8);
    uint32_t iCount = (uint32_t)iBuf.size();

    std::cout << "[MSH] Vertex count: " << vCount << std::endl;
    std::cout << "[MSH] Index count: " << iCount << std::endl;

    // Convert to byte arrays
    std::vector<uint8_t> rawV(vBuf.size() * 4);
    memcpy(rawV.data(), vBuf.data(), rawV.size());

    std::vector<uint8_t> rawI(iBuf.size() * 2);
    memcpy(rawI.data(), iBuf.data(), rawI.size());

    std::cout << "[MSH] Vertex data size: " << rawV.size() << " bytes" << std::endl;
    std::cout << "[MSH] Index data size: " << rawI.size() << " bytes" << std::endl;

    // Struct 0: Root "MESH"
    uint32_t root = gff.AddStruct("MESH");

    // Struct 1: Mesh chunk "msgr"
    uint32_t chunk = gff.AddStruct("msgr");

    // Structs 2,3,4: Vertex declarators "decl"
    uint32_t d1 = gff.AddStruct("decl");
    uint32_t d2 = gff.AddStruct("decl");
    uint32_t d3 = gff.AddStruct("decl");

    // === Chunk fields ===
    gff.AddStringField(chunk, 2, model.name);  // Name

    uint32_t vSize = 32;  // 8 floats * 4 bytes
    uint32_t pType = 4;   // Triangle list
    uint32_t iFormat = 0; // 16-bit indices
    uint32_t zero = 0;

    gff.AddUInt32Field(chunk, 8000, vSize);      // VertexSize
    gff.AddUInt32Field(chunk, 8001, vCount);     // VertexCount
    gff.AddUInt32Field(chunk, 8002, iCount);     // IndexCount
    gff.AddUInt32Field(chunk, 8003, pType);      // PrimitiveType
    gff.AddUInt32Field(chunk, 8004, iFormat);    // IndexFormat
    gff.AddUInt32Field(chunk, 8005, zero);       // BaseVertexIndex
    gff.AddUInt32Field(chunk, 8006, zero);       // VertexOffset
    gff.AddUInt32Field(chunk, 8007, zero);       // MinIndex
    gff.AddUInt32Field(chunk, 8008, vCount);     // ReferencedVerts
    gff.AddUInt32Field(chunk, 8009, zero);       // IndexOffset

    // === Declarator fields ===
    // Position declarator (d1 = struct index 2)
    gff.AddUInt32Field(d1, 8026, 0);     // Stream
    gff.AddUInt32Field(d1, 8027, 0);     // Offset
    gff.AddUInt32Field(d1, 8028, 2);     // DataType (FLOAT3)
    gff.AddUInt32Field(d1, 8029, 0);     // Usage (POSITION)
    gff.AddUInt32Field(d1, 8030, 0);     // UsageIndex
    gff.AddUInt32Field(d1, 8031, 0);     // Method

    // Normal declarator (d2 = struct index 3)
    gff.AddUInt32Field(d2, 8026, 0);     // Stream
    gff.AddUInt32Field(d2, 8027, 12);    // Offset
    gff.AddUInt32Field(d2, 8028, 2);     // DataType (FLOAT3)
    gff.AddUInt32Field(d2, 8029, 3);     // Usage (NORMAL)
    gff.AddUInt32Field(d2, 8030, 0);     // UsageIndex
    gff.AddUInt32Field(d2, 8031, 0);     // Method

    // TexCoord declarator (d3 = struct index 4)
    gff.AddUInt32Field(d3, 8026, 0);     // Stream
    gff.AddUInt32Field(d3, 8027, 24);    // Offset
    gff.AddUInt32Field(d3, 8028, 1);     // DataType (FLOAT2)
    gff.AddUInt32Field(d3, 8029, 5);     // Usage (TEXCOORD)
    gff.AddUInt32Field(d3, 8030, 0);     // UsageIndex
    gff.AddUInt32Field(d3, 8031, 0);     // Method

    // Add declarator list to chunk (struct indices 2,3,4)
    gff.AddStructListField(chunk, 8025, 2, {d1, d2, d3});

    // === Root fields ===
    // Add chunk list to root (struct index 1)
    gff.AddStructListField(root, 8021, 1, {chunk});

    // Add vertex and index data to root
    gff.AddBinaryField(root, 8022, rawV);
    gff.AddBinaryField(root, 8023, rawI);

    auto result = gff.Build();
    std::cout << "[MSH] Generated MSH size: " << result.size() << " bytes" << std::endl;

    // Debug header
    std::cout << "[MSH] Header: ";
    for (size_t i = 0; i < std::min((size_t)32, result.size()); ++i) {
        printf("%02X ", result[i]);
    }
    std::cout << std::endl;

    return result;
}

std::string DAOImporter::GenerateMAO(const std::string& matName, const std::string& diffuse, const std::string& normal, const std::string& specular) {
    std::stringstream ss;
    ss << "<?xml version=\"1.0\" ?>\n<MaterialObject Name=\"" << matName << "\">\n";
    ss << "    <Material Name=\"Prop.mat\" />\n";
    ss << "    <DefaultSemantic Name=\"Default\" />\n";
    ss << "    <Texture Name=\"mml_tDiffuse\" ResName=\"" << diffuse << "\" />\n";
    ss << "    <Texture Name=\"mml_tNormalMap\" ResName=\"" << normal << "\" />\n";
    ss << "    <Texture Name=\"mml_tSpecularMask\" ResName=\"" << specular << "\" />\n";
    ss << "</MaterialObject>";
    return ss.str();
}

enum class ERFVersion { Unknown, V20, V22, V30 };

bool DAOImporter::RepackERF(const std::string& erfPath,
                            const std::map<std::string, std::vector<uint8_t>>& newFiles)
{
    std::cout << "\n[RepackERF] === " << fs::path(erfPath).filename().string() << " ===" << std::endl;
    std::cout << "[RepackERF] Adding " << newFiles.size() << " files" << std::endl;
    for (const auto& [name, data] : newFiles) {
        std::cout << "  + " << name << " (" << data.size() << " bytes)" << std::endl;
    }

    if (!fs::exists(erfPath)) {
        std::cerr << "[RepackERF] ERROR: File does not exist!" << std::endl;
        return false;
    }

    uintmax_t fileSize = fs::file_size(erfPath);
    std::cout << "[RepackERF] Current size: " << fileSize << " bytes" << std::endl;

    if (fileSize < 32) {
        std::cerr << "[RepackERF] ERROR: File corrupted. Restore via Steam." << std::endl;
        return false;
    }

    // Read entire file
    std::vector<uint8_t> entireFile(fileSize);
    {
        std::ifstream inFile(erfPath, std::ios::binary);
        if (!inFile) return false;
        inFile.read(reinterpret_cast<char*>(entireFile.data()), fileSize);
    }

    // Detect version
    ERFVersion version = ERFVersion::Unknown;
    bool utf16 = false;

    auto chkUtf16 = [&](const char* s) {
        for (int i = 0; i < 8; ++i)
            if (entireFile[i*2] != (uint8_t)s[i] || entireFile[i*2+1] != 0) return false;
        return true;
    };
    auto chkAscii = [&](const char* s) { return memcmp(entireFile.data(), s, 8) == 0; };

    if (chkUtf16("ERF V2.0")) { version = ERFVersion::V20; utf16 = true; }
    else if (chkUtf16("ERF V2.2")) { version = ERFVersion::V22; utf16 = true; }
    else if (chkAscii("ERF V2.0")) { version = ERFVersion::V20; }
    else if (chkAscii("ERF V2.2")) { version = ERFVersion::V22; }

    if (version == ERFVersion::Unknown) {
        std::cerr << "[RepackERF] ERROR: Unknown format" << std::endl;
        return false;
    }

    uint32_t hdrSize = (version == ERFVersion::V20) ? 32 : 48;
    uint32_t entrySize = (version == ERFVersion::V20) ? 72 : 76;
    uint32_t fcOff = utf16 ? 16 : 8;

    uint32_t fileCount = 0, year = 0, day = 0, flags = 0, moduleId = 0;
    uint8_t digest[16] = {0};
    memcpy(&fileCount, &entireFile[fcOff], 4);
    memcpy(&year, &entireFile[fcOff + 4], 4);
    memcpy(&day, &entireFile[fcOff + 8], 4);
    if (version == ERFVersion::V22) {
        memcpy(&flags, &entireFile[fcOff + 16], 4);
        memcpy(&moduleId, &entireFile[fcOff + 20], 4);
        memcpy(digest, &entireFile[fcOff + 24], 16);
    }

    std::cout << "[RepackERF] Format: V" << (version == ERFVersion::V20 ? "2.0" : "2.2")
              << ", entries: " << fileCount << std::endl;

    // Collect all files
    std::map<std::string, std::vector<uint8_t>> allFiles = newFiles;
    size_t pos = hdrSize;

    for (uint32_t i = 0; i < fileCount; ++i) {
        std::string name;
        uint32_t offset = 0, size = 0;

        if (utf16) {
            for (int j = 0; j < 32; ++j) {
                uint16_t ch = entireFile[pos + j*2] | (entireFile[pos + j*2 + 1] << 8);
                if (ch == 0) break;
                name += (char)(ch & 0xFF);
            }
            pos += 64;
        } else {
            for (int j = 0; j < 32 && entireFile[pos + j]; ++j) name += (char)entireFile[pos + j];
            pos += 64;
        }

        memcpy(&offset, &entireFile[pos], 4); pos += 4;
        memcpy(&size, &entireFile[pos], 4); pos += 4;
        if (version == ERFVersion::V22) pos += 4;

        bool skip = false;
        for (const auto& [n, _] : newFiles) if (ToLower(n) == ToLower(name)) { skip = true; break; }

        if (!skip && allFiles.find(name) == allFiles.end() && offset + size <= fileSize) {
            allFiles[name] = std::vector<uint8_t>(entireFile.begin() + offset, entireFile.begin() + offset + size);
        }
    }

    std::cout << "[RepackERF] Total after merge: " << allFiles.size() << std::endl;

    // ========== BACKUP LOGIC ==========
    fs::path backupDir = fs::current_path() / "backups";
    std::string erfName = fs::path(erfPath).filename().string();
    fs::path backupPath = backupDir / (erfName + ".bak");

    if (!fs::exists(backupPath)) {
        bool shouldBackup = true;

        if (m_backupCallback) {
            shouldBackup = m_backupCallback(erfName, backupDir.string());
        }

        if (shouldBackup) {
            std::cout << "[RepackERF] Creating backup: " << erfName << ".bak" << std::endl;

            if (!fs::exists(backupDir)) {
                fs::create_directories(backupDir);
            }

            std::ofstream backupFile(backupPath, std::ios::binary);
            if (backupFile) {
                backupFile.write(reinterpret_cast<const char*>(entireFile.data()), entireFile.size());
                std::cout << "[RepackERF] Backup created successfully" << std::endl;
            } else {
                std::cerr << "[RepackERF] WARNING: Could not create backup!" << std::endl;
            }
        } else {
            std::cout << "[RepackERF] Backup skipped by user" << std::endl;
        }
    } else {
        std::cout << "[RepackERF] Backup exists (skipping)" << std::endl;
    }
    // ========== END BACKUP LOGIC ==========

    // Sort and write
    std::vector<std::pair<std::string, std::vector<uint8_t>>> ordered(allFiles.begin(), allFiles.end());
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return ToLower(a.first) < ToLower(b.first); });

    // Debug: show final file list
    std::cout << "[RepackERF] Final file list (first 10 new entries):" << std::endl;
    int shown = 0;
    for (const auto& [n, d] : ordered) {
        bool isNew = false;
        for (const auto& [newName, _] : newFiles) {
            if (ToLower(newName) == ToLower(n)) { isNew = true; break; }
        }
        if (isNew && shown < 10) {
            std::cout << "  [NEW] " << n << " (" << d.size() << " bytes)" << std::endl;
            shown++;
        }
    }

    std::vector<uint8_t> out;
    auto w16 = [&](uint16_t v) { out.push_back(v & 0xFF); out.push_back((v >> 8) & 0xFF); };
    auto w32 = [&](uint32_t v) { w16(v & 0xFFFF); w16((v >> 16) & 0xFFFF); };
    auto wMagic = [&](const char* s) { for (int i = 0; i < 8; ++i) w16((uint16_t)(uint8_t)s[i]); };
    auto wName = [&](const std::string& s) { for (size_t i = 0; i < 32; ++i) w16(i < s.size() ? (uint16_t)(uint8_t)s[i] : 0); };

    uint32_t cnt = (uint32_t)ordered.size();
    if (version == ERFVersion::V20) {
        wMagic("ERF V2.0"); w32(cnt); w32(year); w32(day); w32(0xFFFFFFFF);
    } else {
        wMagic("ERF V2.2"); w32(cnt); w32(year); w32(day); w32(0xFFFFFFFF);
        w32(flags); w32(moduleId); out.insert(out.end(), digest, digest + 16);
    }

    uint32_t dataStart = hdrSize + cnt * entrySize;
    uint32_t running = dataStart;
    for (const auto& [n, d] : ordered) {
        wName(n); w32(running); w32((uint32_t)d.size());
        if (version == ERFVersion::V22) w32((uint32_t)d.size());
        running += (uint32_t)d.size();
    }
    for (const auto& [n, d] : ordered) out.insert(out.end(), d.begin(), d.end());

    // Write output
    {
        std::ofstream outFile(erfPath, std::ios::binary | std::ios::trunc);
        if (!outFile) { std::cerr << "[RepackERF] ERROR: Cannot write" << std::endl; return false; }
        outFile.write(reinterpret_cast<const char*>(out.data()), out.size());
    }

    std::cout << "[RepackERF] SUCCESS (" << out.size() << " bytes)" << std::endl;
    return true;
}

bool DAOImporter::ConvertAndAddToERF(const std::string& glbPath, const std::string& erfPath) {
    DAOModelData modelData;
    if (!LoadGLB(glbPath, modelData)) return false;

    std::map<std::string, std::vector<uint8_t>> files;
    std::string baseName = modelData.name;

    files[baseName + ".msh"] = GenerateMSH(modelData);
    files[baseName + ".mmh"] = GenerateMMH(modelData, baseName + ".msh");

    for (const auto& mat : modelData.materials) {
        std::string xml = GenerateMAO(mat.name, mat.diffuseMap, mat.normalMap, mat.specularMap);
        files[mat.name + ".mao"].assign(xml.begin(), xml.end());
    }

    for (const auto& tex : modelData.textures) {
        if (tex.width > 0 && tex.height > 0 && !tex.data.empty() && !tex.ddsName.empty()) {
            files[tex.ddsName] = ConvertToDDS(tex.data, tex.width, tex.height, tex.channels);
        }
    }

    return RepackERF(erfPath, files);
}