#include "shader.h"

#include "d3d_context.h"

#include <vector>

#include <cstring>
#include <iostream>

static bool s_shadersInitialized = false;

static bool s_shadersAvailable   = false;

static ShaderProgram s_modelShader;

static ShaderProgram s_simpleShader;

static ShaderProgram s_simpleLineShader;

static ShaderProgram s_skyShader;

static ID3D11Buffer* s_cbPerFrame    = nullptr;

static ID3D11Buffer* s_cbPerMaterial = nullptr;

static ID3D11Buffer* s_cbSimple      = nullptr;
static ID3D11Buffer* s_cbTerrain     = nullptr;
static ID3D11Buffer* s_cbWater       = nullptr;
static ID3D11Buffer* s_cbSkyDome     = nullptr;

static const char* MODEL_VS = R"(
cbuffer CBPerFrame : register(b0) {
    row_major float4x4 uModelViewProj;
    row_major float4x4 uModelView;
    float4   uViewPos;
    float4   uLightDir;
    float    uAmbientStrength;
    float    uSpecularPower;
    float2   pad0;
    float4   uLightColor;
    float4   uFogColor;
    float4   uFogParams;
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
    float3 worldNormal : TEXCOORD4;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(float4(input.position, 1.0), uModelViewProj);
    output.eyePos   = mul(float4(input.position, 1.0), uModelView).xyz;
    output.worldPos = input.position;
    output.normal   = normalize(mul(float4(input.normal, 0.0), uModelView).xyz);
    output.worldNormal = input.normal;
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
    float4   uLightColor;
    float4   uFogColor;
    float4   uFogParams;
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
    float4 uHighlightColor;
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
SamplerState sampPoint         : register(s1);
cbuffer CBTerrain : register(b2) {
    float4 uPalDim;
    float4 uPalParam;
    float4 uUVScales0;
    float4 uUVScales1;
    float4 uReliefScales0;
    float4 uReliefScales1;
    int    uIsTerrain;
    int    uTerrainDebug;
    int2   tpad;
};

cbuffer CBWater : register(b3) {
    float4 uWave0;
    float4 uWave1;
    float4 uWave2;
    float4 uWaterColor;
    float4 uWaterVisual;
    float4 uBodyColor;
    float  uTime;
    int    uIsWater;
    int    uHasCubemap;
    int    wpad;
};

TextureCube texEnvCube : register(t9);

struct PSInput {
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 texcoord : TEXCOORD2;
    float3 eyePos   : TEXCOORD3;
    float3 worldNormal : TEXCOORD4;
};

float4 main(PSInput input) : SV_TARGET {
    float4 diffuseColor;
    float3 baseNormal = float3(0.0, 0.0, 1.0);
    float2 uvDDX = ddx(input.texcoord);
    float2 uvDDY = ddy(input.texcoord);
    if (uIsTerrain != 0) {
        float2 uv = input.texcoord;

        float cellW = uPalDim.x;
        float cellH = uPalDim.y;
        int cols = (int)uPalDim.z;
        int rows = (int)uPalDim.w;
        float padX = uPalParam.x;
        float padY = uPalParam.y;
        float usableW = uPalParam.z;
        float usableH = uPalParam.w;
        float scales[8] = { uUVScales0.x, uUVScales0.y, uUVScales0.z, uUVScales0.w,
                             uUVScales1.x, uUVScales1.y, uUVScales1.z, uUVScales1.w };
        float rScales[8] = { uReliefScales0.x, uReliefScales0.y, uReliefScales0.z, uReliefScales0.w,
                              uReliefScales1.x, uReliefScales1.y, uReliefScales1.z, uReliefScales1.w };
        float3 viewDir = normalize(uViewPos.xyz - input.worldPos);
        float totalCells = (float)(cols * rows);

        uint mw, mh;
        texSpecular.GetDimensions(mw, mh);
        float2 maskSize = float2(mw, mh);
        float2 texelSize = 1.0 / maskSize;
        float2 pixelPos = uv * maskSize - 0.5;
        float2 frac_pp = frac(pixelPos);
        float2 base = (floor(pixelPos) + 0.5) * texelSize;
        float2 off10 = float2(texelSize.x, 0);
        float2 off01 = float2(0, texelSize.y);
        float2 off11 = texelSize;

        float bw[4] = {
            (1 - frac_pp.x) * (1 - frac_pp.y),
            frac_pp.x * (1 - frac_pp.y),
            (1 - frac_pp.x) * frac_pp.y,
            frac_pp.x * frac_pp.y
        };
        float2 corners[4] = { base, base + off10, base + off01, base + off11 };

        float cellWeights[8] = { 0,0,0,0,0,0,0,0 };

        for (int corner = 0; corner < 4; corner++) {
            float4 mv = texSpecular.SampleLevel(sampPoint, corners[corner], 0);
            float4 ma = texTint.SampleLevel(sampPoint, corners[corner], 0);
            float4 ma2 = texAgeDiffuse.SampleLevel(sampPoint, corners[corner], 0);
            float mAll[8] = { ma.r, ma.g, ma.b, ma.a, ma2.r, ma2.g, ma2.b, ma2.a };
            float mvV[3] = { mv.r, mv.g, mv.b };
            for (int ch = 0; ch < 3; ch++) {
                int ci = clamp((int)(mvV[ch] * 7.5 + 0.5), 0, (int)(totalCells - 1));
                cellWeights[ci] += mAll[ci] * bw[corner];
            }
        }

        float3 blendedColor = float3(0, 0, 0);
        float3 blendedNormal = float3(0, 0, 0);
        float totalWeight = 0.0;

        for (int ci = 0; ci < (int)totalCells; ci++) {
            float w = cellWeights[ci];
            if (w < 0.001) continue;

            float s = scales[ci];
            int col = ci / rows;
            int row = ci % rows;
            float2 cellOrigin = float2(col * cellW + padX, row * cellH + padY);

            float2 tileUV = frac(uv * s);
            float2 palUV = cellOrigin + tileUV * float2(usableW, usableH);

            float2 dx = uvDDX * s * float2(usableW, usableH);
            float2 dy = uvDDY * s * float2(usableW, usableH);

            float rs = rScales[ci];
            if (rs > 0.0001) {
                float h = texAgeNormal.SampleGrad(sampLinear, palUV, dx, dy).r;
                float2 offset = viewDir.xy * h * rs;
                palUV += offset * float2(usableW, usableH);
                palUV = clamp(palUV, cellOrigin, cellOrigin + float2(usableW, usableH));
            }

            blendedColor += texDiffuse.SampleGrad(sampLinear, palUV, dx, dy).rgb * w;
            blendedNormal += (texNormal.SampleGrad(sampLinear, palUV, dx, dy).rgb * 2.0 - 1.0) * w;
            totalWeight += w;
        }

        if (totalWeight > 0.001) {
            blendedColor /= totalWeight;
            blendedNormal /= totalWeight;
        } else {
            blendedColor = float3(0.5, 0.5, 0.5);
            blendedNormal = float3(0, 0, 1);
        }

        diffuseColor = float4(blendedColor, 1.0);
        baseNormal = normalize(blendedNormal);

        if (uTerrainDebug != 0) {
            float3 dbgColors[8] = {
                float3(1,0,0), float3(0,1,0), float3(0,0,1), float3(1,1,0),
                float3(1,0,1), float3(0,1,1), float3(1,0.5,0), float3(0.5,0,1)
            };
            float4 mv0 = texSpecular.SampleLevel(sampPoint, base, 0);
            int domCell = clamp((int)(mv0.r * 7.5 + 0.5), 0, 7);
            diffuseColor = float4(dbgColors[domCell], 1.0);
        }
    } else if (uIsWater != 0) {
        float2 uv = input.worldPos.xy;
        float t = uTime;

        float scale0 = max(uWave0.z, 0.01);
        float scale1 = max(uWave1.z, 0.01);
        float scale2 = max(uWave2.z, 0.01);
        float2 uv0 = uv * scale0 + uWave0.xy * t;
        float2 uv1 = uv * scale1 + uWave1.xy * t;
        float2 uv2 = uv * scale2 + uWave2.xy * t;

        float3 n0 = texNormal.Sample(sampLinear, uv0).rgb * 2.0 - 1.0;
        float3 n1 = texNormal.Sample(sampLinear, uv1).rgb * 2.0 - 1.0;
        float3 n2 = texNormal.Sample(sampLinear, uv2).rgb * 2.0 - 1.0;

        float3 nTest = n0 + n1 + n2;
        if (dot(nTest + 3.0, nTest + 3.0) < 0.01) {
            float2 p0 = uv * 8.0  + float2(t * 0.03,  t * 0.02);
            float2 p1 = uv * 12.0 + float2(-t * 0.02, t * 0.025);
            float2 p2 = uv * 20.0 + float2(t * 0.015, -t * 0.018);
            n0 = normalize(float3(sin(p0.x * 6.28) * cos(p0.y * 6.28), sin(p0.y * 6.28) * cos(p0.x * 6.28), 2.0));
            n1 = normalize(float3(sin(p1.x * 6.28) * cos(p1.y * 4.0),  sin(p1.y * 6.28) * cos(p1.x * 3.0),  2.0));
            n2 = normalize(float3(sin(p2.x * 5.0)  * cos(p2.y * 6.28), sin(p2.y * 5.0)  * cos(p2.x * 6.28), 2.0));
        }

        float3 nw = uWaterColor.xyz;
        float nwSum = nw.x + nw.y + nw.z;
        if (nwSum < 0.001) { nw = float3(1, 1, 1); nwSum = 3.0; }
        float3 blendedN = normalize((n0 * nw.x + n1 * nw.y + n2 * nw.z) / nwSum);

        float bumpScale = max(uWaterVisual.w, 1.0);
        float3 viewDir = normalize(uViewPos.xyz - input.worldPos);
        float3 geomN = normalize(input.worldNormal);
        float3 surfaceN = normalize(geomN + float3(blendedN.xy, 0) * bumpScale);

        float NdotV = saturate(dot(surfaceN, viewDir));
        float fresnelPow = max(uWaterVisual.x, 0.1);
        float fresnel = pow(1.0 - NdotV, fresnelPow);

        float3 reflDir = reflect(-viewDir, surfaceN);
        float3 sunDir = normalize(uLightDir.xyz);
        float3 sunCol = uLightColor.rgb * uLightColor.a;

        float3 reflColor;
        if (uHasCubemap != 0) {
            reflColor = texEnvCube.Sample(sampLinear, reflDir).rgb;
        } else {
            float reflElev = saturate(reflDir.z);
            float3 zenith = float3(0.15, 0.3, 0.65);
            zenith = lerp(zenith, sunCol * 0.3 + float3(0.1, 0.15, 0.35), 0.3);
            float3 horiz = uFogColor.rgb;
            float sunH = saturate(dot(float3(reflDir.x, reflDir.y, 0), float3(sunDir.x, sunDir.y, 0)));
            horiz = lerp(horiz, sunCol * 0.6, sunH * 0.4);
            reflColor = lerp(horiz, zenith, pow(reflElev, 0.5));
            float sd = dot(reflDir, sunDir);
            reflColor += pow(saturate(sd), 128.0) * 0.6 * sunCol;
            reflColor += pow(saturate(sd), 8.0) * 0.15 * sunCol;
        }

        float3 bodyColor = uBodyColor.rgb;

        float3 waterCol = lerp(bodyColor, reflColor, fresnel);

        float3 H = normalize(sunDir + viewDir);
        float NdotH = saturate(dot(surfaceN, H));
        float specPow = max(uWaterVisual.z, 1.0);
        float specInt = uWaterVisual.y;
        waterCol += pow(NdotH, specPow) * specInt * sunCol;

        float alpha = saturate(uWaterColor.w + fresnel);
        diffuseColor = float4(waterCol, alpha);
    } else if (uUseDiffuse != 0) {
        diffuseColor = texDiffuse.Sample(sampLinear, input.texcoord);
        if (uUseAlphaTest != 0 && diffuseColor.a < 0.1)
            discard;
    } else {
        diffuseColor = float4(0.7, 0.7, 0.7, 1.0);
    }
    if (uIsTerrain == 0 && uIsWater == 0 && uUseNormal != 0) {
        baseNormal = texNormal.Sample(sampLinear, input.texcoord).rgb * 2.0 - 1.0;
    }
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
    if (uIsTerrain == 0) {
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
    }
    if (uIsWater != 0) {
        if (uHighlightColor.a > 0.0)
            diffuseColor.rgb = lerp(diffuseColor.rgb, uHighlightColor.rgb, uHighlightColor.a);
        return diffuseColor;
    }
    float3 N = normalize(input.normal);
    if (uUseNormal != 0 || uIsTerrain != 0 || (uIsFaceMesh != 0 && (uUseAge != 0 || uUseStubble != 0))) {
        N = normalize(N + baseNormal * 0.3);
    }
    float3 L = normalize(uLightDir.xyz);
    float3 V = normalize(-input.eyePos);
    float NdotL = max(dot(N, L), 0.0);
    float3 sunCol = uLightColor.rgb * uLightColor.a;
    float3 ambient  = uAmbientStrength * diffuseColor.rgb;
    float3 diffuse  = NdotL * diffuseColor.rgb * sunCol;
    float3 specular = float3(0.0, 0.0, 0.0);
    if (uIsTerrain == 0 && uUseSpecular != 0 && NdotL > 0.0) {
        float3 H = normalize(L + V);
        float NdotH = max(dot(N, H), 0.0);
        float spec = pow(NdotH, uSpecularPower);
        float4 specMap = texSpecular.Sample(sampLinear, input.texcoord);
        specular = spec * specMap.rgb * 0.5 * sunCol;
    }
    float3 finalColor = ambient + diffuse + specular;
    float fogIntensity = uFogColor.a;
    if (fogIntensity > 0.001) {
        float dist = length(input.eyePos);
        float fogCap = uFogParams.x;
        float fogZenith = max(uFogParams.y, 1.0);
        float fogFactor = saturate((dist / fogZenith) * fogIntensity);
        fogFactor = min(fogFactor, fogCap);
        finalColor = lerp(finalColor, uFogColor.rgb, fogFactor);
    }
    if (uHighlightColor.a > 0.0)
        finalColor = lerp(finalColor, uHighlightColor.rgb, uHighlightColor.a);
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

static const char* SKY_VS = R"(
cbuffer CBSkyDome : register(b0) {
    row_major float4x4 uViewProj;
    float4 uSunDir;
    float4 uSunColor;
    float4 uFogColor;
    float4 uCloudColor;
    float4 uCloudParams;
    float4 uAtmoParams;
    float4 uTimeAndPad;
};
struct VSInput { float3 position : POSITION; float3 normal : NORMAL; float2 texcoord : TEXCOORD0; };
struct VSOutput { float4 position : SV_POSITION; float3 worldDir : TEXCOORD0; float2 texcoord : TEXCOORD1; };
VSOutput main(VSInput input) {
    VSOutput output;
    output.position = mul(float4(input.position, 1.0), uViewProj);
    output.position.z = output.position.w * 0.9999;
    output.worldDir = normalize(input.position);
    output.texcoord = input.texcoord;
    return output;
}
)";

static const char* SKY_PS = R"(
cbuffer CBSkyDome : register(b0) {
    row_major float4x4 uViewProj;
    float4 uSunDir;
    float4 uSunColor;
    float4 uFogColor;
    float4 uCloudColor;
    float4 uCloudParams;
    float4 uAtmoParams;
    float4 uTimeAndPad;
};
float hash2(float2 p) {
    float3 p3 = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}
float noise2(float2 p) {
    float2 i = floor(p), f = frac(p);
    f = f*f*(3.0-2.0*f);
    return lerp(lerp(hash2(i), hash2(i+float2(1,0)), f.x),
                lerp(hash2(i+float2(0,1)), hash2(i+float2(1,1)), f.x), f.y);
}
float fbm2(float2 p) {
    float v=0, a=0.5; float2 sh=float2(100,100);
    for(int i=0;i<5;i++){v+=a*noise2(p);p=p*2.0+sh;a*=0.5;}
    return v;
}
struct VSOutput { float4 position : SV_POSITION; float3 worldDir : TEXCOORD0; float2 texcoord : TEXCOORD1; };
float4 main(VSOutput input) : SV_TARGET {
    float3 dir = normalize(input.worldDir);
    float3 sunDir = normalize(uSunDir.xyz);
    float elev = dir.z;
    float3 zenith = float3(0.15,0.3,0.65);
    zenith = lerp(zenith, uSunColor.rgb*0.3+float3(0.1,0.15,0.35), 0.3);
    float3 horiz = uFogColor.rgb;
    float sunH = saturate(dot(float3(dir.x,dir.y,0), float3(sunDir.x,sunDir.y,0)));
    horiz = lerp(horiz, uSunColor.rgb*0.6, sunH*0.4);
    float3 sky = lerp(horiz, zenith, pow(saturate(elev), 0.5));
    float sd = dot(dir, sunDir);
    sky += (smoothstep(0.9994,0.9998,sd) + pow(saturate(sd),128.0)*0.6 + pow(saturate(sd),8.0)*0.15) * uSunColor.rgb * uAtmoParams.x * 0.1;
    float cd = uCloudParams.x;
    if(cd>0.01 && elev>-0.05){
        float t=800.0/max(elev,0.01);
        float2 cuv=dir.xy*t*0.0005; cuv.x+=uTimeAndPad.x*uCloudParams.z*0.0001;
        float cn=fbm2(cuv*3.0);
        float cs=smoothstep(1.0-cd,1.0-cd*uCloudParams.y,cn)*smoothstep(-0.05,0.15,elev);
        float cl=saturate(dot(float3(0,0,1),sunDir)*0.5+0.5);
        float3 cc=lerp(uCloudColor.rgb*0.5,uCloudColor.rgb+uSunColor.rgb*0.3,cl);
        sky=lerp(sky,cc,cs*0.85);
    }
    if(elev<0) sky=lerp(sky,horiz*0.3,saturate(-elev*3.0));
    return float4(sky,1.0);
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

    return true;

}

static bool createSkyShader(ID3D11Device* device) {
    ID3DBlob* vsBlob = compileShader(SKY_VS, "main", "vs_5_0");
    if (!vsBlob) return false;
    ID3DBlob* psBlob = compileShader(SKY_PS, "main", "ps_5_0");
    if (!psBlob) { vsBlob->Release(); return false; }
    HRESULT hr;
    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &s_skyShader.vs);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); return false; }
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &s_skyShader.ps);
    if (FAILED(hr)) { vsBlob->Release(); psBlob->Release(); return false; }
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    hr = device->CreateInputLayout(layout, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &s_skyShader.inputLayout);
    vsBlob->Release(); psBlob->Release();
    if (FAILED(hr)) return false;
    std::cout << "[SHADER] Sky dome shader created" << std::endl;
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
    if (!makeCB(sizeof(CBTerrain), &s_cbTerrain))           return false;
    if (!makeCB(sizeof(CBWater), &s_cbWater))              return false;
    if (!makeCB(sizeof(CBSkyDome), &s_cbSkyDome))          return false;

    return true;

}

