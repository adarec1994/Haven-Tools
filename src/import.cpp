#include <import.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <filesystem>

// Define TINYGLTF_IMPLEMENTATION in one .cpp file to include the implementation.
// Assuming the user has this header. If not, this section needs a placeholder.
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

namespace fs = std::filesystem;

std::string FindErfPath(const fs::path& root, const std::string& filename) {
    // Check strict case first (common on Windows)
    if (fs::exists(root / filename)) {
        return (root / filename).string();
    }

    try {
        // Recursive search
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (entry.is_regular_file()) {
                std::string entryName = entry.path().filename().string();

                // Case-insensitive comparison
                std::string a = entryName;
                std::string b = filename;
                std::transform(a.begin(), a.end(), a.begin(), ::tolower);
                std::transform(b.begin(), b.end(), b.begin(), ::tolower);

                if (a == b) {
                    return entry.path().string();
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error searching directory: " << e.what() << std::endl;
    }

    return ""; // Not found
}

// =======================================================================================
// GFF Builder Class
// Handles the binary serialization of BioWare GFF formats (MMH, MSH)
// Logic reversed from DAOGFFFormat.ms
// =======================================================================================
class GFFBuilder {
public:
    struct Field {
        uint32_t label;
        uint16_t type;
        uint16_t flags; // 1=List, 2=Struct, 3=Ref
        std::vector<uint8_t> data; // raw data or index
        uint32_t structIndex = 0xFFFFFFFF; // For struct fields
    };

    struct Struct {
        uint32_t type; // 4-char code
        std::vector<Field> fields;
    };

    enum GFFType {
        GFF_UINT8 = 0,
        GFF_INT8 = 1,
        GFF_UINT16 = 2,
        GFF_INT16 = 3,
        GFF_UINT32 = 4,
        GFF_INT32 = 5,
        GFF_UINT64 = 6,
        GFF_FLOAT = 8,
        GFF_VECTOR3 = 10,
        GFF_VECTOR4 = 12,
        GFF_QUAT = 13,
        GFF_STRING = 14,
        GFF_COLOR = 15
    };

    GFFBuilder(uint32_t fileType) : m_fileType(fileType) {}

    uint32_t AddStruct(const std::string& type) {
        Struct s;
        uint32_t typeInt = 0;
        std::memcpy(&typeInt, type.c_str(), std::min((size_t)4, type.length()));
        s.type = typeInt;
        m_structs.push_back(s);
        return (uint32_t)m_structs.size() - 1;
    }

    void AddField(uint32_t structIdx, uint32_t label, uint16_t type, const void* data, size_t size) {
        Field f;
        f.label = label;
        f.type = type;
        f.flags = 0;
        f.data.resize(size);
        std::memcpy(f.data.data(), data, size);
        m_structs[structIdx].fields.push_back(f);
    }

    void AddStringField(uint32_t structIdx, uint32_t label, const std::string& str) {
        // Simple implementation: String data directly in field for small strings,
        // or reference to data block. For GFF V4.0, strings are often refs.
        // We will store string bytes in the Data Block and point to it.
        // Or for ECString (Type 14), it's an offset.
        m_queuedStrings.push_back({structIdx, (uint32_t)m_structs[structIdx].fields.size(), str});

        Field f;
        f.label = label;
        f.type = GFF_STRING;
        f.flags = 0;
        f.data.resize(4); // Placeholder for offset
        m_structs[structIdx].fields.push_back(f);
    }

    // Helper for specialized list additions would go here
    // For MSH/MMH specifically, we often add child lists.
    // This is a simplified Generic list adder
    void AddListField(uint32_t structIdx, uint32_t label, const std::vector<uint32_t>& childStructIndices) {
        Field f;
        f.label = label;
        f.type = 0; // Generic
        f.flags = 1; // List flag

        // We store the child indices temporarily, will flatten during write
        // Serialization of lists is complex in GFF, storing a ref to a list in the data block
        m_queuedLists.push_back({structIdx, (uint32_t)m_structs[structIdx].fields.size(), childStructIndices});

        f.data.resize(4); // Placeholder offset
        m_structs[structIdx].fields.push_back(f);
    }

    // Add raw binary data field (for Vertex Buffers)
    void AddBinaryField(uint32_t structIdx, uint32_t label, const std::vector<uint8_t>& buffer) {
        // This is not a standard field type, usually just an offset (Type 0 or similar pointing to data)
        // In MSH, vertex streams are just offsets.
        m_queuedBinary.push_back({structIdx, (uint32_t)m_structs[structIdx].fields.size(), buffer});

        Field f;
        f.label = label;
        f.type = GFF_UINT32; // Offset is u32
        f.flags = 0;
        f.data.resize(4);
        m_structs[structIdx].fields.push_back(f);
    }

    std::vector<uint8_t> Build() {
        std::vector<uint8_t> buffer;

        // 1. Header (56 bytes)
        // Magic(4) Ver(4) Plat(4) Type(4) FileVer(4) StructCount(4) DataOffset(4)
        uint32_t magic = 0x47464620; // "GFF "
        uint32_t version = 0x342E3056; // "V4.0" (LE) -> 56 34 2E 30
        uint32_t platform = 0; // PC
        uint32_t fileVer = 0; // Default

        uint32_t structCount = (uint32_t)m_structs.size();
        uint32_t dataOffsetLoc = 24; // Location to write data offset later

        // Write header
        auto write32 = [&](uint32_t v) {
            uint8_t b[4]; memcpy(b, &v, 4);
            buffer.insert(buffer.end(), b, b+4);
        };

        write32(magic);
        write32(version);
        write32(platform);
        write32(m_fileType);
        write32(fileVer);
        write32(structCount);
        write32(0); // Placeholder Data Offset

        // 2. Struct Definitions
        // For each struct: Type(4), FieldCount(4), FieldOffset(4), StructSize(4)
        uint32_t fieldsArrayStart = 28 + (structCount * 16);
        uint32_t currentFieldOffset = 0;

        std::vector<uint32_t> fieldOffsets;

        for (const auto& s : m_structs) {
            write32(s.type);
            write32((uint32_t)s.fields.size());
            write32(currentFieldOffset); // Relative to start of Fields Array?
            // In GFF V4, FieldOffset is byte offset from start of file usually, or start of fields block
            // DAO implementation usually implies absolute offset in file
            fieldOffsets.push_back(currentFieldOffset); // Store relative for now
            write32((uint32_t)s.fields.size() * 12); // Struct Size (approx, 12 bytes per field def)

            currentFieldOffset += (uint32_t)s.fields.size() * 12; // 12 bytes per field
        }

        // Fix up Field Offsets to be absolute
        uint32_t absoluteFieldsStart = (uint32_t)buffer.size();
        for (size_t i = 0; i < structCount; i++) {
            uint32_t pos = 28 + (i * 16) + 8;
            uint32_t absOffset = absoluteFieldsStart + fieldOffsets[i];
            memcpy(&buffer[pos], &absOffset, 4);
        }

        // 3. Fields Array
        // Label(4), Type(2), Flags(2), DataOrOffset(4)
        // We need to calculate the Data Block layout now

        std::vector<uint8_t> dataBlock;

        // Helper to append data and return offset
        auto appendData = [&](const void* d, size_t s) -> uint32_t {
            uint32_t off = (uint32_t)dataBlock.size();
            const uint8_t* bytes = (const uint8_t*)d;
            dataBlock.insert(dataBlock.end(), bytes, bytes + s);
            return off;
        };

        // Process Queued Strings
        for (auto& q : m_queuedStrings) {
            // Write length (4 bytes) + chars
            uint32_t len = (uint32_t)q.str.length();
            uint32_t strOff = appendData(&len, 4);
            // In DAO GFF, string characters are stored, sometimes null terminated, sometimes just len
            // We append chars
            size_t actualStart = dataBlock.size();
            dataBlock.insert(dataBlock.end(), q.str.begin(), q.str.end());

            // Update the field data
            uint32_t finalOff = strOff; // Point to length
            memcpy(m_structs[q.structIdx].fields[q.fieldIdx].data.data(), &finalOff, 4);
        }

        // Process Queued Binary Buffers
        for (auto& q : m_queuedBinary) {
             uint32_t binOff = appendData(q.data.data(), q.data.size());
             memcpy(m_structs[q.structIdx].fields[q.fieldIdx].data.data(), &binOff, 4);
        }

        // Process Queued Lists
        // Lists are stored in Data Block as: Count(4) + [Indices/Offsets]
        for (auto& q : m_queuedLists) {
            uint32_t count = (uint32_t)q.childIndices.size();
            uint32_t listOff = appendData(&count, 4);

            for (uint32_t idx : q.childIndices) {
                // For struct lists, we might store struct index.
                // In generic lists, it's often a reference struct.
                // Simplified: Assuming struct index list
                appendData(&idx, 4);
            }
             memcpy(m_structs[q.structIdx].fields[q.fieldIdx].data.data(), &listOff, 4);
        }

        // Write Fields
        for (const auto& s : m_structs) {
            for (const auto& f : s.fields) {
                write32(f.label);

                uint16_t typeAndFlags[2];
                typeAndFlags[0] = f.type;
                typeAndFlags[1] = f.flags;
                uint8_t tfBytes[4];
                memcpy(tfBytes, typeAndFlags, 4);
                buffer.insert(buffer.end(), tfBytes, tfBytes+4);

                // DataOffset or Inline Data
                if (f.data.size() == 4) {
                    buffer.insert(buffer.end(), f.data.begin(), f.data.end());
                } else if (f.data.size() < 4) {
                    buffer.insert(buffer.end(), f.data.begin(), f.data.end());
                    for(size_t k=0; k<4-f.data.size(); k++) buffer.push_back(0);
                } else {
                    // Data too large for inline, should have been queued
                    uint32_t off = appendData(f.data.data(), f.data.size());
                    write32(off);
                }
            }
        }

        // 4. Update Header with Data Offset
        uint32_t dataOffsetVal = (uint32_t)buffer.size();
        memcpy(&buffer[24], &dataOffsetVal, 4);

        // 5. Append Data Block
        buffer.insert(buffer.end(), dataBlock.begin(), dataBlock.end());

        return buffer;
    }

private:
    uint32_t m_fileType;
    std::vector<Struct> m_structs;

    struct QueuedString { uint32_t structIdx; uint32_t fieldIdx; std::string str; };
    std::vector<QueuedString> m_queuedStrings;

    struct QueuedList { uint32_t structIdx; uint32_t fieldIdx; std::vector<uint32_t> childIndices; };
    std::vector<QueuedList> m_queuedLists;

    struct QueuedBinary { uint32_t structIdx; uint32_t fieldIdx; std::vector<uint8_t> data; };
    std::vector<QueuedBinary> m_queuedBinary;
};

// =======================================================================================
// DAO Importer Implementation
// =======================================================================================

DAOImporter::DAOImporter() {}
DAOImporter::~DAOImporter() {}

// Helper to sanitize texture paths
std::string SanitizeTexturePath(std::string path) {
    if (path.find('.') != std::string::npos) {
        path = path.substr(0, path.find_last_of('.'));
    }
    return path + ".dds";
}

bool DAOImporter::ImportToDirectory(const std::string& glbPath, const std::string& targetDir) {
    std::cout << "[ImportToDirectory] ENTER" << std::endl;
    std::cout << "  GLB = " << glbPath << std::endl;
    std::cout << "  Search Root = " << targetDir << std::endl;

    DAOModelData modelData;
    if (!LoadGLB(glbPath, modelData)) {
        std::cerr << "Failed to load GLB: " << glbPath << std::endl;
        return false;
    }

    const fs::path baseDir(targetDir);

    // 1. Find the 3 specific ERFs recursively
    std::string meshErfPathStr = FindErfPath(baseDir, "modelmeshdata.erf");
    std::string hierErfPathStr = FindErfPath(baseDir, "modelhierarchies.erf");
    std::string matErfPathStr  = FindErfPath(baseDir, "materialobjects.erf");

    // 2. Validate Existence
    if (meshErfPathStr.empty()) {
        std::cerr << "Error: Could not find 'modelmeshdata.erf' in " << targetDir << " or subdirectories." << std::endl;
        return false;
    }
    if (hierErfPathStr.empty()) {
        std::cerr << "Error: Could not find 'modelhierarchies.erf' in " << targetDir << " or subdirectories." << std::endl;
        return false;
    }
    if (matErfPathStr.empty()) {
        std::cerr << "Error: Could not find 'materialobjects.erf' in " << targetDir << " or subdirectories." << std::endl;
        return false;
    }

    std::cout << "Found targets:" << std::endl;
    std::cout << "  Mesh: " << meshErfPathStr << std::endl;
    std::cout << "  Hier: " << hierErfPathStr << std::endl;
    std::cout << "  Mat:  " << matErfPathStr << std::endl;

    // 3. Prepare Texture Names (Heuristics)
    const std::string baseName = fs::path(glbPath).stem().string();
    std::string diffTex = "default_diff.dds";
    std::string normTex = "default_norm.dds";
    std::string specTex = "default_spec.dds";

    for (const auto& path : modelData.texturePaths) {
        std::string lower = path;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (lower.find("normal") != std::string::npos || lower.find("_n.") != std::string::npos || lower.find("_norm") != std::string::npos) {
            normTex = SanitizeTexturePath(path);
        } else if (lower.find("spec") != std::string::npos || lower.find("_s.") != std::string::npos || lower.find("metal") != std::string::npos || lower.find("rough") != std::string::npos) {
            specTex = SanitizeTexturePath(path);
        } else {
            diffTex = SanitizeTexturePath(path);
        }
    }

    // 4. Generate Files and Populate Maps

    // -- MSH --
    std::map<std::string, std::vector<uint8_t>> meshFiles;
    const std::string mshName = baseName + ".msh";
    meshFiles[mshName] = GenerateMSH(modelData);

    // -- MMH --
    std::map<std::string, std::vector<uint8_t>> hierFiles;
    const std::string mmhName = baseName + ".mmh";
    hierFiles[mmhName] = GenerateMMH(modelData, mshName);

    // -- MAO --
    std::map<std::string, std::vector<uint8_t>> matFiles;

    // Get unique material names
    std::vector<std::string> materials;
    materials.reserve(modelData.parts.size());
    for (const auto& p : modelData.parts) {
        std::string m = p.materialName;
        if (m.empty()) m = baseName;
        bool exists = false;
        for (const auto& e : materials) {
            if (e == m) { exists = true; break; }
        }
        if (!exists) materials.push_back(m);
    }
    if (materials.empty()) {
        materials.push_back(baseName);
    }

    for (const auto& matName : materials) {
        const std::string maoName = matName + ".mao";
        const std::string maoXml  = GenerateMAO(matName, diffTex, normTex, specTex);
        matFiles[maoName] = std::vector<uint8_t>(maoXml.begin(), maoXml.end());
    }

    // 5. Repack (Write to found paths)
    bool okMesh = RepackERF(meshErfPathStr, meshFiles);
    bool okHier = RepackERF(hierErfPathStr, hierFiles);
    bool okMat  = RepackERF(matErfPathStr,  matFiles);

    return okMesh && okHier && okMat;
}

bool DAOImporter::ConvertAndAddToERF(const std::string& glbPath, const std::string& erfPath) {
    std::cout << "[ConvertAndAddToERF] ENTER" << std::endl;
    std::cout << "  GLB = " << glbPath << std::endl;
    std::cout << "  ERF = " << erfPath << std::endl;
    DAOModelData modelData;

    if (!LoadGLB(glbPath, modelData)) {
        std::cerr << "Failed to load GLB: " << glbPath << std::endl;
        return false;
    }

    // Use the folder of the selected ERF as the base location for the three target ERFs
    const fs::path baseDir = fs::path(erfPath).parent_path();

    const fs::path meshErfPath = baseDir / "modelmeshdata.erf";
    const fs::path hierErfPath = baseDir / "modelhierarchies.erf";
    const fs::path matErfPath  = baseDir / "materialobjects.erf";

    // Must exist (per your rule: insert/update existing files only)
    if (!fs::exists(meshErfPath)) {
        std::cerr << "Error: Missing target ERF: " << meshErfPath.string() << std::endl;
        return false;
    }
    if (!fs::exists(hierErfPath)) {
        std::cerr << "Error: Missing target ERF: " << hierErfPath.string() << std::endl;
        return false;
    }
    if (!fs::exists(matErfPath)) {
        std::cerr << "Error: Missing target ERF: " << matErfPath.string() << std::endl;
        return false;
    }

    // Original filename stem must be used for .msh/.mmh
    const std::string baseName = fs::path(glbPath).stem().string();

    // Attempt to identify textures by name/suffix (global heuristic as you had)
    std::string diffTex = "default_diff.dds";
    std::string normTex = "default_norm.dds";
    std::string specTex = "default_spec.dds";

    for (const auto& path : modelData.texturePaths) {
        std::string lower = path;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (lower.find("normal") != std::string::npos || lower.find("_n.") != std::string::npos || lower.find("_norm") != std::string::npos) {
            normTex = SanitizeTexturePath(path);
        } else if (lower.find("spec") != std::string::npos || lower.find("_s.") != std::string::npos || lower.find("metal") != std::string::npos || lower.find("rough") != std::string::npos) {
            specTex = SanitizeTexturePath(path);
        } else {
            diffTex = SanitizeTexturePath(path);
        }
    }

    // -----------------------------------------------------------------------------------
    // Build NEW FILES per-target ERF
    // -----------------------------------------------------------------------------------

    // 1) MSH goes into modelmeshdata.erf (filename == original stem)
    std::map<std::string, std::vector<uint8_t>> meshFiles;
    const std::string mshName = baseName + ".msh";
    meshFiles[mshName] = GenerateMSH(modelData);

    // 2) MMH goes into modelhierarchies.erf (filename == original stem)
    std::map<std::string, std::vector<uint8_t>> hierFiles;
    const std::string mmhName = baseName + ".mmh";
    hierFiles[mmhName] = GenerateMMH(modelData, mshName);

    // 3) MAO(s) go into materialobjects.erf (filename == material name)
    std::map<std::string, std::vector<uint8_t>> matFiles;

    // collect unique material names from parts (fallback to baseName if empty)
    std::vector<std::string> materials;
    materials.reserve(modelData.parts.size());
    for (const auto& p : modelData.parts) {
        std::string m = p.materialName;
        if (m.empty()) m = baseName;
        // de-dupe
        bool exists = false;
        for (const auto& e : materials) {
            if (e == m) { exists = true; break; }
        }
        if (!exists) materials.push_back(m);
    }
    if (materials.empty()) {
        materials.push_back(baseName);
    }

    for (const auto& matName : materials) {
        const std::string maoName = matName + ".mao";
        const std::string maoXml  = GenerateMAO(matName, diffTex, normTex, specTex);
        matFiles[maoName] = std::vector<uint8_t>(maoXml.begin(), maoXml.end());
    }

    // -----------------------------------------------------------------------------------
    // Repack into the three specific ERFs
    // -----------------------------------------------------------------------------------
    bool okMesh = RepackERF(meshErfPath.string(), meshFiles);
    bool okHier = RepackERF(hierErfPath.string(), hierFiles);
    bool okMat  = RepackERF(matErfPath.string(),  matFiles);

    return okMesh && okHier && okMat;
}


bool DAOImporter::LoadGLB(const std::string& path, DAOModelData& outData) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, path);
    if (!warn.empty()) std::cout << "GLB Warn: " << warn << std::endl;
    if (!err.empty()) std::cerr << "GLB Err: " << err << std::endl;
    if (!ret) return false;

    outData.name = fs::path(path).stem().string();

    // Basic extraction logic - iterates meshes and primitives
    for (const auto& mesh : model.meshes) {
        for (const auto& primitive : mesh.primitives) {
            DAOModelData::MeshPart part;

            // Get Material Name
            if (primitive.material >= 0) {
                part.materialName = model.materials[primitive.material].name;
            }

            // Extract Vertices (POSITION)
            if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("POSITION")];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

                const float* positions = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);

                for (size_t i = 0; i < accessor.count; ++i) {
                    DAOModelData::Vertex v = {};
                    v.x = positions[i * 3 + 0];
                    v.y = positions[i * 3 + 1];
                    v.z = positions[i * 3 + 2];
                    part.vertices.push_back(v);
                }
            }

            // Extract Normals
            if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("NORMAL")];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                const float* normals = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);

                for (size_t i = 0; i < accessor.count && i < part.vertices.size(); ++i) {
                    part.vertices[i].nx = normals[i * 3 + 0];
                    part.vertices[i].ny = normals[i * 3 + 1];
                    part.vertices[i].nz = normals[i * 3 + 2];
                }
            }

            // Extract UVs
            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                const float* uvs = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);

                for (size_t i = 0; i < accessor.count && i < part.vertices.size(); ++i) {
                    part.vertices[i].u = uvs[i * 2 + 0];
                    part.vertices[i].v = uvs[i * 2 + 1];
                }
            }

            // Extract Indices
            if (primitive.indices >= 0) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.indices];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

                if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                    for (size_t i = 0; i < accessor.count; ++i) part.indices.push_back(buf[i]);
                } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
                    for (size_t i = 0; i < accessor.count; ++i) part.indices.push_back((uint16_t)buf[i]);
                }
            }

            outData.parts.push_back(part);
        }
    }

    // Extract Textures
    for (const auto& img : model.images) {
        if (!img.uri.empty()) outData.texturePaths.push_back(img.uri);
        else outData.texturePaths.push_back(img.name);
    }

    return true;
}

