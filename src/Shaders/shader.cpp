#include "shader.h"
#include "d3d_context.h"
#include <iostream>
#include <vector>
#include <cstring>

static bool s_shadersInitialized = false;
static bool s_shadersAvailable   = false;

static ShaderProgram s_modelShader;
static ShaderProgram s_simpleShader;
static ShaderProgram s_simpleLineShader;

static ID3D11Buffer* s_cbPerFrame    = nullptr;
static ID3D11Buffer* s_cbPerMaterial = nullptr;
static ID3D11Buffer* s_cbSimple      = nullptr;
static const char* MODEL_VS = R"(
cbuffer CBPerFrame : register(b0) {
    row_major float4x4 uModelViewProj;
    row_major float4x4 uModelView;
    float4   uViewPos;
    float4   uLightDir;
    float    uAmbientStrength;
    float    uSpecularPower;
    float2   pad0;
};

struct VSInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texcoord : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 texcoord : TEXCOORD2;
    float3 eyePos   : TEXCOORD3;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(float4(input.position, 1.0), uModelViewProj);
    output.eyePos   = mul(float4(input.position, 1.0), uModelView).xyz;
    output.worldPos = input.position;
    output.normal   = normalize(mul(float4(input.normal, 0.0), uModelView).xyz);
    output.texcoord = input.texcoord;
    return output;
}
)";

static const char* MODEL_PS = R"(
cbuffer CBPerFrame : register(b0) {
    row_major float4x4 uModelViewProj;
    row_major float4x4 uModelView;
    float4   uViewPos;
    float4   uLightDir;
    float    uAmbientStrength;
    float    uSpecularPower;
    float2   pad0;
};

cbuffer CBPerMaterial : register(b1) {
    float4 uTintColor;
    float4 uTintZone1;
    float4 uTintZone2;
    float4 uTintZone3;
    float  uAgeAmount;
    float4 uStubbleAmount;
    float4 uTattooAmount;
    float4 uTattooColor1;
    float4 uTattooColor2;
    float4 uTattooColor3;
    int    uUseDiffuse;
    int    uUseNormal;
    int    uUseSpecular;
    int    uUseTint;
    int    uUseAlphaTest;
    int    uIsEyeMesh;
    int    uIsFaceMesh;
    int    uUseAge;
    int    uUseStubble;
    int    uUseTattoo;
    float2 pad1;
};

Texture2D    texDiffuse        : register(t0);
Texture2D    texNormal         : register(t1);
Texture2D    texSpecular       : register(t2);
Texture2D    texTint           : register(t3);
Texture2D    texAgeDiffuse     : register(t4);
Texture2D    texAgeNormal      : register(t5);
Texture2D    texStubble        : register(t6);
Texture2D    texStubbleNormal  : register(t7);
Texture2D    texTattoo         : register(t8);
SamplerState sampLinear        : register(s0);

struct PSInput {
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 texcoord : TEXCOORD2;
    float3 eyePos   : TEXCOORD3;
};

