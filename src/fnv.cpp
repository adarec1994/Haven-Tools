#include "fnv.h"

uint32_t fnv32(const std::string& s) {
    uint32_t hash = 2166136261u;
    for (char c : s) {
        hash = (hash * 16777619u) ^ static_cast<uint8_t>(c);
    }
    return hash;
}

uint64_t fnv64(const std::string& s) {
    uint64_t hash = 14695981039346656037ull;
    for (char c : s) {
        hash = (hash * 1099511628211ull) ^ static_cast<uint8_t>(c);
    }
    return hash;
}