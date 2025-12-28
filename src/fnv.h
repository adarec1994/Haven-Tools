#pragma once
#include <cstdint>
#include <string>

uint32_t fnv32(const std::string& s);
uint64_t fnv64(const std::string& s);