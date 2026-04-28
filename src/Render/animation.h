#pragma once
#include "Mesh.h"
#include "types.h"
#include <vector>
#include <string>

Animation loadANI(const std::vector<uint8_t>& data, const std::string& filename);

void findAnimationsForModel(AppState& state, const std::string& baseName);

void resolveX360AnimHashes(Animation& anim, const Skeleton& skeleton);

void applyAnimation(Model& model, const Animation& anim, float time, const std::vector<Bone>& basePose);

void computeBoneWorldTransforms(Model& model);

void decompressQuat(uint32_t quat32, uint32_t quat64, uint16_t quat48, int quality,
                    float& outX, float& outY, float& outZ, float& outW);