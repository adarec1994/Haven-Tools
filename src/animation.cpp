#include "animation.h"
#include "Gff.h"
#include "erf.h"
#include <algorithm>
#include <set>
#include <cmath>
void decompressQuat(uint32_t quat32, uint32_t quat64, uint16_t quat48, int quality,
                    float& outX, float& outY, float& outZ, float& outW) {
    float q1, q2, q3, q0;
    int order;
    const float SQRT2 = 1.41421356f;
    if (quality == 2) {
        int raw1 = (quat32 >> 22) & 0x3FF;
        int raw2 = (quat32 >> 12) & 0x3FF;
        int raw3 = (quat32 >> 2) & 0x3FF;
        order = quat32 & 0x3;
        q1 = (raw1 - 512) / (SQRT2 * 511.0f);
        q2 = (raw2 - 512) / (SQRT2 * 511.0f);
        q3 = (raw3 - 512) / (SQRT2 * 511.0f);
    }
    else if (quality == 4) {
        int raw1 = (quat32 >> 11) & 0x1FFFFF;
        int raw2 = ((quat32 & 0x7FF) << 10) | ((quat64 >> 22) & 0x3FF);
        int raw3 = (quat64 >> 2) & 0xFFFFF;
        order = quat64 & 0x3;
        q1 = (raw1 - 1048576) / (SQRT2 * 1048575.0f);
        q2 = (raw2 - 1048576) / (SQRT2 * 1048575.0f);
        q3 = (raw3 - 524288) / (SQRT2 * 524287.0f);
    }
    else if (quality == 3) {
        int raw1 = (quat32 >> 1) & 0x7FFF;
        int raw2 = (quat64 >> 1) & 0x7FFF;
        int raw3 = (quat48 >> 1) & 0x7FFF;
        order = ((quat32 & 1) << 1) | (quat64 & 1);
        q1 = (raw1 - 16384) / (SQRT2 * 16383.0f);
        q2 = (raw2 - 16384) / (SQRT2 * 16383.0f);
        q3 = (raw3 - 16384) / (SQRT2 * 16383.0f);
    }
    else {
        outX = 0; outY = 0; outZ = 0; outW = 1;
        return;
    }
    float sq = 1.0f - q1*q1 - q2*q2 - q3*q3;
    q0 = (sq > 0) ? std::sqrt(sq) : 0.0f;
    if (order == 0) {
        outX = q0; outY = q1; outZ = q2; outW = q3;
    } else if (order == 1) {
        outX = q1; outY = q0; outZ = q2; outW = q3;
    } else if (order == 2) {
        outX = q1; outY = q2; outZ = q0; outW = q3;
    } else {
        outX = q1; outY = q2; outZ = q3; outW = q0;
    }
}
Animation loadANI(const std::vector<uint8_t>& data, const std::string& filename) {
    Animation anim;
    anim.filename = filename;
    if (data.size() < 16) return anim;
    GFFFile gff;
    if (!gff.load(data)) {
        return anim;
    }
    anim.name = gff.readStringByLabel(0, 4007, 0);
    if (anim.name.empty()) anim.name = gff.readStringByLabel(0, 4006, 0);
    if (anim.name.empty()) anim.name = filename;
    const GFFField* lenField = gff.findField(0, 4009);
    if (lenField) {
        anim.duration = gff.readFloatAt(gff.dataOffset() + lenField->dataOffset);
    }
    if (anim.duration <= 0) anim.duration = 1.0f;
    std::vector<GFFStructRef> nodeList = gff.readStructList(0, 4005, 0);
    int tracksWithKeyframes = 0;
    for (const auto& nodeRef : nodeList) {
        AnimTrack track;
        std::string fullName = gff.readStringByLabel(nodeRef.structIndex, 4000, nodeRef.offset);
        track.boneName = fullName;
        if (!track.boneName.empty()) {
            // PC path: name is a string like "root_rotation" or "root_translation"
            if (track.boneName.find("_rotation") != std::string::npos) {
                track.isRotation = true;
                track.boneName = track.boneName.substr(0, track.boneName.find("_rotation"));
            } else if (track.boneName.find("_translation") != std::string::npos) {
                track.isTranslation = true;
                track.boneName = track.boneName.substr(0, track.boneName.find("_translation"));
            } else {
                continue;
            }
        } else {
            // X360 path: label 4000 is uint32 hash, not ECString
            const GFFField* nameField = gff.findField(nodeRef.structIndex, 4000);
            if (nameField && (nameField->typeId == 4 || nameField->typeId == 5)) {
                track.nameHash = gff.readUInt32At(gff.dataOffset() + nameField->dataOffset + nodeRef.offset);
            }
            // Determine rotation vs translation from the quality/target field (label 4001)
            // Quality 2,3,4 = rotation (quat32/quat48/quat64), quality 6 = translation (vec3)
            const GFFField* qualField = gff.findField(nodeRef.structIndex, 4001);
            if (qualField) {
                uint32_t qDataPos = gff.dataOffset() + qualField->dataOffset + nodeRef.offset;
                uint32_t quality = 0;
                if (qualField->typeId == 0) quality = gff.readUInt8At(qDataPos);
                else if (qualField->typeId == 2) quality = gff.readUInt16At(qDataPos);
                else if (qualField->typeId == 4) quality = gff.readUInt32At(qDataPos);
                if (quality == 6) {
                    track.isTranslation = true;
                } else if (quality >= 2 && quality <= 5) {
                    track.isRotation = true;
                } else {
                    continue;
                }
            } else {
                // Fallback: try to determine from struct ref type
                GFFStructRef dataRef = gff.readStructRef(nodeRef.structIndex, 4004, nodeRef.offset);
                if (dataRef.structIndex > 0 && dataRef.structIndex < (int)gff.structs().size()) {
                    const auto& refStruct = gff.structs()[dataRef.structIndex];
                    if (refStruct.fieldCount >= 3) {
                        track.isRotation = true; // Default assumption
                    } else {
                        continue;
                    }
                } else {
                    continue;
                }
            }
        }
        const GFFField* targetField = gff.findField(nodeRef.structIndex, 4001);
        uint32_t target = 2;
        if (targetField) {
            uint32_t dataPos = gff.dataOffset() + targetField->dataOffset + nodeRef.offset;
            if (targetField->typeId == 0) {
                target = gff.readUInt8At(dataPos);
            } else if (targetField->typeId == 1) {
                target = (int8_t)gff.readUInt8At(dataPos);
            } else if (targetField->typeId == 2) {
                target = gff.readUInt16At(dataPos);
            } else if (targetField->typeId == 3) {
                target = (int16_t)gff.readUInt16At(dataPos);
            } else if (targetField->typeId == 4 || targetField->typeId == 5) {
                target = gff.readUInt32At(dataPos);
            } else {
                uint8_t val8 = gff.readUInt8At(dataPos);
                uint16_t val16 = gff.readUInt16At(dataPos);
                uint32_t val32 = gff.readUInt32At(dataPos);
                if (val8 >= 2 && val8 <= 6) target = val8;
                else if (val16 >= 2 && val16 <= 6) target = val16;
                else target = val32;
            }
        }
        GFFStructRef data1 = gff.readStructRef(nodeRef.structIndex, 4004, nodeRef.offset);
        if (data1.structIndex == 0 && data1.offset == 0) {
            continue;
        }
        std::vector<GFFStructRef> keyframes = gff.readStructList(data1.structIndex, 4004, data1.offset);
        for (const auto& kfRef : keyframes) {
            AnimKeyframe kf;
            const GFFField* timeField = gff.findField(kfRef.structIndex, 4035);
            if (timeField) {
                uint16_t timeVal = gff.readUInt16At(gff.dataOffset() + timeField->dataOffset + kfRef.offset);
                kf.time = (float)timeVal / 65535.0f * anim.duration;
            }
            const GFFField* d0 = gff.findField(kfRef.structIndex, 4036);
            const GFFField* d1 = gff.findField(kfRef.structIndex, 4037);
            const GFFField* d2 = gff.findField(kfRef.structIndex, 4038);
            if (track.isRotation && d0) {
                uint32_t off = gff.dataOffset() + d0->dataOffset + kfRef.offset;
                if (target == 2) {
                    uint32_t quat32 = gff.readUInt32At(off);
                    decompressQuat(quat32, 0, 0, 2, kf.x, kf.y, kf.z, kf.w);
                }
                else if (target == 4) {
                    uint32_t quat64_low = gff.readUInt32At(off);
                    uint32_t quat64_high = gff.readUInt32At(off + 4);
                    uint32_t quat32 = quat64_high;
                    uint32_t quat64 = quat64_low;
                    decompressQuat(quat32, quat64, 0, 4, kf.x, kf.y, kf.z, kf.w);
                }
                else if (target == 3) {
                    uint32_t q32 = gff.readUInt16At(off);
                    uint32_t q64 = 0;
                    uint16_t q48 = 0;
                    if (d1) {
                        q64 = gff.readUInt16At(gff.dataOffset() + d1->dataOffset + kfRef.offset);
                    }
                    if (d2) {
                        q48 = gff.readUInt16At(gff.dataOffset() + d2->dataOffset + kfRef.offset);
                    }
                    decompressQuat(q32, q64, q48, 3, kf.x, kf.y, kf.z, kf.w);
                }
                else {
                    kf.x = 0; kf.y = 0; kf.z = 0; kf.w = 1;
                }
            }
            else if (track.isTranslation && target == 6) {
                if (d0 && d1 && d2) {
                    kf.x = gff.readFloatAt(gff.dataOffset() + d0->dataOffset + kfRef.offset);
                    kf.y = gff.readFloatAt(gff.dataOffset() + d1->dataOffset + kfRef.offset);
                    kf.z = gff.readFloatAt(gff.dataOffset() + d2->dataOffset + kfRef.offset);
                    kf.w = 0;
                }
            }
            track.keyframes.push_back(kf);
        }
        if (!track.keyframes.empty()) {
            tracksWithKeyframes++;
            anim.tracks.push_back(track);
        }
    }
    return anim;
}

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

