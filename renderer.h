#pragma once
#include "types.h"
#include "Mesh.h"

// Initialize OpenGL extensions
void loadGLExtensions();

// Build skinning cache for a mesh (call once after loading model)
void buildSkinningCache(Mesh& mesh, const Model& model);

// Render the model with given camera and settings
// Note: Model is non-const because skinning cache may be built on first animated render
void renderModel(Model& model, const Camera& camera, const RenderSettings& settings,
                 int width, int height, bool animating = false);

// Drawing primitives for collision shapes
void drawSolidBox(float x, float y, float z);
void drawSolidSphere(float radius, int slices, int stacks);
void drawSolidCapsule(float radius, float height, int slices, int stacks);

// Transform vertex by skeleton for skinned animation
void transformVertexBySkeleton(const Vertex& v, const Mesh& mesh, const Model& model,
                               float& outX, float& outY, float& outZ,
                               float& outNX, float& outNY, float& outNZ);