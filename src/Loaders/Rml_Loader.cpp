#include "rml_loader.h"
#include "Gff.h"
#include <cmath>
#include <algorithm>

static const uint32_t LABEL_POSITION = 4;
static const uint32_t LABEL_ORIENTATION = 5;
static const uint32_t LABEL_ENV_ROOM_MODEL_LIST = 3050;
static const uint32_t LABEL_ENV_MODEL_SCALE = 3059;
static const uint32_t LABEL_ENV_MODEL_ID = 3061;
static const uint32_t LABEL_ENV_MODEL_NAME = 3062;
static const uint32_t LABEL_ENV_MODEL_FILE = 3063;
static const uint32_t LABEL_ENV_ROOM_SPT_LIST = 0xd1a;
static const uint32_t LABEL_SPT_TREE_ID = 0xd1d;
static const uint32_t LABEL_SPT_SCALE = 0xd1c;

bool parseRML(const std::vector<uint8_t>& data, RMLData& outData) {
    if (data.size() < 8) return false;

    std::string magic(data.begin(), data.begin() + 8);
    if (magic != "GFF V4.0") return false;

    GFFFile gff;
    if (!gff.load(data)) return false;
    if (gff.structs().empty()) return false;

    outData.roomPosX = 0; outData.roomPosY = 0; outData.roomPosZ = 0;
    const GFFField* posField = gff.findField(0, LABEL_POSITION);
    if (posField && posField->typeId == 10) {
        uint32_t pos = gff.dataOffset() + posField->dataOffset;
        outData.roomPosX = gff.readFloatAt(pos);
        outData.roomPosY = gff.readFloatAt(pos + 4);
        outData.roomPosZ = gff.readFloatAt(pos + 8);
    }

    std::vector<GFFStructRef> mdlList = gff.readStructList(0, LABEL_ENV_ROOM_MODEL_LIST, 0);

    for (const auto& mdlRef : mdlList) {
        RMLPropInstance prop;

        uint32_t off = mdlRef.offset;
        uint32_t si = mdlRef.structIndex;

        const GFFField* pf = gff.findField(si, LABEL_POSITION);
        if (pf && pf->typeId == 10) {
            uint32_t p = gff.dataOffset() + pf->dataOffset + off;
            prop.posX = gff.readFloatAt(p);
            prop.posY = gff.readFloatAt(p + 4);
            prop.posZ = gff.readFloatAt(p + 8);
        }

        const GFFField* of_ = gff.findField(si, LABEL_ORIENTATION);
        if (of_ && of_->typeId == 13) {
            uint32_t p = gff.dataOffset() + of_->dataOffset + off;
            prop.orientX = gff.readFloatAt(p);
            prop.orientY = gff.readFloatAt(p + 4);
            prop.orientZ = gff.readFloatAt(p + 8);
            prop.orientW = gff.readFloatAt(p + 12);
        }

        const GFFField* sf = gff.findField(si, LABEL_ENV_MODEL_SCALE);
        if (sf && sf->typeId == 8) {
            prop.scale = gff.readFloatAt(gff.dataOffset() + sf->dataOffset + off);
        }

        const GFFField* idf = gff.findField(si, LABEL_ENV_MODEL_ID);
        if (idf && idf->typeId == 5) {
            prop.modelId = gff.readInt32At(gff.dataOffset() + idf->dataOffset + off);
        }

        prop.modelName = gff.readStringByLabel(si, LABEL_ENV_MODEL_NAME, off);

        prop.modelFile = gff.readStringByLabel(si, LABEL_ENV_MODEL_FILE, off);

        if (!prop.modelName.empty() || !prop.modelFile.empty()) {
            outData.props.push_back(prop);
        }
    }

    std::vector<GFFStructRef> sptList = gff.readStructList(0, LABEL_ENV_ROOM_SPT_LIST, 0);
    for (const auto& sptRef : sptList) {
        RMLSptInstance inst;
        uint32_t off = sptRef.offset;
        uint32_t si = sptRef.structIndex;

        const GFFField* pf = gff.findField(si, LABEL_POSITION);
        if (pf && pf->typeId == 10) {
            uint32_t p = gff.dataOffset() + pf->dataOffset + off;
            inst.posX = gff.readFloatAt(p);
            inst.posY = gff.readFloatAt(p + 4);
            inst.posZ = gff.readFloatAt(p + 8);
        }

        const GFFField* of_ = gff.findField(si, LABEL_ORIENTATION);
        if (of_ && of_->typeId == 13) {
            uint32_t p = gff.dataOffset() + of_->dataOffset + off;
            inst.orientX = gff.readFloatAt(p);
            inst.orientY = gff.readFloatAt(p + 4);
            inst.orientZ = gff.readFloatAt(p + 8);
            inst.orientW = gff.readFloatAt(p + 12);
        }

        const GFFField* idf = gff.findField(si, LABEL_SPT_TREE_ID);
        if (idf && idf->typeId == 5) {
            inst.treeId = gff.readInt32At(gff.dataOffset() + idf->dataOffset + off);
        }

        const GFFField* sf = gff.findField(si, LABEL_SPT_SCALE);
        if (sf && sf->typeId == 8) {
            inst.scale = gff.readFloatAt(gff.dataOffset() + sf->dataOffset + off);
        }

        outData.sptInstances.push_back(inst);
    }

    return true;
}

static void quatRotate(float qx, float qy, float qz, float qw,
                       float px, float py, float pz,
                       float& rx, float& ry, float& rz) {
    float tx = 2.0f * (qy * pz - qz * py);
    float ty = 2.0f * (qz * px - qx * pz);
    float tz = 2.0f * (qx * py - qy * px);
    rx = px + qw * tx + (qy * tz - qz * ty);
    ry = py + qw * ty + (qz * tx - qx * tz);
    rz = pz + qw * tz + (qx * ty - qy * tx);
}

void transformModelVertices(Model& model, float px, float py, float pz,
                           float qx, float qy, float qz, float qw,
                           float scale) {

    float qlen = std::sqrt(qx*qx + qy*qy + qz*qz + qw*qw);
    if (qlen > 0.00001f) { qx /= qlen; qy /= qlen; qz /= qlen; qw /= qlen; }
    else { qx = 0; qy = 0; qz = 0; qw = 1; }

    for (auto& mesh : model.meshes) {
        for (auto& v : mesh.vertices) {

            float sx = v.x * scale;
            float sy = v.y * scale;
            float sz = v.z * scale;

            float rx, ry, rz;
            quatRotate(qx, qy, qz, qw, sx, sy, sz, rx, ry, rz);

            v.x = rx + px;
            v.y = ry + py;
            v.z = rz + pz;

            float rnx, rny, rnz;
            quatRotate(qx, qy, qz, qw, v.nx, v.ny, v.nz, rnx, rny, rnz);
            v.nx = rnx;
            v.ny = rny;
            v.nz = rnz;
        }
        mesh.calculateBounds();
    }
}