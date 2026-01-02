#include "tnt_loader.h"
#include <cstring>
#include <iostream>
#include <algorithm>

static float readFloat(const uint8_t* data, size_t offset) {
    float val;
    memcpy(&val, &data[offset], 4);
    return val;
}

TintColor TintData::getPrimaryColor() const {
    if (numColors >= 10) {
        const TintColor& c9 = colors[9];
        if (c9.r > 0.01f && c9.g > 0.01f && c9.b > 0.01f &&
            (c9.r < 0.99f || c9.g < 0.99f || c9.b < 0.99f)) {
            return c9;
        }
    }
    if (numColors >= 3) {
        const TintColor& c2 = colors[2];
        if (c2.r > 0.01f || c2.g > 0.01f || c2.b > 0.01f) {
            if (c2.r < 0.99f || c2.g < 0.99f || c2.b < 0.99f) {
                return c2;
            }
        }
    }
    if (numColors >= 9) {
        const TintColor& c8 = colors[8];
        if (c8.r > 0.01f || c8.g > 0.01f || c8.b > 0.01f) {
            if (c8.r < 0.99f || c8.g < 0.99f || c8.b < 0.99f) {
                return c8;
            }
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

    if (data.size() < 0xC0) {
        std::cout << "[TNT] File too small: " << data.size() << " bytes" << std::endl;
        return false;
    }
    if (memcmp(data.data(), "GFF V4.0", 8) != 0) {
        std::cout << "[TNT] Not a GFF V4.0 file" << std::endl;
        return false;
    }
    
    const uint8_t* ptr = data.data();
    size_t colorStart = 0xB0;
    size_t maxColors = (data.size() - colorStart) / 16;
    
    if (maxColors > 10) maxColors = 10;
    
    outTint.numColors = (int)maxColors;
    
    for (size_t i = 0; i < maxColors; i++) {
        size_t offset = colorStart + i * 16;
        outTint.colors[i].r = readFloat(ptr, offset);
        outTint.colors[i].g = readFloat(ptr, offset + 4);
        outTint.colors[i].b = readFloat(ptr, offset + 8);
        outTint.colors[i].a = readFloat(ptr, offset + 12);
    }
    
    return true;
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