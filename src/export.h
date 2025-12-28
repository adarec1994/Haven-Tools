#pragma once
#include "Mesh.h"
#include <string>
#include <vector>

bool exportToGLB(const Model& model, const std::vector<Animation>& animations, const std::string& outputPath);