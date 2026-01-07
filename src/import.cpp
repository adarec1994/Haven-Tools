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
#include <cmath>
#include <limits>
#include <iomanip>

// Define implementations for external single-header libraries
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

namespace fs = std::filesystem;

// ============================================================================
// GFFBuilder Class
// Handles the construction of BioWare's Generic File Format (GFF v4.0)
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
        uint32_t structSize;
    };

    GFFBuilder(const char* fileType) {
        memset(m_fileType, ' ', 4);
        if(fileType) memcpy(m_fileType, fileType, std::min((size_t)4, strlen(fileType)));
    }

    uint32_t AddStruct(const char* type) {
        Struct s;
        memset(s.type, ' ', 4);
        if(type) memcpy(s.type, type, std::min((size_t)4, strlen(type)));
        s.structSize = 0;
        m_structs.push_back(s);
        std::cout << "[GFFBuilder] Added Struct: " << std::string(s.type, 4) << " (ID: " << m_structs.size()-1 << ")" << std::endl;
        return (uint32_t)m_structs.size() - 1;
    }

    void AddUInt32Field(uint32_t structIdx, uint32_t label, uint32_t value) {
        m_queuedUInt32s.push_back({structIdx, (uint32_t)m_structs[structIdx].fields.size(), value});
        Field f = {label, 4, 0, 0}; // Type 4 = UINT32
        m_structs[structIdx].fields.push_back(f);
    }

    void AddFloatField(uint32_t structIdx, uint32_t label, float value) {
        uint32_t asInt;
        memcpy(&asInt, &value, 4);
        m_queuedUInt32s.push_back({structIdx, (uint32_t)m_structs[structIdx].fields.size(), asInt});
        Field f = {label, 8, 0, 0}; // Type 8 = FLOAT32
        m_structs[structIdx].fields.push_back(f);
    }

    // --- FIX: UTF-16 String Handling ---
    void AddStringField(uint32_t structIdx, uint32_t label, const std::string& str) {
        std::vector<uint16_t> wideStr;
        for (char c : str) wideStr.push_back((uint16_t)(unsigned char)c);

        m_queuedStrings.push_back({structIdx, (uint32_t)m_structs[structIdx].fields.size(), wideStr});
        Field f = {label, 14, 0, 0}; // Type 14 = ECString
        m_structs[structIdx].fields.push_back(f);
        // Debug: Print string being added
        std::cout << "[GFFBuilder] Added String Field (Label " << label << "): " << str << std::endl;
    }

    void AddVector3Field(uint32_t structIdx, uint32_t label, float x, float y, float z) {
        m_queuedVector3s.push_back({structIdx, (uint32_t)m_structs[structIdx].fields.size(), x, y, z});
        Field f = {label, 10, 0, 0}; // Type 10 = Vector3f
        m_structs[structIdx].fields.push_back(f);
    }

    void AddStructListField(uint32_t structIdx, uint32_t label, uint32_t childStructType, const std::vector<uint32_t>& childIndices) {
        m_queuedStructLists.push_back({structIdx, (uint32_t)m_structs[structIdx].fields.size(), childStructType, childIndices});
        Field f = {label, (uint16_t)childStructType, 0x8000 | 0x4000, 0}; // List | Struct
        m_structs[structIdx].fields.push_back(f);
    }

    void AddBinaryField(uint32_t structIdx, uint32_t label, const std::vector<uint8_t>& data) {
        m_queuedBinary.push_back({structIdx, (uint32_t)m_structs[structIdx].fields.size(), data});
        Field f = {label, 0, 0x8000, 0}; // Type 0 (UINT8) | List flag
        m_structs[structIdx].fields.push_back(f);
    }

    std::vector<uint8_t> Build() {
        std::cout << "[GFFBuilder] Building binary GFF..." << std::endl;
        for (auto& s : m_structs) s.structSize = (uint32_t)(s.fields.size() * 12);

        std::vector<uint8_t> dataBlock;

        auto appendToData = [&](const void* d, size_t len) -> uint32_t {
            uint32_t off = (uint32_t)dataBlock.size();
            const uint8_t* p = (const uint8_t*)d;
            dataBlock.insert(dataBlock.end(), p, p + len);
            return off;
        };
        auto writeU32ToData = [&](uint32_t v) -> uint32_t { return appendToData(&v, 4); };

        // 1. Strings (UTF-16LE with Length Prefix)
        std::map<std::pair<uint32_t, uint32_t>, uint32_t> stringDataOffsets;
        for (auto& q : m_queuedStrings) {
            uint32_t charCount = (uint32_t)q.wideStr.size();
            uint32_t off = appendToData(&charCount, 4);
            // Write raw bytes (Little Endian)
            for (uint16_t c : q.wideStr) {
                uint8_t low = c & 0xFF;
                uint8_t high = (c >> 8) & 0xFF;
                dataBlock.push_back(low);
                dataBlock.push_back(high);
            }
            // Null terminator (2 bytes)
            dataBlock.push_back(0); dataBlock.push_back(0);
            stringDataOffsets[{q.structIdx, q.fieldIdx}] = off;
        }

        std::map<std::pair<uint32_t, uint32_t>, uint32_t> stringRefOffsets;
        for (auto& q : m_queuedStrings) {
            uint32_t strDataOff = stringDataOffsets[{q.structIdx, q.fieldIdx}];
            stringRefOffsets[{q.structIdx, q.fieldIdx}] = writeU32ToData(strDataOff);
        }

        // 2. Binary
        std::map<std::pair<uint32_t, uint32_t>, uint32_t> binaryOffsets;
        for (auto& q : m_queuedBinary) {
            uint32_t count = (uint32_t)q.data.size();
            uint32_t off = appendToData(&count, 4);
            appendToData(q.data.data(), q.data.size());
            binaryOffsets[{q.structIdx, q.fieldIdx}] = off;
        }

        // 3. Vector3
        std::map<std::pair<uint32_t, uint32_t>, uint32_t> vector3Offsets;
        for (auto& q : m_queuedVector3s) {
            float floats[3] = {q.x, q.y, q.z};
            vector3Offsets[{q.structIdx, q.fieldIdx}] = appendToData(floats, 12);
        }

        // 4. UInt32
        std::map<std::pair<uint32_t, uint32_t>, uint32_t> uint32Offsets;
        for (auto& q : m_queuedUInt32s) {
             uint32Offsets[{q.structIdx, q.fieldIdx}] = writeU32ToData(q.value);
        }

        // 5. Lists
        std::map<std::pair<uint32_t, uint32_t>, uint32_t> structListOffsets;
        for (auto& q : m_queuedStructLists) {
            uint32_t count = (uint32_t)q.childIndices.size();
            uint32_t listStart = writeU32ToData(count);
            for (uint32_t childIdx : q.childIndices) {
                const auto& childStruct = m_structs[childIdx];
                for (size_t fi = 0; fi < childStruct.fields.size(); fi++) {
                    bool found = false;
                    for (const auto& uq : m_queuedUInt32s) {
                        if (uq.structIdx == childIdx && uq.fieldIdx == fi) {
                            writeU32ToData(uq.value); found = true; break;
                        }
                    }
                    if(found) continue;
                    for (const auto& sq : m_queuedStrings) {
                        if (sq.structIdx == childIdx && sq.fieldIdx == fi) {
                            writeU32ToData(stringDataOffsets[{childIdx, (uint32_t)fi}]); found = true; break;
                        }
                    }
                    if(found) continue;
                    for (const auto& vq : m_queuedVector3s) {
                        if (vq.structIdx == childIdx && vq.fieldIdx == fi) {
                            writeU32ToData(vector3Offsets[{childIdx, (uint32_t)fi}]); found = true; break;
                        }
                    }
                    if(found) continue;
                    for (const auto& slq : m_queuedStructLists) {
                        if (slq.structIdx == childIdx && slq.fieldIdx == fi) {
                            if(structListOffsets.count({childIdx, (uint32_t)fi}))
                                writeU32ToData(structListOffsets[{childIdx, (uint32_t)fi}]);
                            else
                                writeU32ToData(0);
                            found = true; break;
                        }
                    }
                    if (!found) writeU32ToData(0);
                }
            }
            structListOffsets[{q.structIdx, q.fieldIdx}] = listStart;
        }

        // 6. Offsets
        for (size_t si = 0; si < m_structs.size(); si++) {
            bool isChildStruct = false;
            for (const auto& sl : m_queuedStructLists) {
                for (uint32_t idx : sl.childIndices) { if (idx == si) { isChildStruct = true; break; } }
                if (isChildStruct) break;
            }

            for (size_t fi = 0; fi < m_structs[si].fields.size(); fi++) {
                auto& field = m_structs[si].fields[fi];
                if (isChildStruct) {
                    field.dataOrOffset = (uint32_t)(fi * 4);
                } else {
                    auto key = std::make_pair((uint32_t)si, (uint32_t)fi);
                    if (stringRefOffsets.count(key)) field.dataOrOffset = stringRefOffsets[key];
                    else if (binaryOffsets.count(key)) field.dataOrOffset = binaryOffsets[key];
                    else if (vector3Offsets.count(key)) field.dataOrOffset = vector3Offsets[key];
                    else if (uint32Offsets.count(key)) field.dataOrOffset = uint32Offsets[key];
                    else if (structListOffsets.count(key)) field.dataOrOffset = structListOffsets[key];
                }
            }
        }

        std::vector<uint8_t> buffer;
        auto writeBytes = [&](const void* data, size_t len) {
            const uint8_t* p = (const uint8_t*)data;
            buffer.insert(buffer.end(), p, p + len);
        };
        auto writeU32 = [&](uint32_t v) { writeBytes(&v, 4); };
        auto writeU16 = [&](uint16_t v) { writeBytes(&v, 2); };

        // --- FIX: Explicit Character Writing to prevent Byte Swapping ---
        // Reference Header: GFF V4.0PC  MESHV0.1
        const char* magic = "GFF ";
        writeBytes(magic, 4);

        const char* version = "V4.0";
        writeBytes(version, 4);

        const char* platform = "PC  ";
        writeBytes(platform, 4);

        writeBytes(m_fileType, 4); // "MESH" or "MMH "

        const char* dataVer = "V0.1"; // MATCH REFERENCE V0.1 (Was V1.0)
        writeBytes(dataVer, 4);

        writeU32((uint32_t)m_structs.size());
        size_t dataOffsetPos = buffer.size();
        writeU32(0);

        std::vector<size_t> fieldOffsetPositions;
        for (const auto& s : m_structs) {
            writeBytes(s.type, 4);
            writeU32((uint32_t)s.fields.size());
            fieldOffsetPositions.push_back(buffer.size());
            writeU32(0);
            writeU32(s.structSize);
        }

        for (size_t i = 0; i < m_structs.size(); i++) {
            uint32_t currentPos = (uint32_t)buffer.size();
            memcpy(&buffer[fieldOffsetPositions[i]], &currentPos, 4);

            for (const auto& f : m_structs[i].fields) {
                writeU32(f.label);
                writeU16(f.typeId);
                writeU16(f.flags);
                writeU32(f.dataOrOffset);
            }
        }

        uint32_t currentPos = (uint32_t)buffer.size();
        memcpy(&buffer[dataOffsetPos], &currentPos, 4);
        buffer.insert(buffer.end(), dataBlock.begin(), dataBlock.end());

        std::cout << "[GFFBuilder] Final buffer size: " << buffer.size() << " bytes." << std::endl;
        return buffer;
    }


