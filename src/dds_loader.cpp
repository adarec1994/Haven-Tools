#include "dds_loader.h"
#include <iostream>
#include <cstring>

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

#define DDSCAPS2_CUBEMAP 0x200

bool isDDSCubemap(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(DDSHeader)) return false;
    const DDSHeader* header = reinterpret_cast<const DDSHeader*>(data.data());
    if (header->magic != 0x20534444) return false;
    return (header->caps2 & DDSCAPS2_CUBEMAP) != 0;
}

static size_t computeMipSize(int w, int h, uint32_t fourCC, int bpp) {
    if (fourCC == FOURCC_DXT1) {
        return (size_t)(((w + 3) / 4) * ((h + 3) / 4)) * 8;
    } else if (fourCC == FOURCC_DXT5 || fourCC == FOURCC_DXT3 || fourCC == FOURCC_DXT4 ||
               fourCC == FOURCC_ATI2 || fourCC == FOURCC_BC5U || fourCC == FOURCC_BC5S) {
        return (size_t)(((w + 3) / 4) * ((h + 3) / 4)) * 16;
    } else if (fourCC == FOURCC_ATI1 || fourCC == FOURCC_BC4U || fourCC == FOURCC_BC4S) {
        return (size_t)(((w + 3) / 4) * ((h + 3) / 4)) * 8;
    } else {
        return (size_t)w * h * (bpp > 0 ? bpp : 4);
    }
}

static size_t computeMipChainSize(int w, int h, int mipCount, uint32_t fourCC, int bpp) {
    size_t total = 0;
    for (int m = 0; m < mipCount; m++) {
        int mw = w >> m; if (mw < 1) mw = 1;
        int mh = h >> m; if (mh < 1) mh = 1;
        total += computeMipSize(mw, mh, fourCC, bpp);
    }
    return total;
}

bool decodeDDSCubemapFaces(const std::vector<uint8_t>& data, std::vector<uint8_t> faces[6], int& faceSize) {
    if (data.size() < sizeof(DDSHeader)) return false;
    const DDSHeader* header = reinterpret_cast<const DDSHeader*>(data.data());
    if (header->magic != 0x20534444) return false;
    if (!(header->caps2 & DDSCAPS2_CUBEMAP)) return false;

    int w = header->width;
    int h = header->height;
    if (w != h) return false;
    faceSize = w;

    int mipCount = header->mipMapCount;
    if (mipCount == 0) mipCount = 1;

    const uint32_t DDPF_FOURCC = 0x4;
    uint32_t fourCC = 0;
    int bpp = 0;
    bool isCompressed = (header->pixelFormat.flags & DDPF_FOURCC) != 0;
    if (isCompressed) {
        fourCC = header->pixelFormat.fourCC;
    } else {
        bpp = header->pixelFormat.rgbBitCount / 8;
        if (bpp == 0) bpp = 4;
    }

    size_t mip0Size = computeMipSize(w, h, fourCC, bpp);
    size_t faceChainSize = computeMipChainSize(w, h, mipCount, fourCC, bpp);
    size_t headerSize = sizeof(DDSHeader);
    const uint8_t* pixelData = data.data() + headerSize;
    size_t pixelDataSize = data.size() - headerSize;

    if (pixelDataSize < faceChainSize * 6) return false;

    for (int face = 0; face < 6; face++) {
        const uint8_t* faceStart = pixelData + face * faceChainSize;
        std::vector<uint8_t> faceRaw(faceStart, faceStart + mip0Size);

        faces[face].resize(w * h * 4);
        if (isCompressed) {
            if (fourCC == FOURCC_DXT1) {
                const uint8_t* src = faceRaw.data();
                for (int by = 0; by < h; by += 4) {
                    for (int bx = 0; bx < w; bx += 4) {
                        uint8_t block[4 * 4 * 4];
                        decompressDXT1Block(src, block, 16);
                        src += 8;
                        for (int y = 0; y < 4 && by + y < h; y++)
                            for (int x = 0; x < 4 && bx + x < w; x++) {
                                int di = ((by + y) * w + (bx + x)) * 4;
                                memcpy(&faces[face][di], &block[y * 16 + x * 4], 4);
                            }
                    }
                }
            } else if (fourCC == FOURCC_DXT5 || fourCC == FOURCC_DXT3) {
                const uint8_t* src = faceRaw.data();
                for (int by = 0; by < h; by += 4) {
                    for (int bx = 0; bx < w; bx += 4) {
                        uint8_t block[4 * 4 * 4];
                        decompressDXT5Block(src, block, 16);
                        src += 16;
                        for (int y = 0; y < 4 && by + y < h; y++)
                            for (int x = 0; x < 4 && bx + x < w; x++) {
                                int di = ((by + y) * w + (bx + x)) * 4;
                                memcpy(&faces[face][di], &block[y * 16 + x * 4], 4);
                            }
                    }
                }
            } else {
                memset(faces[face].data(), 128, faces[face].size());
            }
        } else {
            const uint8_t* src = faceRaw.data();
            for (int i = 0; i < w * h; i++) {
                if (bpp >= 3) {
                    faces[face][i * 4 + 0] = src[i * bpp + 0];
                    faces[face][i * 4 + 1] = src[i * bpp + 1];
                    faces[face][i * 4 + 2] = src[i * bpp + 2];
                    faces[face][i * 4 + 3] = (bpp >= 4) ? src[i * bpp + 3] : 255;
                }
            }
        }
    }
    return true;
}

