#pragma once
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cstdint>
#include <string>

#pragma comment(lib, "d3dcompiler.lib")


struct alignas(16) CBPerFrame {
    float modelViewProj[16];
    float modelView[16];
    float viewPos[4];
    float lightDir[4];
    float ambientStrength;
    float specularPower;
    float padding0[2];
    float lightColor[4];
    float fogColor[4];
    float fogParams[4];
    float probeMatR[16];      // SH light-probe irradiance matrices (R,G,B); ambient = [n 1] M [n 1]^T
    float probeMatG[16];
    float probeMatB[16];
    float probeParams[4];     // x = 1 if a probe is loaded, else 0
};

struct alignas(16) CBSkyDome {
    float viewProj[16];
    float sunDir[4];
    float sunColor[4];
    float fogColor[4];
    float cloudColor[4];
    float cloudParams[4];
    float atmoParams[4];
    float timeAndPad[4];
    float scatterParams[4];   // (rayleighMult, mieMult, turbidity, mieEccentricity) from ATMO
};

struct alignas(16) CBPerMaterial {
    float tintColor[4];
    float tintZone1[4];
    float tintZone2[4];
    float tintZone3[4];
    float tintZone4[4];      // diffuse, sampled from tintMask.a
    float tintSpecZone1[4];  // specular RGB per zone (xyz used)
    float tintSpecZone2[4];
    float tintSpecZone3[4];
    float tintSpecZone4[4];
    float tintDiffOpacity[4]; // per-zone diffuse opacity: x=z1 y=z2 z=z3 w=z4
    float tintSpecOpacity[4]; // per-zone specular opacity
    float ageAmount;
    float _pad_age[3];
    float stubbleAmount[4];
    float tattooAmount[4];
    float tattooColor1[4];
    float tattooColor2[4];
    float tattooColor3[4];

    int useDiffuse;
    int useNormal;
    int useSpecular;
    int useTint;
    int useAlphaTest;
    int isEyeMesh;
    int isFaceMesh;
    int useAge;
    int useStubble;
    int useTattoo;
    float padding1[2];
    float highlightColor[4];
    // Tint blend mode: [0]=1.0 → game-correct additive math (armor with TNT preset),
    // [0]=0.0 (default) → multiplicative chain (head/clothes/etc., manual sliders).
    float tintReplaceFlag[4];
};

struct alignas(16) CBSimple {
    float modelViewProj[16];
    float color[4];
};

struct alignas(16) CBTerrain {
    float palDim[4];
    float palParam[4];
    float uvScales[8];
    float reliefScales[8];
    int isTerrain;
    int terrainDebug;
    int _pad[2];
};

struct alignas(16) CBWater {
    float wave0[4];
    float wave1[4];
    float wave2[4];
    float waterColor[4];
    float waterVisual[4];
    float bodyColor[4];
    float time;
    int isWater;
    int hasCubemap;
    int _pad[1];
};

struct ShaderProgram {
    ID3D11VertexShader*  vs = nullptr;
    ID3D11PixelShader*   ps = nullptr;
    ID3D11InputLayout*   inputLayout = nullptr;
    bool valid = false;

    void release();
};


bool initShaderSystem();
void cleanupShaderSystem();
bool shadersAvailable();

ShaderProgram& getModelShader();
ShaderProgram& getSimpleShader();
ShaderProgram& getSimpleLineShader();

ID3D11Buffer* getPerFrameCB();
ID3D11Buffer* getPerMaterialCB();
ID3D11Buffer* getSimpleCB();

void updatePerFrameCB(const CBPerFrame& data);
void updatePerMaterialCB(const CBPerMaterial& data);
void updateSimpleCB(const CBSimple& data);
void updateTerrainCB(const CBTerrain& data);
ID3D11Buffer* getTerrainCB();
void updateWaterCB(const CBWater& data);
ID3D11Buffer* getWaterCB();

ShaderProgram& getSkyShader();
ID3D11Buffer* getSkyDomeCB();
void updateSkyDomeCB(const CBSkyDome& data);

ID3DBlob* compileShader(const char* source, const char* entryPoint, const char* target);