private:
    char m_fileType[4];
    struct QueuedString { uint32_t structIdx; uint32_t fieldIdx; std::vector<uint16_t> wideStr; };
    struct QueuedStructList { uint32_t structIdx; uint32_t fieldIdx; uint32_t childType; std::vector<uint32_t> childIndices; };
    struct QueuedBinary { uint32_t structIdx; uint32_t fieldIdx; std::vector<uint8_t> data; };
    struct QueuedUInt32 { uint32_t structIdx; uint32_t fieldIdx; uint32_t value; };
    struct QueuedVector3 { uint32_t structIdx; uint32_t fieldIdx; float x, y, z; };

    std::vector<Struct> m_structs;
    std::vector<QueuedString> m_queuedStrings;
    std::vector<QueuedStructList> m_queuedStructLists;
    std::vector<QueuedBinary> m_queuedBinary;
    std::vector<QueuedUInt32> m_queuedUInt32s;
    std::vector<QueuedVector3> m_queuedVector3s;
};

// ============================================================================
// Simple XML Parser Helper
// ============================================================================
struct SimpleXMLNode {
    std::string name;
    std::string content;
    std::map<std::string, std::string> attributes;
    std::vector<SimpleXMLNode> children;
};

static bool WriteAllText(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(text.data(), (std::streamsize)text.size());
    return true;
}

