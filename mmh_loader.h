#pragma once
#include "Mesh.h"
#include "types.h"
#include <vector>
#include <string>

Material parseMAO(const std::string& maoContent, const std::string& materialName);

void loadMMH(const std::vector<uint8_t>& data, Model& model);

bool loadPHY(const std::vector<uint8_t>& data, Model& model);

bool loadModelFromEntry(AppState& state, const ERFEntry& entry);