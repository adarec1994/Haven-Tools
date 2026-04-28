#pragma once
#include <vector>
#include <cstdint>

// Xbox 360 XDS texture format support.
//
// XDS = raw GPU-tiled texture data + 52-byte footer with the X360 fetch
// constant. Implementations live in X360_Texture.cpp; the same declarations
// are kept in dds_loader.h so existing callers don't change include paths.

bool isXDS(const std::vector<uint8_t>& data);
bool decodeXDSToRGBA(const std::vector<uint8_t>& data,
                     std::vector<uint8_t>& rgba,
                     int& width, int& height);