static void DumpMSHXmlNextToGLB(const std::string& glbPath,
                                const std::string& baseName,
                                const std::string& xml)
{
    namespace fs = std::filesystem;

    fs::path glbDir = fs::path(glbPath).parent_path();
    fs::path outXml = glbDir / (baseName + ".msh.xml");

    if (!WriteAllText(outXml, xml)) {
        std::cerr << "[XML] Failed to write: " << outXml.string()
                  << " (errno=" << errno << ")\n";
    } else {
        std::cout << "[XML] Wrote: " << outXml.string() << "\n";
    }
}


class SimpleXMLParser {
public:
    static SimpleXMLNode Parse(const std::string& xml) {
        SimpleXMLNode root;
        size_t pos = 0;
        ParseNode(xml, pos, root);
        if (!root.children.empty()) return root.children[0];
        return root;
    }

private:
    static void SkipWhitespace(const std::string& xml, size_t& pos) {
        while (pos < xml.length() && isspace((unsigned char)xml[pos])) pos++;
    }

    static void ParseNode(const std::string& xml, size_t& pos, SimpleXMLNode& parent) {
        while (pos < xml.length()) {
            SkipWhitespace(xml, pos);
            if (pos >= xml.length() || xml[pos] != '<') return;

            if (pos + 1 < xml.length() && xml[pos+1] == '/') {
                while (pos < xml.length() && xml[pos] != '>') pos++;
                pos++;
                return;
            }

            pos++;
            // Ignore processing instructions <? ... ?>
            if (pos < xml.length() && (xml[pos] == '?' || xml[pos] == '!')) {
                while (pos < xml.length() && xml[pos] != '>') pos++;
                pos++;
                continue;
            }

            size_t endName = pos;
            while (endName < xml.length() && !isspace((unsigned char)xml[endName]) && xml[endName] != '>' && xml[endName] != '/') endName++;

            SimpleXMLNode node;
            node.name = xml.substr(pos, endName - pos);
            pos = endName;

            while (pos < xml.length() && xml[pos] != '>' && xml[pos] != '/') {
                SkipWhitespace(xml, pos);
                if (xml[pos] == '>' || xml[pos] == '/') break;
                size_t attrStart = pos;
                while (pos < xml.length() && xml[pos] != '=') pos++;
                std::string attrName = xml.substr(attrStart, pos - attrStart);
                pos++;
                SkipWhitespace(xml, pos);
                char quote = xml[pos]; pos++;
                size_t valStart = pos;
                while (pos < xml.length() && xml[pos] != quote) pos++;
                std::string attrVal = xml.substr(valStart, pos - valStart);
                pos++;
                node.attributes[attrName] = attrVal;
            }

            if (xml[pos] == '/') {
                while (pos < xml.length() && xml[pos] != '>') pos++;
                pos++;
                parent.children.push_back(node);
                continue;
            }

            pos++;
            size_t nextTag = xml.find('<', pos);
            if (nextTag != std::string::npos && xml[nextTag+1] == '/') {
                node.content = xml.substr(pos, nextTag - pos);
                pos = nextTag;
                while (pos < xml.length() && xml[pos] != '>') pos++;
                pos++;
            } else {
                ParseNode(xml, pos, node);
            }
            parent.children.push_back(node);
        }
    }
};

