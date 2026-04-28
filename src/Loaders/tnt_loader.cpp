#include "tnt_loader.h"
#include <cstring>
#include <algorithm>
static float readFloat(const uint8_t* data, size_t offset) {
    float val;
    memcpy(&val, &data[offset], 4);
    return val;
}
TintColor TintData::getPrimaryColor() const {
    if (numColors >= 3) {
        const TintColor& c2 = colors[2];
        bool isWhite = (c2.r > 0.99f && c2.g > 0.99f && c2.b > 0.99f);
        if (!isWhite) {
            return c2;
        }
    }
    if (numColors >= 9) {
        const TintColor& c8 = colors[8];
        bool isBlack = (c8.r < 0.01f && c8.g < 0.01f && c8.b < 0.01f);
        bool isWhite = (c8.r > 0.99f && c8.g > 0.99f && c8.b > 0.99f);
        if (!isBlack && !isWhite) {
            return c8;
        }
    }
    for (int i = 0; i < numColors; i++) {
        const TintColor& c = colors[i];
        bool isWhite = (c.r > 0.99f && c.g > 0.99f && c.b > 0.99f);
        if (!isWhite) {
            return c;
        }
    }
    return TintColor();
}
TintColor TintData::getSecondaryColor() const {
    if (numColors >= 9) {
        return colors[8];
    }
    return TintColor();
}
bool loadTNT(const std::vector<uint8_t>& data, TintData& outTint) {
    outTint = TintData();
    if (data.size() < 0x30) {
        return false;
    }
    if (memcmp(data.data(), "GFF V4.0", 8) != 0) {
        return false;
    }
    const uint8_t* ptr = data.data();

    // GFF V4.0 header layout:
    //   0x14: number of structs (we expect 1)
    //   0x18: data section offset (typically 0xB0)
    //   0x1C: struct type tag ("GFF ")
    //   0x20: number of fields in struct
    //   0x24: field defs offset (relative to file start)
    // Each field def is 12 bytes: { uint32 label, uint32 type, uint32 dataOffset }
    //   - `label` for tint colours is the GFF ID, e.g. 14000 = TINT_MASK_DIFFUSE_R
    //   - `dataOffset` is RELATIVE to the data section start (header[0x18])
    //
    // The fields are NOT guaranteed to be stored in the data section in field-ID
    // order, so we MUST look up each field by its label and pull from its
    // computed absolute offset. Reading sequential 16-byte chunks would be wrong.

    uint32_t dataSectionOff;
    uint32_t numFields;
    uint32_t fieldDefsOff;
    memcpy(&dataSectionOff, ptr + 0x18, 4);
    memcpy(&numFields, ptr + 0x20, 4);
    memcpy(&fieldDefsOff, ptr + 0x24, 4);

    if (dataSectionOff + 16 > data.size()) return false;
    if (fieldDefsOff + numFields * 12 > data.size()) return false;
    if (numFields > 64) return false; // sanity cap

    // Field IDs we care about (in the order the rest of the codebase expects):
    //   colors[0] = TINT_MASK_DIFFUSE_R  (14000)
    //   colors[1] = TINT_MASK_DIFFUSE_G  (14001)
    //   colors[2] = TINT_MASK_DIFFUSE_B  (14002)
    //   colors[3] = TINT_MASK_SPECULAR_R (14003)
    //   colors[4] = TINT_MASK_SPECULAR_G (14004)
    //   colors[5] = TINT_MASK_SPECULAR_B (14005)
    //   colors[6] = TINT_MASK_DIFFUSE_A  (14006)
    //   colors[7] = TINT_MASK_SPECULAR_A (14007)
    //   colors[8] = TINT_MASK_DIFFUSE_OPACITY  (14008)
    //   colors[9] = TINT_MASK_SPECULAR_OPACITY (14009)
    int loaded = 0;
    for (uint32_t i = 0; i < numFields; ++i) {
        size_t entryOff = fieldDefsOff + i * 12;
        uint32_t label, ftype, dataOff;
        memcpy(&label,   ptr + entryOff + 0, 4);
        memcpy(&ftype,   ptr + entryOff + 4, 4);
        memcpy(&dataOff, ptr + entryOff + 8, 4);
        (void)ftype;

        // Map field ID 14000..14009 -> colors[0..9].
        if (label < 14000 || label > 14009) continue;
        int slot = (int)(label - 14000);

        size_t absOff = dataSectionOff + dataOff;
        if (absOff + 16 > data.size()) continue;

        outTint.colors[slot].r = readFloat(ptr, absOff + 0);
        outTint.colors[slot].g = readFloat(ptr, absOff + 4);
        outTint.colors[slot].b = readFloat(ptr, absOff + 8);
        outTint.colors[slot].a = readFloat(ptr, absOff + 12);
        if (slot >= loaded) loaded = slot + 1;
    }
    outTint.numColors = loaded;
    return loaded > 0;
}
const TintData* TintCache::getTint(const std::string& name) const {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    auto it = m_tints.find(lower);
    if (it != m_tints.end()) {
        return &it->second;
    }
    return nullptr;
}
void TintCache::addTint(const std::string& name, const TintData& tint) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    m_tints[lower] = tint;
    m_tints[lower].name = name;
}
bool TintCache::hasTint(const std::string& name) const {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return m_tints.find(lower) != m_tints.end();
}
void TintCache::clear() {
    m_tints.clear();
}
std::vector<std::string> TintCache::getTintNames() const {
    std::vector<std::string> names;
    for (const auto& [name, tint] : m_tints) {
        names.push_back(name);
    }
    return names;
}