#pragma once
#include "Mesh.h"
#include "types.h"
#include <vector>
#include <string>

// Parse MAO (material) file content
Material parseMAO(const std::string& maoContent, const std::string& materialName);

// Load MMH file data into a model (skeleton, materials, etc.)
void loadMMH(const std::vector<uint8_t>& data, Model& model);

// Load PHY file data into a model (collision shapes)
bool loadPHY(const std::vector<uint8_t>& data, Model& model);

// Load complete model from ERF entry (MSH + MMH + PHY + MAO + textures)
bool loadModelFromEntry(AppState& state, const ERFEntry& entry);