// ============================================================================
// DAOImporter Implementation
// ============================================================================

DAOImporter::DAOImporter() : m_backupCallback(nullptr), m_progressCallback(nullptr) {}
DAOImporter::~DAOImporter() {}

// Helpers
bool DAOImporter::BackupExists(const std::string& erfPath) { return fs::exists(fs::path(erfPath).parent_path() / "backups" / (fs::path(erfPath).filename().string() + ".bak")); }
std::string DAOImporter::GetBackupDir() { return (fs::current_path() / "backups").string(); }
static std::string ToLower(std::string s) { std::transform(s.begin(), s.end(), s.begin(), ::tolower); return s; }
static std::string CleanName(const std::string& input) { return ToLower(fs::path(input).stem().string()); }
static std::string FindErfPath(const fs::path& root, const std::string& filename) {
    if(fs::exists(root/filename)) return (root/filename).string();
    return "";
}
static std::vector<uint8_t> ConvertToDDS(const std::vector<uint8_t>& imageData, int width, int height, int channels) {
    // Basic uncompressed DDS output
    std::vector<uint8_t> dds;
    auto writeU32 = [&](uint32_t v) { dds.push_back(v & 0xFF); dds.push_back((v >> 8) & 0xFF); dds.push_back((v >> 16) & 0xFF); dds.push_back((v >> 24) & 0xFF); };
    dds.push_back('D'); dds.push_back('D'); dds.push_back('S'); dds.push_back(' ');
    writeU32(124); writeU32(0x1 | 0x2 | 0x4 | 0x1000); writeU32(height); writeU32(width); writeU32(width * 4); writeU32(0); writeU32(1);
    for (int i = 0; i < 11; ++i) writeU32(0);
    writeU32(32); writeU32(0x41); writeU32(0); writeU32(32);
    writeU32(0x00FF0000); writeU32(0x0000FF00); writeU32(0x000000FF); writeU32(0xFF000000);
    writeU32(0x1000); for (int i = 0; i < 4; ++i) writeU32(0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int srcIdx = (y * width + x) * channels;
            uint8_t r = imageData[srcIdx];
            uint8_t g = (channels > 1) ? imageData[srcIdx + 1] : r;
            uint8_t b = (channels > 2) ? imageData[srcIdx + 2] : r;
            uint8_t a = (channels > 3) ? imageData[srcIdx + 3] : 255;
            dds.push_back(b); dds.push_back(g); dds.push_back(r); dds.push_back(a);
        }
    }
    return dds;
}
void DAOImporter::ReportProgress(float progress, const std::string& status) { if(m_progressCallback) m_progressCallback(progress, status); }

