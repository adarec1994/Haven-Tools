#pragma once
#include <cstring>

// Detection helper for big-endian Aurora-engine platforms (Xbox 360 / PS3).
// Used by GFF parsing to decide whether to byte-swap on read.
namespace X360 {

inline bool isPlatformTag(const char* tag4) {
    return std::memcmp(tag4, "X360", 4) == 0
        || std::memcmp(tag4, "PS3 ", 4) == 0;
}

}
