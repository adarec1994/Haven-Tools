#include "d3d_context.h"
#include <iostream>
#include <unordered_map>
#include <cstring>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "dds_loader.h"

static D3DContext s_d3d;

D3DContext& getD3DContext() {
    return s_d3d;
}

struct TextureEntry {
    ID3D11Texture2D*          tex = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
};

static uint32_t s_nextTexId = 1;
static std::unordered_map<uint32_t, TextureEntry> s_textures;

uint32_t createTexture2D(const uint8_t* rgbaData, int width, int height) {
    if (!s_d3d.device || !rgbaData || width <= 0 || height <= 0) return 0;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width            = width;
    desc.Height           = height;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem          = rgbaData;
    init.SysMemPitch      = width * 4;

    TextureEntry entry;
    HRESULT hr = s_d3d.device->CreateTexture2D(&desc, &init, &entry.tex);
    if (FAILED(hr)) return 0;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                    = desc.Format;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels       = 1;

    hr = s_d3d.device->CreateShaderResourceView(entry.tex, &srvDesc, &entry.srv);
    if (FAILED(hr)) {
        entry.tex->Release();
        return 0;
    }

    uint32_t id = s_nextTexId++;
    s_textures[id] = entry;
    return id;
}

uint32_t createTextureFromDDS(const std::vector<uint8_t>& ddsData) {
    std::vector<uint8_t> rgba;
    int w, h;
    decodeDDSToRGBA(ddsData, rgba, w, h);
    if (rgba.empty()) return 0;
    return createTexture2D(rgba.data(), w, h);
}

uint32_t createTextureFromDDSHair(const std::vector<uint8_t>& ddsData) {
    std::vector<uint8_t> rgba;
    int w, h;
    if (!decodeDDSToRGBA(ddsData, rgba, w, h)) return 0;
    for (int i = 0; i < w * h; i++) {
        uint8_t r = rgba[i * 4 + 0];
        uint8_t b = rgba[i * 4 + 2];
        rgba[i * 4 + 0] = r;
        rgba[i * 4 + 1] = r;
        rgba[i * 4 + 2] = r;
        rgba[i * 4 + 3] = b;
    }
    return createTexture2D(rgba.data(), w, h);
}

void destroyTexture(uint32_t texId) {
    auto it = s_textures.find(texId);
    if (it == s_textures.end()) return;
    if (it->second.srv) it->second.srv->Release();
    if (it->second.tex) it->second.tex->Release();
    s_textures.erase(it);
}

ID3D11ShaderResourceView* getTextureSRV(uint32_t texId) {
    auto it = s_textures.find(texId);
    if (it != s_textures.end()) return it->second.srv;
    return nullptr;
}

bool DynamicVertexBuffer::create(ID3D11Device* device, uint32_t maxVertices, uint32_t vertexStride) {
    stride = vertexStride;
    capacity = maxVertices;

    D3D11_BUFFER_DESC bd = {};
    bd.Usage          = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth      = maxVertices * vertexStride;
    bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = device->CreateBuffer(&bd, nullptr, &buffer);
    return SUCCEEDED(hr);
}

void DynamicVertexBuffer::update(ID3D11DeviceContext* ctx, const void* data, uint32_t count) {
    if (!buffer || !data || count == 0) return;
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(ctx->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        uint32_t bytes = (count < capacity ? count : capacity) * stride;
        memcpy(mapped.pData, data, bytes);
        ctx->Unmap(buffer, 0);
    }
}

void DynamicVertexBuffer::destroy() {
    if (buffer) { buffer->Release(); buffer = nullptr; }
}

void MeshBuffer::destroy() {
    if (vertexBuffer) { vertexBuffer->Release(); vertexBuffer = nullptr; }
    if (indexBuffer)  { indexBuffer->Release();  indexBuffer  = nullptr; }
    valid = false;
}

