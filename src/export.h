#pragma once
#include "Mesh.h"
#include <string>
#include <vector>

struct ExportOptions {
    bool includeCollision = false;
    bool includeAnimations = true;
};

bool exportToGLB(const Model& model, const std::vector<Animation>& animations, const std::string& outputPath, const ExportOptions& options = {});
bool exportToFBX(const Model& model, const std::vector<Animation>& animations, const std::string& outputPath, const ExportOptions& options = {});