float4 main(PSInput input) : SV_TARGET {
    float4 diffuseColor;
    if (uUseDiffuse != 0) {
        diffuseColor = texDiffuse.Sample(sampLinear, input.texcoord);
        if (uUseAlphaTest != 0 && diffuseColor.a < 0.1)
            discard;
    } else {
        diffuseColor = float4(0.7, 0.7, 0.7, 1.0);
    }

    float3 baseNormal = float3(0.0, 0.0, 1.0);
    if (uUseNormal != 0) {
        baseNormal = texNormal.Sample(sampLinear, input.texcoord).rgb * 2.0 - 1.0;
    }

    // Face mesh: age, stubble, tattoo
    if (uIsFaceMesh != 0) {
        if (uUseAge != 0 && uAgeAmount > 0.0) {
            float4 ageDiffuse = texAgeDiffuse.Sample(sampLinear, input.texcoord);
            diffuseColor.rgb = lerp(diffuseColor.rgb, ageDiffuse.rgb, uAgeAmount);
            float3 ageNormal = texAgeNormal.Sample(sampLinear, input.texcoord).rgb * 2.0 - 1.0;
            baseNormal = lerp(baseNormal, ageNormal, uAgeAmount);
        }
        if (uUseStubble != 0) {
            float4 stubbleMask = texStubble.Sample(sampLinear, input.texcoord);
            float stubbleR = stubbleMask.r * uStubbleAmount.r;
            float stubbleG = stubbleMask.g * uStubbleAmount.g;
            float stubbleB = stubbleMask.b * uStubbleAmount.b;
            float stubbleA = stubbleMask.a * uStubbleAmount.a;
            float totalStubble = max(stubbleR, max(stubbleG, max(stubbleB, stubbleA)));
            if (totalStubble > 0.0) {
                diffuseColor.rgb = lerp(diffuseColor.rgb, diffuseColor.rgb * 0.3, totalStubble);
                float3 stubbleNormal = texStubbleNormal.Sample(sampLinear, input.texcoord).rgb * 2.0 - 1.0;
                baseNormal = lerp(baseNormal, stubbleNormal, totalStubble);
            }
        }
        if (uUseTattoo != 0) {
            float4 tattooMask = texTattoo.Sample(sampLinear, input.texcoord);
            if (tattooMask.r > 0.01 && uTattooAmount.r > 0.0)
                diffuseColor.rgb = lerp(diffuseColor.rgb, uTattooColor1.rgb, tattooMask.r * uTattooAmount.r);
            if (tattooMask.g > 0.01 && uTattooAmount.g > 0.0)
                diffuseColor.rgb = lerp(diffuseColor.rgb, uTattooColor2.rgb, tattooMask.g * uTattooAmount.g);
            if (tattooMask.b > 0.01 && uTattooAmount.b > 0.0)
                diffuseColor.rgb = lerp(diffuseColor.rgb, uTattooColor3.rgb, tattooMask.b * uTattooAmount.b);
        }
    }

    // Tinting
    if (uIsEyeMesh != 0 && uUseTint != 0) {
        float4 tintMask = texTint.Sample(sampLinear, input.texcoord);
        float irisAmount = tintMask.r;
        float3 irisColor = uTintColor.rgb * (0.5 + diffuseColor.rgb * 0.5);
        diffuseColor.rgb = lerp(diffuseColor.rgb, irisColor, irisAmount);
    } else {
        diffuseColor.rgb *= uTintColor.rgb;
        if (uUseTint != 0) {
            float4 tintMask = texTint.Sample(sampLinear, input.texcoord);
            float3 zoneColor = diffuseColor.rgb;
            zoneColor = lerp(zoneColor, zoneColor * uTintZone1.rgb, tintMask.r);
            zoneColor = lerp(zoneColor, zoneColor * uTintZone2.rgb, tintMask.g);
            zoneColor = lerp(zoneColor, zoneColor * uTintZone3.rgb, tintMask.b);
            diffuseColor.rgb = zoneColor;
        }
    }

    // Lighting
    float3 N = normalize(input.normal);
    if (uUseNormal != 0 || (uIsFaceMesh != 0 && (uUseAge != 0 || uUseStubble != 0))) {
        N = normalize(N + baseNormal * 0.3);
    }
    float3 L = normalize(float3(0.3, 0.5, 1.0));
    float3 V = normalize(-input.eyePos);
    float NdotL = max(dot(N, L), 0.0);
    float3 ambient  = uAmbientStrength * diffuseColor.rgb;
    float3 diffuse  = NdotL * diffuseColor.rgb;
    float3 specular = float3(0.0, 0.0, 0.0);
    if (uUseSpecular != 0 && NdotL > 0.0) {
        float3 H = normalize(L + V);
        float NdotH = max(dot(N, H), 0.0);
        float spec = pow(NdotH, uSpecularPower);
        float4 specMap = texSpecular.Sample(sampLinear, input.texcoord);
        specular = spec * specMap.rgb * 0.5;
    }

    float3 finalColor = ambient + diffuse + specular;
    return float4(saturate(finalColor), diffuseColor.a);
}
)";


static const char* SIMPLE_VS = R"(
cbuffer CBSimple : register(b0) {
    row_major float4x4 uModelViewProj;
    float4   uColor;
};

