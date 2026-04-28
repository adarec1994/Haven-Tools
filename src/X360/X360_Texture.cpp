#include "X360_Texture.h"
#include "dds_loader.h"  // for decompressDXT1Block / decompressDXT5Block (shared with regular DDS)

#include <algorithm>
#include <cstring>
#include <cmath>

// ── Xbox 360 XDS texture support ──────────────────────────────────────────────
// XDS = raw GPU-tiled texture data + 52-byte footer with Xbox 360 fetch constant

namespace {

constexpr uint8_t XDS_GPU_DXT1 = 0x52;
constexpr uint8_t XDS_GPU_DXT3 = 0x53;
constexpr uint8_t XDS_GPU_DXT5 = 0x54;
constexpr uint8_t XDS_GPU_DXN  = 0x71;  // BC5 / ATI2
constexpr int      XDS_FOOTER  = 52;

struct XDSInfo {
    int      width;
    int      height;
    uint8_t  gpuFormat;
    int      blockSize;    // bytes per 4x4 block
    bool     tiled;
    uint32_t texDataSize;  // bytes before footer
};

bool parseXDSFooter(const std::vector<uint8_t>& data, XDSInfo& info) {
    if (data.size() < (size_t)XDS_FOOTER + 64) return false;

    uint32_t magic = 0;
    std::memcpy(&magic, data.data(), 4);
    if (magic == 0x20534444) return false; // "DDS "

    size_t footerOff = data.size() - XDS_FOOTER;
    auto r32 = [&](size_t off) -> uint32_t {
        uint32_t v; std::memcpy(&v, &data[footerOff + off], 4);
        return ((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00) |
               ((v << 8) & 0xFF0000) | ((v << 24) & 0xFF000000u);
    };

    if (r32(0) != 3 || r32(4) != 1) return false;
    if (r32(8) != 0 || r32(12) != 0 || r32(16) != 0) return false;
    if (r32(20) != 0xFFFF0000u || r32(24) != 0xFFFF0000u) return false;

    uint32_t dw7 = r32(28);
    uint32_t dw8 = r32(32);
    uint32_t dw9 = r32(36);

    info.gpuFormat   = dw8 & 0xFF;
    info.width       = (dw9 & 0x1FFF) + 1;
    info.height      = ((dw9 >> 13) & 0x1FFF) + 1;
    info.tiled       = (dw7 >> 31) & 1;
    info.texDataSize = (uint32_t)(data.size() - XDS_FOOTER);

    switch (info.gpuFormat) {
        case XDS_GPU_DXT1: info.blockSize = 8;  break;
        case XDS_GPU_DXT3: info.blockSize = 16; break;
        case XDS_GPU_DXT5: info.blockSize = 16; break;
        case XDS_GPU_DXN:  info.blockSize = 16; break;
        default: return false;
    }

    if (info.width <= 0 || info.height <= 0 || info.width > 4096 || info.height > 4096)
        return false;

    return true;
}

void endianSwap16(std::vector<uint8_t>& buf) {
    for (size_t i = 0; i + 1 < buf.size(); i += 2)
        std::swap(buf[i], buf[i + 1]);
}

// Xbox 360 Xenos GPU tiled texture addressing
// Adapted from NCDyson/RareView (https://github.com/NCDyson/RareView)
// Originally from GTA IV Xbox 360 Texture Editor
int XGAddress2DTiledX(uint32_t blockOffset, uint32_t widthInBlocks, uint32_t texelBytePitch) {
    uint32_t alignedWidth = (widthInBlocks + 31) & ~31;
    uint32_t logBpp = (texelBytePitch >> 2) + ((texelBytePitch >> 1) >> (texelBytePitch >> 2));
    uint32_t offsetByte = blockOffset << logBpp;
    uint32_t offsetTile = ((offsetByte & ~0xFFFu) >> 3) + ((offsetByte & 0x700) >> 2) + (offsetByte & 0x3F);
    uint32_t offsetMacro = offsetTile >> (7 + logBpp);

    uint32_t macroX = ((offsetMacro % (alignedWidth >> 5)) << 2);
    uint32_t tile = ((((offsetTile >> (5 + logBpp)) & 2) + (offsetByte >> 6)) & 3);
    uint32_t macro = (macroX + tile) << 3;
    uint32_t micro = (((((offsetTile >> 1) & ~0xFu) + (offsetTile & 0xF)) & ((texelBytePitch << 3) - 1))) >> logBpp;

    return (int)(macro + micro);
}

int XGAddress2DTiledY(uint32_t blockOffset, uint32_t widthInBlocks, uint32_t texelBytePitch) {
    uint32_t alignedWidth = (widthInBlocks + 31) & ~31;
    uint32_t logBpp = (texelBytePitch >> 2) + ((texelBytePitch >> 1) >> (texelBytePitch >> 2));
    uint32_t offsetByte = blockOffset << logBpp;
    uint32_t offsetTile = ((offsetByte & ~0xFFFu) >> 3) + ((offsetByte & 0x700) >> 2) + (offsetByte & 0x3F);
    uint32_t offsetMacro = offsetTile >> (7 + logBpp);

    uint32_t macroY = ((offsetMacro / (alignedWidth >> 5)) << 2);
    uint32_t tile = ((offsetTile >> (6 + logBpp)) & 1) + (((offsetByte & 0x800) >> 10));
    uint32_t macro = (macroY + tile) << 3;
    uint32_t micro = ((((offsetTile & (((texelBytePitch << 6) - 1) & ~0x1Fu)) + ((offsetTile & 0xF) << 1)) >> (3 + logBpp)) & ~1u);

    return (int)(macro + micro + ((offsetTile & 0x10) >> 4));
}

std::vector<uint8_t> untileBC(const uint8_t* src, size_t srcLen,
                              int width, int height, int blockSize)
{
    int blocksW = std::max(1, width / 4);
    int blocksH = std::max(1, height / 4);

    // The tiling requires minimum 32-block alignment; for small textures
    // we need to untile at the aligned size and then extract the real region
    int alignedW = (int)((blocksW + 31) & ~31);
    int alignedH = (int)((blocksH + 31) & ~31);

    // Untile into padded buffer using aligned dimensions
    size_t paddedSize = (size_t)alignedW * alignedH * blockSize;
    std::vector<uint8_t> padded(paddedSize, 0);

    for (int j = 0; j < alignedH; j++) {
        for (int i = 0; i < alignedW; i++) {
            uint32_t blockOffset = (uint32_t)(j * alignedW + i);
            int x = XGAddress2DTiledX(blockOffset, (uint32_t)alignedW, (uint32_t)blockSize);
            int y = XGAddress2DTiledY(blockOffset, (uint32_t)alignedW, (uint32_t)blockSize);

            size_t srcOff = (size_t)blockOffset * blockSize;
            size_t dstOff = ((size_t)y * alignedW + x) * blockSize;

            if (srcOff + blockSize <= srcLen && dstOff + blockSize <= paddedSize)
                std::memcpy(&padded[dstOff], &src[srcOff], blockSize);
        }
    }

    // Extract the real region from the padded buffer
    if (alignedW == blocksW && alignedH == blocksH) {
        padded.resize((size_t)blocksW * blocksH * blockSize);
        return padded;
    }

    std::vector<uint8_t> out((size_t)blocksW * blocksH * blockSize, 0);
    for (int by = 0; by < blocksH; by++) {
        std::memcpy(&out[(size_t)by * blocksW * blockSize],
                     &padded[(size_t)by * alignedW * blockSize],
                     (size_t)blocksW * blockSize);
    }
    return out;
}

// BC5/DXN block decompressor for normal maps (X reconstructs Z, alpha=255).
// Only used by XDS in this codebase, so it lives with the X360 module.
void decompressDXNBlock(const uint8_t* block, uint8_t* out, int stride) {
    for (int ch = 0; ch < 2; ch++) {
        const uint8_t* chBlock = block + ch * 8;
        uint8_t a0 = chBlock[0], a1 = chBlock[1];
        uint8_t alphas[8];
        alphas[0] = a0; alphas[1] = a1;
        if (a0 > a1) {
            for (int i = 2; i < 8; i++)
                alphas[i] = (uint8_t)(((8 - i) * a0 + (i - 1) * a1) / 7);
        } else {
            for (int i = 2; i < 6; i++)
                alphas[i] = (uint8_t)(((6 - i) * a0 + (i - 1) * a1) / 5);
            alphas[6] = 0; alphas[7] = 255;
        }
        uint64_t bits = 0;
        for (int i = 0; i < 6; i++)
            bits |= (uint64_t)chBlock[2 + i] << (8 * i);

        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 4; x++) {
                int idx = (int)((bits >> ((y * 4 + x) * 3)) & 7);
                out[y * stride + x * 4 + ch] = alphas[idx];
            }
        }
    }
    // Reconstruct Z from XY, set alpha to 255
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            uint8_t* p = out + y * stride + x * 4;
            float nx = (p[0] / 255.0f) * 2.0f - 1.0f;
            float ny = (p[1] / 255.0f) * 2.0f - 1.0f;
            float nz2 = 1.0f - nx * nx - ny * ny;
            float nz = nz2 > 0.0f ? sqrtf(nz2) : 0.0f;
            p[2] = (uint8_t)((nz * 0.5f + 0.5f) * 255.0f);
            p[3] = 255;
        }
    }
}

} // anonymous namespace

