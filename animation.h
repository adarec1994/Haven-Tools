#pragma once
#include "Mesh.h"
#include "types.h"
#include <vector>
#include <string>

// Load animation from ANI file data
Animation loadANI(const std::vector<uint8_t>& data, const std::string& filename);

// Find available animations for a model
void findAnimationsForModel(AppState& state, const std::string& baseName);

// Apply animation to model at given time
void applyAnimation(Model& model, const Animation& anim, float time);

// Quaternion decompression
void decompressQuat(uint32_t quat32, uint32_t quat64, uint16_t quat48, int quality,
                    float& outX, float& outY, float& outZ, float& outW);