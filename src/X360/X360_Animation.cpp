#include "Mesh.h"
#include "animation.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>

// Xbox 360 ANI files reference bones by hash instead of name. This module
// tries a battery of hash algorithms against the skeleton's bone names and
// export table to recover the bone bindings.
//
// The public entry point is resolveX360AnimHashes() (declared in animation.h).

// Hash algorithms to try for X360 ANI node name resolution
static uint32_t hashFNV1a(const std::string& str) {
    uint32_t h = 2166136261u;
    for (char c : str) { h ^= (uint8_t)std::tolower(c); h *= 16777619u; }
    return h;
}
static uint32_t hashFNV1a_NoLower(const std::string& str) {
    uint32_t h = 2166136261u;
    for (char c : str) { h ^= (uint8_t)c; h *= 16777619u; }
    return h;
}
static uint32_t hashFNV1(const std::string& str) {
    uint32_t h = 2166136261u;
    for (char c : str) { h = (h * 16777619u) ^ (uint8_t)std::tolower(c); }
    return h;
}
static uint32_t hashDJB2(const std::string& str) {
    uint32_t h = 5381;
    for (char c : str) { h = h * 33 + (uint8_t)std::tolower(c); }
    return h;
}
static uint32_t hashJenkins(const std::string& str) {
    uint32_t h = 0;
    for (char c : str) {
        h += (uint8_t)std::tolower(c);
        h += (h << 10); h ^= (h >> 6);
    }
    h += (h << 3); h ^= (h >> 11); h += (h << 15);
    return h;
}
static uint32_t hashSDBM(const std::string& str) {
    uint32_t h = 0;
    for (char c : str) { h = (uint8_t)std::tolower(c) + (h << 6) + (h << 16) - h; }
    return h;
}
// FNV-1a on UTF-16LE bytes (wchar_t representation)
static uint32_t hashFNV1a_Wide(const std::string& str) {
    uint32_t h = 2166136261u;
    for (char c : str) {
        uint8_t lo = (uint8_t)std::tolower(c);
        h ^= lo; h *= 16777619u;
        h ^= 0;  h *= 16777619u; // high byte of UTF-16 for ASCII is 0
    }
    return h;
}
// FNV-1a on UTF-16BE bytes
static uint32_t hashFNV1a_WideBE(const std::string& str) {
    uint32_t h = 2166136261u;
    for (char c : str) {
        uint8_t lo = (uint8_t)std::tolower(c);
        h ^= 0;  h *= 16777619u; // high byte first for BE
        h ^= lo; h *= 16777619u;
    }
    return h;
}

using HashFunc = uint32_t(*)(const std::string&);
struct HashAlgo { const char* name; HashFunc func; };
static const HashAlgo s_hashAlgos[] = {
    {"FNV-1a",         hashFNV1a},
    {"FNV-1a-NoLower", hashFNV1a_NoLower},
    {"FNV-1",          hashFNV1},
    {"DJB2",           hashDJB2},
    {"Jenkins",        hashJenkins},
    {"SDBM",           hashSDBM},
    {"FNV-1a-Wide",    hashFNV1a_Wide},
    {"FNV-1a-WideBE",  hashFNV1a_WideBE},
};

