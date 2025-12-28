#include "dds_loader.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
typedef void (APIENTRY *PFNGLCOMPRESSEDTEXIMAGE2DPROC)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data);
static PFNGLCOMPRESSEDTEXIMAGE2DPROC glCompressedTexImage2D = nullptr;
void initGLCompressedTexImage2D() {
    if (!glCompressedTexImage2D) {
        glCompressedTexImage2D = (PFNGLCOMPRESSEDTEXIMAGE2DPROC)wglGetProcAddress("glCompressedTexImage2D");
    }
}
#else
#include <GL/gl.h>
#include <GL/glext.h>
void initGLCompressedTexImage2D() {}
#endif

#ifndef GL_BGR
#define GL_BGR 0x80E0
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_RED
#define GL_RED 0x1903
#endif
#ifndef GL_ALPHA
#define GL_ALPHA 0x1906
#endif

#pragma pack(push, 1)
struct DDSPixelFormat {
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t rgbBitCount;
    uint32_t rBitMask;
    uint32_t gBitMask;
    uint32_t bBitMask;
    uint32_t aBitMask;
};

struct DDSHeader {
    uint32_t magic;
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipMapCount;
    uint32_t reserved1[11];
    DDSPixelFormat pixelFormat;
    uint32_t caps;
    uint32_t caps2;
    uint32_t caps3;
    uint32_t caps4;
    uint32_t reserved2;
};

struct DDSHeaderDX10 {
    uint32_t dxgiFormat;
    uint32_t resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlags2;
};
#pragma pack(pop)

#define FOURCC(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
#define FOURCC_DXT1 FOURCC('D', 'X', 'T', '1')
#define FOURCC_DXT2 FOURCC('D', 'X', 'T', '2')
#define FOURCC_DXT3 FOURCC('D', 'X', 'T', '3')
#define FOURCC_DXT4 FOURCC('D', 'X', 'T', '4')
#define FOURCC_DXT5 FOURCC('D', 'X', 'T', '5')
#define FOURCC_ATI1 FOURCC('A', 'T', 'I', '1')
#define FOURCC_ATI2 FOURCC('A', 'T', 'I', '2')
#define FOURCC_BC4U FOURCC('B', 'C', '4', 'U')
#define FOURCC_BC4S FOURCC('B', 'C', '4', 'S')
#define FOURCC_BC5U FOURCC('B', 'C', '5', 'U')
#define FOURCC_BC5S FOURCC('B', 'C', '5', 'S')
#define FOURCC_DX10 FOURCC('D', 'X', '1', '0')