bool initShaderSystem() {

    if (s_shadersInitialized) return s_shadersAvailable;

    s_shadersInitialized = true;

    D3DContext& d3d = getD3DContext();

    if (!d3d.valid) {

        s_shadersAvailable = false;

        return false;

    }

    if (!createConstantBuffers(d3d.device))      { s_shadersAvailable = false; return false; }

    if (!createModelShader(d3d.device))           { s_shadersAvailable = false; return false; }

    if (!createSimpleShader(d3d.device))          { s_shadersAvailable = false; return false; }

    if (!createSimpleLineShader(d3d.device))      { s_shadersAvailable = false; return false; }
    if (!createSkyShader(d3d.device))             { std::cerr << "[SHADER] Sky shader failed (non-fatal)" << std::endl; }

    s_shadersAvailable = true;

    return true;

}

void cleanupShaderSystem() {

    s_modelShader.release();

    s_simpleShader.release();

    s_simpleLineShader.release();
    s_skyShader.release();

    if (s_cbPerFrame)    { s_cbPerFrame->Release();    s_cbPerFrame    = nullptr; }

    if (s_cbPerMaterial) { s_cbPerMaterial->Release(); s_cbPerMaterial = nullptr; }

    if (s_cbSimple)      { s_cbSimple->Release();      s_cbSimple      = nullptr; }
    if (s_cbTerrain)     { s_cbTerrain->Release();     s_cbTerrain     = nullptr; }
    if (s_cbWater)       { s_cbWater->Release();       s_cbWater       = nullptr; }
    if (s_cbSkyDome)     { s_cbSkyDome->Release();     s_cbSkyDome     = nullptr; }

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
ID3D11Buffer* getTerrainCB()     { return s_cbTerrain; }
ID3D11Buffer* getWaterCB()       { return s_cbWater; }

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
void updateTerrainCB(const CBTerrain& data)           { updateCB(s_cbTerrain, &data, sizeof(data)); }
void updateWaterCB(const CBWater& data)               { updateCB(s_cbWater, &data, sizeof(data)); }

ShaderProgram& getSkyShader()        { return s_skyShader; }
ID3D11Buffer* getSkyDomeCB()         { return s_cbSkyDome; }
void updateSkyDomeCB(const CBSkyDome& data) { updateCB(s_cbSkyDome, &data, sizeof(data)); }