std::vector<uint8_t> DAOImporter::GenerateMMH(const DAOModelData& model, const std::string& mshFilename) {
    // 541609293 = MMH File Type
    GFFBuilder gff(541609293);

    // Create Root Struct (MMH Header info)
    uint32_t rootIdx = gff.AddStruct("MMH "); // Pseudo type

    // Add Fields based on DAOModelImport.ms logic
    // 6000: Model Name (String)
    gff.AddStringField(rootIdx, 6000, model.name);

    // 6005: MSH File Name (String) -> The core link to geometry
    std::string mshResName = mshFilename;
    if (mshResName.find('.') != std::string::npos) mshResName = mshResName.substr(0, mshResName.find_last_of('.'));
    gff.AddStringField(rootIdx, 6005, mshResName);

    // 6999: Children (List of Nodes/Objects)
    // We create a "mshh" struct (Mesh Header) as a child
    uint32_t mshhIdx = gff.AddStruct("mshh");
    gff.AddStringField(mshhIdx, 6000, model.name); // Mesh Name
    gff.AddStringField(mshhIdx, 6006, model.name); // Group Name (Matches Chunk inside MSH)

    // 6001: Material Object (Links to MAO)
    std::string matName = model.name; // Default
    if (!model.parts.empty() && !model.parts[0].materialName.empty()) matName = model.parts[0].materialName;
    gff.AddStringField(mshhIdx, 6001, matName);

    // Add 'mshh' to root children list
    gff.AddListField(rootIdx, 6999, {mshhIdx});

    return gff.Build();
}

