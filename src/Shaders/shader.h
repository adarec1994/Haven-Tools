#pragma once
#include <string>
#include <cstdint>
#include "d3d_context.h"
#include <d3dcompiler.h>

struct ShaderProgram {
    ID3D11VertexShader*  vs = nullptr;
    ID3D11PixelShader*   ps = nullptr;
    ID3D11InputLayout*   inputLayout = nullptr;
    bool valid = false;

    void release();
};

struct alignas(16) CBPerFrame {
    float modelViewProj[16];
    float modelView[16];
    float viewPos[4];
    float lightDir[4];
    float ambientStrength;
    float specularPower;
    float pad0[2];
};

struct alignas(16) CBPerMaterial {
    float tintColor[4];
    float tintZone1[4];
    float tintZone2[4];
    float tintZone3[4];
    float ageAmount;
    float _pad_age[3];  // HLSL pads float4 to 16-byte boundary after lone float
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
};

struct alignas(16) CBSimple {
    float modelViewProj[16];
    float color[4];
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