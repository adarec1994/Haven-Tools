#include "renderer.h"
#include "Shaders/shader.h"
#include "Shaders/d3d_context.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <vector>
#include "terrain_loader.h"


static void mat4Identity(float* m) {
    memset(m, 0, 64);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat4Multiply(const float* a, const float* b, float* out) {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            out[i * 4 + j] = 0;
            for (int k = 0; k < 4; k++)
                out[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j];
        }
}

static void mat4Perspective(float* m, float fovY, float aspect, float zn, float zf) {
    mat4Identity(m);
    float tanHalf = tanf(fovY / 2.0f);
    m[0]  = 1.0f / (aspect * tanHalf);
    m[5]  = 1.0f / tanHalf;
    m[10] = zf / (zn - zf);
    m[11] = -1.0f;
    m[14] = (zn * zf) / (zn - zf);
    m[15] = 0.0f;
}

static void mat4Translate(float* m, float x, float y, float z) {
    float t[16];
    mat4Identity(t);
    t[12] = x; t[13] = y; t[14] = z;
    float tmp[16];
    mat4Multiply(m, t, tmp);
    memcpy(m, tmp, 64);
}

static void mat4RotateX(float* m, float angle) {
    float c = cosf(angle), s = sinf(angle);
    float r[16]; mat4Identity(r);
    r[5] = c;  r[6] = s;
    r[9] = -s; r[10] = c;
    float tmp[16];
    mat4Multiply(m, r, tmp);
    memcpy(m, tmp, 64);
}

static void mat4RotateY(float* m, float angle) {
    float c = cosf(angle), s = sinf(angle);
    float r[16]; mat4Identity(r);
    r[0] = c;  r[2] = -s;
    r[8] = s;  r[10] = c;
    float tmp[16];
    mat4Multiply(m, r, tmp);
    memcpy(m, tmp, 64);
}

static void mat4RotateZ(float* m, float angle) {
    float c = cosf(angle), s = sinf(angle);
    float r[16]; mat4Identity(r);
    r[0] = c;  r[1] = s;
    r[4] = -s; r[5] = c;
    float tmp[16];
    mat4Multiply(m, r, tmp);
    memcpy(m, tmp, 64);
}

static void mat4Scale(float* m, float sx, float sy, float sz) {
    float s[16]; mat4Identity(s);
    s[0] = sx; s[5] = sy; s[10] = sz;
    float tmp[16];
    mat4Multiply(m, s, tmp);
    memcpy(m, tmp, 64);
}

static void mat4RotateAxis(float* m, float angle, float ax, float ay, float az) {
    float len = sqrtf(ax*ax + ay*ay + az*az);
    if (len < 0.0001f) return;
    ax /= len; ay /= len; az /= len;
    float c = cosf(angle), s = sinf(angle), t = 1.0f - c;
    float r[16]; mat4Identity(r);
    r[0] = t*ax*ax + c;     r[1] = t*ax*ay + s*az;  r[2] = t*ax*az - s*ay;
    r[4] = t*ax*ay - s*az;  r[5] = t*ay*ay + c;     r[6] = t*ay*az + s*ax;
    r[8] = t*ax*az + s*ay;  r[9] = t*ay*az - s*ax;  r[10] = t*az*az + c;
    float tmp[16];
    mat4Multiply(m, r, tmp);
    memcpy(m, tmp, 64);
}

static void mat4Transpose(const float* in, float* out) {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            out[j * 4 + i] = in[i * 4 + j];
}


struct ColorVertex {
    float x, y, z;
    float r, g, b, a;
};

struct SimpleVertex {
    float x, y, z;
    float nx, ny, nz;
};

struct ModelVertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
};


static const uint32_t MAX_BATCH_VERTS = 256000;
static DynamicVertexBuffer s_lineBuffer;
static DynamicVertexBuffer s_triBuffer;
static DynamicVertexBuffer s_modelBuffer;
static ID3D11Buffer*       s_modelIndexBuffer = nullptr;
static bool s_rendererInit = false;

static std::vector<ColorVertex>  s_lineBatch;
static std::vector<SimpleVertex> s_triBatch;


void initRenderer() {
    if (s_rendererInit) return;
    D3DContext& d3d = getD3DContext();
    if (!d3d.valid) return;

    s_lineBuffer.create(d3d.device, MAX_BATCH_VERTS, sizeof(ColorVertex));
    s_triBuffer.create(d3d.device, MAX_BATCH_VERTS, sizeof(SimpleVertex));
    s_modelBuffer.create(d3d.device, MAX_BATCH_VERTS, sizeof(ModelVertex));

    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage          = D3D11_USAGE_DYNAMIC;
    ibd.ByteWidth      = MAX_BATCH_VERTS * sizeof(uint32_t);
    ibd.BindFlags      = D3D11_BIND_INDEX_BUFFER;
    ibd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    d3d.device->CreateBuffer(&ibd, nullptr, &s_modelIndexBuffer);

    initShaderSystem();
    s_rendererInit = true;
}

