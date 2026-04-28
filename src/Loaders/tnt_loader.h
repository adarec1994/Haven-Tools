#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <map>

struct TintColor {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct TintData {
    std::string name;
    TintColor colors[10];
    int numColors = 0;

    TintColor getPrimaryColor() const;

    TintColor getSecondaryColor() const;
};

bool loadTNT(const std::vector<uint8_t>& data, TintData& outTint);

class TintCache {
public:

    const TintData* getTint(const std::string& name) const;

    void addTint(const std::string& name, const TintData& tint);

    bool hasTint(const std::string& name) const;

    void clear();

    std::vector<std::string> getTintNames() const;
    
private:
    std::map<std::string, TintData> m_tints;
};