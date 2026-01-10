#pragma once
#include "Mesh.h"
#include <string>
#include <vector>
struct ExportOptions {
    bool includeCollision = false;
    bool includeAnimations = true;
    bool includeArmature = true;
    bool bakeCharacterSettings = false;
    float hairColor[3] = {0.4f, 0.25f, 0.15f};
    float skinColor[3] = {1.0f, 1.0f, 1.0f};
    float eyeColor[3] = {0.4f, 0.3f, 0.2f};
    float ageAmount = 0.0f;
    float tintZone1[3] = {1.0f, 1.0f, 1.0f};
    float tintZone2[3] = {1.0f, 1.0f, 1.0f};
    float tintZone3[3] = {1.0f, 1.0f, 1.0f};
    float fbxScale = 1.0f;
};
bool exportToGLB(const Model& model, const std::vector<Animation>& animations, const std::string& outputPath, const ExportOptions& options = {});
bool exportToFBX(const Model& model, const std::vector<Animation>& animations, const std::string& outputPath, const ExportOptions& options = {});