uint32_t loadDDSTexture(const std::vector<uint8_t>& data) {
    initGLCompressedTexImage2D();

    if (data.size() < sizeof(DDSHeader)) {
        return 0;
    }

    const DDSHeader* header = reinterpret_cast<const DDSHeader*>(data.data());
    if (header->magic != 0x20534444) {
        return 0;
    }

    uint32_t width = header->width;
    uint32_t height = header->height;
    uint32_t mipCount = header->mipMapCount;
    if (mipCount == 0) mipCount = 1;

    const uint32_t DDPF_FOURCC = 0x4;
    if (header->pixelFormat.flags & DDPF_FOURCC) {
        uint32_t fourCC = header->pixelFormat.fourCC;
        if (fourCC == FOURCC_DXT5 || fourCC == FOURCC_DXT3 || fourCC == FOURCC_DXT4) {
            std::vector<uint8_t> rgba;
            int w, h;
            if (decodeDDSToRGBA(data, rgba, w, h)) {
                int nonOpaquePixels = 0;
                int totalPixels = w * h;
                for (int i = 0; i < totalPixels; i++) {
                    if (rgba[i * 4 + 3] < 255) nonOpaquePixels++;
                }

                uint32_t texId;
                glGenTextures(1, &texId);
                glBindTexture(GL_TEXTURE_2D, texId);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
                return texId;
            }
        }
    }

    size_t headerSize = sizeof(DDSHeader);
    uint32_t format = 0;
    uint32_t blockSize = 16;
    bool compressed = false;
    uint32_t internalFormat = GL_RGBA;

    const uint32_t DDPF_ALPHAPIXELS = 0x1;
    const uint32_t DDPF_ALPHA = 0x2;
    const uint32_t DDPF_RGB = 0x40;
    const uint32_t DDPF_LUMINANCE = 0x20000;

    if (header->pixelFormat.flags & DDPF_FOURCC) {
        uint32_t fourCC = header->pixelFormat.fourCC;
        char fcc[5] = {0};
        memcpy(fcc, &fourCC, 4);

        switch (fourCC) {
            case FOURCC_DXT1:
                format = 0x83F1;
                blockSize = 8;
                compressed = true;
                break;
            case FOURCC_DXT2:
            case FOURCC_DXT3:
                format = 0x83F2;
                blockSize = 16;
                compressed = true;
                break;
            case FOURCC_DXT4:
            case FOURCC_DXT5:
                format = 0x83F3;
                blockSize = 16;
                compressed = true;
                break;
            case FOURCC_ATI1:
            case FOURCC_BC4U:
                format = 0x8DBB;
                blockSize = 8;
                compressed = true;
                break;
            case FOURCC_ATI2:
            case FOURCC_BC5U:
                format = 0x8DBD;
                blockSize = 16;
                compressed = true;
                break;
            case FOURCC_DX10:
                return 0;
            default:
                return 0;
        }
    } else if (header->pixelFormat.flags & DDPF_RGB) {
        compressed = false;
        if (header->pixelFormat.rgbBitCount == 32) {
            format = GL_RGBA;
            internalFormat = GL_RGBA;
            if (header->pixelFormat.bBitMask == 0x000000FF) {
                format = GL_BGRA;
            }
        } else if (header->pixelFormat.rgbBitCount == 24) {
            format = GL_RGB;
            internalFormat = GL_RGB;
            if (header->pixelFormat.bBitMask == 0x000000FF) {
                format = GL_BGR;
            }
        } else if (header->pixelFormat.rgbBitCount == 16) {
            compressed = false;
            format = GL_RGB;
            internalFormat = GL_RGB;
        } else {
            return 0;
        }
    } else if (header->pixelFormat.flags & DDPF_LUMINANCE) {
        compressed = false;
        if (header->pixelFormat.rgbBitCount == 8) {
            format = GL_RED;
            internalFormat = GL_RED;
        } else {
            return 0;
        }
    } else if (header->pixelFormat.flags & DDPF_ALPHA) {
        compressed = false;
        format = GL_ALPHA;
        internalFormat = GL_ALPHA;
    } else {
        return 0;
    }

    uint32_t texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipCount > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    const uint8_t* dataPtr = data.data() + headerSize;
    size_t offset = 0;

    for (uint32_t level = 0; level < mipCount && (width || height); level++) {
        if (width == 0) width = 1;
        if (height == 0) height = 1;

        if (compressed) {
            uint32_t size = ((width + 3) / 4) * ((height + 3) / 4) * blockSize;
            if (offset + size > data.size() - headerSize) break;
            glCompressedTexImage2D(GL_TEXTURE_2D, level, format, width, height, 0, size, dataPtr + offset);
            offset += size;
        } else {
            uint32_t bytesPerPixel = header->pixelFormat.rgbBitCount / 8;
            if (bytesPerPixel == 0) bytesPerPixel = 1;
            uint32_t size = width * height * bytesPerPixel;
            if (offset + size > data.size() - headerSize) break;

            if (header->pixelFormat.rgbBitCount == 16) {
                std::vector<uint8_t> converted(width * height * 3);
                const uint16_t* src16 = reinterpret_cast<const uint16_t*>(dataPtr + offset);

                uint32_t rMask = header->pixelFormat.rBitMask;
                uint32_t gMask = header->pixelFormat.gBitMask;
                uint32_t bMask = header->pixelFormat.bBitMask;

                for (uint32_t i = 0; i < width * height; i++) {
                    uint16_t pixel = src16[i];
                    uint8_t r, g, b;

                    if (rMask == 0xF800 && gMask == 0x07E0 && bMask == 0x001F) {
                        r = ((pixel >> 11) & 0x1F) * 255 / 31;
                        g = ((pixel >> 5) & 0x3F) * 255 / 63;
                        b = (pixel & 0x1F) * 255 / 31;
                    } else if (rMask == 0x7C00 && gMask == 0x03E0 && bMask == 0x001F) {
                        r = ((pixel >> 10) & 0x1F) * 255 / 31;
                        g = ((pixel >> 5) & 0x1F) * 255 / 31;
                        b = (pixel & 0x1F) * 255 / 31;
                    } else if (rMask == 0x0F00 && gMask == 0x00F0 && bMask == 0x000F) {
                        r = ((pixel >> 8) & 0x0F) * 255 / 15;
                        g = ((pixel >> 4) & 0x0F) * 255 / 15;
                        b = (pixel & 0x0F) * 255 / 15;
                    } else {
                        r = (pixel & rMask) ? 255 : 0;
                        g = (pixel & gMask) ? 255 : 0;
                        b = (pixel & bMask) ? 255 : 0;
                    }

                    converted[i * 3 + 0] = r;
                    converted[i * 3 + 1] = g;
                    converted[i * 3 + 2] = b;
                }

                glTexImage2D(GL_TEXTURE_2D, level, GL_RGB, width, height, 0,
                            GL_RGB, GL_UNSIGNED_BYTE, converted.data());
            } else {
                glTexImage2D(GL_TEXTURE_2D, level, internalFormat, width, height, 0,
                            format, GL_UNSIGNED_BYTE, dataPtr + offset);
            }
            offset += size;
        }

        width /= 2;
        height /= 2;
    }

    return texId;
}

