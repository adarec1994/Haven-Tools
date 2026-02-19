#pragma once
#include <vector>
#include <cstdint>

bool decodeDDSToRGBA(const std::vector<uint8_t>& data, std::vector<uint8_t>& rgba, int& width, int& height);
bool decodeTGAToRGBA(const std::vector<uint8_t>& data, std::vector<uint8_t>& rgba, int& width, int& height);
void encodePNG(const std::vector<uint8_t>& rgba, int width, int height, std::vector<uint8_t>& png);
bool isDDSCubemap(const std::vector<uint8_t>& data);
bool decodeDDSCubemapFaces(const std::vector<uint8_t>& data, std::vector<uint8_t> faces[6], int& faceSize);