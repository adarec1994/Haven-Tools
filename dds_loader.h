#pragma once
#include <vector>
#include <cstdint>

// Load a DDS texture and return OpenGL texture ID (0 on failure)
uint32_t loadDDSTexture(const std::vector<uint8_t>& data);