uint32_t loadDDSTextureHair(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> rgba;
    int w, h;
    if (!decodeDDSToRGBA(data, rgba, w, h)) {
        return 0;
    }

    for (int i = 0; i < w * h; i++) {
        uint8_t r = rgba[i * 4 + 0];
        uint8_t b = rgba[i * 4 + 2];
        rgba[i * 4 + 0] = r;
        rgba[i * 4 + 1] = r;
        rgba[i * 4 + 2] = r;
        rgba[i * 4 + 3] = b;
    }

    uint32_t texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    int level = 0;
    int mipW = w, mipH = h;
    std::vector<uint8_t> mipData = rgba;

    while (mipW > 0 && mipH > 0) {
        glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, mipW, mipH, 0, GL_RGBA, GL_UNSIGNED_BYTE, mipData.data());

        if (mipW == 1 && mipH == 1) break;

        int newW = std::max(1, mipW / 2);
        int newH = std::max(1, mipH / 2);
        std::vector<uint8_t> newMip(newW * newH * 4);

        for (int y = 0; y < newH; y++) {
            for (int x = 0; x < newW; x++) {
                int srcX = x * 2;
                int srcY = y * 2;

                int r = 0, g = 0, b = 0, a = 0;
                int count = 0;
                for (int dy = 0; dy < 2 && srcY + dy < mipH; dy++) {
                    for (int dx = 0; dx < 2 && srcX + dx < mipW; dx++) {
                        int idx = ((srcY + dy) * mipW + (srcX + dx)) * 4;
                        r += mipData[idx + 0];
                        g += mipData[idx + 1];
                        b += mipData[idx + 2];
                        a = std::max(a, (int)mipData[idx + 3]);
                        count++;
                    }
                }

                int dstIdx = (y * newW + x) * 4;
                newMip[dstIdx + 0] = r / count;
                newMip[dstIdx + 1] = g / count;
                newMip[dstIdx + 2] = b / count;
                newMip[dstIdx + 3] = a;
            }
        }

        mipData = std::move(newMip);
        mipW = newW;
        mipH = newH;
        level++;
    }

    return texId;
}

static void decompressDXT1Block(const uint8_t* block, uint8_t* out, int stride) {
    uint16_t c0 = block[0] | (block[1] << 8);
    uint16_t c1 = block[2] | (block[3] << 8);

    uint8_t colors[4][4];
    colors[0][0] = ((c0 >> 11) & 0x1F) * 255 / 31;
    colors[0][1] = ((c0 >> 5) & 0x3F) * 255 / 63;
    colors[0][2] = (c0 & 0x1F) * 255 / 31;
    colors[0][3] = 255;

    colors[1][0] = ((c1 >> 11) & 0x1F) * 255 / 31;
    colors[1][1] = ((c1 >> 5) & 0x3F) * 255 / 63;
    colors[1][2] = (c1 & 0x1F) * 255 / 31;
    colors[1][3] = 255;

    if (c0 > c1) {
        colors[2][0] = (2 * colors[0][0] + colors[1][0]) / 3;
        colors[2][1] = (2 * colors[0][1] + colors[1][1]) / 3;
        colors[2][2] = (2 * colors[0][2] + colors[1][2]) / 3;
        colors[2][3] = 255;
        colors[3][0] = (colors[0][0] + 2 * colors[1][0]) / 3;
        colors[3][1] = (colors[0][1] + 2 * colors[1][1]) / 3;
        colors[3][2] = (colors[0][2] + 2 * colors[1][2]) / 3;
        colors[3][3] = 255;
    } else {
        colors[2][0] = (colors[0][0] + colors[1][0]) / 2;
        colors[2][1] = (colors[0][1] + colors[1][1]) / 2;
        colors[2][2] = (colors[0][2] + colors[1][2]) / 2;
        colors[2][3] = 255;
        colors[3][0] = 0; colors[3][1] = 0; colors[3][2] = 0; colors[3][3] = 0;
    }

    uint32_t bits = block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24);
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int idx = bits & 3;
            bits >>= 2;
            uint8_t* p = out + y * stride + x * 4;
            p[0] = colors[idx][0];
            p[1] = colors[idx][1];
            p[2] = colors[idx][2];
            p[3] = colors[idx][3];
        }
    }
}