// ----------------------------------------------------------------------------
// XML Generation
// ----------------------------------------------------------------------------
std::string DAOImporter::GenerateMSH_XML(const DAOModelData& model) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(6);
    ss << "<?xml version=\"1.0\" ?>\n<Mesh Name=\"" << model.name << "\">\n";
    ss << "    <Version>V4.0</Version>\n";

    std::vector<float> vBuf;
    std::vector<int> iBuf;
    float minX = 99999.0f, minY = 99999.0f, minZ = 99999.0f;
    float maxX = -99999.0f, maxY = -99999.0f, maxZ = -99999.0f;

    struct GroupInfo { std::string name; std::string material; size_t startIndex; size_t indexCount; };
    std::vector<GroupInfo> groups;
    size_t currentVertexOffset = 0;

    for (const auto& part : model.parts) {
        std::string groupName = ToLower(part.materialName);
        if (groupName.find("uhm_") != 0) groupName = "uhm_" + groupName;

        GroupInfo info;
        info.name = groupName;
        info.material = part.materialName;
        info.startIndex = iBuf.size();
        info.indexCount = part.indices.size();
        groups.push_back(info);

        for (const auto& v : part.vertices) {
            vBuf.push_back(v.x); vBuf.push_back(v.y); vBuf.push_back(v.z);
            vBuf.push_back(v.nx); vBuf.push_back(v.ny); vBuf.push_back(v.nz);
            vBuf.push_back(v.u); vBuf.push_back(v.v);

            if (v.x < minX) minX = v.x; if (v.x > maxX) maxX = v.x;
            if (v.y < minY) minY = v.y; if (v.y > maxY) maxY = v.y;
            if (v.z < minZ) minZ = v.z; if (v.z > maxZ) maxZ = v.z;
        }

        for (auto idx : part.indices) {
            iBuf.push_back((int)(idx + currentVertexOffset));
        }

        currentVertexOffset += part.vertices.size();
    }

    ss << "    <VertexData Count=\"" << (vBuf.size()/8) << "\">";
    for(size_t i=0; i<vBuf.size(); ++i) { if(i>0)ss<<" "; ss<<vBuf[i]; }
    ss << "</VertexData>\n";
    ss << "    <IndexData Count=\"" << iBuf.size() << "\">";
    for(size_t i=0; i<iBuf.size(); ++i) { if(i>0)ss<<" "; ss<<iBuf[i]; }
    ss << "</IndexData>\n";
    ss << "    <IndexFormat>INDEX16</IndexFormat>\n    <MeshGroups>\n";
    for(const auto& g : groups) ss << "        <MeshGroup Name=\"" << g.name << "\" Material=\"" << g.material << "\">\n            <Faces Start=\"" << g.startIndex << "\" Count=\"" << g.indexCount << "\" />\n        </MeshGroup>\n";
    ss << "    </MeshGroups>\n    <VertexDecls>\n        <Decl Type=\"0\" Usage=\"0\" Method=\"0\" />\n        <Decl Type=\"2\" Usage=\"3\" Method=\"0\" />\n        <Decl Type=\"1\" Usage=\"5\" Method=\"0\" />\n    </VertexDecls>\n";

    float centerX = (minX + maxX) * 0.5f;
    float centerY = (minY + maxY) * 0.5f;
    float centerZ = (minZ + maxZ) * 0.5f;
    float radius = std::sqrt(std::pow(maxX - minX, 2) + std::pow(maxY - minY, 2) + std::pow(maxZ - minZ, 2)) * 0.5f;

    ss << "    <Bounds>\n        <Min x=\"" << minX << "\" y=\"" << minY << "\" z=\"" << minZ << "\" />\n        <Max x=\"" << maxX << "\" y=\"" << maxY << "\" z=\"" << maxZ << "\" />\n        <Center x=\"" << centerX << "\" y=\"" << centerY << "\" z=\"" << centerZ << "\" />\n        <Radius>" << radius << "</Radius>\n    </Bounds>\n</Mesh>";
    return ss.str();
}

