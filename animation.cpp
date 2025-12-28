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
        if (track.boneName.find("_rotation") != std::string::npos) {
            track.isRotation = true;
            track.boneName = track.boneName.substr(0, track.boneName.find("_rotation"));
        } else if (track.boneName.find("_translation") != std::string::npos) {
            track.isTranslation = true;
            track.boneName = track.boneName.substr(0, track.boneName.find("_translation"));
        } else {
            continue;
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