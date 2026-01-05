#include <import.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <cerrno>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

namespace fs = std::filesystem;

class GFFBuilder {
public:
    struct Field {
        uint32_t label;
        uint16_t type;
        uint16_t flags;
        std::vector<uint8_t> data;
        uint32_t structIndex = 0xFFFFFFFF;
    };

    struct Struct {
        uint32_t type;
        std::vector<Field> fields;
    };

    enum GFFType { GFF_UINT32 = 4, GFF_STRING = 14 };

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
        Field f; f.label = label; f.type = type; f.flags = 0;
        f.data.resize(size); std::memcpy(f.data.data(), data, size);
        m_structs[structIdx].fields.push_back(f);
    }

    void AddStringField(uint32_t structIdx, uint32_t label, const std::string& str) {
        m_queuedStrings.push_back({structIdx, (uint32_t)m_structs[structIdx].fields.size(), str});
        Field f; f.label = label; f.type = GFF_STRING; f.flags = 0; f.data.resize(4);
        m_structs[structIdx].fields.push_back(f);
    }

    void AddListField(uint32_t structIdx, uint32_t label, const std::vector<uint32_t>& childIndices) {
        m_queuedLists.push_back({structIdx, (uint32_t)m_structs[structIdx].fields.size(), childIndices});
        Field f; f.label = label; f.type = 0; f.flags = 1; f.data.resize(4);
        m_structs[structIdx].fields.push_back(f);
    }

    void AddBinaryField(uint32_t structIdx, uint32_t label, const std::vector<uint8_t>& buffer) {
        m_queuedBinary.push_back({structIdx, (uint32_t)m_structs[structIdx].fields.size(), buffer});
        Field f; f.label = label; f.type = GFF_UINT32; f.flags = 0; f.data.resize(4);
        m_structs[structIdx].fields.push_back(f);
    }

    std::vector<uint8_t> Build() {
        std::vector<uint8_t> buffer;
        auto write32 = [&](uint32_t v) { uint8_t b[4]; memcpy(b, &v, 4); buffer.insert(buffer.end(), b, b+4); };

        write32(0x47464620); write32(0x342E3056); write32(0); write32(m_fileType); write32(0);
        write32((uint32_t)m_structs.size()); write32(0);

        uint32_t currentFieldOffset = 0;
        std::vector<uint32_t> fieldOffsets;
        for (const auto& s : m_structs) {
            write32(s.type); write32((uint32_t)s.fields.size()); write32(currentFieldOffset);
            fieldOffsets.push_back(currentFieldOffset);
            write32((uint32_t)s.fields.size() * 12);
            currentFieldOffset += (uint32_t)s.fields.size() * 12;
        }

        uint32_t absFieldsStart = (uint32_t)buffer.size();
        for (size_t i = 0; i < m_structs.size(); i++) {
            uint32_t abs = absFieldsStart + fieldOffsets[i];
            memcpy(&buffer[28 + (i * 16) + 8], &abs, 4);
        }

        std::vector<uint8_t> dataBlock;
        auto appendData = [&](const void* d, size_t s) -> uint32_t {
            uint32_t off = (uint32_t)dataBlock.size();
            const uint8_t* b = (const uint8_t*)d;
            dataBlock.insert(dataBlock.end(), b, b+s);
            return off;
        };

        for (auto& q : m_queuedStrings) {
            uint32_t len = (uint32_t)q.str.length();
            uint32_t off = appendData(&len, 4);
            dataBlock.insert(dataBlock.end(), q.str.begin(), q.str.end());
            memcpy(m_structs[q.structIdx].fields[q.fieldIdx].data.data(), &off, 4);
        }
        for (auto& q : m_queuedBinary) {
             uint32_t off = appendData(q.data.data(), q.data.size());
             memcpy(m_structs[q.structIdx].fields[q.fieldIdx].data.data(), &off, 4);
        }
        for (auto& q : m_queuedLists) {
            uint32_t count = (uint32_t)q.childIndices.size();
            uint32_t off = appendData(&count, 4);
            for (uint32_t idx : q.childIndices) appendData(&idx, 4);
             memcpy(m_structs[q.structIdx].fields[q.fieldIdx].data.data(), &off, 4);
        }

        for (const auto& s : m_structs) {
            for (const auto& f : s.fields) {
                write32(f.label);
                uint16_t tf[2] = {f.type, f.flags};
                uint8_t b[4]; memcpy(b, tf, 4); buffer.insert(buffer.end(), b, b+4);
                if (f.data.size() == 4) buffer.insert(buffer.end(), f.data.begin(), f.data.end());
                else { uint32_t off = appendData(f.data.data(), f.data.size()); write32(off); }
            }
        }

        uint32_t dataOffsetVal = (uint32_t)buffer.size();
        memcpy(&buffer[24], &dataOffsetVal, 4);
        buffer.insert(buffer.end(), dataBlock.begin(), dataBlock.end());
        return buffer;
    }

