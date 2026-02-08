#pragma once
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <cstdint>
#include <vector>
#include <string>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

struct GLFWwindow;

struct D3DContext {
    ID3D11Device*           device          = nullptr;
    ID3D11DeviceContext*    context         = nullptr;
    IDXGISwapChain*         swapChain       = nullptr;
    ID3D11RenderTargetView* renderTargetView = nullptr;
    ID3D11DepthStencilView* depthStencilView = nullptr;
    ID3D11Texture2D*        depthStencilBuffer = nullptr;

    ID3D11RasterizerState*   rsSolid         = nullptr;
    ID3D11RasterizerState*   rsWireframe     = nullptr;
    ID3D11RasterizerState*   rsNoCull        = nullptr;
    ID3D11DepthStencilState* dssDefault      = nullptr;
    ID3D11DepthStencilState* dssNoDepth      = nullptr;
    ID3D11DepthStencilState* dssLessEqual    = nullptr;
    ID3D11BlendState*        bsOpaque        = nullptr;
    ID3D11BlendState*        bsAlpha         = nullptr;
    ID3D11SamplerState*      samplerLinear   = nullptr;
    ID3D11SamplerState*      samplerPoint    = nullptr;

    int width  = 0;
    int height = 0;
    bool valid = false;
};

bool initD3D(GLFWwindow* window, D3DContext& ctx);
void cleanupD3D(D3DContext& ctx);
void resizeD3D(D3DContext& ctx, int width, int height);
void beginFrame(D3DContext& ctx, float r, float g, float b, float a);
void endFrame(D3DContext& ctx);

D3DContext& getD3DContext();

uint32_t createTexture2D(const uint8_t* rgbaData, int width, int height);
uint32_t createTextureFromDDS(const std::vector<uint8_t>& ddsData);
uint32_t createTextureFromDDSHair(const std::vector<uint8_t>& ddsData);
void     destroyTexture(uint32_t texId);
ID3D11ShaderResourceView* getTextureSRV(uint32_t texId);

struct DynamicVertexBuffer {
    ID3D11Buffer* buffer = nullptr;
    uint32_t capacity = 0;
    uint32_t stride = 0;

    bool create(ID3D11Device* device, uint32_t maxVertices, uint32_t vertexStride);
    void update(ID3D11DeviceContext* ctx, const void* data, uint32_t count);
    void destroy();
};

struct MeshBuffer {
    ID3D11Buffer* vertexBuffer = nullptr;
    ID3D11Buffer* indexBuffer  = nullptr;
    uint32_t vertexCount = 0;
    uint32_t indexCount  = 0;
    uint32_t stride      = 0;
    bool valid           = false;

    void destroy();
};