static void decompressDXT5Block(const uint8_t* block, uint8_t* out, int stride) {
    uint8_t a0 = block[0], a1 = block[1];
    uint8_t alphas[8];
    alphas[0] = a0; alphas[1] = a1;
    if (a0 > a1) {
        for (int i = 2; i < 8; i++) alphas[i] = ((8 - i) * a0 + (i - 1) * a1) / 7;
    } else {
        for (int i = 2; i < 6; i++) alphas[i] = ((6 - i) * a0 + (i - 1) * a1) / 5;
        alphas[6] = 0; alphas[7] = 255;
    }

    uint64_t alphaBits = 0;
    for (int i = 0; i < 6; i++) alphaBits |= (uint64_t)block[2 + i] << (8 * i);

    decompressDXT1Block(block + 8, out, stride);

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int idx = (alphaBits >> ((y * 4 + x) * 3)) & 7;
            out[y * stride + x * 4 + 3] = alphas[idx];
        }
    }
}

bool decodeDDSToRGBA(const std::vector<uint8_t>& data, std::vector<uint8_t>& rgba, int& width, int& height) {
    if (data.size() < sizeof(DDSHeader)) return false;

    const DDSHeader* header = reinterpret_cast<const DDSHeader*>(data.data());
    if (header->magic != 0x20534444) return false;

    width = header->width;
    height = header->height;
    size_t headerSize = sizeof(DDSHeader);

    rgba.resize(width * height * 4);

    const uint32_t DDPF_FOURCC = 0x4;
    const uint32_t DDPF_RGB = 0x40;

    if (header->pixelFormat.flags & DDPF_FOURCC) {
        uint32_t fourCC = header->pixelFormat.fourCC;
        const uint8_t* src = data.data() + headerSize;

        if (fourCC == FOURCC_DXT1) {
            int stride = width * 4;
            for (int by = 0; by < height; by += 4) {
                for (int bx = 0; bx < width; bx += 4) {
                    uint8_t block[4 * 4 * 4];
                    decompressDXT1Block(src, block, 16);
                    src += 8;
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
        } else if (fourCC == FOURCC_DXT5 || fourCC == FOURCC_DXT3 || fourCC == FOURCC_DXT4) {
            int stride = width * 4;
            for (int by = 0; by < height; by += 4) {
                for (int bx = 0; bx < width; bx += 4) {
                    uint8_t block[4 * 4 * 4];
                    decompressDXT5Block(src, block, 16);
                    src += 16;
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
        return false;
    } else if (header->pixelFormat.flags & DDPF_RGB) {
        const uint8_t* src = data.data() + headerSize;
        int bpp = header->pixelFormat.rgbBitCount / 8;
        bool bgr = (header->pixelFormat.bBitMask == 0x000000FF);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int di = (y * width + x) * 4;
                if (bpp == 4) {
                    if (bgr) {
                        rgba[di + 0] = src[2]; rgba[di + 1] = src[1]; rgba[di + 2] = src[0]; rgba[di + 3] = src[3];
                    } else {
                        rgba[di + 0] = src[0]; rgba[di + 1] = src[1]; rgba[di + 2] = src[2]; rgba[di + 3] = src[3];
                    }
                } else if (bpp == 3) {
                    if (bgr) {
                        rgba[di + 0] = src[2]; rgba[di + 1] = src[1]; rgba[di + 2] = src[0];
                    } else {
                        rgba[di + 0] = src[0]; rgba[di + 1] = src[1]; rgba[di + 2] = src[2];
                    }
                    rgba[di + 3] = 255;
                } else if (bpp == 2) {
                    uint16_t pixel = src[0] | (src[1] << 8);
                    uint32_t rMask = header->pixelFormat.rBitMask;
                    uint32_t gMask = header->pixelFormat.gBitMask;
                    uint32_t bMask = header->pixelFormat.bBitMask;

                    if (rMask == 0xF800 && gMask == 0x07E0 && bMask == 0x001F) {
                        rgba[di + 0] = ((pixel >> 11) & 0x1F) * 255 / 31;
                        rgba[di + 1] = ((pixel >> 5) & 0x3F) * 255 / 63;
                        rgba[di + 2] = (pixel & 0x1F) * 255 / 31;
                    } else if (rMask == 0x7C00 && gMask == 0x03E0 && bMask == 0x001F) {
                        rgba[di + 0] = ((pixel >> 10) & 0x1F) * 255 / 31;
                        rgba[di + 1] = ((pixel >> 5) & 0x1F) * 255 / 31;
                        rgba[di + 2] = (pixel & 0x1F) * 255 / 31;
                    } else if (rMask == 0x0F00 && gMask == 0x00F0 && bMask == 0x000F) {
                        rgba[di + 0] = ((pixel >> 8) & 0x0F) * 255 / 15;
                        rgba[di + 1] = ((pixel >> 4) & 0x0F) * 255 / 15;
                        rgba[di + 2] = (pixel & 0x0F) * 255 / 15;
                    } else {
                        rgba[di + 0] = ((pixel >> 11) & 0x1F) * 255 / 31;
                        rgba[di + 1] = ((pixel >> 5) & 0x3F) * 255 / 63;
                        rgba[di + 2] = (pixel & 0x1F) * 255 / 31;
                    }
                    rgba[di + 3] = 255;
                }
                src += bpp;
            }
        }
        return true;
    }
    return false;
}

void encodePNG(const std::vector<uint8_t>& rgba, int w, int h, std::vector<uint8_t>& png) {
    uint32_t crc_table[256];
    for (int n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xedb88320 ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
    auto crc32 = [&](const uint8_t* data, size_t len) {
        uint32_t c = 0xffffffff;
        for (size_t i = 0; i < len; i++) c = crc_table[(c ^ data[i]) & 0xff] ^ (c >> 8);
        return c ^ 0xffffffff;
    };
    auto write32be = [](std::vector<uint8_t>& v, uint32_t val) {
        v.push_back((val >> 24) & 0xff); v.push_back((val >> 16) & 0xff);
        v.push_back((val >> 8) & 0xff); v.push_back(val & 0xff);
    };
    auto writeChunk = [&](const char* type, const std::vector<uint8_t>& data) {
        write32be(png, (uint32_t)data.size());
        png.insert(png.end(), type, type + 4);
        png.insert(png.end(), data.begin(), data.end());
        std::vector<uint8_t> forCrc(type, type + 4);
        forCrc.insert(forCrc.end(), data.begin(), data.end());
        write32be(png, crc32(forCrc.data(), forCrc.size()));
    };

    png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

    std::vector<uint8_t> ihdr(13);
    ihdr[0] = (w >> 24) & 0xff; ihdr[1] = (w >> 16) & 0xff; ihdr[2] = (w >> 8) & 0xff; ihdr[3] = w & 0xff;
    ihdr[4] = (h >> 24) & 0xff; ihdr[5] = (h >> 16) & 0xff; ihdr[6] = (h >> 8) & 0xff; ihdr[7] = h & 0xff;
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    writeChunk("IHDR", ihdr);

    std::vector<uint8_t> raw;
    for (int y = 0; y < h; y++) {
        raw.push_back(0);
        raw.insert(raw.end(), rgba.begin() + y * w * 4, rgba.begin() + (y + 1) * w * 4);
    }

    std::vector<uint8_t> deflated;
    deflated.push_back(0x78); deflated.push_back(0x01);

    size_t pos = 0;
    while (pos < raw.size()) {
        size_t remain = raw.size() - pos;
        size_t blockLen = remain > 65535 ? 65535 : remain;
        bool last = (pos + blockLen >= raw.size());
        deflated.push_back(last ? 1 : 0);
        deflated.push_back(blockLen & 0xff);
        deflated.push_back((blockLen >> 8) & 0xff);
        deflated.push_back(~blockLen & 0xff);
        deflated.push_back((~blockLen >> 8) & 0xff);
        deflated.insert(deflated.end(), raw.begin() + pos, raw.begin() + pos + blockLen);
        pos += blockLen;
    }

    uint32_t adler = 1;
    for (size_t i = 0; i < raw.size(); i++) {
        uint32_t s1 = adler & 0xffff, s2 = (adler >> 16) & 0xffff;
        s1 = (s1 + raw[i]) % 65521;
        s2 = (s2 + s1) % 65521;
        adler = (s2 << 16) | s1;
    }
    deflated.push_back((adler >> 24) & 0xff);
    deflated.push_back((adler >> 16) & 0xff);
    deflated.push_back((adler >> 8) & 0xff);
    deflated.push_back(adler & 0xff);

    writeChunk("IDAT", deflated);
    writeChunk("IEND", {});
}