private:
    uint32_t m_fileType;
    struct QueuedString { uint32_t structIdx; uint32_t fieldIdx; std::string str; };
    struct QueuedList { uint32_t structIdx; uint32_t fieldIdx; std::vector<uint32_t> childIndices; };
    struct QueuedBinary { uint32_t structIdx; uint32_t fieldIdx; std::vector<uint8_t> data; };
    std::vector<Struct> m_structs;
    std::vector<QueuedString> m_queuedStrings;
    std::vector<QueuedList> m_queuedLists;
    std::vector<QueuedBinary> m_queuedBinary;
};

DAOImporter::DAOImporter() {}
DAOImporter::~DAOImporter() {}

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
            if (entry.is_regular_file()) {
                if (ToLower(entry.path().filename().string()) == ToLower(filename)) {
                    return entry.path().string();
                }
            }
        }
    } catch (...) {}
    return "";
}

static std::vector<uint8_t> ConvertToDDS(const std::vector<uint8_t>& imageData, int width, int height, int channels) {
    std::vector<uint8_t> dds;

    dds.push_back('D'); dds.push_back('D'); dds.push_back('S'); dds.push_back(' ');

    auto writeU32 = [&](uint32_t v) {
        dds.push_back(v & 0xFF);
        dds.push_back((v >> 8) & 0xFF);
        dds.push_back((v >> 16) & 0xFF);
        dds.push_back((v >> 24) & 0xFF);
    };

    writeU32(124);
    uint32_t flags = 0x1 | 0x2 | 0x4 | 0x1000;
    writeU32(flags);
    writeU32(height);
    writeU32(width);
    writeU32(width * 4);
    writeU32(0);
    writeU32(1);
    for (int i = 0; i < 11; ++i) writeU32(0);

    writeU32(32);
    writeU32(0x41);
    writeU32(0);
    writeU32(32);
    writeU32(0x00FF0000);
    writeU32(0x0000FF00);
    writeU32(0x000000FF);
    writeU32(0xFF000000);

    writeU32(0x1000);
    for (int i = 0; i < 4; ++i) writeU32(0);

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

bool DAOImporter::ImportToDirectory(const std::string& glbPath, const std::string& targetDir) {
    std::cout << "[Import] Processing: " << fs::path(glbPath).filename().string() << std::endl;

    DAOModelData modelData;
    if (!LoadGLB(glbPath, modelData)) return false;

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

    std::string baseName = ToLower(fs::path(glbPath).stem().string());

    std::map<std::string, std::vector<uint8_t>> meshFiles, hierFiles, matFiles, texFiles;

    std::string mshFile = baseName + ".msh";
    meshFiles[mshFile] = GenerateMSH(modelData);
    std::cout << "  + Generated: " << mshFile << " (" << meshFiles[mshFile].size() << " bytes)" << std::endl;

    std::string mmhFile = baseName + ".mmh";
    hierFiles[mmhFile] = GenerateMMH(modelData, mshFile);
    std::cout << "  + Generated: " << mmhFile << " (" << hierFiles[mmhFile].size() << " bytes)" << std::endl;

    for (const auto& tex : modelData.textures) {
        if (tex.width > 0 && tex.height > 0 && !tex.data.empty()) {
            std::string ddsName = tex.ddsName;
            std::vector<uint8_t> ddsData = ConvertToDDS(tex.data, tex.width, tex.height, tex.channels);
            texFiles[ddsName] = std::move(ddsData);
            std::cout << "  + Generated texture: " << ddsName << " (" << tex.width << "x" << tex.height << ")" << std::endl;
        }
    }

    for (const auto& mat : modelData.materials) {
        std::string maoFile = mat.name + ".mao";
        std::string xml = GenerateMAO(mat.name, mat.diffuseMap, mat.normalMap, mat.specularMap);
        matFiles[maoFile].assign(xml.begin(), xml.end());
        std::cout << "  + Generated: " << maoFile << std::endl;
        std::cout << "      diffuse:  " << mat.diffuseMap << std::endl;
        std::cout << "      normal:   " << mat.normalMap << std::endl;
        std::cout << "      specular: " << mat.specularMap << std::endl;
    }

    std::cout << "\n[Import] Updating ERF files..." << std::endl;

    bool ok1 = RepackERF(meshErf, meshFiles);
    bool ok2 = RepackERF(hierErf, hierFiles);
    bool ok3 = RepackERF(matErf, matFiles);
    bool ok4 = true;
    if (!texErf.empty() && !texFiles.empty()) {
        ok4 = RepackERF(texErf, texFiles);
    }

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

    std::map<int, std::string> imageIndexToDdsName;
    for (size_t i = 0; i < model.images.size(); ++i) {
        const auto& img = model.images[i];
        std::string srcName = img.uri.empty() ? img.name : img.uri;
        if (srcName.empty()) srcName = "texture_" + std::to_string(i);
        std::string ddsName = CleanName(srcName) + ".dds";
        imageIndexToDdsName[(int)i] = ddsName;

        if (img.width > 0 && img.height > 0 && !img.image.empty()) {
            DAOModelData::Texture tex;
            tex.originalName = srcName;
            tex.ddsName = ddsName;
            tex.width = img.width;
            tex.height = img.height;
            tex.channels = img.component;
            tex.data = img.image;
            outData.textures.push_back(tex);
        }
    }

    auto getTextureDdsName = [&](int textureIndex) -> std::string {
        if (textureIndex < 0 || textureIndex >= (int)model.textures.size()) return "";
        int imgIdx = model.textures[textureIndex].source;
        if (imageIndexToDdsName.count(imgIdx)) return imageIndexToDdsName[imgIdx];
        return "";
    };

    for (size_t i = 0; i < model.materials.size(); ++i) {
        const auto& gltfMat = model.materials[i];
        DAOModelData::Material mat;
        mat.name = CleanName(gltfMat.name.empty() ? "material_" + std::to_string(i) : gltfMat.name);

        if (gltfMat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
            mat.diffuseMap = getTextureDdsName(gltfMat.pbrMetallicRoughness.baseColorTexture.index);
        }
        if (gltfMat.normalTexture.index >= 0) {
            mat.normalMap = getTextureDdsName(gltfMat.normalTexture.index);
        }
        if (gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
            mat.specularMap = getTextureDdsName(gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index);
        }
        if (gltfMat.occlusionTexture.index >= 0 && mat.specularMap.empty()) {
            mat.specularMap = getTextureDdsName(gltfMat.occlusionTexture.index);
        }

        if (mat.diffuseMap.empty()) mat.diffuseMap = "default_diff.dds";
        if (mat.normalMap.empty()) mat.normalMap = "default_norm.dds";
        if (mat.specularMap.empty()) mat.specularMap = "default_spec.dds";

        outData.materials.push_back(mat);
    }

    if (outData.materials.empty()) {
        DAOModelData::Material defaultMat;
        defaultMat.name = outData.name;
        defaultMat.diffuseMap = "default_diff.dds";
        defaultMat.normalMap = "default_norm.dds";
        defaultMat.specularMap = "default_spec.dds";
        outData.materials.push_back(defaultMat);
    }

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
    GFFBuilder gff(541609293);
    uint32_t root = gff.AddStruct("MMH ");
    gff.AddStringField(root, 6000, model.name);

    std::string mshBase = mshFilename;
    if (mshBase.find('.') != std::string::npos) mshBase = mshBase.substr(0, mshBase.find_last_of('.'));
    gff.AddStringField(root, 6005, mshBase);

    uint32_t mshh = gff.AddStruct("mshh");
    gff.AddStringField(mshh, 6000, model.name);
    gff.AddStringField(mshh, 6006, model.name);

    std::string mat = model.name;
    if (!model.parts.empty() && !model.parts[0].materialName.empty()) {
        mat = model.parts[0].materialName;
    } else if (!model.materials.empty()) {
        mat = model.materials[0].name;
    }

    gff.AddStringField(mshh, 6001, mat);
    gff.AddListField(root, 6999, {mshh});
    return gff.Build();
}

std::vector<uint8_t> DAOImporter::GenerateMSH(const DAOModelData& model) {
    GFFBuilder gff(1213416781);
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

    std::vector<uint8_t> rawV(vBuf.size()*4), rawI(iBuf.size()*2);
    memcpy(rawV.data(), vBuf.data(), rawV.size());
    memcpy(rawI.data(), iBuf.data(), rawI.size());

    uint32_t root = gff.AddStruct("MSH ");
    uint32_t chunk = gff.AddStruct("msgr");
    gff.AddStringField(chunk, 2, model.name);

    uint32_t vCount = (uint32_t)vBuf.size()/8;
    uint32_t vSize = 32;
    uint32_t iCount = (uint32_t)iBuf.size();
    uint32_t pType = 4;

    gff.AddField(chunk, 8000, 4, &vSize, 4);
    gff.AddField(chunk, 8001, 4, &vCount, 4);
    gff.AddField(chunk, 8002, 4, &iCount, 4);
    gff.AddField(chunk, 8003, 4, &pType, 4);

    uint32_t d1 = gff.AddStruct("decl");
    uint32_t t2=2, u0=0;
    gff.AddField(d1, 8028, 4, &t2, 4); gff.AddField(d1, 8029, 4, &u0, 4);

    uint32_t d2 = gff.AddStruct("decl");
    uint32_t u3=3, o12=12;
    gff.AddField(d2, 8028, 4, &t2, 4); gff.AddField(d2, 8029, 4, &u3, 4); gff.AddField(d2, 8027, 4, &o12, 4);

    uint32_t d3 = gff.AddStruct("decl");
    uint32_t t1=1, u5=5, o24=24;
    gff.AddField(d3, 8028, 4, &t1, 4); gff.AddField(d3, 8029, 4, &u5, 4); gff.AddField(d3, 8027, 4, &o24, 4);

    gff.AddListField(chunk, 8025, {d1, d2, d3});
    gff.AddListField(root, 8021, {chunk});
    gff.AddBinaryField(root, 8022, rawV);
    gff.AddBinaryField(root, 8023, rawI);

    return gff.Build();
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
        std::cerr << "[RepackERF] ERROR: File corrupted (too small). Restore via Steam." << std::endl;
        return false;
    }

    std::vector<uint8_t> entireFile(fileSize);
    {
        std::ifstream inFile(erfPath, std::ios::binary);
        if (!inFile) { std::cerr << "[RepackERF] ERROR: Cannot read" << std::endl; return false; }
        inFile.read(reinterpret_cast<char*>(entireFile.data()), fileSize);
    }

    ERFVersion version = ERFVersion::Unknown;
    bool utf16 = false;

    auto chkUtf16 = [&](const char* s) {
        for (int i = 0; i < 8; ++i) if (entireFile[i*2] != (uint8_t)s[i] || entireFile[i*2+1] != 0) return false;
        return true;
    };
    auto chkAscii = [&](const char* s) { return std::memcmp(entireFile.data(), s, 8) == 0; };

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
    std::memcpy(&fileCount, &entireFile[fcOff], 4);
    std::memcpy(&year, &entireFile[fcOff + 4], 4);
    std::memcpy(&day, &entireFile[fcOff + 8], 4);
    if (version == ERFVersion::V22) {
        std::memcpy(&flags, &entireFile[fcOff + 16], 4);
        std::memcpy(&moduleId, &entireFile[fcOff + 20], 4);
        std::memcpy(digest, &entireFile[fcOff + 24], 16);
    }

    std::cout << "[RepackERF] Format: V" << (version == ERFVersion::V20 ? "2.0" : "2.2")
              << ", entries: " << fileCount << std::endl;

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

        std::memcpy(&offset, &entireFile[pos], 4); pos += 4;
        std::memcpy(&size, &entireFile[pos], 4); pos += 4;
        if (version == ERFVersion::V22) pos += 4;

        bool skip = false;
        for (const auto& [n, _] : newFiles) if (ToLower(n) == ToLower(name)) { skip = true; break; }

        if (!skip && allFiles.find(name) == allFiles.end() && offset + size <= fileSize) {
            allFiles[name] = std::vector<uint8_t>(entireFile.begin() + offset, entireFile.begin() + offset + size);
        }
    }

    std::cout << "[RepackERF] Total after merge: " << allFiles.size() << std::endl;

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
        if (tex.width > 0 && tex.height > 0 && !tex.data.empty()) {
            files[tex.ddsName] = ConvertToDDS(tex.data, tex.width, tex.height, tex.channels);
        }
    }

    return RepackERF(erfPath, files);
}