void cleanupRenderer() {
    s_lineBuffer.destroy();
    s_triBuffer.destroy();
    s_modelBuffer.destroy();
    if (s_modelIndexBuffer) { s_modelIndexBuffer->Release(); s_modelIndexBuffer = nullptr; }
    cleanupShaderSystem();
    s_rendererInit = false;
}


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
            if (v.boneWeights[i] > 0.0001f && v.boneIndices[i] > maxBoneIdx)
                maxBoneIdx = v.boneIndices[i];
        }
    }
    if (maxBoneIdx < 0) { mesh.skinningCacheBuilt = true; return; }

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

        float bx, by, bz, bnx, bny, bnz;
        if (mesh.skipInvBind) {
            bx = v.x; by = v.y; bz = v.z;
            bnx = v.nx; bny = v.ny; bnz = v.nz;
        } else {
            quatRotate(bone.invBindRotX, bone.invBindRotY, bone.invBindRotZ, bone.invBindRotW,
                       v.x, v.y, v.z, bx, by, bz);
            bx += bone.invBindPosX; by += bone.invBindPosY; bz += bone.invBindPosZ;
            quatRotate(bone.invBindRotX, bone.invBindRotY, bone.invBindRotZ, bone.invBindRotW,
                       v.nx, v.ny, v.nz, bnx, bny, bnz);
        }

        float wx, wy, wz;
        quatRotate(bone.worldRotX, bone.worldRotY, bone.worldRotZ, bone.worldRotW,
                   bx, by, bz, wx, wy, wz);
        wx += bone.worldPosX; wy += bone.worldPosY; wz += bone.worldPosZ;
        float wnx, wny, wnz;
        quatRotate(bone.worldRotX, bone.worldRotY, bone.worldRotZ, bone.worldRotW,
                   bnx, bny, bnz, wnx, wny, wnz);
        finalX += wx * weight; finalY += wy * weight; finalZ += wz * weight;
        finalNX += wnx * weight; finalNY += wny * weight; finalNZ += wnz * weight;
        totalWeight += weight;
    }
    if (totalWeight > 0.0001f) {
        outX = finalX / totalWeight; outY = finalY / totalWeight; outZ = finalZ / totalWeight;
        float len = sqrtf(finalNX*finalNX + finalNY*finalNY + finalNZ*finalNZ);
        if (len > 0.0001f) { outNX = finalNX/len; outNY = finalNY/len; outNZ = finalNZ/len; }
        else { outNX = v.nx; outNY = v.ny; outNZ = v.nz; }
    } else {
        outX = v.x; outY = v.y; outZ = v.z;
        outNX = v.nx; outNY = v.ny; outNZ = v.nz;
    }
}


static void flushLines(const float* mvp) {
    if (s_lineBatch.empty()) return;
    D3DContext& d3d = getD3DContext();
    auto& shader = getSimpleLineShader();
    if (!shader.valid) return;

    CBSimple cb;
    memcpy(cb.modelViewProj, mvp, 64);
    memset(cb.color, 0, 16);
    updateSimpleCB(cb);

    s_lineBuffer.update(d3d.context, s_lineBatch.data(), (uint32_t)s_lineBatch.size());

    d3d.context->IASetInputLayout(shader.inputLayout);
    UINT stride = sizeof(ColorVertex), offset = 0;
    d3d.context->IASetVertexBuffers(0, 1, &s_lineBuffer.buffer, &stride, &offset);
    d3d.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    d3d.context->VSSetShader(shader.vs, nullptr, 0);
    d3d.context->PSSetShader(shader.ps, nullptr, 0);
    ID3D11Buffer* cbs[] = { getSimpleCB() };
    d3d.context->VSSetConstantBuffers(0, 1, cbs);

    d3d.context->Draw((UINT)s_lineBatch.size(), 0);
    s_lineBatch.clear();
}

static void addLine(float x0, float y0, float z0, float x1, float y1, float z1,
                    float r, float g, float b, float a = 1.0f) {
    s_lineBatch.push_back({x0, y0, z0, r, g, b, a});
    s_lineBatch.push_back({x1, y1, z1, r, g, b, a});
}

static void addLineColor(float x0, float y0, float z0, float r0, float g0, float b0,
                          float x1, float y1, float z1, float r1, float g1, float b1) {
    s_lineBatch.push_back({x0, y0, z0, r0, g0, b0, 1.0f});
    s_lineBatch.push_back({x1, y1, z1, r1, g1, b1, 1.0f});
}

static void addPoint(float x, float y, float z, float r, float g, float b, float size) {
    float hs = size * 0.003f;
    addLine(x - hs, y, z, x + hs, y, z, r, g, b);
    addLine(x, y - hs, z, x, y + hs, z, r, g, b);
    addLine(x, y, z - hs, x, y, z + hs, r, g, b);
}