bool isXDS(const std::vector<uint8_t>& data) {
    XDSInfo info;
    return parseXDSFooter(data, info);
}

bool decodeXDSToRGBA(const std::vector<uint8_t>& data, std::vector<uint8_t>& rgba,
                     int& width, int& height)
{
    XDSInfo info;
    if (!parseXDSFooter(data, info)) return false;

    width = info.width;
    height = info.height;

    std::vector<uint8_t> tex(data.begin(), data.begin() + info.texDataSize);

    // Step 1: 16-bit endian swap (Xbox 360 BE → little-endian)
    endianSwap16(tex);

    // Step 2: untile (base mip only)
    if (info.tiled)
        tex = untileBC(tex.data(), tex.size(), width, height, info.blockSize);

    // Step 3: decompress to RGBA
    rgba.resize((size_t)width * height * 4);
    const uint8_t* src = tex.data();

    for (int by = 0; by < height; by += 4) {
        for (int bx = 0; bx < width; bx += 4) {
            uint8_t block[4 * 4 * 4] = {};

            if (info.gpuFormat == XDS_GPU_DXT1) {
                decompressDXT1Block(src, block, 16);
                src += 8;
            } else if (info.gpuFormat == XDS_GPU_DXT5 || info.gpuFormat == XDS_GPU_DXT3) {
                decompressDXT5Block(src, block, 16);
                src += 16;
            } else if (info.gpuFormat == XDS_GPU_DXN) {
                decompressDXNBlock(src, block, 16);
                src += 16;
            }

            for (int y = 0; y < 4 && by + y < height; y++) {
                for (int x = 0; x < 4 && bx + x < width; x++) {
                    int di = ((by + y) * width + (bx + x)) * 4;
                    rgba[di + 0] = block[y * 16 + x * 4 + 0];
                    rgba[di + 1] = block[y * 16 + x * 4 + 1];
                    rgba[di + 2] = block[y * 16 + x * 4 + 2];
                    rgba[di + 3] = block[y * 16 + x * 4 + 3];
                }
            }
        }
    }
    return true;
}
