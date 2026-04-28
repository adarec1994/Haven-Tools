#pragma once
#include <cstdint>

// Field-label quirks that GFF parsing has to special-case on X360 / PS3.
// Centralises the magic numbers so they aren't sprinkled through Gff.cpp.
namespace X360 { namespace GffQuirks {

// On X360 GFF4, label 6999 ("CHILDREN") is stored as a list-length sentinel
// that needs the BE byte-swap suppressed when reading list metadata.
constexpr uint32_t LIST_LENGTH_LABEL = 6999;

inline bool isListLengthLabel(uint32_t label) {
    return label == LIST_LENGTH_LABEL;
}

}}