void findAnimationsForModel(AppState& state, const std::string& modelBaseName) {
    state.availableAnimFiles.clear();
    state.selectedAnimIndex = -1;
    state.animPlaying = false;
    state.animTime = 0.0f;
    state.currentAnim = Animation();

    std::set<std::string> foundNames;

    if (!state.currentModelAnimations.empty()) {

        std::set<std::string> targetAnims;
        for (const auto& anim : state.currentModelAnimations) {
            std::string animLower = anim;
            std::transform(animLower.begin(), animLower.end(), animLower.begin(), ::tolower);
            targetAnims.insert(animLower + ".ani");
        }

        for (const auto& erfPath : state.erfFiles) {
            ERFFile erf;
            if (erf.open(erfPath)) {
                for (const auto& entry : erf.entries()) {
                    if (isAnimFile(entry.name)) {
                        std::string entryLower = entry.name;
                        std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);

                        if (targetAnims.find(entryLower) != targetAnims.end() &&
                            foundNames.find(entryLower) == foundNames.end()) {
                            foundNames.insert(entryLower);
                            state.availableAnimFiles.push_back({entry.name, erfPath});
                        }
                    }
                }
            }
        }
    }

    if (state.availableAnimFiles.empty()) {
        std::string baseNameLower = modelBaseName;
        std::transform(baseNameLower.begin(), baseNameLower.end(), baseNameLower.begin(), ::tolower);
        std::string prefix;
        if (baseNameLower.length() >= 2) {
            prefix = baseNameLower.substr(0, 2);
        } else {
            prefix = baseNameLower;
        }

        for (const auto& erfPath : state.erfFiles) {
            ERFFile erf;
            if (erf.open(erfPath)) {
                for (const auto& entry : erf.entries()) {
                    if (isAnimFile(entry.name)) {
                        std::string entryLower = entry.name;
                        std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
                        if (entryLower.find(prefix) == 0 && foundNames.find(entryLower) == foundNames.end()) {
                            foundNames.insert(entryLower);
                            state.availableAnimFiles.push_back({entry.name, erfPath});
                        }
                    }
                }
            }
        }
    }

    std::sort(state.availableAnimFiles.begin(), state.availableAnimFiles.end());
    state.basePoseBones = state.currentModel.skeleton.bones;
}
void dumpAllAnimFileNames(const AppState& state) {
    std::set<std::string> allAnis;
    for (const auto& erfPath : state.erfFiles) {
        ERFFile erf;
        if (erf.open(erfPath)) {
            for (const auto& entry : erf.entries()) {
                if (isAnimFile(entry.name)) {
                    allAnis.insert(entry.name);
                }
            }
        }
    }
    for (const auto& name : allAnis) {
    }
}
void applyAnimation(Model& model, const Animation& anim, float time, const std::vector<Bone>& basePose) {
    if (anim.tracks.empty()) return;
    if (basePose.empty() || basePose.size() != model.skeleton.bones.size()) return;

    static std::string lastModelName;
    std::string modelName = model.skeleton.bones.empty() ? "" : model.skeleton.bones[0].name;
    if (modelName != lastModelName) {
        lastModelName = modelName;
        for (size_t i = 0; i < model.skeleton.bones.size() && i < 10; i++) {
            const auto& b = model.skeleton.bones[i];
        }
    }

    for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
        model.skeleton.bones[i].posX = basePose[i].posX;
        model.skeleton.bones[i].posY = basePose[i].posY;
        model.skeleton.bones[i].posZ = basePose[i].posZ;
        model.skeleton.bones[i].rotX = basePose[i].rotX;
        model.skeleton.bones[i].rotY = basePose[i].rotY;
        model.skeleton.bones[i].rotZ = basePose[i].rotZ;
        model.skeleton.bones[i].rotW = basePose[i].rotW;
    }

    auto quatRotate = [](float qx, float qy, float qz, float qw,
                         float vx, float vy, float vz,
                         float& ox, float& oy, float& oz) {
        float tx = 2.0f * (qy * vz - qz * vy);
        float ty = 2.0f * (qz * vx - qx * vz);
        float tz = 2.0f * (qx * vy - qy * vx);
        ox = vx + qw * tx + (qy * tz - qz * ty);
        oy = vy + qw * ty + (qz * tx - qx * tz);
        oz = vz + qw * tz + (qx * ty - qy * tx);
    };

    auto quatMul = [](float q1x, float q1y, float q1z, float q1w,
                      float q2x, float q2y, float q2z, float q2w,
                      float& rx, float& ry, float& rz, float& rw) {
        rw = q1w*q2w - q1x*q2x - q1y*q2y - q1z*q2z;
        rx = q1w*q2x + q1x*q2w + q1y*q2z - q1z*q2y;
        ry = q1w*q2y - q1x*q2z + q1y*q2w + q1z*q2x;
        rz = q1w*q2z + q1x*q2y - q1y*q2x + q1z*q2w;
    };

    for (const auto& track : anim.tracks) {
        if (track.boneIndex < 0 || track.boneIndex >= (int)model.skeleton.bones.size()) continue;
        if (track.keyframes.empty()) continue;

        size_t k0 = 0, k1 = 0;
        for (size_t i = 0; i < track.keyframes.size(); i++) {
            if (track.keyframes[i].time <= time) k0 = i;
            if (track.keyframes[i].time >= time) { k1 = i; break; }
            k1 = i;
        }

        float t = 0.0f;
        if (k0 != k1 && track.keyframes[k1].time != track.keyframes[k0].time) {
            t = (time - track.keyframes[k0].time) / (track.keyframes[k1].time - track.keyframes[k0].time);
        }

        const AnimKeyframe& kf0 = track.keyframes[k0];
        const AnimKeyframe& kf1 = track.keyframes[k1];
        Bone& bone = model.skeleton.bones[track.boneIndex];

        if (track.isRotation) {
            float dot = kf0.x*kf1.x + kf0.y*kf1.y + kf0.z*kf1.z + kf0.w*kf1.w;
            float sign = (dot < 0) ? -1.0f : 1.0f;
            float rx = kf0.x * (1-t) + kf1.x * sign * t;
            float ry = kf0.y * (1-t) + kf1.y * sign * t;
            float rz = kf0.z * (1-t) + kf1.z * sign * t;
            float rw = kf0.w * (1-t) + kf1.w * sign * t;
            float len = std::sqrt(rx*rx + ry*ry + rz*rz + rw*rw);
            if (len > 0.0001f) { rx /= len; ry /= len; rz /= len; rw /= len; }
            bone.rotX = rx;
            bone.rotY = ry;
            bone.rotZ = rz;
            bone.rotW = rw;
        }
        else if (track.isTranslation) {
            std::string boneName = model.skeleton.bones[track.boneIndex].name;
            std::string boneNameLower = boneName;
            std::transform(boneNameLower.begin(), boneNameLower.end(), boneNameLower.begin(), ::tolower);
            if (boneNameLower == "god" || boneNameLower == "gob") {
                continue;
            }
            float tx = kf0.x * (1-t) + kf1.x * t;
            float ty = kf0.y * (1-t) + kf1.y * t;
            float tz = kf0.z * (1-t) + kf1.z * t;

            const Bone& base = basePose[track.boneIndex];
            bone.posX = base.posX + tx;
            bone.posY = base.posY + ty;
            bone.posZ = base.posZ + tz;
        }
    }

    std::vector<int> processingOrder;
    std::vector<bool> processed(model.skeleton.bones.size(), false);
    while (processingOrder.size() < model.skeleton.bones.size()) {
        bool addedAny = false;
        for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
            if (processed[i]) continue;
            const Bone& bone = model.skeleton.bones[i];
            if (bone.parentIndex < 0 || processed[bone.parentIndex]) {
                processingOrder.push_back((int)i);
                processed[i] = true;
                addedAny = true;
            }
        }
        if (!addedAny) {
            for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
                if (!processed[i]) {
                    processingOrder.push_back((int)i);
                    processed[i] = true;
                }
            }
            break;
        }
    }

    for (int boneIdx : processingOrder) {
        Bone& bone = model.skeleton.bones[boneIdx];
        if (bone.parentIndex < 0) {
            bone.worldPosX = bone.posX;
            bone.worldPosY = bone.posY;
            bone.worldPosZ = bone.posZ;
            bone.worldRotX = bone.rotX;
            bone.worldRotY = bone.rotY;
            bone.worldRotZ = bone.rotZ;
            bone.worldRotW = bone.rotW;
        } else {
            const Bone& parent = model.skeleton.bones[bone.parentIndex];
            float rotatedX, rotatedY, rotatedZ;
            quatRotate(parent.worldRotX, parent.worldRotY, parent.worldRotZ, parent.worldRotW,
                       bone.posX, bone.posY, bone.posZ,
                       rotatedX, rotatedY, rotatedZ);
            bone.worldPosX = parent.worldPosX + rotatedX;
            bone.worldPosY = parent.worldPosY + rotatedY;
            bone.worldPosZ = parent.worldPosZ + rotatedZ;
            quatMul(parent.worldRotX, parent.worldRotY, parent.worldRotZ, parent.worldRotW,
                    bone.rotX, bone.rotY, bone.rotZ, bone.rotW,
                    bone.worldRotX, bone.worldRotY, bone.worldRotZ, bone.worldRotW);
        }
    }
}