static bool createPipelineStates(D3DContext& ctx) {
    {
        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_BACK;
        rd.FrontCounterClockwise = FALSE;
        rd.DepthClipEnable = TRUE;
        ctx.device->CreateRasterizerState(&rd, &ctx.rsSolid);
    }
    {
        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode = D3D11_FILL_WIREFRAME;
        rd.CullMode = D3D11_CULL_NONE;
        rd.FrontCounterClockwise = FALSE;
        rd.DepthClipEnable = TRUE;
        ctx.device->CreateRasterizerState(&rd, &ctx.rsWireframe);
    }
    {
        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.FrontCounterClockwise = FALSE;
        rd.DepthClipEnable = TRUE;
        ctx.device->CreateRasterizerState(&rd, &ctx.rsNoCull);
    }
    {
        D3D11_DEPTH_STENCIL_DESC dd = {};
        dd.DepthEnable    = TRUE;
        dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dd.DepthFunc      = D3D11_COMPARISON_LESS;
        ctx.device->CreateDepthStencilState(&dd, &ctx.dssDefault);
    }
    {
        D3D11_DEPTH_STENCIL_DESC dd = {};
        dd.DepthEnable = FALSE;
        dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        ctx.device->CreateDepthStencilState(&dd, &ctx.dssNoDepth);
    }
    {
        D3D11_DEPTH_STENCIL_DESC dd = {};
        dd.DepthEnable    = TRUE;
        dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dd.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
        ctx.device->CreateDepthStencilState(&dd, &ctx.dssLessEqual);
    }
    {
        D3D11_BLEND_DESC bd = {};
        bd.RenderTarget[0].BlendEnable    = FALSE;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        ctx.device->CreateBlendState(&bd, &ctx.bsOpaque);
    }
    {
        D3D11_BLEND_DESC bd = {};
        bd.RenderTarget[0].BlendEnable    = TRUE;
        bd.RenderTarget[0].SrcBlend       = D3D11_BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlend      = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp        = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        bd.RenderTarget[0].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        ctx.device->CreateBlendState(&bd, &ctx.bsAlpha);
    }
    {
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.MaxAnisotropy = 1;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        ctx.device->CreateSamplerState(&sd, &ctx.samplerLinear);
    }
    {
        D3D11_SAMPLER_DESC sd = {};
        sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.MaxAnisotropy = 1;
        sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        ctx.device->CreateSamplerState(&sd, &ctx.samplerPoint);
    }
    return true;
}

static void createRenderTargets(D3DContext& ctx) {
    ID3D11Texture2D* backBuffer = nullptr;
    ctx.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (backBuffer) {
        ctx.device->CreateRenderTargetView(backBuffer, nullptr, &ctx.renderTargetView);
        backBuffer->Release();
    }

    D3D11_TEXTURE2D_DESC dd = {};
    dd.Width            = ctx.width;
    dd.Height           = ctx.height;
    dd.MipLevels        = 1;
    dd.ArraySize        = 1;
    dd.Format           = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dd.SampleDesc.Count = 1;
    dd.Usage            = D3D11_USAGE_DEFAULT;
    dd.BindFlags        = D3D11_BIND_DEPTH_STENCIL;

    ctx.device->CreateTexture2D(&dd, nullptr, &ctx.depthStencilBuffer);
    if (ctx.depthStencilBuffer) {
        ctx.device->CreateDepthStencilView(ctx.depthStencilBuffer, nullptr, &ctx.depthStencilView);
    }
}

static void releaseRenderTargets(D3DContext& ctx) {
    if (ctx.renderTargetView)   { ctx.renderTargetView->Release();   ctx.renderTargetView   = nullptr; }
    if (ctx.depthStencilView)   { ctx.depthStencilView->Release();   ctx.depthStencilView   = nullptr; }
    if (ctx.depthStencilBuffer) { ctx.depthStencilBuffer->Release(); ctx.depthStencilBuffer = nullptr; }
}