static void buildBoxTris(float hx, float hy, float hz, std::vector<SimpleVertex>& out) {
    auto face = [&](float nx, float ny, float nz,
                    float ax, float ay, float az, float bx, float by, float bz,
                    float cx, float cy, float cz, float dx, float dy, float dz) {
        out.push_back({ax, ay, az, nx, ny, nz});
        out.push_back({bx, by, bz, nx, ny, nz});
        out.push_back({cx, cy, cz, nx, ny, nz});
        out.push_back({ax, ay, az, nx, ny, nz});
        out.push_back({cx, cy, cz, nx, ny, nz});
        out.push_back({dx, dy, dz, nx, ny, nz});
    };
    float x = hx, y = hy, z = hz;
    face( 0, 0, 1, -x,-y, z,  x,-y, z,  x, y, z, -x, y, z);
    face( 0, 0,-1,  x,-y,-z, -x,-y,-z, -x, y,-z,  x, y,-z);
    face( 0, 1, 0, -x, y,-z, -x, y, z,  x, y, z,  x, y,-z);
    face( 0,-1, 0, -x,-y,-z,  x,-y,-z,  x,-y, z, -x,-y, z);
    face( 1, 0, 0,  x,-y,-z,  x, y,-z,  x, y, z,  x,-y, z);
    face(-1, 0, 0, -x,-y, z, -x, y, z, -x, y,-z, -x,-y,-z);
}

static void buildSphereTris(float radius, int slices, int stacks, std::vector<SimpleVertex>& out) {
    const float PI = 3.14159265f;
    for (int i = 0; i < stacks; i++) {
        float lat0 = PI * (-0.5f + (float)i / stacks);
        float lat1 = PI * (-0.5f + (float)(i+1) / stacks);
        float z0 = sinf(lat0), zr0 = cosf(lat0);
        float z1 = sinf(lat1), zr1 = cosf(lat1);
        for (int j = 0; j < slices; j++) {
            float lng0 = 2.0f * PI * (float)j / slices;
            float lng1 = 2.0f * PI * (float)(j+1) / slices;
            float x0 = cosf(lng0), y0 = sinf(lng0);
            float x1 = cosf(lng1), y1 = sinf(lng1);

            SimpleVertex v00 = {x0*zr0*radius, y0*zr0*radius, z0*radius, x0*zr0, y0*zr0, z0};
            SimpleVertex v10 = {x1*zr0*radius, y1*zr0*radius, z0*radius, x1*zr0, y1*zr0, z0};
            SimpleVertex v01 = {x0*zr1*radius, y0*zr1*radius, z1*radius, x0*zr1, y0*zr1, z1};
            SimpleVertex v11 = {x1*zr1*radius, y1*zr1*radius, z1*radius, x1*zr1, y1*zr1, z1};

            out.push_back(v00); out.push_back(v01); out.push_back(v11);
            out.push_back(v00); out.push_back(v11); out.push_back(v10);
        }
    }
}

static void buildCapsuleTris(float radius, float height, int slices, int stacks, std::vector<SimpleVertex>& out) {
    const float PI = 3.14159265f;
    float hh = height / 2.0f;
    for (int j = 0; j < slices; j++) {
        float a0 = 2.0f * PI * (float)j / slices;
        float a1 = 2.0f * PI * (float)(j+1) / slices;
        float x0 = cosf(a0), y0 = sinf(a0);
        float x1 = cosf(a1), y1 = sinf(a1);
        SimpleVertex v0 = {radius*x0, radius*y0, -hh, x0, y0, 0};
        SimpleVertex v1 = {radius*x1, radius*y1, -hh, x1, y1, 0};
        SimpleVertex v2 = {radius*x1, radius*y1,  hh, x1, y1, 0};
        SimpleVertex v3 = {radius*x0, radius*y0,  hh, x0, y0, 0};
        out.push_back(v0); out.push_back(v2); out.push_back(v1);
        out.push_back(v0); out.push_back(v3); out.push_back(v2);
    }
    for (int sign = -1; sign <= 1; sign += 2) {
        float zOff = (sign > 0) ? hh : -hh;
        for (int i = 0; i < stacks/2; i++) {
            float lat0 = PI * (float)i / stacks * sign;
            float lat1 = PI * (float)(i+1) / stacks * sign;
            float sz0 = sinf(lat0), cr0 = cosf(lat0);
            float sz1 = sinf(lat1), cr1 = cosf(lat1);
            for (int j = 0; j < slices; j++) {
                float a0 = 2.0f * PI * (float)j / slices;
                float a1 = 2.0f * PI * (float)(j+1) / slices;
                float cx0 = cosf(a0), cy0 = sinf(a0);
                float cx1 = cosf(a1), cy1 = sinf(a1);
                SimpleVertex va = {cx0*cr0*radius, cy0*cr0*radius, sz0*radius + zOff, cx0*cr0, cy0*cr0, sz0};
                SimpleVertex vb = {cx1*cr0*radius, cy1*cr0*radius, sz0*radius + zOff, cx1*cr0, cy1*cr0, sz0};
                SimpleVertex vc = {cx1*cr1*radius, cy1*cr1*radius, sz1*radius + zOff, cx1*cr1, cy1*cr1, sz1};
                SimpleVertex vd = {cx0*cr1*radius, cy0*cr1*radius, sz1*radius + zOff, cx0*cr1, cy0*cr1, sz1};
                out.push_back(va); out.push_back(vc); out.push_back(vb);
                out.push_back(va); out.push_back(vd); out.push_back(vc);
            }
        }
    }
}

