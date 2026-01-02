#pragma once
#include <vector>
#include <string>
#include <map>

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
    const MorphMeshTarget* findTarget(const std::string& name) const;
    MorphMeshTarget* findTarget(const std::string& name);
    const MorphMeshTarget* getFaceTarget() const;
    const MorphMeshTarget* getEyesTarget() const;
    const MorphMeshTarget* getLashesTarget() const;
    bool hasVertexData() const { return !meshTargets.empty() && !meshTargets[0].vertices.empty(); }
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