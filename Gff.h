#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <cstring>

// GFF Field Flags
enum GFFFieldFlags : uint16_t {
    FLAG_LIST = 0x8000,
    FLAG_STRUCT = 0x4000,
    FLAG_REFERENCE = 0x2000
};

struct GFFHeader {
    uint32_t magic;         // "GFF " = 0x47464620
    uint32_t version;       // "V4.0" = 0x56342E30
    uint32_t platform;
    uint32_t fileType;      // "MMH " etc
    uint32_t fileVersion;
    uint32_t structCount;
    uint32_t dataOffset;
};

struct GFFField {
    uint32_t label;
    uint16_t typeId;
    uint16_t flags;
    uint32_t dataOffset;
};

struct GFFStruct {
    char structType[5];     // 4 chars + null
    uint32_t fieldCount;
    uint32_t fieldOffset;
    uint32_t structSize;
    std::vector<GFFField> fields;
};

// Reference to a struct in a list: (structIndex, offset)
struct GFFStructRef {
    uint32_t structIndex;
    uint32_t offset;
};

class GFFFile {
public:
    GFFFile();
    ~GFFFile();

    bool load(const std::string& path);
    bool load(const std::vector<uint8_t>& data);
    void close();

    bool isLoaded() const { return m_loaded; }
    bool isMMH() const;
    bool isMSH() const;

    const GFFHeader& header() const { return m_header; }
    const std::vector<GFFStruct>& structs() const { return m_structs; }

    // Find field by label in a struct
    const GFFField* findField(const GFFStruct& st, uint32_t label) const;
    const GFFField* findField(uint32_t structIndex, uint32_t label) const;

    // Read data by field label from a struct at given offset
    std::string readStringByLabel(uint32_t structIndex, uint32_t label, uint32_t baseOffset = 0);
    int32_t readInt32ByLabel(uint32_t structIndex, uint32_t label, uint32_t baseOffset = 0);
    uint32_t readUInt32ByLabel(uint32_t structIndex, uint32_t label, uint32_t baseOffset = 0);
    float readFloatByLabel(uint32_t structIndex, uint32_t label, uint32_t baseOffset = 0);

    // Read struct lists
    std::vector<GFFStructRef> readStructList(uint32_t structIndex, uint32_t label, uint32_t baseOffset = 0);

    // Get raw data offset (for vertex/index buffers)
    uint32_t getListDataOffset(uint32_t structIndex, uint32_t label, uint32_t baseOffset = 0);

    // Direct data access
    uint32_t dataOffset() const { return m_header.dataOffset; }
    const std::vector<uint8_t>& rawData() const { return m_data; }

    // Read from raw data at absolute position
    template<typename T>
    T readAt(uint32_t pos) const {
        if (pos + sizeof(T) > m_data.size()) return T{};
        T val;
        std::memcpy(&val, &m_data[pos], sizeof(T));
        return val;
    }

    float readFloatAt(uint32_t pos) const { return readAt<float>(pos); }
    int32_t readInt32At(uint32_t pos) const { return readAt<int32_t>(pos); }
    uint32_t readUInt32At(uint32_t pos) const { return readAt<uint32_t>(pos); }
    uint16_t readUInt16At(uint32_t pos) const { return readAt<uint16_t>(pos); }
    int16_t readInt16At(uint32_t pos) const { return readAt<int16_t>(pos); }
    uint8_t readUInt8At(uint32_t pos) const { return readAt<uint8_t>(pos); }

private:
    bool parseHeader();
    bool parseStructs();

    GFFHeader m_header;
    std::vector<GFFStruct> m_structs;
    std::vector<uint8_t> m_data;
    bool m_loaded;
};

// Common field IDs used in MMH/MSH files
namespace GFFFieldID {
    // Common
    constexpr uint32_t NAME = 2;           // Chunk name (in MSH)
    constexpr uint32_t NODE_NAME = 6000;   // Node name (in MMH)
    constexpr uint32_t CHILDREN = 6999;
    constexpr uint32_t MESH_NAME = 6006;   // Mesh name reference

    // MSH root level
    constexpr uint32_t MESH_CHUNKS = 8021;
    constexpr uint32_t VERTEX_BUFFER = 8022;
    constexpr uint32_t INDEX_BUFFER = 8023;

    // Mesh chunk data (in MSH)
    constexpr uint32_t VERTEX_SIZE = 8000;
    constexpr uint32_t VERTEX_COUNT = 8001;
    constexpr uint32_t INDEX_COUNT = 8002;
    constexpr uint32_t PRIMITIVE_TYPE = 8003;
    constexpr uint32_t INDEX_FORMAT = 8004;
    constexpr uint32_t BASE_VERTEX_INDEX = 8005;
    constexpr uint32_t VERTEX_OFFSET = 8006;
    constexpr uint32_t MIN_INDEX = 8007;
    constexpr uint32_t REFERENCED_VERTS = 8008;
    constexpr uint32_t INDEX_OFFSET = 8009;
    constexpr uint32_t VERTEX_DECLARATOR = 8025;

    // Vertex declarator fields
    constexpr uint32_t DECL_STREAM = 8026;
    constexpr uint32_t DECL_OFFSET = 8027;
    constexpr uint32_t DECL_DATATYPE = 8028;
    constexpr uint32_t DECL_USAGE = 8029;
    constexpr uint32_t DECL_USAGE_INDEX = 8030;
}

// Vertex declaration data types
namespace VertexDeclType {
    constexpr uint32_t FLOAT1 = 0;
    constexpr uint32_t FLOAT2 = 1;
    constexpr uint32_t FLOAT3 = 2;
    constexpr uint32_t FLOAT4 = 3;
    constexpr uint32_t COLOR = 4;
    constexpr uint32_t UBYTE4 = 5;
    constexpr uint32_t SHORT2 = 6;
    constexpr uint32_t SHORT4 = 7;
    constexpr uint32_t UBYTE4N = 8;
    constexpr uint32_t SHORT2N = 9;
    constexpr uint32_t SHORT4N = 10;
    constexpr uint32_t USHORT2N = 11;
    constexpr uint32_t USHORT4N = 12;
    constexpr uint32_t FLOAT16_2 = 15;
    constexpr uint32_t FLOAT16_4 = 16;
}

// Vertex usage types
namespace VertexUsage {
    constexpr uint32_t POSITION = 0;
    constexpr uint32_t BLENDWEIGHT = 1;
    constexpr uint32_t BLENDINDICES = 2;
    constexpr uint32_t NORMAL = 3;
    constexpr uint32_t TEXCOORD = 5;
    constexpr uint32_t TANGENT = 6;
    constexpr uint32_t BINORMAL = 7;
    constexpr uint32_t COLOR = 10;
}