void drawSolidBox(float x, float y, float z) {
}
void drawSolidSphere(float radius, int slices, int stacks) {}
void drawSolidCapsule(float radius, float height, int slices, int stacks) {}


static void drawSimpleTris(const std::vector<SimpleVertex>& verts, const float* mvp,
                           float r, float g, float b, float a) {
    if (verts.empty()) return;
    D3DContext& d3d = getD3DContext();
    auto& shader = getSimpleShader();
    if (!shader.valid) return;

    CBSimple cb;
    memcpy(cb.modelViewProj, mvp, 64);
    cb.color[0] = r; cb.color[1] = g; cb.color[2] = b; cb.color[3] = a;
    updateSimpleCB(cb);

    s_triBuffer.update(d3d.context, verts.data(), (uint32_t)verts.size());

    d3d.context->IASetInputLayout(shader.inputLayout);
    UINT stride = sizeof(SimpleVertex), offset = 0;
    d3d.context->IASetVertexBuffers(0, 1, &s_triBuffer.buffer, &stride, &offset);
    d3d.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    d3d.context->VSSetShader(shader.vs, nullptr, 0);
    d3d.context->PSSetShader(shader.ps, nullptr, 0);
    ID3D11Buffer* cbs[] = { getSimpleCB() };
    d3d.context->VSSetConstantBuffers(0, 1, cbs);
    d3d.context->PSSetConstantBuffers(0, 1, cbs);

    d3d.context->Draw((UINT)verts.size(), 0);
}


