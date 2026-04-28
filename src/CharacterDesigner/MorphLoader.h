#pragma once
#include <vector>
#include <string>
#include <map>
#include <cstdint>

struct SkinTone {
    float r, g, b;
    const char* name;
};

inline const SkinTone SKIN_TONES[] = {
    {1.00f, 0.87f, 0.77f, "Light"},
    {0.87f, 0.72f, 0.60f, "Medium"},
    {0.65f, 0.50f, 0.40f, "Dark"},
    {0.45f, 0.35f, 0.30f, "Very Dark"},
};
inline const int NUM_SKIN_TONES = 4;

struct HairColor {
    float r, g, b;
    const char* name;
};

inline const std::pair<const char*, HairColor> HAIR_COLORS[] = {
    {"blk", {0.05f, 0.05f, 0.05f, "Black"}},
    {"bln", {0.85f, 0.75f, 0.55f, "Blonde"}},
    {"brn", {0.35f, 0.25f, 0.15f, "Brown"}},
    {"red", {0.55f, 0.20f, 0.10f, "Red"}},
    {"org", {0.70f, 0.40f, 0.15f, "Orange"}},
    {"gry", {0.50f, 0.50f, 0.50f, "Grey"}},
    {"wht", {0.90f, 0.90f, 0.90f, "White"}},
};
inline const int NUM_HAIR_COLORS = 7;

struct MorphVertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct MorphMeshTarget {
    std::string name;
    std::string category;
    int index = 1;
    std::vector<MorphVertex> vertices;
};

struct MorphData {
    std::string name;
    std::string displayName;
    std::vector<std::string> modelRefs;
    std::vector<std::string> textureSlots;
    std::vector<MorphMeshTarget> meshTargets;

    std::string hairModel;
    std::string beardModel;
    std::string skinTexture;
    std::string hairTexture;
    std::string eyeTexture;

    std::string lipsTint;
    std::string eyeshadowTint;
    std::string blushTint;

    const MorphMeshTarget* findTarget(const std::string& name) const;
    MorphMeshTarget* findTarget(const std::string& name);
    const MorphMeshTarget* getFaceTarget() const;
    const MorphMeshTarget* getEyesTarget() const;
    const MorphMeshTarget* getLashesTarget() const;

    bool hasVertexData() const { return !meshTargets.empty() && !meshTargets[0].vertices.empty(); }

    int getHairStyleIndex() const;
    int getBeardStyleIndex() const;

    bool getSkinColor(float& r, float& g, float& b) const;
    bool getHairColor(float& r, float& g, float& b) const;
};

struct MorphPresetEntry {
    std::string filename;
    std::string displayName;
    int presetNumber = 0;
};

bool loadMOR(const std::vector<uint8_t>& data, MorphData& outMorph);
void debugPrintMorph(const MorphData& morph);

void applyMorphBlend(
    const std::vector<MorphVertex>& baseVertices,
    const std::vector<MorphVertex>& morphVertices,
    float amount,
    std::vector<MorphVertex>& outVertices
);