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
        std::cout << "      DDS too small" << std::endl;
        return 0;
    }

    const DDSHeader* header = reinterpret_cast<const DDSHeader*>(data.data());
    if (header->magic != 0x20534444) {
        std::cout << "      Not a DDS file (bad magic)" << std::endl;
        return 0;
    }

    uint32_t width = header->width;
    uint32_t height = header->height;
    uint32_t mipCount = header->mipMapCount;
    if (mipCount == 0) mipCount = 1;

    size_t headerSize = sizeof(DDSHeader);
    uint32_t format = 0;
    uint32_t blockSize = 16;
    bool compressed = false;
    uint32_t internalFormat = GL_RGBA;

    const uint32_t DDPF_ALPHAPIXELS = 0x1;
    const uint32_t DDPF_ALPHA = 0x2;
    const uint32_t DDPF_FOURCC = 0x4;
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
                std::cout << "      DX10 extended format not fully supported" << std::endl;
                return 0;
            default:
                std::cout << "      Unsupported FourCC: " << fcc << " (0x" << std::hex << fourCC << std::dec << ")" << std::endl;
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
            std::cout << "      16-bit RGB not yet supported" << std::endl;
            return 0;
        } else {
            std::cout << "      Unsupported RGB bit count: " << header->pixelFormat.rgbBitCount << std::endl;
            return 0;
        }
    } else if (header->pixelFormat.flags & DDPF_LUMINANCE) {
        compressed = false;
        if (header->pixelFormat.rgbBitCount == 8) {
            format = GL_RED;
            internalFormat = GL_RED;
        } else {
            std::cout << "      Unsupported luminance bit count: " << header->pixelFormat.rgbBitCount << std::endl;
            return 0;
        }
    } else if (header->pixelFormat.flags & DDPF_ALPHA) {
        compressed = false;
        format = GL_ALPHA;
        internalFormat = GL_ALPHA;
    } else {
        std::cout << "      Unsupported pixel format flags: 0x" << std::hex << header->pixelFormat.flags << std::dec << std::endl;
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
            glTexImage2D(GL_TEXTURE_2D, level, internalFormat, width, height, 0,
                        format, GL_UNSIGNED_BYTE, dataPtr + offset);
            offset += size;
        }

        width /= 2;
        height /= 2;
    }

    return texId;
}