void renderModel(Model& model, const Camera& camera, const RenderSettings& settings,
                 int width, int height, bool animating, int selectedBone) {

    if (!s_rendererInit) initRenderer();
    if (!shadersAvailable()) return;

    D3DContext& d3d = getD3DContext();

    float proj[16], view[16], mvp[16];
    float aspect = (float)width / (float)height;
    float fov = 45.0f * 3.14159f / 180.0f;
    mat4Perspective(proj, fov, aspect, 0.1f, 1000.0f);

    mat4Identity(view);
    mat4RotateX(view, -90.0f * 3.14159f / 180.0f);
    mat4Translate(view, -camera.x, -camera.y, -camera.z);
    mat4RotateY(view, -camera.yaw);
    mat4RotateX(view, -camera.pitch);

    mat4Multiply(view, proj, mvp);

    float viewPos[3] = { camera.x, camera.y, camera.z };

    d3d.context->OMSetDepthStencilState(d3d.dssDefault, 0);
    d3d.context->RSSetState(d3d.rsNoCull);
    float blendFactor[4] = {0,0,0,0};
    d3d.context->OMSetBlendState(d3d.bsOpaque, blendFactor, 0xFFFFFFFF);

    if (settings.showGrid) {
        float gridSize = 10.0f, gridStep = 1.0f;
        for (float i = -gridSize; i <= gridSize; i += gridStep) {
            addLine(-gridSize, i, 0, gridSize, i, 0, 0.3f, 0.3f, 0.3f);
            addLine(i, -gridSize, 0, i, gridSize, 0, 0.3f, 0.3f, 0.3f);
        }
        flushLines(mvp);
    }

    if (settings.showAxes) {
        addLine(0,0,0, 2,0,0, 1,0,0);
        addLine(0,0,0, 0,2,0, 0,1,0);
        addLine(0,0,0, 0,0,2, 0,0,1);
        flushLines(mvp);
    }

    if (g_terrainLoader.isLoaded()) {
        d3d.context->RSSetState(d3d.rsNoCull);
        renderTerrain(mvp);
    }

    if (!model.meshes.empty()) {
        if (settings.wireframe) {
            d3d.context->RSSetState(d3d.rsWireframe);
        } else {
            d3d.context->RSSetState(d3d.rsNoCull);
        }

        CBPerFrame perFrame;
        memcpy(perFrame.modelViewProj, mvp, 64);
        memcpy(perFrame.modelView, view, 64);
        perFrame.viewPos[0] = viewPos[0]; perFrame.viewPos[1] = viewPos[1];
        perFrame.viewPos[2] = viewPos[2]; perFrame.viewPos[3] = 0;
        perFrame.lightDir[0] = 0.3f; perFrame.lightDir[1] = 0.5f;
        perFrame.lightDir[2] = 1.0f; perFrame.lightDir[3] = 0;
        perFrame.ambientStrength = 0.35f;
        perFrame.specularPower = 32.0f;
        updatePerFrameCB(perFrame);

        bool useShaders = !settings.wireframe && settings.showTextures;

        for (int pass = 0; pass < 2; pass++) {
            for (size_t meshIdx = 0; meshIdx < model.meshes.size(); meshIdx++) {
                if (meshIdx < settings.meshVisible.size() && settings.meshVisible[meshIdx] == 0) continue;
                auto& mesh = model.meshes[meshIdx];

                std::string meshNameLower = mesh.name;
                std::transform(meshNameLower.begin(), meshNameLower.end(), meshNameLower.begin(), ::tolower);
                std::string matNameLower = mesh.materialName;
                std::transform(matNameLower.begin(), matNameLower.end(), matNameLower.begin(), ::tolower);

                bool isBald = meshNameLower.find("bld") != std::string::npos ||
                              matNameLower.find("bld") != std::string::npos;
                bool isAlphaMesh = !isBald && (meshNameLower.find("har") != std::string::npos ||
                                    matNameLower.find("har") != std::string::npos ||
                                    meshNameLower.find("brd") != std::string::npos ||
                                    matNameLower.find("brd") != std::string::npos ||
                                    meshNameLower.find("lash") != std::string::npos ||
                                    meshNameLower.find("brow") != std::string::npos);
                if ((pass == 0 && isAlphaMesh) || (pass == 1 && !isAlphaMesh)) continue;

                if (animating && mesh.hasSkinning && !mesh.skinningCacheBuilt)
                    buildSkinningCache(mesh, model);

                bool isHairMesh = !isBald && (meshNameLower.find("har") != std::string::npos ||
                                   matNameLower.find("har") != std::string::npos ||
                                   meshNameLower.find("brd") != std::string::npos ||
                                   matNameLower.find("brd") != std::string::npos);
                bool isEyeMesh = meshNameLower.find("uem") != std::string::npos ||
                                 matNameLower.find("_eye") != std::string::npos ||
                                 matNameLower.find("eye_") != std::string::npos;
                bool isFaceMesh = meshNameLower.find("uhm") != std::string::npos ||
                                  matNameLower.find("_hed_") != std::string::npos ||
                                  matNameLower.find("hed_fem") != std::string::npos ||
                                  matNameLower.find("hed_mal") != std::string::npos;
                bool isSkinMesh = !isEyeMesh && (isBald ||
                                  meshNameLower.find("hed") != std::string::npos ||
                                  meshNameLower.find("uhm") != std::string::npos ||
                                  meshNameLower.find("ulm") != std::string::npos ||
                                  meshNameLower.find("face") != std::string::npos ||
                                  matNameLower.find("_skn") != std::string::npos ||
                                  matNameLower.find("skn_") != std::string::npos ||
                                  matNameLower.find("skin") != std::string::npos ||
                                  meshNameLower.find("_skn") != std::string::npos);

                if (isAlphaMesh)
                    d3d.context->OMSetBlendState(d3d.bsAlpha, blendFactor, 0xFFFFFFFF);
                else
                    d3d.context->OMSetBlendState(d3d.bsOpaque, blendFactor, 0xFFFFFFFF);

                const float* zone1 = settings.tintZone1;
                const float* zone2 = settings.tintZone2;
                const float* zone3 = settings.tintZone3;

                if (meshNameLower.find("hed") != std::string::npos ||
                    meshNameLower.find("uhm") != std::string::npos ||
                    meshNameLower.find("uem") != std::string::npos ||
                    meshNameLower.find("ulm") != std::string::npos ||
                    meshNameLower.find("face") != std::string::npos ||
                    isBald) {
                    zone1 = settings.headZone1; zone2 = settings.headZone2; zone3 = settings.headZone3;
                } else if (matNameLower.find("_arm_") != std::string::npos ||
                           matNameLower.find("_mas") != std::string::npos ||
                           matNameLower.find("_med") != std::string::npos ||
                           matNameLower.find("_hvy") != std::string::npos ||
                           matNameLower.find("_lgt") != std::string::npos) {
                    zone1 = settings.armorZone1; zone2 = settings.armorZone2; zone3 = settings.armorZone3;
                } else if (matNameLower.find("_cth_") != std::string::npos ||
                           matNameLower.find("_clo") != std::string::npos) {
                    zone1 = settings.clothesZone1; zone2 = settings.clothesZone2; zone3 = settings.clothesZone3;
                } else if (matNameLower.find("_boo_") != std::string::npos ||
                           matNameLower.find("_boot") != std::string::npos) {
                    zone1 = settings.bootsZone1; zone2 = settings.bootsZone2; zone3 = settings.bootsZone3;
                } else if (matNameLower.find("_glv_") != std::string::npos ||
                           matNameLower.find("_glove") != std::string::npos) {
                    zone1 = settings.glovesZone1; zone2 = settings.glovesZone2; zone3 = settings.glovesZone3;
                } else if (matNameLower.find("_hlm_") != std::string::npos ||
                           matNameLower.find("_helm") != std::string::npos ||
                           meshNameLower.find("helmet") != std::string::npos) {
                    zone1 = settings.helmetZone1; zone2 = settings.helmetZone2; zone3 = settings.helmetZone3;
                }

                const Material* mat = nullptr;
                if (mesh.materialIndex >= 0 && mesh.materialIndex < (int)model.materials.size())
                    mat = &model.materials[mesh.materialIndex];

                bool hasDiffuse  = mat && mat->diffuseTexId != 0 && settings.showTextures;
                bool hasNormal   = mat && mat->normalTexId != 0 && settings.useNormalMaps;
                bool hasSpecular = mat && mat->specularTexId != 0 && settings.useSpecularMaps;
                bool hasTint     = mat && mat->tintTexId != 0 && settings.useTintMaps;
                bool hasAge      = mat && mat->ageDiffuseTexId != 0 && mat->ageNormalTexId != 0;
                bool hasStubble  = mat && mat->browStubbleTexId != 0 && mat->browStubbleNormalTexId != 0;
                bool hasTattoo   = mat && mat->tattooTexId != 0;

                CBPerMaterial perMat = {};
                if (isHairMesh) {
                    perMat.tintColor[0] = settings.hairColor[0]; perMat.tintColor[1] = settings.hairColor[1];
                    perMat.tintColor[2] = settings.hairColor[2]; perMat.tintColor[3] = 1.0f;
                } else if (isEyeMesh) {
                    perMat.tintColor[0] = settings.eyeColor[0]; perMat.tintColor[1] = settings.eyeColor[1];
                    perMat.tintColor[2] = settings.eyeColor[2]; perMat.tintColor[3] = 1.0f;
                } else if (isSkinMesh) {
                    perMat.tintColor[0] = settings.skinColor[0]; perMat.tintColor[1] = settings.skinColor[1];
                    perMat.tintColor[2] = settings.skinColor[2]; perMat.tintColor[3] = 1.0f;
                } else {
                    perMat.tintColor[0] = perMat.tintColor[1] = perMat.tintColor[2] = perMat.tintColor[3] = 1.0f;
                }
                memcpy(perMat.tintZone1, zone1, 12); perMat.tintZone1[3] = 0;
                memcpy(perMat.tintZone2, zone2, 12); perMat.tintZone2[3] = 0;
                memcpy(perMat.tintZone3, zone3, 12); perMat.tintZone3[3] = 0;
                perMat.ageAmount = settings.ageAmount;
                memcpy(perMat.stubbleAmount, settings.stubbleAmount, 16);
                perMat.tattooAmount[0] = settings.tattooAmount[0];
                perMat.tattooAmount[1] = settings.tattooAmount[1];
                perMat.tattooAmount[2] = settings.tattooAmount[2];
                memcpy(perMat.tattooColor1, settings.tattooColor1, 12);
                memcpy(perMat.tattooColor2, settings.tattooColor2, 12);
                memcpy(perMat.tattooColor3, settings.tattooColor3, 12);
                perMat.useDiffuse  = (useShaders && hasDiffuse) ? 1 : 0;
                perMat.useNormal   = (useShaders && hasNormal) ? 1 : 0;
                perMat.useSpecular = (useShaders && hasSpecular) ? 1 : 0;
                perMat.useTint     = (useShaders && hasTint) ? 1 : 0;
                perMat.useAlphaTest = isAlphaMesh ? 1 : 0;
                perMat.isEyeMesh   = isEyeMesh ? 1 : 0;
                perMat.isFaceMesh  = isFaceMesh ? 1 : 0;
                perMat.useAge      = (useShaders && hasAge) ? 1 : 0;
                perMat.useStubble  = (useShaders && hasStubble) ? 1 : 0;
                perMat.useTattoo   = (useShaders && hasTattoo) ? 1 : 0;
                updatePerMaterialCB(perMat);

                ID3D11ShaderResourceView* srvs[9] = {};
                if (useShaders && hasDiffuse)  srvs[0] = getTextureSRV(mat->diffuseTexId);
                if (useShaders && hasNormal)   srvs[1] = getTextureSRV(mat->normalTexId);
                if (useShaders && hasSpecular) srvs[2] = getTextureSRV(mat->specularTexId);
                if (useShaders && hasTint)     srvs[3] = getTextureSRV(mat->tintTexId);
                if (useShaders && hasAge) {
                    srvs[4] = getTextureSRV(mat->ageDiffuseTexId);
                    srvs[5] = getTextureSRV(mat->ageNormalTexId);
                }
                if (useShaders && hasStubble) {
                    srvs[6] = getTextureSRV(mat->browStubbleTexId);
                    srvs[7] = getTextureSRV(mat->browStubbleNormalTexId);
                }
                if (useShaders && hasTattoo) srvs[8] = getTextureSRV(mat->tattooTexId);
                d3d.context->PSSetShaderResources(0, 9, srvs);
                d3d.context->PSSetSamplers(0, 1, &d3d.samplerLinear);

                auto& modelShader = getModelShader();
                d3d.context->IASetInputLayout(modelShader.inputLayout);
                d3d.context->VSSetShader(modelShader.vs, nullptr, 0);
                d3d.context->PSSetShader(modelShader.ps, nullptr, 0);
                ID3D11Buffer* vsCBs[] = { getPerFrameCB() };
                ID3D11Buffer* psCBs[] = { getPerFrameCB(), getPerMaterialCB() };
                d3d.context->VSSetConstantBuffers(0, 1, vsCBs);
                d3d.context->PSSetConstantBuffers(0, 2, psCBs);

                std::vector<ModelVertex> vertData(mesh.vertices.size());
                for (size_t vi = 0; vi < mesh.vertices.size(); vi++) {
                    const Vertex& v = mesh.vertices[vi];
                    float px = v.x, py = v.y, pz = v.z;
                    float nx = v.nx, ny = v.ny, nz = v.nz;
                    if (animating && mesh.hasSkinning)
                        transformVertexBySkeleton(v, mesh, model, px, py, pz, nx, ny, nz);
                    vertData[vi] = { px, py, pz, nx, ny, nz, v.u, 1.0f - v.v };
                }

                s_modelBuffer.update(d3d.context, vertData.data(), (uint32_t)vertData.size());

                D3D11_MAPPED_SUBRESOURCE mapped;
                if (SUCCEEDED(d3d.context->Map(s_modelIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                    uint32_t copySize = (uint32_t)(mesh.indices.size() * sizeof(uint32_t));
                    memcpy(mapped.pData, mesh.indices.data(), copySize);
                    d3d.context->Unmap(s_modelIndexBuffer, 0);
                }

                UINT stride = sizeof(ModelVertex), offset = 0;
                d3d.context->IASetVertexBuffers(0, 1, &s_modelBuffer.buffer, &stride, &offset);
                d3d.context->IASetIndexBuffer(s_modelIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
                d3d.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                d3d.context->DrawIndexed((UINT)mesh.indices.size(), 0, 0);

                ID3D11ShaderResourceView* nullSRVs[9] = {};
                d3d.context->PSSetShaderResources(0, 9, nullSRVs);
            }
        }

        d3d.context->RSSetState(d3d.rsNoCull);
        d3d.context->OMSetBlendState(d3d.bsOpaque, blendFactor, 0xFFFFFFFF);
    }

    if (settings.showCollision && !model.collisionShapes.empty()) {
        bool wireframe = settings.collisionWireframe;
        if (wireframe) d3d.context->RSSetState(d3d.rsWireframe);
        else {
            d3d.context->RSSetState(d3d.rsNoCull);
            d3d.context->OMSetBlendState(d3d.bsAlpha, blendFactor, 0xFFFFFFFF);
        }

        for (const auto& shape : model.collisionShapes) {
            float cr = 0.0f, cg = 1.0f, cb = 1.0f, ca = wireframe ? 1.0f : 0.3f;

            float shapeMat[16]; mat4Identity(shapeMat);
            mat4Translate(shapeMat, shape.posX, shape.posY, shape.posZ);
            float rotW = shape.rotW;
            if (rotW > 1.0f) rotW = 1.0f;
            if (rotW < -1.0f) rotW = -1.0f;
            if (rotW < 0.9999f && rotW > -0.9999f) {
                float angle = 2.0f * acosf(rotW);
                float s = sqrtf(1.0f - rotW * rotW);
                if (s > 0.001f)
                    mat4RotateAxis(shapeMat, angle, shape.rotX / s, shape.rotY / s, shape.rotZ / s);
            }
            float shapeMVP[16];
            mat4Multiply(shapeMat, mvp, shapeMVP);

            std::vector<SimpleVertex> tris;
            switch (shape.type) {
                case CollisionShapeType::Box:
                    buildBoxTris(shape.boxX, shape.boxY, shape.boxZ, tris);
                    break;
                case CollisionShapeType::Sphere:
                    buildSphereTris(shape.radius, 16, 12, tris);
                    break;
                case CollisionShapeType::Capsule:
                    buildCapsuleTris(shape.radius, shape.height, 16, 12, tris);
                    break;
                case CollisionShapeType::Mesh:
                    if (!shape.meshVerts.empty() && !shape.meshIndices.empty()) {
                        for (size_t i = 0; i + 2 < shape.meshIndices.size(); i += 3) {
                            uint32_t i0 = shape.meshIndices[i], i1 = shape.meshIndices[i+1], i2 = shape.meshIndices[i+2];
                            if (i0*3+2 >= shape.meshVerts.size() || i1*3+2 >= shape.meshVerts.size() || i2*3+2 >= shape.meshVerts.size()) continue;
                            float v0x = shape.meshVerts[i0*3], v0y = shape.meshVerts[i0*3+1], v0z = shape.meshVerts[i0*3+2];
                            float v1x = shape.meshVerts[i1*3], v1y = shape.meshVerts[i1*3+1], v1z = shape.meshVerts[i1*3+2];
                            float v2x = shape.meshVerts[i2*3], v2y = shape.meshVerts[i2*3+1], v2z = shape.meshVerts[i2*3+2];
                            float e1x = v1x-v0x, e1y = v1y-v0y, e1z = v1z-v0z;
                            float e2x = v2x-v0x, e2y = v2y-v0y, e2z = v2z-v0z;
                            float nx = e1y*e2z - e1z*e2y, ny = e1z*e2x - e1x*e2z, nz = e1x*e2y - e1y*e2x;
                            float len = sqrtf(nx*nx + ny*ny + nz*nz);
                            if (len > 0.0001f) { nx/=len; ny/=len; nz/=len; }
                            tris.push_back({v0x, v0y, v0z, nx, ny, nz});
                            tris.push_back({v1x, v1y, v1z, nx, ny, nz});
                            tris.push_back({v2x, v2y, v2z, nx, ny, nz});
                        }
                    }
                    break;
            }
            if (!tris.empty()) drawSimpleTris(tris, shapeMVP, cr, cg, cb, ca);
        }

        d3d.context->RSSetState(d3d.rsNoCull);
        d3d.context->OMSetBlendState(d3d.bsOpaque, blendFactor, 0xFFFFFFFF);
    }

    if (settings.showSkeleton && !model.skeleton.bones.empty()) {
        d3d.context->OMSetDepthStencilState(d3d.dssNoDepth, 0);

        float vpW = (float)width, vpH = (float)height;

        struct BoneProj { float sx, sy; bool vis; };
        std::vector<BoneProj> bp(model.skeleton.bones.size());
        for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
            const auto& bone = model.skeleton.bones[i];
            float wx = bone.worldPosX, wy = bone.worldPosY, wz = bone.worldPosZ;
            float cx = wx*mvp[0] + wy*mvp[4] + wz*mvp[8]  + mvp[12];
            float cy = wx*mvp[1] + wy*mvp[5] + wz*mvp[9]  + mvp[13];
            float cw = wx*mvp[3] + wy*mvp[7] + wz*mvp[11] + mvp[15];
            if (cw <= 0.001f) { bp[i].vis = false; continue; }
            bp[i].vis = true;
            bp[i].sx = (cx/cw * 0.5f + 0.5f) * vpW;
            bp[i].sy = (1.0f - (cy/cw * 0.5f + 0.5f)) * vpH;
        }

        float ortho[16] = {
            2.0f/vpW,  0,         0, 0,
            0,        -2.0f/vpH,  0, 0,
            0,         0,         1, 0,
           -1.0f,      1.0f,      0, 1
        };

        for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
            const auto& bone = model.skeleton.bones[i];
            if (bone.parentIndex >= 0 && bp[i].vis && bp[bone.parentIndex].vis) {
                bool hl = (selectedBone == (int)i) || (selectedBone == bone.parentIndex);
                if (hl)
                    addLine(bp[i].sx, bp[i].sy, 0, bp[bone.parentIndex].sx, bp[bone.parentIndex].sy, 0, 1.0f, 0, 1.0f);
                else
                    addLine(bp[i].sx, bp[i].sy, 0, bp[bone.parentIndex].sx, bp[bone.parentIndex].sy, 0, 0, 1.0f, 0.39f);
            }
        }
        flushLines(ortho);

        auto makeCircle = [](std::vector<SimpleVertex>& tris, float cx, float cy, float radius, int segs) {
            for (int s = 0; s < segs; s++) {
                float a0 = s * 6.2831853f / segs;
                float a1 = (s+1) * 6.2831853f / segs;
                tris.push_back({cx, cy, 0, 0, 0, 1});
                tris.push_back({cx + cosf(a0)*radius, cy + sinf(a0)*radius, 0, 0, 0, 1});
                tris.push_back({cx + cosf(a1)*radius, cy + sinf(a1)*radius, 0, 0, 0, 1});
            }
        };

        std::vector<SimpleVertex> outlineTris, rootTris, childTris, selTris;
        for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
            if (!bp[i].vis) continue;
            bool isRoot = (model.skeleton.bones[i].parentIndex < 0);
            bool isSel  = ((int)i == selectedBone);
            float r = isSel ? 7.0f : (isRoot ? 5.0f : 3.0f);
            makeCircle(outlineTris, bp[i].sx, bp[i].sy, r + 1.0f, 16);
            if (isSel)       makeCircle(selTris,   bp[i].sx, bp[i].sy, r, 16);
            else if (isRoot) makeCircle(rootTris,  bp[i].sx, bp[i].sy, r, 16);
            else             makeCircle(childTris, bp[i].sx, bp[i].sy, r, 16);
        }
        drawSimpleTris(outlineTris, ortho, 0, 0, 0, 0.78f);
        drawSimpleTris(rootTris,    ortho, 1.0f, 0.0f, 0.0f, 1.0f);
        drawSimpleTris(childTris,   ortho, 1.0f, 1.0f, 0.2f, 1.0f);
        drawSimpleTris(selTris,     ortho, 1.0f, 0, 1.0f, 1.0f);

        if (selectedBone >= 0 && selectedBone < (int)model.skeleton.bones.size()) {
            const auto& bone = model.skeleton.bones[selectedBone];
            float al = 0.1f;
            addLine(bone.worldPosX, bone.worldPosY, bone.worldPosZ,
                    bone.worldPosX + al, bone.worldPosY, bone.worldPosZ, 1, 0, 0);
            addLine(bone.worldPosX, bone.worldPosY, bone.worldPosZ,
                    bone.worldPosX, bone.worldPosY + al, bone.worldPosZ, 0, 1, 0);
            addLine(bone.worldPosX, bone.worldPosY, bone.worldPosZ,
                    bone.worldPosX, bone.worldPosY, bone.worldPosZ + al, 0, 0, 1);
            flushLines(mvp);
        }

        d3d.context->OMSetDepthStencilState(d3d.dssDefault, 0);
    }
}