std::vector<uint8_t> DAOImporter::GenerateMSH(const DAOModelData& model) {
    // 1213416781 = MSH File Type
    GFFBuilder gff(1213416781);

    // Flatten all vertices for the single buffer
    std::vector<float> vertexBuffer;
    std::vector<uint16_t> indexBuffer;
    uint32_t indexOffset = 0;

    // Simple logic: One chunk for the whole model for this converter
    // Complex models would split chunks
    for (const auto& part : model.parts) {
        for (const auto& v : part.vertices) {
            vertexBuffer.push_back(v.x); vertexBuffer.push_back(v.y); vertexBuffer.push_back(v.z);
            vertexBuffer.push_back(v.nx); vertexBuffer.push_back(v.ny); vertexBuffer.push_back(v.nz);
            vertexBuffer.push_back(v.u); vertexBuffer.push_back(v.v);
        }
        for (uint16_t idx : part.indices) {
            indexBuffer.push_back(idx + (uint16_t)(indexOffset));
        }
        indexOffset += (uint32_t)part.vertices.size();
    }

    // Convert float buffer to bytes
    std::vector<uint8_t> rawVerts(vertexBuffer.size() * sizeof(float));
    memcpy(rawVerts.data(), vertexBuffer.data(), rawVerts.size());

    // Convert index buffer to bytes
    std::vector<uint8_t> rawIndices(indexBuffer.size() * sizeof(uint16_t));
    memcpy(rawIndices.data(), indexBuffer.data(), rawIndices.size());

    // Create Root Struct
    uint32_t rootIdx = gff.AddStruct("MSH ");

    // 8021: Mesh Chunks (List)
    uint32_t chunkIdx = gff.AddStruct("msgr"); // Mesh Group/Chunk
    gff.AddStringField(chunkIdx, 2, model.name); // Chunk Name

    uint32_t vertCount = (uint32_t)vertexBuffer.size() / 8; // 8 floats per vert (3pos+3norm+2uv)
    gff.AddField(chunkIdx, 8000, GFFBuilder::GFF_UINT32, &vertCount, 4); // VertexCount? No, 8000 is VertexSize
    uint32_t vertSize = 8 * 4; // 32 bytes
    gff.AddField(chunkIdx, 8000, GFFBuilder::GFF_UINT32, &vertSize, 4);

    // 8001: Vertex Count
    gff.AddField(chunkIdx, 8001, GFFBuilder::GFF_UINT32, &vertCount, 4);

    // 8002: Index Count
    uint32_t idxCount = (uint32_t)indexBuffer.size();
    gff.AddField(chunkIdx, 8002, GFFBuilder::GFF_UINT32, &idxCount, 4);

    // 8003: Primitive Type (4 = Triangles)
    uint32_t primType = 4;
    gff.AddField(chunkIdx, 8003, GFFBuilder::GFF_UINT32, &primType, 4);

    // 8025: Vertex Declarator (List of Structs defining stream)
    // Stream 1: Pos (Type 2=Float3, Usage 0=Pos)
    uint32_t declPos = gff.AddStruct("decl");
    uint32_t dtPos = 2; // Float3
    uint32_t duPos = 0; // Position
    gff.AddField(declPos, 8028, GFFBuilder::GFF_UINT32, &dtPos, 4);
    gff.AddField(declPos, 8029, GFFBuilder::GFF_UINT32, &duPos, 4);

    // Stream 2: Normal (Type 2=Float3, Usage 3=Normal)
    uint32_t declNorm = gff.AddStruct("decl");
    uint32_t dtNorm = 2;
    uint32_t duNorm = 3;
    uint32_t offNorm = 12; // Offset in vertex
    gff.AddField(declNorm, 8028, GFFBuilder::GFF_UINT32, &dtNorm, 4);
    gff.AddField(declNorm, 8029, GFFBuilder::GFF_UINT32, &duNorm, 4);
    gff.AddField(declNorm, 8027, GFFBuilder::GFF_UINT32, &offNorm, 4);

    // Stream 3: TexCoord (Type 1=Float2, Usage 5=TexCoord)
    uint32_t declUV = gff.AddStruct("decl");
    uint32_t dtUV = 1;
    uint32_t duUV = 5;
    uint32_t offUV = 24;
    gff.AddField(declUV, 8028, GFFBuilder::GFF_UINT32, &dtUV, 4);
    gff.AddField(declUV, 8029, GFFBuilder::GFF_UINT32, &duUV, 4);
    gff.AddField(declUV, 8027, GFFBuilder::GFF_UINT32, &offUV, 4);

    gff.AddListField(chunkIdx, 8025, {declPos, declNorm, declUV});

    // Add Chunk to Root
    gff.AddListField(rootIdx, 8021, {chunkIdx});

    // Add Binary Data Blobs (8022=Verts, 8023=Indices)
    gff.AddBinaryField(rootIdx, 8022, rawVerts);
    gff.AddBinaryField(rootIdx, 8023, rawIndices);

    return gff.Build();
}

