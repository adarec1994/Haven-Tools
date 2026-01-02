#include "mor_loader.h"
#include <algorithm>
#include <cstring>
#include <iostream>

static std::string readUTF16(const uint8_t* data, size_t maxSize, size_t offset, size_t maxChars) {
    std::string result;
    for (size_t i = 0; i < maxChars && offset + i * 2 + 1 < maxSize; i++) {
        uint16_t ch = data[offset + i * 2] | (data[offset + i * 2 + 1] << 8);
        if (ch == 0) break;
        if (ch < 128) result += (char)ch;
    }
    return result;
}

static uint32_t readU32(const uint8_t* data, size_t offset) {
    return data[offset] | (data[offset+1] << 8) | (data[offset+2] << 16) | (data[offset+3] << 24);
}

static float readFloat(const uint8_t* data, size_t offset) {
    float val;
    memcpy(&val, &data[offset], 4);
    return val;
}

static void parseTargetName(const std::string& name, std::string& category, int& index) {
    category = name;
    index = 1;
    size_t mPos = name.find('M');
    if (mPos != std::string::npos && mPos > 0 && mPos + 1 < name.size()) {
        char c = name[mPos + 1];
        if (c >= '0' && c <= '9') {
            category = name.substr(0, mPos);
            index = c - '0';
        }
    }
}

static bool isMorphTargetName(const std::string& s) {
    if (s.length() < 5 || s.length() > 10) return false;
    if (s.find('M') == std::string::npos) return false;
    return (s.find("Face") == 0 || s.find("Eyes") == 0 || s.find("Lashes") == 0 ||
            s.find("Hair") == 0 || s.find("Beard") == 0);
}

const MorphMeshTarget* MorphData::findTarget(const std::string& name) const {
    for (const auto& t : meshTargets) {
        if (t.name == name) return &t;
    }
    return nullptr;
}

MorphMeshTarget* MorphData::findTarget(const std::string& name) {
    for (auto& t : meshTargets) {
        if (t.name == name) return &t;
    }
    return nullptr;
}

const MorphMeshTarget* MorphData::getFaceTarget() const {
    return findTarget("FaceM1");
}

const MorphMeshTarget* MorphData::getEyesTarget() const {
    return findTarget("EyesM1");
}

const MorphMeshTarget* MorphData::getLashesTarget() const {
    return findTarget("LashesM1");
}

void debugPrintMorph(const MorphData& morph) {
    std::cout << "=== MORPH DATA: " << morph.name << " ===" << std::endl;
    std::cout << "Model refs: " << morph.modelRefs.size() << std::endl;
    for (const auto& r : morph.modelRefs) std::cout << "  - " << r << std::endl;
    std::cout << "Mesh targets: " << morph.meshTargets.size() << std::endl;
    for (const auto& t : morph.meshTargets) {
        std::cout << "  - " << t.name << ": " << t.vertices.size() << " vertices" << std::endl;
        if (!t.vertices.empty()) {
            const auto& v = t.vertices[0];
            std::cout << "    First vertex: (" << v.x << ", " << v.y << ", " << v.z << ")" << std::endl;
        }
    }
}

void applyMorphBlend(
    const std::vector<MorphVertex>& baseVertices,
    const std::vector<MorphVertex>& morphVertices,
    float amount,
    std::vector<MorphVertex>& outVertices)
{
    size_t count = std::min(baseVertices.size(), morphVertices.size());
    outVertices.resize(count);

    float invAmount = 1.0f - amount;

    for (size_t i = 0; i < count; i++) {
        outVertices[i].x = baseVertices[i].x * invAmount + morphVertices[i].x * amount;
        outVertices[i].y = baseVertices[i].y * invAmount + morphVertices[i].y * amount;
        outVertices[i].z = baseVertices[i].z * invAmount + morphVertices[i].z * amount;
    }
}