void resolveX360AnimHashes(Animation& anim, const Skeleton& skeleton) {
    // Count tracks that need hash resolution
    int needResolve = 0;
    for (const auto& t : anim.tracks) {
        if (t.boneName.empty() && t.nameHash != 0) needResolve++;
    }
    if (needResolve == 0) return;

    printf("[ANI-DEBUG] resolveX360AnimHashes: %d tracks need resolution, skeleton has %zu exports, %zu bones\n",
        needResolve, skeleton.exports.size(), skeleton.bones.size());

    // Build candidate names from actual xprt export data if available
    std::vector<std::pair<std::string, std::string>> candidates; // (hashInput, boneName)
    if (!skeleton.exports.empty()) {
        for (const auto& ex : skeleton.exports) {
            candidates.push_back({ex.exportName, ex.boneName});
        }
    }
    // Also add constructed names: "bonename_rotation" and "bonename_translation"
    for (const auto& bone : skeleton.bones) {
        std::string nameLower = bone.name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        candidates.push_back({nameLower + "_rotation", bone.name});
        candidates.push_back({nameLower + "_translation", bone.name});
        // GOB/GOD style: "bonename_bonenamerotation"
        candidates.push_back({nameLower + "_" + nameLower + "rotation", bone.name});
        candidates.push_back({nameLower + "_" + nameLower + "translation", bone.name});
    }

    // Collect all hashes we need to match
    std::set<uint32_t> targetHashes;
    for (const auto& t : anim.tracks) {
        if (t.boneName.empty() && t.nameHash != 0)
            targetHashes.insert(t.nameHash);
    }

    // Try each hash algorithm
    for (const auto& algo : s_hashAlgos) {
        std::unordered_map<uint32_t, std::string> hashMap;
        for (const auto& [input, boneName] : candidates) {
            uint32_t h = algo.func(input);
            hashMap[h] = boneName;
        }
        // Count matches
        int matched = 0;
        for (uint32_t h : targetHashes) {
            if (hashMap.find(h) != hashMap.end()) matched++;
        }
        if (matched > 0 && matched >= (int)targetHashes.size() / 2) {
            // Good enough match - apply this algorithm
            printf("[ANI] X360 hash resolved with %s: %d/%d tracks matched\n",
                   algo.name, matched, (int)targetHashes.size());
            for (auto& track : anim.tracks) {
                if (track.boneName.empty() && track.nameHash != 0) {
                    auto it = hashMap.find(track.nameHash);
                    if (it != hashMap.end()) {
                        track.boneName = it->second;
                    }
                }
            }
            return;
        }
    }

    // Debug: print first few track hashes and export controller indices for comparison
    {
        printf("[ANI-DEBUG] First 10 track hashes:\n");
        int count = 0;
        for (const auto& t : anim.tracks) {
            if (t.boneName.empty() && t.nameHash != 0 && count < 10) {
                printf("[ANI-DEBUG]   track[%d] hash=0x%08X (%u) isRot=%d isTrans=%d\n",
                    count, t.nameHash, t.nameHash, t.isRotation ? 1 : 0, t.isTranslation ? 1 : 0);
                count++;
            }
        }
        printf("[ANI-DEBUG] First 10 exports (by controllerIndex):\n");
        std::vector<const BoneExport*> sortedExports;
        for (const auto& ex : skeleton.exports) sortedExports.push_back(&ex);
        std::sort(sortedExports.begin(), sortedExports.end(),
            [](const BoneExport* a, const BoneExport* b) { return a->controllerIndex < b->controllerIndex; });
        for (int i = 0; i < 10 && i < (int)sortedExports.size(); i++) {
            printf("[ANI-DEBUG]   export[%d] ctrlIdx=%u bone='%s' name='%s' isRot=%d\n",
                i, sortedExports[i]->controllerIndex, sortedExports[i]->boneName.c_str(),
                sortedExports[i]->exportName.c_str(), sortedExports[i]->isRotation ? 1 : 0);
        }
    }

    // Strategy: try direct controllerIndex matching
    // The ANI hash might simply be the controllerIndex from xprt data
    if (!skeleton.exports.empty()) {
        std::unordered_map<uint32_t, std::string> ctrlIdxMap;
        for (const auto& ex : skeleton.exports) {
            ctrlIdxMap[ex.controllerIndex] = ex.boneName;
        }
        int matched = 0;
        for (const auto& t : anim.tracks) {
            if (t.boneName.empty() && t.nameHash != 0) {
                if (ctrlIdxMap.find(t.nameHash) != ctrlIdxMap.end()) matched++;
            }
        }
        printf("[ANI-DEBUG] controllerIndex direct match: %d/%d\n", matched, needResolve);
        if (matched > 0 && matched >= needResolve / 2) {
            for (auto& track : anim.tracks) {
                if (track.boneName.empty() && track.nameHash != 0) {
                    auto it = ctrlIdxMap.find(track.nameHash);
                    if (it != ctrlIdxMap.end()) {
                        track.boneName = it->second;
                    }
                }
            }
            printf("[ANI] X360 hash resolved via controllerIndex: %d/%d tracks\n", matched, needResolve);
            return;
        }

        // Try byte-swapped controller index (in case of endian mismatch)
        std::unordered_map<uint32_t, std::string> ctrlIdxSwapMap;
        for (const auto& ex : skeleton.exports) {
            uint32_t swapped = ((ex.controllerIndex & 0xFF) << 24) |
                              ((ex.controllerIndex & 0xFF00) << 8) |
                              ((ex.controllerIndex & 0xFF0000) >> 8) |
                              ((ex.controllerIndex & 0xFF000000) >> 24);
            ctrlIdxSwapMap[swapped] = ex.boneName;
        }
        matched = 0;
        for (const auto& t : anim.tracks) {
            if (t.boneName.empty() && t.nameHash != 0) {
                if (ctrlIdxSwapMap.find(t.nameHash) != ctrlIdxSwapMap.end()) matched++;
            }
        }
        printf("[ANI-DEBUG] controllerIndex byte-swapped match: %d/%d\n", matched, needResolve);
        if (matched > 0 && matched >= needResolve / 2) {
            for (auto& track : anim.tracks) {
                if (track.boneName.empty() && track.nameHash != 0) {
                    auto it = ctrlIdxSwapMap.find(track.nameHash);
                    if (it != ctrlIdxSwapMap.end()) {
                        track.boneName = it->second;
                    }
                }
            }
            printf("[ANI] X360 hash resolved via byte-swapped controllerIndex: %d/%d tracks\n", matched, needResolve);
            return;
        }
    }

    // No algorithm matched - try using skeleton's exportHashMap if available
    if (!skeleton.exportHashMap.empty()) {
        int matched = 0;
        for (auto& track : anim.tracks) {
            if (track.boneName.empty() && track.nameHash != 0) {
                auto it = skeleton.exportHashMap.find(track.nameHash);
                if (it != skeleton.exportHashMap.end()) {
                    track.boneName = it->second;
                    matched++;
                }
            }
        }
        if (matched > 0) {
            printf("[ANI] X360 hash resolved via exportHashMap: %d/%d tracks\n",
                   matched, needResolve);
            return;
        }
    }

    // Fallback: match by controller index ordering from xprt data
    // Sort ANI hashes and xprt exports separately by rot/trans groups
    // If counts match within a group, assume same ordering
    if (!skeleton.exports.empty()) {
        // Build sorted lists of rotation and translation exports
        std::vector<const BoneExport*> rotExports, transExports;
        for (const auto& ex : skeleton.exports) {
            if (ex.isRotation) rotExports.push_back(&ex);
            else transExports.push_back(&ex);
        }
        // Sort by controller index
        auto byIdx = [](const BoneExport* a, const BoneExport* b) { return a->controllerIndex < b->controllerIndex; };
        std::sort(rotExports.begin(), rotExports.end(), byIdx);
        std::sort(transExports.begin(), transExports.end(), byIdx);

        // Collect ANI tracks by type, sorted by hash
        std::vector<AnimTrack*> rotTracks, transTracks;
        for (auto& track : anim.tracks) {
            if (track.boneName.empty() && track.nameHash != 0) {
                if (track.isRotation) rotTracks.push_back(&track);
                else if (track.isTranslation) transTracks.push_back(&track);
            }
        }
        auto byHash = [](const AnimTrack* a, const AnimTrack* b) { return a->nameHash < b->nameHash; };
        std::sort(rotTracks.begin(), rotTracks.end(), byHash);
        std::sort(transTracks.begin(), transTracks.end(), byHash);

        int matched = 0;
        // Match rotation tracks if counts are close enough
        if (!rotTracks.empty() && rotTracks.size() <= rotExports.size()) {
            // The ANI may only animate a subset of bones. If counts match exactly, do direct mapping.
            if (rotTracks.size() == rotExports.size()) {
                for (size_t i = 0; i < rotTracks.size(); i++) {
                    rotTracks[i]->boneName = rotExports[i]->boneName;
                    matched++;
                }
            }
        }
        if (!transTracks.empty() && transTracks.size() <= transExports.size()) {
            if (transTracks.size() == transExports.size()) {
                for (size_t i = 0; i < transTracks.size(); i++) {
                    transTracks[i]->boneName = transExports[i]->boneName;
                    matched++;
                }
            }
        }
        if (matched > 0) {
            printf("[ANI] X360 hash resolved via controller index ordering: %d/%d tracks (EXPERIMENTAL)\n",
                   matched, needResolve);
            return;
        }
    }

    printf("[ANI] WARNING: X360 ANI '%s' has %d hash-based tracks that could not be resolved.\n"
           "       Animations will not play. A PC ANI file for this model could provide the mapping.\n",
           anim.name.c_str(), needResolve);
}