// ----------------------------------------------------------------------------
// XML to Binary MSH Converter (FIXED)
// ----------------------------------------------------------------------------
std::vector<uint8_t> DAOImporter::ConvertXMLToMSH(const std::string& xmlContent) {
    std::cout << "[Converter] Parsing generated XML..." << std::endl;
    SimpleXMLNode root = SimpleXMLParser::Parse(xmlContent);
    if (root.name != "Mesh") {
        std::cerr << "Invalid XML: Root is <" << root.name << ">, expected <Mesh>" << std::endl;
        return {};
    }

    std::vector<float> vertices;
    std::vector<uint16_t> indices;
    float min[3] = {0}, max[3] = {0}, center[3] = {0};
    float radius = 0.0f;

    for (const auto& node : root.children) {
        if (node.name == "VertexData") {
            std::stringstream ss(node.content);
            float f; while(ss >> f) vertices.push_back(f);
        } else if (node.name == "IndexData") {
            std::stringstream ss(node.content);
            int i; while(ss >> i) indices.push_back((uint16_t)i);
        } else if (node.name == "Bounds") {
            for(const auto& b : node.children) {
                if (b.name == "Min") { min[0]=std::stof(b.attributes.at("x")); min[1]=std::stof(b.attributes.at("y")); min[2]=std::stof(b.attributes.at("z")); }
                if (b.name == "Max") { max[0]=std::stof(b.attributes.at("x")); max[1]=std::stof(b.attributes.at("y")); max[2]=std::stof(b.attributes.at("z")); }
                if (b.name == "Center") { center[0]=std::stof(b.attributes.at("x")); center[1]=std::stof(b.attributes.at("y")); center[2]=std::stof(b.attributes.at("z")); }
                if (b.name == "Radius") radius = std::stof(b.content);
            }
        }
    }

    // --- FIX: Use "MESH" filetype (all caps, but mapped to lowercase structs later) ---
    GFFBuilder gff("MESH"); // Matches "MSH " or "MESH" from reference

    std::vector<uint8_t> rawV(vertices.size() * 4);
    memcpy(rawV.data(), vertices.data(), rawV.size());
    std::vector<uint8_t> rawI(indices.size() * 2);
    memcpy(rawI.data(), indices.data(), rawI.size());

    // --- FIX: Use lowercase struct names like reference 'c_admpart_0.msh' ---
    uint32_t rootS = gff.AddStruct("mesh");
    uint32_t chunk = gff.AddStruct("chnk");
    uint32_t d1 = gff.AddStruct("decl");
    uint32_t d2 = gff.AddStruct("decl");
    uint32_t d3 = gff.AddStruct("decl");

    // --- FIX: Use UTF-16 String ---
    gff.AddStringField(chunk, 2, root.attributes.count("Name") ? root.attributes.at("Name") : "mesh");

    gff.AddUInt32Field(chunk, 8000, 32);
    gff.AddUInt32Field(chunk, 8001, (uint32_t)(vertices.size() / 8));
    gff.AddUInt32Field(chunk, 8002, (uint32_t)indices.size());
    gff.AddUInt32Field(chunk, 8003, 4);
    gff.AddUInt32Field(chunk, 8004, 0);
    gff.AddUInt32Field(chunk, 8008, (uint32_t)(vertices.size() / 8));

    gff.AddVector3Field(chunk, 8014, min[0], min[1], min[2]);
    gff.AddVector3Field(chunk, 8015, max[0], max[1], max[2]);
    gff.AddVector3Field(chunk, 8016, center[0], center[1], center[2]);
    gff.AddFloatField(chunk, 8017, radius);

    gff.AddUInt32Field(d1, 8027, 0);  gff.AddUInt32Field(d1, 8028, 2); gff.AddUInt32Field(d1, 8029, 0);
    gff.AddUInt32Field(d2, 8027, 12); gff.AddUInt32Field(d2, 8028, 2); gff.AddUInt32Field(d2, 8029, 3);
    gff.AddUInt32Field(d3, 8027, 24); gff.AddUInt32Field(d3, 8028, 1); gff.AddUInt32Field(d3, 8029, 5);

    gff.AddStructListField(chunk, 8025, 2, {d1, d2, d3});
    gff.AddStructListField(rootS, 8021, 1, {chunk});

    gff.AddBinaryField(rootS, 8022, rawV);
    gff.AddBinaryField(rootS, 8023, rawI);

    return gff.Build();
}

std::vector<uint8_t> DAOImporter::GenerateMSH(const DAOModelData& model) {
    std::string xml = GenerateMSH_XML(model);
    return ConvertXMLToMSH(xml);
}

// ----------------------------------------------------------------------------
// Helper: Generate PHY (Physics) Files
// ----------------------------------------------------------------------------
static std::vector<uint8_t> GeneratePHYHelper(const DAOModelData& model, const std::string& mshFilename) {
    std::cout << "[PHY] Generating PHY for: " << model.name << std::endl;
    GFFBuilder gff("PHY ");
    uint32_t root = gff.AddStruct("PHY ");
    uint32_t mshh = gff.AddStruct("mshh");

    gff.AddStringField(root, 6000, model.name);
    std::string mshBase = mshFilename;
    if (mshBase.find('.') != std::string::npos) mshBase = mshBase.substr(0, mshBase.find_last_of('.'));
    gff.AddStringField(root, 6005, mshBase);

    gff.AddStringField(mshh, 6000, model.name);
    gff.AddStringField(mshh, 6006, model.name);

    std::string mat = model.name;
    if (!model.parts.empty() && !model.parts[0].materialName.empty()) mat = model.parts[0].materialName;
    else if (!model.materials.empty()) mat = model.materials[0].name;

    // Enforce uhm_ prefix if needed by physics engine
    if (mat.find("uhm_") != 0) mat = "uhm_" + mat;

    gff.AddStringField(mshh, 6001, mat);
    gff.AddStructListField(root, 6999, 1, {mshh});

    return gff.Build();
}