std::string DAOImporter::GenerateMAO(const std::string& materialName, const std::string& diffuse, const std::string& normal, const std::string& specular) {
    // Basic XML structure for DAO Materials
    std::stringstream ss;
    ss << "<?xml version=\"1.0\" ?>\n";
    ss << "<MaterialObject Name=\"" << materialName << "\">\n";
    ss << "    <Material Name=\"Prop.mat\" />\n";
    ss << "    <DefaultSemantic Name=\"Default\" />\n";
    ss << "    <Texture Name=\"mml_tDiffuse\" ResName=\"" << diffuse << "\" />\n";
    ss << "    <Texture Name=\"mml_tNormalMap\" ResName=\"" << normal << "\" />\n";
    ss << "    <Texture Name=\"mml_tSpecularMask\" ResName=\"" << specular << "\" />\n";
    ss << "</MaterialObject>";
    return ss.str();
}

bool DAOImporter::RepackERF(
    const std::string& erfPath,
    const std::map<std::string, std::vector<uint8_t>>& newFiles
) {
    std::cout << "[RepackERF] ENTER: " << erfPath << std::endl;
    std::cout << "[RepackERF] New files count = " << newFiles.size() << std::endl;

    if (!fs::exists(erfPath)) {
        std::cout << "[RepackERF] ERROR: ERF does not exist: " << erfPath << std::endl;
        return false;
    }

    // Read existing ERF
    ERFFile erf;
    std::map<std::string, std::vector<uint8_t>> allFiles = newFiles;

    std::cout << "[RepackERF] Opening existing ERF..." << std::endl;

    if (!erf.open(erfPath)) {
        std::cout << "[RepackERF] ERROR: Failed to open existing ERF" << std::endl;
        return false;
    }

    std::cout << "[RepackERF] Existing entries = " << erf.entries().size() << std::endl;

    for (const auto& entry : erf.entries()) {
        if (allFiles.find(entry.name) == allFiles.end()) {
            auto data = erf.readEntry(entry);
            std::cout << "[RepackERF] Preserve entry: "
                      << entry.name << " (" << data.size() << " bytes)" << std::endl;
            allFiles[entry.name] = std::move(data);
        } else {
            std::cout << "[RepackERF] Overwrite entry: " << entry.name << std::endl;
        }
    }

    erf.close();

    std::cout << "[RepackERF] Total files after merge = "
              << allFiles.size() << std::endl;

    // Write new ERF
    std::cout << "[RepackERF] Writing ERF..." << std::endl;

    std::ofstream out(erfPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        std::cout << "[RepackERF] ERROR: Cannot open ERF for writing" << std::endl;
        return false;
    }

    // Header
    const char* magic = "ERF V2.0";
    out.write(magic, 8);
    uint64_t zeros = 0;
    out.write(reinterpret_cast<const char*>(&zeros), 8);

    uint32_t fileCount = (uint32_t)allFiles.size();
    out.write(reinterpret_cast<const char*>(&fileCount), 4);

    uint32_t dummy = 0;
    out.write(reinterpret_cast<const char*>(&dummy), 4);
    out.write(reinterpret_cast<const char*>(&dummy), 4);
    out.write(reinterpret_cast<const char*>(&dummy), 4);

    if (!out.good()) {
        std::cout << "[RepackERF] ERROR: Failed writing header" << std::endl;
        out.close();
        return false;
    }

    // Entry table
    uint32_t dataStartOffset = 32 + (fileCount * 72);
    uint32_t currentOffset = dataStartOffset;

    std::cout << "[RepackERF] Writing entry table..." << std::endl;

    for (const auto& [name, data] : allFiles) {
        char nameBuf[64] = {0};
        std::strncpy(nameBuf, name.c_str(), 63);
        out.write(nameBuf, 64);

        out.write(reinterpret_cast<const char*>(&currentOffset), 4);

        uint32_t size = (uint32_t)data.size();
        out.write(reinterpret_cast<const char*>(&size), 4);

        std::cout << "  Entry: " << name
                  << " offset=" << currentOffset
                  << " size=" << size << std::endl;

        currentOffset += size;
    }

    if (!out.good()) {
        std::cout << "[RepackERF] ERROR: Failed writing entry table" << std::endl;
        out.close();
        return false;
    }

    // Data blobs
    std::cout << "[RepackERF] Writing data blobs..." << std::endl;

    for (const auto& [name, data] : allFiles) {
        if (!data.empty()) {
            out.write(reinterpret_cast<const char*>(data.data()), data.size());
            if (!out.good()) {
                std::cout << "[RepackERF] ERROR writing data for: "
                          << name << std::endl;
                out.close();
                return false;
            }
        }
    }

    out.flush();
    out.close();

    std::cout << "[RepackERF] SUCCESS: Wrote ERF "
              << erfPath << " with "
              << allFiles.size() << " entries" << std::endl;

    return true;
}
