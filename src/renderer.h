#pragma once
#include "types.h"
#include "Mesh.h"

void initRenderer();
void cleanupRenderer();

void buildSkinningCache(Mesh& mesh, const Model& model);

void renderModel(Model& model, const Camera& camera, const RenderSettings& settings,
                 int width, int height, bool animating = false, int selectedBone = -1);

void drawSolidBox(float x, float y, float z);
void drawSolidSphere(float radius, int slices, int stacks);
void drawSolidCapsule(float radius, float height, int slices, int stacks);

void transformVertexBySkeleton(const Vertex& v, const Mesh& mesh, const Model& model,
                               float& outX, float& outY, float& outZ,
                               float& outNX, float& outNY, float& outNZ);

inline void loadGLExtensions() {}