// ----------------------------------------------------------------------------
// Helper: Generate MMH (Model Hierarchy) Files
// ----------------------------------------------------------------------------
std::vector<uint8_t> DAOImporter::GenerateMMH(const DAOModelData& model, const std::string& mshFilename) {
    std::cout << "[MMH] Generating MMH for: " << model.name << std::endl;
    GFFBuilder gff("MMH ");
    uint32_t root = gff.AddStruct("MMH ");
    uint32_t mshh = gff.AddStruct("mshh");

    gff.AddStringField(root, 6000, model.name);
    std::string mshBase = mshFilename;
    if (mshBase.find('.') != std::string::npos) mshBase = mshBase.substr(0, mshBase.find_last_of('.'));
    gff.AddStringField(root, 6005, mshBase);

    gff.AddStringField(mshh, 6000, model.name);
    gff.AddStringField(mshh, 6006, model.name);

    std::string mat = model.name;
    if (!model.parts.empty() && !model.parts[0].materialName.empty()) mat = model.parts[0].materialName;
    else if (!model.materials.empty()) mat = model.materials[0].name;

    if (mat.find("uhm_") != 0) mat = "uhm_" + mat;

    std::cout << "[MMH] Material assignment: " << mat << std::endl;

    gff.AddStringField(mshh, 6001, mat);
    gff.AddStructListField(root, 6999, 1, {mshh});

    return gff.Build();
}

// ----------------------------------------------------------------------------
// Helper: Generate MAO (Material Object) XML
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// ERF Repacker
// ----------------------------------------------------------------------------
enum class ERFVersion { Unknown, V20, V22, V30 };

bool DAOImporter::RepackERF(const std::string& erfPath, const std::map<std::string, std::vector<uint8_t>>& newFiles)
{
    if (!fs::exists(erfPath)) return false;
    uintmax_t fileSize = fs::file_size(erfPath);
    if (fileSize < 32) return false;

    std::vector<uint8_t> entireFile(fileSize);
    {
        std::ifstream inFile(erfPath, std::ios::binary);
        if (!inFile) return false;
        inFile.read(reinterpret_cast<char*>(entireFile.data()), fileSize);
    }

    ERFVersion version = ERFVersion::Unknown;
    bool utf16 = false;

    auto chkUtf16 = [&](const char* s) { for (int i=0; i<8; ++i) if (entireFile[i*2] != (uint8_t)s[i]) return false; return true; };
    auto chkAscii = [&](const char* s) { return memcmp(entireFile.data(), s, 8) == 0; };

    if (chkUtf16("ERF V2.0")) { version = ERFVersion::V20; utf16 = true; }
    else if (chkUtf16("ERF V2.2")) { version = ERFVersion::V22; utf16 = true; }
    else if (chkAscii("ERF V2.0")) { version = ERFVersion::V20; }
    else if (chkAscii("ERF V2.2")) { version = ERFVersion::V22; }

    if (version == ERFVersion::Unknown) return false;

    uint32_t hdrSize = (version == ERFVersion::V20) ? 32 : 48;
    uint32_t entrySize = (version == ERFVersion::V20) ? 72 : 76;
    uint32_t fcOff = utf16 ? 16 : 8;

    uint32_t fileCount, year, day, flags = 0, moduleId = 0;
    uint8_t digest[16] = {0};
    memcpy(&fileCount, &entireFile[fcOff], 4);
    memcpy(&year, &entireFile[fcOff + 4], 4);
    memcpy(&day, &entireFile[fcOff + 8], 4);
    if (version == ERFVersion::V22) {
        memcpy(&flags, &entireFile[fcOff + 16], 4);
        memcpy(&moduleId, &entireFile[fcOff + 20], 4);
        memcpy(digest, &entireFile[fcOff + 24], 16);
    }

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

    // Create Backup
    fs::path backupDir = fs::current_path() / "backups";
    std::string erfName = fs::path(erfPath).filename().string();
    fs::path backupPath = backupDir / (erfName + ".bak");

    if (!fs::exists(backupPath)) {
        bool shouldBackup = m_backupCallback ? m_backupCallback(erfName, backupDir.string()) : true;
        if (shouldBackup) {
            if (!fs::exists(backupDir)) fs::create_directories(backupDir);
            std::ofstream backupFile(backupPath, std::ios::binary);
            if (backupFile) backupFile.write(reinterpret_cast<const char*>(entireFile.data()), entireFile.size());
        }
    }

    std::vector<std::pair<std::string, std::vector<uint8_t>>> ordered(allFiles.begin(), allFiles.end());
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return ToLower(a.first) < ToLower(b.first); });

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

    std::ofstream outFile(erfPath, std::ios::binary | std::ios::trunc);
    if (!outFile) return false;
    outFile.write(reinterpret_cast<const char*>(out.data()), out.size());

    return true;
}