bool loadMOR(const std::vector<uint8_t>& data, MorphData& outMorph) {
    outMorph = MorphData();

    if (data.size() < 32) {
        std::cout << "[MOR] File too small: " << data.size() << " bytes" << std::endl;
        return false;
    }

    if (memcmp(data.data(), "GFF V4.0", 8) != 0) {
        std::cout << "[MOR] Not a GFF V4.0 file" << std::endl;
        return false;
    }

    const uint8_t* ptr = data.data();
    size_t size = data.size();
    std::cout << "[MOR] Parsing " << size << " bytes" << std::endl;

    struct TargetPattern {
        const char* utf16;
        size_t len;
        const char* name;
    };

    std::vector<std::pair<size_t, std::string>> targetPositions;

    const uint8_t* faceM1 = (const uint8_t*)"F\x00""a\x00""c\x00""e\x00""M\x00""1\x00";
    const uint8_t* eyesM1 = (const uint8_t*)"E\x00""y\x00""e\x00""s\x00""M\x00""1\x00";
    const uint8_t* lashesM1 = (const uint8_t*)"L\x00""a\x00""s\x00""h\x00""e\x00""s\x00""M\x00""1\x00";
    const uint8_t* hairM1 = (const uint8_t*)"H\x00""a\x00""i\x00""r\x00""M\x00""1\x00";
    const uint8_t* hairM2 = (const uint8_t*)"H\x00""a\x00""i\x00""r\x00""M\x00""2\x00";
    const uint8_t* hairM3 = (const uint8_t*)"H\x00""a\x00""i\x00""r\x00""M\x00""3\x00";
    const uint8_t* beardM1 = (const uint8_t*)"B\x00""e\x00""a\x00""r\x00""d\x00""M\x00""1\x00";
    const uint8_t* beardM2 = (const uint8_t*)"B\x00""e\x00""a\x00""r\x00""d\x00""M\x00""2\x00";
    const uint8_t* beardM3 = (const uint8_t*)"B\x00""e\x00""a\x00""r\x00""d\x00""M\x00""3\x00";

    struct SearchPattern {
        const uint8_t* pattern;
        size_t patternLen;
        std::string name;
    };

    std::vector<SearchPattern> patterns = {
        {faceM1, 12, "FaceM1"},
        {eyesM1, 12, "EyesM1"},
        {lashesM1, 16, "LashesM1"},
        {hairM1, 12, "HairM1"},
        {hairM2, 12, "HairM2"},
        {hairM3, 12, "HairM3"},
        {beardM1, 14, "BeardM1"},
        {beardM2, 14, "BeardM2"},
        {beardM3, 14, "BeardM3"},
    };

    for (const auto& sp : patterns) {
        for (size_t pos = 0; pos + sp.patternLen < size; pos++) {
            if (memcmp(ptr + pos, sp.pattern, sp.patternLen) == 0) {
                if (pos + sp.patternLen + 2 < size &&
                    ptr[pos + sp.patternLen] == 0 && ptr[pos + sp.patternLen + 1] == 0) {
                    targetPositions.push_back({pos, sp.name});
                    break;
                }
            }
        }
    }

    std::sort(targetPositions.begin(), targetPositions.end());

    std::cout << "[MOR] Found " << targetPositions.size() << " morph targets" << std::endl;

    for (size_t i = 0; i < targetPositions.size(); i++) {
        size_t pos = targetPositions[i].first;
        const std::string& name = targetPositions[i].second;

        size_t nameEnd = pos;
        while (nameEnd + 2 < size && (ptr[nameEnd] != 0 || ptr[nameEnd + 1] != 0)) {
            nameEnd += 2;
        }
        nameEnd += 2;
        size_t vertexStart = nameEnd;

        while (vertexStart + 2 < size && ptr[vertexStart] == 0xFF && ptr[vertexStart + 1] == 0xFF) {
            vertexStart += 2;
        }

        if (vertexStart + 4 < size) {
            uint32_t floatCount = readU32(ptr, vertexStart);

            if (floatCount > 0 && floatCount < 50000 && floatCount % 4 == 0) {
                size_t vertexCount = floatCount / 4;
                size_t dataStart = vertexStart + 4;

                if (dataStart + floatCount * 4 <= size) {
                    MorphMeshTarget target;
                    target.name = name;
                    parseTargetName(name, target.category, target.index);
                    target.vertices.resize(vertexCount);

                    for (size_t v = 0; v < vertexCount; v++) {
                        size_t offset = dataStart + v * 16;
                        target.vertices[v].x = readFloat(ptr, offset);
                        target.vertices[v].y = readFloat(ptr, offset + 4);
                        target.vertices[v].z = readFloat(ptr, offset + 8);
                    }

                    std::cout << "[MOR] " << name << ": " << vertexCount << " vertices at 0x"
                              << std::hex << dataStart << std::dec << std::endl;

                    outMorph.meshTargets.push_back(std::move(target));
                }
            }
        }
    }

    for (size_t pos = 0; pos + 30 < size; pos++) {
        uint32_t strLen = readU32(ptr, pos);
        if (strLen >= 10 && strLen <= 20) {
            std::string s = readUTF16(ptr, size, pos + 4, strLen);
            std::string sLower = s;
            std::transform(sLower.begin(), sLower.end(), sLower.begin(), ::tolower);

            if ((sLower.find("_uhm_") != std::string::npos ||
                 sLower.find("_uem_") != std::string::npos ||
                 sLower.find("_ulm_") != std::string::npos ||
                 sLower.find("_har_") != std::string::npos) &&
                sLower.find("_bas") != std::string::npos) {
                bool found = false;
                for (const auto& r : outMorph.modelRefs) {
                    if (r == s) { found = true; break; }
                }
                if (!found) {
                    outMorph.modelRefs.push_back(s);
                }
            }
            if (sLower.find("_pcc_") != std::string::npos) {
                outMorph.name = s;
            }
        }
    }

    std::cout << "[MOR] Loaded " << outMorph.meshTargets.size() << " mesh targets, "
              << outMorph.modelRefs.size() << " model refs" << std::endl;

    return !outMorph.meshTargets.empty();
}