bool initD3D(GLFWwindow* window, D3DContext& ctx) {
    HWND hwnd = glfwGetWin32Window(window);
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    ctx.width = w;
    ctx.height = h;

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount       = 1;
    scd.BufferDesc.Width  = w;
    scd.BufferDesc.Height = h;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator   = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow      = hwnd;
    scd.SampleDesc.Count  = 1;
    scd.Windowed          = TRUE;
    scd.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        nullptr, 0, D3D11_SDK_VERSION,
        &scd, &ctx.swapChain, &ctx.device, &featureLevel, &ctx.context
    );

    if (FAILED(hr)) {
        std::cerr << "[D3D11] Failed to create device and swap chain: 0x"
                  << std::hex << hr << std::dec << std::endl;
        return false;
    }

    std::cout << "[D3D11] Device created, feature level: 0x"
              << std::hex << featureLevel << std::dec << std::endl;

    createRenderTargets(ctx);
    createPipelineStates(ctx);

    ctx.valid = true;
    s_d3d = ctx;
    return true;
}

void resizeD3D(D3DContext& ctx, int width, int height) {
    if (!ctx.valid || width <= 0 || height <= 0) return;
    ctx.width = width;
    ctx.height = height;

    ctx.context->OMSetRenderTargets(0, nullptr, nullptr);
    releaseRenderTargets(ctx);

    ctx.swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    createRenderTargets(ctx);
}

void beginFrame(D3DContext& ctx, float r, float g, float b, float a) {
    float clearColor[4] = { r, g, b, a };
    ctx.context->ClearRenderTargetView(ctx.renderTargetView, clearColor);
    ctx.context->ClearDepthStencilView(ctx.depthStencilView,
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    ctx.context->OMSetRenderTargets(1, &ctx.renderTargetView, ctx.depthStencilView);

    D3D11_VIEWPORT vp = {};
    vp.Width    = (float)ctx.width;
    vp.Height   = (float)ctx.height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    ctx.context->RSSetViewports(1, &vp);
}

void endFrame(D3DContext& ctx) {
    ctx.swapChain->Present(1, 0);
}

void cleanupD3D(D3DContext& ctx) {
    for (auto& [id, entry] : s_textures) {
        if (entry.srv) entry.srv->Release();
        if (entry.tex) entry.tex->Release();
    }
    s_textures.clear();

    releaseRenderTargets(ctx);
    if (ctx.rsSolid)        { ctx.rsSolid->Release();        ctx.rsSolid        = nullptr; }
    if (ctx.rsWireframe)    { ctx.rsWireframe->Release();    ctx.rsWireframe    = nullptr; }
    if (ctx.rsNoCull)       { ctx.rsNoCull->Release();       ctx.rsNoCull       = nullptr; }
    if (ctx.dssDefault)     { ctx.dssDefault->Release();     ctx.dssDefault     = nullptr; }
    if (ctx.dssNoDepth)     { ctx.dssNoDepth->Release();     ctx.dssNoDepth     = nullptr; }
    if (ctx.dssLessEqual)   { ctx.dssLessEqual->Release();   ctx.dssLessEqual   = nullptr; }
    if (ctx.bsOpaque)       { ctx.bsOpaque->Release();       ctx.bsOpaque       = nullptr; }
    if (ctx.bsAlpha)        { ctx.bsAlpha->Release();        ctx.bsAlpha        = nullptr; }
    if (ctx.samplerLinear)  { ctx.samplerLinear->Release();  ctx.samplerLinear  = nullptr; }
    if (ctx.samplerPoint)   { ctx.samplerPoint->Release();   ctx.samplerPoint   = nullptr; }
    if (ctx.context)        { ctx.context->Release();        ctx.context        = nullptr; }
    if (ctx.swapChain)      { ctx.swapChain->Release();      ctx.swapChain      = nullptr; }
    if (ctx.device)         { ctx.device->Release();         ctx.device         = nullptr; }
    ctx.valid = false;
}