bool DAOImporter::ConvertAndAddToERF(const std::string& glbPath, const std::string& erfPath) {
    DAOModelData modelData;
    if (!LoadGLB(glbPath, modelData)) return false;

    std::map<std::string, std::vector<uint8_t>> files;
    std::string baseName = modelData.name;

    // Use the new XML-based pipeline
    files[baseName + ".msh"] = GenerateMSH(modelData);
    files[baseName + ".mmh"] = GenerateMMH(modelData, baseName + ".msh");
    files[baseName + ".phy"] = GeneratePHYHelper(modelData, baseName + ".msh");

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

bool DAOImporter::ImportToDirectory(const std::string& glbPath, const std::string& targetDir) {
    DAOModelData modelData;
    if (!LoadGLB(glbPath, modelData)) return false;

    std::string baseName = modelData.name;

    // dump the generated XML next to the GLB
    {
        std::string xml = GenerateMSH_XML(modelData);
        DumpMSHXmlNextToGLB(glbPath, baseName, xml);
    }

    // KEEP ONLY ONE OF THESE (delete the duplicate)
    auto saveFile = [&](const std::string& name, const std::vector<uint8_t>& data) {
        std::ofstream out(std::filesystem::path(targetDir) / name, std::ios::binary);
        if (out) out.write(reinterpret_cast<const char*>(data.data()), data.size());
    };

    saveFile(baseName + ".msh", GenerateMSH(modelData));
    saveFile(baseName + ".mmh", GenerateMMH(modelData, baseName + ".msh"));
    saveFile(baseName + ".phy", GeneratePHYHelper(modelData, baseName + ".msh"));

    for (const auto& mat : modelData.materials) {
        std::string xml = GenerateMAO(mat.name, mat.diffuseMap, mat.normalMap, mat.specularMap);
        std::vector<uint8_t> data(xml.begin(), xml.end());
        saveFile(mat.name + ".mao", data);
    }

    for (const auto& tex : modelData.textures) {
        if (tex.width > 0 && tex.height > 0 && !tex.data.empty() && !tex.ddsName.empty()) {
            saveFile(tex.ddsName, ConvertToDDS(tex.data, tex.width, tex.height, tex.channels));
        }
    }

    return true;
}

bool DAOImporter::LoadGLB(const std::string& path, DAOModelData& outData) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    if (!loader.LoadBinaryFromFile(&model, &err, &warn, path)) return false;
    outData.name = ToLower(fs::path(path).stem().string());

    std::cout << "[LoadGLB] Loading: " << path << std::endl;
    std::cout << "[LoadGLB] Name: " << outData.name << std::endl;

    for (size_t i = 0; i < model.images.size(); ++i) {
        const auto& img = model.images[i];
        if (img.width > 0 && img.height > 0 && !img.image.empty()) {
            DAOModelData::Texture tex;
            tex.originalName = img.uri.empty() ? (img.name.empty() ? "texture_" + std::to_string(i) : img.name) : img.uri;
            tex.ddsName = "";
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

        if (diffuseIdx >= 0 && diffuseIdx < (int)outData.textures.size()) outData.textures[diffuseIdx].ddsName = mat.diffuseMap;
        else mat.diffuseMap = "default_d.dds";

        if (normalIdx >= 0 && normalIdx < (int)outData.textures.size()) outData.textures[normalIdx].ddsName = mat.normalMap;
        else mat.normalMap = "default_n.dds";

        if (specIdx >= 0 && specIdx < (int)outData.textures.size()) outData.textures[specIdx].ddsName = mat.specularMap;
        else mat.specularMap = "default_spec.dds";

        outData.materials.push_back(mat);
        std::cout << "[LoadGLB] Material " << i << ": " << mat.name << std::endl;
    }

    if (outData.materials.empty()) {
        DAOModelData::Material defaultMat;
        defaultMat.name = outData.name;
        defaultMat.diffuseMap = "default_d.dds";
        defaultMat.normalMap = "default_n.dds";
        defaultMat.specularMap = "default_spec.dds";
        outData.materials.push_back(defaultMat);
        std::cout << "[LoadGLB] Created default material: " << defaultMat.name << std::endl;
    }

    for (const auto& mesh : model.meshes) {
        std::cout << "[LoadGLB] Processing mesh: " << (mesh.name.empty() ? "(unnamed)" : mesh.name) << std::endl;
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
            std::cout << "[LoadGLB]   Primitive: " << part.vertices.size() << " verts, " << part.indices.size() << " indices, mat: " << part.materialName << std::endl;
            outData.parts.push_back(part);
        }
    }
    return true;
}