struct VSInput {
    float3 position : POSITION;
    float3 normal   : NORMAL;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 normal   : TEXCOORD0;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(float4(input.position, 1.0), uModelViewProj);
    output.normal   = input.normal;
    return output;
}
)";

static const char* SIMPLE_PS = R"(
cbuffer CBSimple : register(b0) {
    row_major float4x4 uModelViewProj;
    float4   uColor;
};

struct PSInput {
    float4 position : SV_POSITION;
    float3 normal   : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET {
    float3 L = normalize(float3(0.3, 0.5, 1.0));
    float NdotL = max(dot(normalize(input.normal), L), 0.0);
    float shade = 0.35 + 0.65 * NdotL;
    return float4(uColor.rgb * shade, uColor.a);
}
)";


static const char* LINE_VS = R"(
cbuffer CBSimple : register(b0) {
    row_major float4x4 uModelViewProj;
    float4   uColor;
};

struct VSInput {
    float3 position : POSITION;
    float4 color    : COLOR;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(float4(input.position, 1.0), uModelViewProj);
    output.color    = input.color;
    return output;
}
)";

static const char* LINE_PS = R"(
struct PSInput {
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

float4 main(PSInput input) : SV_TARGET {
    return input.color;
}
)";


void ShaderProgram::release() {
    if (vs)          { vs->Release();          vs          = nullptr; }
    if (ps)          { ps->Release();          ps          = nullptr; }
    if (inputLayout) { inputLayout->Release(); inputLayout = nullptr; }
    valid = false;
}

ID3DBlob* compileShader(const char* source, const char* entryPoint, const char* target) {
    ID3DBlob* blob  = nullptr;
    ID3DBlob* error = nullptr;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = D3DCompile(source, strlen(source), nullptr, nullptr, nullptr,
                            entryPoint, target, flags, 0, &blob, &error);
    if (FAILED(hr)) {
        if (error) {
            std::cerr << "[SHADER] Compilation error (" << target << "): "
                      << (char*)error->GetBufferPointer() << std::endl;
            error->Release();
        }
        return nullptr;
    }
    if (error) error->Release();
    return blob;
}

static bool createModelShader(ID3D11Device* device) {
    ID3DBlob* vsBlob = compileShader(MODEL_VS, "main", "vs_5_0");
    if (!vsBlob) return false;

    ID3DBlob* psBlob = compileShader(MODEL_PS, "main", "ps_5_0");
    if (!psBlob) { vsBlob->Release(); return false; }

    HRESULT hr;
    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                    nullptr, &s_modelShader.vs);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); return false; }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                   nullptr, &s_modelShader.ps);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); s_modelShader.release(); return false; }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = device->CreateInputLayout(layout, 3, vsBlob->GetBufferPointer(),
                                   vsBlob->GetBufferSize(), &s_modelShader.inputLayout);
    vsBlob->Release();
    psBlob->Release();

    if (FAILED(hr)) { s_modelShader.release(); return false; }

    s_modelShader.valid = true;
    std::cout << "[SHADER] Model shader created" << std::endl;
    return true;
}

static bool createSimpleShader(ID3D11Device* device) {
    ID3DBlob* vsBlob = compileShader(SIMPLE_VS, "main", "vs_5_0");
    if (!vsBlob) return false;

    ID3DBlob* psBlob = compileShader(SIMPLE_PS, "main", "ps_5_0");
    if (!psBlob) { vsBlob->Release(); return false; }

    HRESULT hr;
    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                    nullptr, &s_simpleShader.vs);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); return false; }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                   nullptr, &s_simpleShader.ps);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); s_simpleShader.release(); return false; }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(),
                                   vsBlob->GetBufferSize(), &s_simpleShader.inputLayout);
    vsBlob->Release();
    psBlob->Release();
    if (FAILED(hr)) { s_simpleShader.release(); return false; }

    s_simpleShader.valid = true;
    std::cout << "[SHADER] Simple shader created" << std::endl;
    return true;
}

