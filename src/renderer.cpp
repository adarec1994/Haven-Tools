#include "renderer.h"
#include <cmath>
#include <iostream>
#include <cstring>
#include <algorithm>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
typedef void (APIENTRY *PFNGLCOMPRESSEDTEXIMAGE2DPROC)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data);
static PFNGLCOMPRESSEDTEXIMAGE2DPROC glCompressedTexImage2D_ptr = nullptr;
void loadGLExtensions() {
    glCompressedTexImage2D_ptr = (PFNGLCOMPRESSEDTEXIMAGE2DPROC)wglGetProcAddress("glCompressedTexImage2D");
}
#else
#include <GL/gl.h>
#include <GL/glext.h>
void loadGLExtensions() {}
#endif
inline void quatRotate(float qx, float qy, float qz, float qw,
                       float vx, float vy, float vz,
                       float& ox, float& oy, float& oz) {
    float tx = 2.0f * (qy * vz - qz * vy);
    float ty = 2.0f * (qz * vx - qx * vz);
    float tz = 2.0f * (qx * vy - qy * vx);
    ox = vx + qw * tx + (qy * tz - qz * ty);
    oy = vy + qw * ty + (qz * tx - qx * tz);
    oz = vz + qw * tz + (qx * ty - qy * tx);
}
void buildSkinningCache(Mesh& mesh, const Model& model) {
    if (mesh.skinningCacheBuilt) return;
    const auto& boneIndexArray = model.boneIndexArray;
    const auto& skeleton = model.skeleton;
    int maxBoneIdx = -1;
    for (const auto& v : mesh.vertices) {
        for (int i = 0; i < 4; i++) {
            if (v.boneWeights[i] > 0.0001f && v.boneIndices[i] > maxBoneIdx) {
                maxBoneIdx = v.boneIndices[i];
            }
        }
    }
    if (maxBoneIdx < 0) {
        mesh.skinningCacheBuilt = true;
        return;
    }
    mesh.skinningBoneMap.resize(maxBoneIdx + 1, -1);
    for (int meshLocalIdx = 0; meshLocalIdx <= maxBoneIdx; meshLocalIdx++) {
        int globalBoneIdx;
        if (!mesh.bonesUsed.empty()) {
            if (meshLocalIdx >= (int)mesh.bonesUsed.size()) continue;
            globalBoneIdx = mesh.bonesUsed[meshLocalIdx];
        } else {
            globalBoneIdx = meshLocalIdx;
        }
        if (globalBoneIdx < 0 || globalBoneIdx >= (int)boneIndexArray.size()) continue;
        const std::string& boneName = boneIndexArray[globalBoneIdx];
        if (boneName.empty()) continue;
        for (size_t j = 0; j < skeleton.bones.size(); j++) {
            std::string skelBoneLower = skeleton.bones[j].name;
            std::string targetLower = boneName;
            std::transform(skelBoneLower.begin(), skelBoneLower.end(), skelBoneLower.begin(), ::tolower);
            std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::tolower);
            if (skelBoneLower == targetLower) {
                mesh.skinningBoneMap[meshLocalIdx] = (int)j;
                break;
            }
        }
    }
    mesh.skinningCacheBuilt = true;
}
void transformVertexBySkeleton(const Vertex& v, const Mesh& mesh, const Model& model,
                               float& outX, float& outY, float& outZ,
                               float& outNX, float& outNY, float& outNZ) {
    const auto& skeleton = model.skeleton;
    const auto& skinningMap = mesh.skinningBoneMap;
    float totalWeight = 0;
    float finalX = 0, finalY = 0, finalZ = 0;
    float finalNX = 0, finalNY = 0, finalNZ = 0;
    for (int i = 0; i < 4; i++) {
        float weight = v.boneWeights[i];
        if (weight < 0.0001f) continue;
        int meshLocalIdx = v.boneIndices[i];
        if (meshLocalIdx < 0 || meshLocalIdx >= (int)skinningMap.size()) continue;
        int skelIdx = skinningMap[meshLocalIdx];
        if (skelIdx < 0) continue;
        const auto& bone = skeleton.bones[skelIdx];
        float bx, by, bz;
        quatRotate(bone.invBindRotX, bone.invBindRotY, bone.invBindRotZ, bone.invBindRotW,
                   v.x, v.y, v.z, bx, by, bz);
        bx += bone.invBindPosX;
        by += bone.invBindPosY;
        bz += bone.invBindPosZ;
        float wx, wy, wz;
        quatRotate(bone.worldRotX, bone.worldRotY, bone.worldRotZ, bone.worldRotW,
                   bx, by, bz, wx, wy, wz);
        wx += bone.worldPosX;
        wy += bone.worldPosY;
        wz += bone.worldPosZ;
        float bnx, bny, bnz;
        quatRotate(bone.invBindRotX, bone.invBindRotY, bone.invBindRotZ, bone.invBindRotW,
                   v.nx, v.ny, v.nz, bnx, bny, bnz);
        float wnx, wny, wnz;
        quatRotate(bone.worldRotX, bone.worldRotY, bone.worldRotZ, bone.worldRotW,
                   bnx, bny, bnz, wnx, wny, wnz);
        finalX += wx * weight;
        finalY += wy * weight;
        finalZ += wz * weight;
        finalNX += wnx * weight;
        finalNY += wny * weight;
        finalNZ += wnz * weight;
        totalWeight += weight;
    }
    if (totalWeight > 0.0001f) {
        outX = finalX / totalWeight;
        outY = finalY / totalWeight;
        outZ = finalZ / totalWeight;
        float len = std::sqrt(finalNX*finalNX + finalNY*finalNY + finalNZ*finalNZ);
        if (len > 0.0001f) {
            outNX = finalNX / len; outNY = finalNY / len; outNZ = finalNZ / len;
        } else {
            outNX = v.nx; outNY = v.ny; outNZ = v.nz;
        }
    } else {
        outX = v.x; outY = v.y; outZ = v.z;
        outNX = v.nx; outNY = v.ny; outNZ = v.nz;
    }
}
void drawSolidBox(float x, float y, float z) {
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);  glVertex3f(-x, -y, z); glVertex3f(x, -y, z); glVertex3f(x, y, z); glVertex3f(-x, y, z);
    glNormal3f(0, 0, -1); glVertex3f(x, -y, -z); glVertex3f(-x, -y, -z); glVertex3f(-x, y, -z); glVertex3f(x, y, -z);
    glNormal3f(0, 1, 0);  glVertex3f(-x, y, -z); glVertex3f(-x, y, z); glVertex3f(x, y, z); glVertex3f(x, y, -z);
    glNormal3f(0, -1, 0); glVertex3f(-x, -y, -z); glVertex3f(x, -y, -z); glVertex3f(x, -y, z); glVertex3f(-x, -y, z);
    glNormal3f(1, 0, 0);  glVertex3f(x, -y, -z); glVertex3f(x, y, -z); glVertex3f(x, y, z); glVertex3f(x, -y, z);
    glNormal3f(-1, 0, 0); glVertex3f(-x, -y, z); glVertex3f(-x, y, z); glVertex3f(-x, y, -z); glVertex3f(-x, -y, -z);
    glEnd();
}
void drawSolidSphere(float radius, int slices, int stacks) {
    for (int i = 0; i < stacks; i++) {
        float lat0 = 3.14159f * (-0.5f + float(i) / stacks);
        float z0 = radius * std::sin(lat0);
        float zr0 = radius * std::cos(lat0);
        float lat1 = 3.14159f * (-0.5f + float(i + 1) / stacks);
        float z1 = radius * std::sin(lat1);
        float zr1 = radius * std::cos(lat1);
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float lng = 2.0f * 3.14159f * float(j) / slices;
            float x = std::cos(lng);
            float y = std::sin(lng);
            glNormal3f(x * zr0 / radius, y * zr0 / radius, z0 / radius);
            glVertex3f(x * zr0, y * zr0, z0);
            glNormal3f(x * zr1 / radius, y * zr1 / radius, z1 / radius);
            glVertex3f(x * zr1, y * zr1, z1);
        }
        glEnd();
    }
}
void drawSolidCapsule(float radius, float height, int slices, int stacks) {
    float halfHeight = height / 2.0f;
    glBegin(GL_QUAD_STRIP);
    for (int j = 0; j <= slices; j++) {
        float lng = 2.0f * 3.14159f * float(j) / slices;
        float x = std::cos(lng);
        float y = std::sin(lng);
        glNormal3f(x, y, 0);
        glVertex3f(radius * x, radius * y, -halfHeight);
        glVertex3f(radius * x, radius * y, halfHeight);
    }
    glEnd();
    for (int i = 0; i < stacks / 2; i++) {
        float lat0 = 3.14159f * float(i) / stacks;
        float z0 = radius * std::sin(lat0);
        float zr0 = radius * std::cos(lat0);
        float lat1 = 3.14159f * float(i + 1) / stacks;
        float z1 = radius * std::sin(lat1);
        float zr1 = radius * std::cos(lat1);
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float lng = 2.0f * 3.14159f * float(j) / slices;
            float x = std::cos(lng);
            float y = std::sin(lng);
            glNormal3f(x * zr0 / radius, y * zr0 / radius, z0 / radius);
            glVertex3f(x * zr0, y * zr0, z0 + halfHeight);
            glNormal3f(x * zr1 / radius, y * zr1 / radius, z1 / radius);
            glVertex3f(x * zr1, y * zr1, z1 + halfHeight);
        }
        glEnd();
    }
    for (int i = 0; i < stacks / 2; i++) {
        float lat0 = -3.14159f * float(i) / stacks;
        float z0 = radius * std::sin(lat0);
        float zr0 = radius * std::cos(lat0);
        float lat1 = -3.14159f * float(i + 1) / stacks;
        float z1 = radius * std::sin(lat1);
        float zr1 = radius * std::cos(lat1);
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float lng = 2.0f * 3.14159f * float(j) / slices;
            float x = std::cos(lng);
            float y = std::sin(lng);
            glNormal3f(x * zr0 / radius, y * zr0 / radius, z0 / radius);
            glVertex3f(x * zr0, y * zr0, z0 - halfHeight);
            glNormal3f(x * zr1 / radius, y * zr1 / radius, z1 / radius);
            glVertex3f(x * zr1, y * zr1, z1 - halfHeight);
        }
        glEnd();
    }
}
void renderModel(Model& model, const Camera& camera, const RenderSettings& settings,
                 int width, int height, bool animating, int selectedBone) {
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    float fov = 45.0f * 3.14159f / 180.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    float top = nearPlane * std::tan(fov / 2.0f);
    float right = top * aspect;
    glFrustum(-right, right, -top, top, nearPlane, farPlane);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(-camera.pitch * 180.0f / 3.14159f, 1, 0, 0);
    glRotatef(-camera.yaw * 180.0f / 3.14159f, 0, 1, 0);
    glTranslatef(-camera.x, -camera.y, -camera.z);
    glRotatef(-90.0f, 1, 0, 0);
    glRotatef(180.0f, 0, 0, 1);
    if (settings.showGrid) {
        glLineWidth(1.0f);
        glBegin(GL_LINES);
        glColor3f(0.3f, 0.3f, 0.3f);
        float gridSize = 10.0f;
        float gridStep = 1.0f;
        for (float i = -gridSize; i <= gridSize; i += gridStep) {
            glVertex3f(-gridSize, i, 0); glVertex3f(gridSize, i, 0);
            glVertex3f(i, -gridSize, 0); glVertex3f(i, gridSize, 0);
        }
        glEnd();
    }
    if (settings.showAxes) {
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        glColor3f(1, 0, 0); glVertex3f(0, 0, 0); glVertex3f(2, 0, 0);
        glColor3f(0, 1, 0); glVertex3f(0, 0, 0); glVertex3f(0, 2, 0);
        glColor3f(0, 0, 1); glVertex3f(0, 0, 0); glVertex3f(0, 0, 2);
        glEnd();
        glLineWidth(1.0f);
    }
    if (!model.meshes.empty()) {
        if (settings.wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glDisable(GL_TEXTURE_2D);
            glColor3f(0.8f, 0.8f, 0.8f);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_LIGHTING);
            glEnable(GL_LIGHT0);
            glEnable(GL_COLOR_MATERIAL);
            float lightPos[] = {1.0f, 1.0f, 1.0f, 0.0f};
            float lightAmbient[] = {0.3f, 0.3f, 0.3f, 1.0f};
            float lightDiffuse[] = {0.7f, 0.7f, 0.7f, 1.0f};
            glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
            glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
            glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
            glColor3f(1.0f, 1.0f, 1.0f);
        }
        for (int pass = 0; pass < 2; pass++) {
            for (size_t meshIdx = 0; meshIdx < model.meshes.size(); meshIdx++) {
                if (meshIdx < settings.meshVisible.size() && settings.meshVisible[meshIdx] == 0) continue;
                auto& mesh = model.meshes[meshIdx];
                std::string meshNameLower = mesh.name;
                std::transform(meshNameLower.begin(), meshNameLower.end(), meshNameLower.begin(), ::tolower);
                std::string matNameLower = mesh.materialName;
                std::transform(matNameLower.begin(), matNameLower.end(), matNameLower.begin(), ::tolower);
                bool isBald = (meshNameLower.find("bld") != std::string::npos ||
                               matNameLower.find("bld") != std::string::npos);
                bool isAlphaMesh = !isBald && (meshNameLower.find("har") != std::string::npos ||
                                    matNameLower.find("har") != std::string::npos ||
                                    meshNameLower.find("lash") != std::string::npos ||
                                    meshNameLower.find("brow") != std::string::npos);
                if ((pass == 0 && isAlphaMesh) || (pass == 1 && !isAlphaMesh)) continue;
                if (animating && mesh.hasSkinning && !mesh.skinningCacheBuilt) {
                    buildSkinningCache(mesh, model);
                }
                uint32_t texId = 0;
                if (!settings.wireframe && settings.showTextures && mesh.materialIndex >= 0 &&
                    mesh.materialIndex < (int)model.materials.size()) {
                    texId = model.materials[mesh.materialIndex].diffuseTexId;
                }
                if (isAlphaMesh) {
                    glEnable(GL_ALPHA_TEST);
                    glAlphaFunc(GL_GREATER, 0.1f);
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                }
                bool isHairMesh = !isBald && (meshNameLower.find("har") != std::string::npos ||
                                   matNameLower.find("har") != std::string::npos);
                if (texId != 0) {
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, texId);
                    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                    if (isHairMesh) {
                        glColor4f(settings.hairColor[0], settings.hairColor[1], settings.hairColor[2], 1.0f);
                    } else {
                        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                    }
                } else {
                    glDisable(GL_TEXTURE_2D);
                    if (!settings.wireframe) {
                        if (isHairMesh) {
                            glColor4f(settings.hairColor[0], settings.hairColor[1], settings.hairColor[2], 1.0f);
                        } else {
                            glColor4f(0.7f, 0.7f, 0.7f, 1.0f);
                        }
                    }
                }
                glBegin(GL_TRIANGLES);
                for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                    for (int j = 0; j < 3; j++) {
                        const auto& v = mesh.vertices[mesh.indices[i + j]];
                        if (texId != 0) glTexCoord2f(v.u, 1.0f - v.v);
                        if (animating && mesh.hasSkinning) {
                            float sx, sy, sz, snx, sny, snz;
                            transformVertexBySkeleton(v, mesh, model, sx, sy, sz, snx, sny, snz);
                            glNormal3f(snx, sny, snz);
                            glVertex3f(sx, sy, sz);
                        } else {
                            glNormal3f(v.nx, v.ny, v.nz);
                            glVertex3f(v.x, v.y, v.z);
                        }
                    }
                }
                glEnd();
                bool isFaceMesh = (meshNameLower.find("hed") != std::string::npos ||
                                   meshNameLower.find("uhm") != std::string::npos ||
                                   meshNameLower.find("face") != std::string::npos);
                if (isFaceMesh && settings.ageAmount > 0.001f && mesh.materialIndex >= 0 &&
                    mesh.materialIndex < (int)model.materials.size()) {
                    const auto& mat = model.materials[mesh.materialIndex];
                    if (mat.ageDiffuseTexId != 0) {
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                        glDepthFunc(GL_LEQUAL);
                        glEnable(GL_TEXTURE_2D);
                        glBindTexture(GL_TEXTURE_2D, mat.ageDiffuseTexId);
                        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                        glColor4f(1.0f, 1.0f, 1.0f, settings.ageAmount);
                        glBegin(GL_TRIANGLES);
                        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                            for (int j = 0; j < 3; j++) {
                                const auto& v = mesh.vertices[mesh.indices[i + j]];
                                glTexCoord2f(v.u, 1.0f - v.v);
                                if (animating && mesh.hasSkinning) {
                                    float sx, sy, sz, snx, sny, snz;
                                    transformVertexBySkeleton(v, mesh, model, sx, sy, sz, snx, sny, snz);
                                    glNormal3f(snx, sny, snz);
                                    glVertex3f(sx, sy, sz);
                                } else {
                                    glNormal3f(v.nx, v.ny, v.nz);
                                    glVertex3f(v.x, v.y, v.z);
                                }
                            }
                        }
                        glEnd();
                        glDisable(GL_BLEND);
                        glDepthFunc(GL_LESS);
                    }
                    if (mat.tattooTexId != 0 && settings.selectedTattoo >= 0) {
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                        glDepthFunc(GL_LEQUAL);
                        glEnable(GL_TEXTURE_2D);
                        glBindTexture(GL_TEXTURE_2D, mat.tattooTexId);
                        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                        glBegin(GL_TRIANGLES);
                        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                            for (int j = 0; j < 3; j++) {
                                const auto& v = mesh.vertices[mesh.indices[i + j]];
                                glTexCoord2f(v.u, 1.0f - v.v);
                                if (animating && mesh.hasSkinning) {
                                    float sx, sy, sz, snx, sny, snz;
                                    transformVertexBySkeleton(v, mesh, model, sx, sy, sz, snx, sny, snz);
                                    glNormal3f(snx, sny, snz);
                                    glVertex3f(sx, sy, sz);
                                } else {
                                    glNormal3f(v.nx, v.ny, v.nz);
                                    glVertex3f(v.x, v.y, v.z);
                                }
                            }
                        }
                        glEnd();
                        glDisable(GL_BLEND);
                        glDepthFunc(GL_LESS);
                    }
                }
                if (isAlphaMesh) {
                    glDisable(GL_ALPHA_TEST);
                    glDisable(GL_BLEND);
                }
            }
        }
        glDisable(GL_TEXTURE_2D);
        if (!settings.wireframe) {
            glDisable(GL_LIGHTING);
            glDisable(GL_LIGHT0);
            glDisable(GL_COLOR_MATERIAL);
        }
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    if (settings.showCollision && !model.collisionShapes.empty()) {
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);
        bool wireframe = settings.collisionWireframe;
        if (wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        glLineWidth(2.0f);
        for (const auto& shape : model.collisionShapes) {
            if (wireframe) {
                glColor3f(0.0f, 1.0f, 1.0f);
            } else {
                glColor4f(0.0f, 1.0f, 1.0f, 0.3f);
            }
            glPushMatrix();
            glTranslatef(shape.posX, shape.posY, shape.posZ);
            float rotW = shape.rotW;
            if (rotW > 1.0f) rotW = 1.0f;
            if (rotW < -1.0f) rotW = -1.0f;
            if (rotW < 0.9999f && rotW > -0.9999f) {
                float angle = 2.0f * std::acos(rotW) * 180.0f / 3.14159f;
                float s = std::sqrt(1.0f - rotW * rotW);
                if (s > 0.001f) glRotatef(angle, shape.rotX / s, shape.rotY / s, shape.rotZ / s);
            }
            switch (shape.type) {
                case CollisionShapeType::Box:
                    if (wireframe) {
                        float x = shape.boxX, y = shape.boxY, z = shape.boxZ;
                        glBegin(GL_LINE_LOOP);
                        glVertex3f(-x, -y, -z); glVertex3f(x, -y, -z); glVertex3f(x, y, -z); glVertex3f(-x, y, -z);
                        glEnd();
                        glBegin(GL_LINE_LOOP);
                        glVertex3f(-x, -y, z); glVertex3f(x, -y, z); glVertex3f(x, y, z); glVertex3f(-x, y, z);
                        glEnd();
                        glBegin(GL_LINES);
                        glVertex3f(-x, -y, -z); glVertex3f(-x, -y, z);
                        glVertex3f(x, -y, -z); glVertex3f(x, -y, z);
                        glVertex3f(x, y, -z); glVertex3f(x, y, z);
                        glVertex3f(-x, y, -z); glVertex3f(-x, y, z);
                        glEnd();
                    } else {
                        drawSolidBox(shape.boxX, shape.boxY, shape.boxZ);
                    }
                    break;
                case CollisionShapeType::Sphere:
                    if (wireframe) {
                        int segments = 24;
                        for (int plane = 0; plane < 3; plane++) {
                            glBegin(GL_LINE_LOOP);
                            for (int i = 0; i < segments; i++) {
                                float a = 2.0f * 3.14159f * float(i) / segments;
                                float c = shape.radius * std::cos(a);
                                float s = shape.radius * std::sin(a);
                                if (plane == 0) glVertex3f(c, s, 0);
                                else if (plane == 1) glVertex3f(c, 0, s);
                                else glVertex3f(0, c, s);
                            }
                            glEnd();
                        }
                    } else {
                        drawSolidSphere(shape.radius, 16, 12);
                    }
                    break;
                case CollisionShapeType::Capsule:
                    if (wireframe) {
                        int segments = 24;
                        float r = shape.radius, h = shape.height / 2.0f;
                        for (float zOff : {-h, h}) {
                            glBegin(GL_LINE_LOOP);
                            for (int i = 0; i < segments; i++) {
                                float a = 2.0f * 3.14159f * float(i) / segments;
                                glVertex3f(r * std::cos(a), r * std::sin(a), zOff);
                            }
                            glEnd();
                        }
                        glBegin(GL_LINES);
                        for (int i = 0; i < 4; i++) {
                            float a = 2.0f * 3.14159f * float(i) / 4;
                            glVertex3f(r * std::cos(a), r * std::sin(a), -h);
                            glVertex3f(r * std::cos(a), r * std::sin(a), h);
                        }
                        glEnd();
                        for (float zSign : {-1.0f, 1.0f}) {
                            for (int jj = 1; jj <= 4; jj++) {
                                float lat = (3.14159f / 2.0f) * float(jj) / 4;
                                float zOff = r * std::sin(lat) * zSign + h * zSign;
                                float rOff = r * std::cos(lat);
                                glBegin(GL_LINE_LOOP);
                                for (int i = 0; i < segments; i++) {
                                    float a = 2.0f * 3.14159f * float(i) / segments;
                                    glVertex3f(rOff * std::cos(a), rOff * std::sin(a), zOff);
                                }
                                glEnd();
                            }
                        }
                    } else {
                        drawSolidCapsule(shape.radius, shape.height, 16, 12);
                    }
                    break;
                case CollisionShapeType::Mesh:
                    if (!shape.meshVerts.empty() && !shape.meshIndices.empty()) {
                        if (wireframe) {
                            for (size_t i = 0; i + 2 < shape.meshIndices.size(); i += 3) {
                                uint32_t i0 = shape.meshIndices[i], i1 = shape.meshIndices[i+1], i2 = shape.meshIndices[i+2];
                                if (i0*3+2 < shape.meshVerts.size() && i1*3+2 < shape.meshVerts.size() && i2*3+2 < shape.meshVerts.size()) {
                                    glBegin(GL_LINE_LOOP);
                                    glVertex3f(shape.meshVerts[i0*3], shape.meshVerts[i0*3+1], shape.meshVerts[i0*3+2]);
                                    glVertex3f(shape.meshVerts[i1*3], shape.meshVerts[i1*3+1], shape.meshVerts[i1*3+2]);
                                    glVertex3f(shape.meshVerts[i2*3], shape.meshVerts[i2*3+1], shape.meshVerts[i2*3+2]);
                                    glEnd();
                                }
                            }
                        } else {
                            glBegin(GL_TRIANGLES);
                            for (size_t i = 0; i + 2 < shape.meshIndices.size(); i += 3) {
                                uint32_t i0 = shape.meshIndices[i], i1 = shape.meshIndices[i+1], i2 = shape.meshIndices[i+2];
                                if (i0*3+2 < shape.meshVerts.size() && i1*3+2 < shape.meshVerts.size() && i2*3+2 < shape.meshVerts.size()) {
                                    float v0x = shape.meshVerts[i0*3], v0y = shape.meshVerts[i0*3+1], v0z = shape.meshVerts[i0*3+2];
                                    float v1x = shape.meshVerts[i1*3], v1y = shape.meshVerts[i1*3+1], v1z = shape.meshVerts[i1*3+2];
                                    float v2x = shape.meshVerts[i2*3], v2y = shape.meshVerts[i2*3+1], v2z = shape.meshVerts[i2*3+2];
                                    float e1x = v1x - v0x, e1y = v1y - v0y, e1z = v1z - v0z;
                                    float e2x = v2x - v0x, e2y = v2y - v0y, e2z = v2z - v0z;
                                    float nx = e1y * e2z - e1z * e2y;
                                    float ny = e1z * e2x - e1x * e2z;
                                    float nz = e1x * e2y - e1y * e2x;
                                    float len = std::sqrt(nx*nx + ny*ny + nz*nz);
                                    if (len > 0.0001f) { nx /= len; ny /= len; nz /= len; }
                                    glNormal3f(nx, ny, nz);
                                    glVertex3f(v0x, v0y, v0z);
                                    glVertex3f(v1x, v1y, v1z);
                                    glVertex3f(v2x, v2y, v2z);
                                }
                            }
                            glEnd();
                        }
                    }
                    break;
            }
            glPopMatrix();
        }
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glLineWidth(1.0f);
        glDisable(GL_BLEND);
    }
    if (settings.showSkeleton && !model.skeleton.bones.empty()) {
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
            const auto& bone = model.skeleton.bones[i];
            if (bone.parentIndex >= 0) {
                const Bone& parent = model.skeleton.bones[bone.parentIndex];
                bool isHighlighted = (selectedBone == (int)i) || (selectedBone == bone.parentIndex);
                if (isHighlighted) {
                    glColor3f(1.0f, 0.0f, 1.0f);
                } else {
                    glColor3f(0.0f, 1.0f, 0.0f);
                }
                glVertex3f(parent.worldPosX, parent.worldPosY, parent.worldPosZ);
                if (selectedBone == (int)i) {
                    glColor3f(1.0f, 1.0f, 0.0f);
                } else if (isHighlighted) {
                    glColor3f(1.0f, 0.0f, 1.0f);
                } else {
                    glColor3f(1.0f, 1.0f, 0.0f);
                }
                glVertex3f(bone.worldPosX, bone.worldPosY, bone.worldPosZ);
            }
        }
        glEnd();
        glPointSize(6.0f);
        glBegin(GL_POINTS);
        for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
            if (selectedBone == (int)i) continue;
            const auto& bone = model.skeleton.bones[i];
            if (bone.parentIndex < 0) {
                glColor3f(1.0f, 0.0f, 0.0f);
            } else {
                glColor3f(1.0f, 1.0f, 0.0f);
            }
            glVertex3f(bone.worldPosX, bone.worldPosY, bone.worldPosZ);
        }
        glEnd();
        if (selectedBone >= 0 && selectedBone < (int)model.skeleton.bones.size()) {
            const auto& bone = model.skeleton.bones[selectedBone];
            glPointSize(14.0f);
            glBegin(GL_POINTS);
            glColor3f(1.0f, 0.0f, 1.0f);
            glVertex3f(bone.worldPosX, bone.worldPosY, bone.worldPosZ);
            glEnd();
            glLineWidth(3.0f);
            float axisLen = 0.1f;
            glBegin(GL_LINES);
            glColor3f(1.0f, 0.0f, 0.0f);
            glVertex3f(bone.worldPosX, bone.worldPosY, bone.worldPosZ);
            glVertex3f(bone.worldPosX + axisLen, bone.worldPosY, bone.worldPosZ);
            glColor3f(0.0f, 1.0f, 0.0f);
            glVertex3f(bone.worldPosX, bone.worldPosY, bone.worldPosZ);
            glVertex3f(bone.worldPosX, bone.worldPosY + axisLen, bone.worldPosZ);
            glColor3f(0.0f, 0.0f, 1.0f);
            glVertex3f(bone.worldPosX, bone.worldPosY, bone.worldPosZ);
            glVertex3f(bone.worldPosX, bone.worldPosY, bone.worldPosZ + axisLen);
            glEnd();
        }
        glPointSize(1.0f);
        glLineWidth(1.0f);
        glEnable(GL_DEPTH_TEST);
    }
    glDisable(GL_DEPTH_TEST);
}