void computeBoneWorldTransforms(Model& model) {
    auto quatRotate = [](float qx, float qy, float qz, float qw,
                         float vx, float vy, float vz,
                         float& ox, float& oy, float& oz) {
        float tx = 2.0f * (qy * vz - qz * vy);
        float ty = 2.0f * (qz * vx - qx * vz);
        float tz = 2.0f * (qx * vy - qy * vx);
        ox = vx + qw * tx + (qy * tz - qz * ty);
        oy = vy + qw * ty + (qz * tx - qx * tz);
        oz = vz + qw * tz + (qx * ty - qy * tx);
    };
    auto quatMul = [](float q1x, float q1y, float q1z, float q1w,
                      float q2x, float q2y, float q2z, float q2w,
                      float& rx, float& ry, float& rz, float& rw) {
        rw = q1w*q2w - q1x*q2x - q1y*q2y - q1z*q2z;
        rx = q1w*q2x + q1x*q2w + q1y*q2z - q1z*q2y;
        ry = q1w*q2y - q1x*q2z + q1y*q2w + q1z*q2x;
        rz = q1w*q2z + q1x*q2y - q1y*q2x + q1z*q2w;
    };
    std::vector<int> order;
    std::vector<bool> done(model.skeleton.bones.size(), false);
    while (order.size() < model.skeleton.bones.size()) {
        bool any = false;
        for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
            if (done[i]) continue;
            if (model.skeleton.bones[i].parentIndex < 0 || done[model.skeleton.bones[i].parentIndex]) {
                order.push_back((int)i);
                done[i] = true;
                any = true;
            }
        }
        if (!any) {
            for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
                if (!done[i]) { order.push_back((int)i); done[i] = true; }
            }
            break;
        }
    }
    for (int idx : order) {
        Bone& bone = model.skeleton.bones[idx];
        if (bone.parentIndex < 0) {
            bone.worldPosX = bone.posX;
            bone.worldPosY = bone.posY;
            bone.worldPosZ = bone.posZ;
            bone.worldRotX = bone.rotX;
            bone.worldRotY = bone.rotY;
            bone.worldRotZ = bone.rotZ;
            bone.worldRotW = bone.rotW;
        } else {
            const Bone& parent = model.skeleton.bones[bone.parentIndex];
            float rx, ry, rz;
            quatRotate(parent.worldRotX, parent.worldRotY, parent.worldRotZ, parent.worldRotW,
                       bone.posX, bone.posY, bone.posZ, rx, ry, rz);
            bone.worldPosX = parent.worldPosX + rx;
            bone.worldPosY = parent.worldPosY + ry;
            bone.worldPosZ = parent.worldPosZ + rz;
            quatMul(parent.worldRotX, parent.worldRotY, parent.worldRotZ, parent.worldRotW,
                    bone.rotX, bone.rotY, bone.rotZ, bone.rotW,
                    bone.worldRotX, bone.worldRotY, bone.worldRotZ, bone.worldRotW);
        }
    }
}