static bool createSimpleLineShader(ID3D11Device* device) {
    ID3DBlob* vsBlob = compileShader(LINE_VS, "main", "vs_5_0");
    if (!vsBlob) return false;

    ID3DBlob* psBlob = compileShader(LINE_PS, "main", "ps_5_0");
    if (!psBlob) { vsBlob->Release(); return false; }

    HRESULT hr;
    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                    nullptr, &s_simpleLineShader.vs);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); return false; }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                   nullptr, &s_simpleLineShader.ps);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); s_simpleLineShader.release(); return false; }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(),
                                   vsBlob->GetBufferSize(), &s_simpleLineShader.inputLayout);
    vsBlob->Release();
    psBlob->Release();
    if (FAILED(hr)) { s_simpleLineShader.release(); return false; }

    s_simpleLineShader.valid = true;
    std::cout << "[SHADER] Simple line shader created" << std::endl;
    return true;
}

static bool createConstantBuffers(ID3D11Device* device) {
    auto makeCB = [&](UINT size, ID3D11Buffer** out) -> bool {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth      = size;
        bd.Usage           = D3D11_USAGE_DYNAMIC;
        bd.BindFlags       = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags  = D3D11_CPU_ACCESS_WRITE;
        return SUCCEEDED(device->CreateBuffer(&bd, nullptr, out));
    };

    if (!makeCB(sizeof(CBPerFrame), &s_cbPerFrame))       return false;
    if (!makeCB(sizeof(CBPerMaterial), &s_cbPerMaterial))  return false;
    if (!makeCB(sizeof(CBSimple), &s_cbSimple))            return false;

    std::cout << "[SHADER] Constant buffers created" << std::endl;
    return true;
}


bool initShaderSystem() {
    if (s_shadersInitialized) return s_shadersAvailable;
    s_shadersInitialized = true;

    D3DContext& d3d = getD3DContext();
    if (!d3d.valid) {
        std::cerr << "[SHADER] D3D context not initialized" << std::endl;
        s_shadersAvailable = false;
        return false;
    }

    if (!createConstantBuffers(d3d.device))      { s_shadersAvailable = false; return false; }
    if (!createModelShader(d3d.device))           { s_shadersAvailable = false; return false; }
    if (!createSimpleShader(d3d.device))          { s_shadersAvailable = false; return false; }
    if (!createSimpleLineShader(d3d.device))      { s_shadersAvailable = false; return false; }

    s_shadersAvailable = true;
    std::cout << "[SHADER] Shader system initialized" << std::endl;
    return true;
}

void cleanupShaderSystem() {
    s_modelShader.release();
    s_simpleShader.release();
    s_simpleLineShader.release();
    if (s_cbPerFrame)    { s_cbPerFrame->Release();    s_cbPerFrame    = nullptr; }
    if (s_cbPerMaterial) { s_cbPerMaterial->Release(); s_cbPerMaterial = nullptr; }
    if (s_cbSimple)      { s_cbSimple->Release();      s_cbSimple      = nullptr; }
    s_shadersInitialized = false;
    s_shadersAvailable   = false;
}

bool shadersAvailable() { return s_shadersAvailable; }

ShaderProgram& getModelShader()      { return s_modelShader; }
ShaderProgram& getSimpleShader()     { return s_simpleShader; }
ShaderProgram& getSimpleLineShader() { return s_simpleLineShader; }

ID3D11Buffer* getPerFrameCB()    { return s_cbPerFrame; }
ID3D11Buffer* getPerMaterialCB() { return s_cbPerMaterial; }
ID3D11Buffer* getSimpleCB()      { return s_cbSimple; }

static void updateCB(ID3D11Buffer* buffer, const void* data, size_t size) {
    D3DContext& d3d = getD3DContext();
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(d3d.context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, data, size);
        d3d.context->Unmap(buffer, 0);
    }
}

void updatePerFrameCB(const CBPerFrame& data)       { updateCB(s_cbPerFrame, &data, sizeof(data)); }
void updatePerMaterialCB(const CBPerMaterial& data)  { updateCB(s_cbPerMaterial, &data, sizeof(data)); }
void updateSimpleCB(const CBSimple& data)            { updateCB(s_cbSimple, &data, sizeof(data)); }