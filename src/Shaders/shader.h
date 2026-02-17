#pragma once
#define NOMINMAX
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
};

struct alignas(16) CBPerMaterial {
    float tintColor[4];
    float tintZone1[4];
    float tintZone2[4];
    float tintZone3[4];
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
    float time;
    int isWater;
    int _pad[2];
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