bool decodeTGAToRGBA(const std::vector<uint8_t>& data, std::vector<uint8_t>& rgba, int& width, int& height) {
    if (data.size() < 18) return false;
    uint8_t idLen = data[0];
    uint8_t colorMapType = data[1];
    uint8_t imageType = data[2];
    width = data[12] | (data[13] << 8);
    height = data[14] | (data[15] << 8);
    uint8_t bpp = data[16];
    uint8_t descriptor = data[17];
    bool topToBottom = (descriptor & 0x20) != 0;
    if (width <= 0 || height <= 0) return false;
    if (colorMapType != 0) return false;
    bool rle = (imageType == 10);
    bool uncompressed = (imageType == 2);
    bool grayscale = (imageType == 3 || imageType == 11);
    if (!rle && !uncompressed && !grayscale) return false;
    int bytesPerPixel = bpp / 8;
    if (bytesPerPixel < 1 || bytesPerPixel > 4) return false;
    size_t offset = 18 + idLen;
    if (offset > data.size()) return false;
    rgba.resize(width * height * 4);
    int totalPixels = width * height;
    int pixelsRead = 0;
    const uint8_t* src = data.data() + offset;
    const uint8_t* srcEnd = data.data() + data.size();
    auto writePixel = [&](const uint8_t* p, int idx) {
        int di = idx * 4;
        if (grayscale) {
            rgba[di] = rgba[di+1] = rgba[di+2] = p[0];
            rgba[di+3] = (bytesPerPixel >= 2) ? p[1] : 255;
        } else if (bytesPerPixel == 4) {
            rgba[di] = p[2]; rgba[di+1] = p[1]; rgba[di+2] = p[0]; rgba[di+3] = p[3];
        } else if (bytesPerPixel == 3) {
            rgba[di] = p[2]; rgba[di+1] = p[1]; rgba[di+2] = p[0]; rgba[di+3] = 255;
        } else if (bytesPerPixel == 2) {
            uint16_t v = p[0] | (p[1] << 8);
            rgba[di] = ((v >> 10) & 0x1F) * 255 / 31;
            rgba[di+1] = ((v >> 5) & 0x1F) * 255 / 31;
            rgba[di+2] = (v & 0x1F) * 255 / 31;
            rgba[di+3] = (v & 0x8000) ? 255 : 0;
        }
    };
    if (rle || imageType == 11) {
        while (pixelsRead < totalPixels && src < srcEnd) {
            uint8_t packet = *src++;
            int count = (packet & 0x7F) + 1;
            if (packet & 0x80) {
                if (src + bytesPerPixel > srcEnd) break;
                for (int j = 0; j < count && pixelsRead < totalPixels; j++)
                    writePixel(src, pixelsRead++);
                src += bytesPerPixel;
            } else {
                for (int j = 0; j < count && pixelsRead < totalPixels; j++) {
                    if (src + bytesPerPixel > srcEnd) break;
                    writePixel(src, pixelsRead++);
                    src += bytesPerPixel;
                }
            }
        }
    } else {
        for (int i = 0; i < totalPixels && src + bytesPerPixel <= srcEnd; i++) {
            writePixel(src, i);
            src += bytesPerPixel;
        }
    }
    if (!topToBottom) {
        int rowBytes = width * 4;
        std::vector<uint8_t> row(rowBytes);
        for (int y = 0; y < height / 2; y++) {
            int top = y * rowBytes;
            int bot = (height - 1 - y) * rowBytes;
            memcpy(row.data(), &rgba[top], rowBytes);
            memcpy(&rgba[top], &rgba[bot], rowBytes);
            memcpy(&rgba[bot], row.data(), rowBytes);
        }
    }
    return true;
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