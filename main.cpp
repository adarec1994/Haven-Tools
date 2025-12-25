#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <cmath>
#include <map>
#include <set>
#include <functional>
#include <iostream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
typedef void (APIENTRY *PFNGLCOMPRESSEDTEXIMAGE2DPROC)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data);
static PFNGLCOMPRESSEDTEXIMAGE2DPROC glCompressedTexImage2D = nullptr;
static void loadGLExtensions() {
    glCompressedTexImage2D = (PFNGLCOMPRESSEDTEXIMAGE2DPROC)wglGetProcAddress("glCompressedTexImage2D");
}
#else
#include <GL/gl.h>
#include <GL/glext.h>
static void loadGLExtensions() {}
#endif
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ImGuiFileDialog.h"
#include "erf.h"
#include "Gff.h"
#include "Mesh.h"
#include "model_loader.h"
#ifndef GL_BGR
#define GL_BGR 0x80E0
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_RED
#define GL_RED 0x1903
#endif
#ifndef GL_ALPHA
#define GL_ALPHA 0x1906
#endif
namespace fs = std::filesystem;
Material parseMAO(const std::string& maoContent, const std::string& materialName) {
    Material mat;
    mat.name = materialName;
    std::cout << "    Parsing MAO content..." << std::endl;
    size_t pos = 0;
    while ((pos = maoContent.find("<Texture", pos)) != std::string::npos) {
        size_t endTag = maoContent.find("/>", pos);
        if (endTag == std::string::npos) {
            endTag = maoContent.find("</Texture>", pos);
            if (endTag == std::string::npos) break;
        }
        std::string tag = maoContent.substr(pos, endTag - pos);
        std::string texName;
        size_t namePos = tag.find("Name=\"");
        if (namePos != std::string::npos) {
            namePos += 6;
            size_t nameEnd = tag.find("\"", namePos);
            if (nameEnd != std::string::npos) {
                texName = tag.substr(namePos, nameEnd - namePos);
            }
        }
        std::string resName;
        size_t resPos = tag.find("ResName=\"");
        if (resPos != std::string::npos) {
            resPos += 9;
            size_t resEnd = tag.find("\"", resPos);
            if (resEnd != std::string::npos) {
                resName = tag.substr(resPos, resEnd - resPos);
            }
        }
        std::cout << "      Texture tag: Name='" << texName << "' ResName='" << resName << "'" << std::endl;
        if (!texName.empty() && !resName.empty()) {
            std::string texNameLower = texName;
            std::transform(texNameLower.begin(), texNameLower.end(), texNameLower.begin(), ::tolower);
            if (texNameLower.find("diffuse") != std::string::npos) {
                mat.diffuseMap = resName;
                std::cout << "      -> Assigned as diffuse map" << std::endl;
            } else if (texNameLower.find("normal") != std::string::npos) {
                mat.normalMap = resName;
            } else if (texNameLower.find("specular") != std::string::npos) {
                mat.specularMap = resName;
            } else if (texNameLower.find("tint") != std::string::npos) {
                mat.tintMap = resName;
            }
        }
        pos = endTag + 2;
    }
    if (mat.diffuseMap.empty()) {
        std::cout << "      WARNING: No diffuse texture found in MAO!" << std::endl;
    }
    return mat;
}
#pragma pack(push, 1)
struct DDSPixelFormat {
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t rgbBitCount;
    uint32_t rBitMask;
    uint32_t gBitMask;
    uint32_t bBitMask;
    uint32_t aBitMask;
};
struct DDSHeader {
    uint32_t magic;
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipMapCount;
    uint32_t reserved1[11];
    DDSPixelFormat pixelFormat;
    uint32_t caps;
    uint32_t caps2;
    uint32_t caps3;
    uint32_t caps4;
    uint32_t reserved2;
};
struct DDSHeaderDX10 {
    uint32_t dxgiFormat;
    uint32_t resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlags2;
};
#pragma pack(pop)
#define FOURCC(a, b, c, d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
#define FOURCC_DXT1 FOURCC('D', 'X', 'T', '1')
#define FOURCC_DXT2 FOURCC('D', 'X', 'T', '2')
#define FOURCC_DXT3 FOURCC('D', 'X', 'T', '3')
#define FOURCC_DXT4 FOURCC('D', 'X', 'T', '4')
#define FOURCC_DXT5 FOURCC('D', 'X', 'T', '5')
#define FOURCC_ATI1 FOURCC('A', 'T', 'I', '1')
#define FOURCC_ATI2 FOURCC('A', 'T', 'I', '2')
#define FOURCC_BC4U FOURCC('B', 'C', '4', 'U')
#define FOURCC_BC4S FOURCC('B', 'C', '4', 'S')
#define FOURCC_BC5U FOURCC('B', 'C', '5', 'U')
#define FOURCC_BC5S FOURCC('B', 'C', '5', 'S')
#define FOURCC_DX10 FOURCC('D', 'X', '1', '0')
uint32_t loadDDSTexture(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(DDSHeader)) {
        std::cout << "      DDS too small" << std::endl;
        return 0;
    }
    const DDSHeader* header = reinterpret_cast<const DDSHeader*>(data.data());
    if (header->magic != 0x20534444) {
        std::cout << "      Not a DDS file (bad magic)" << std::endl;
        return 0;
    }
    uint32_t width = header->width;
    uint32_t height = header->height;
    uint32_t mipCount = header->mipMapCount;
    if (mipCount == 0) mipCount = 1;
    size_t headerSize = sizeof(DDSHeader);
    uint32_t format = 0;
    uint32_t blockSize = 16;
    bool compressed = false;
    uint32_t internalFormat = GL_RGBA;
    const uint32_t DDPF_ALPHAPIXELS = 0x1;
    const uint32_t DDPF_ALPHA = 0x2;
    const uint32_t DDPF_FOURCC = 0x4;
    const uint32_t DDPF_RGB = 0x40;
    const uint32_t DDPF_LUMINANCE = 0x20000;
    if (header->pixelFormat.flags & DDPF_FOURCC) {
        uint32_t fourCC = header->pixelFormat.fourCC;
        char fcc[5] = {0};
        memcpy(fcc, &fourCC, 4);
        switch (fourCC) {
            case FOURCC_DXT1:
                format = 0x83F1;
                blockSize = 8;
                compressed = true;
                break;
            case FOURCC_DXT2:
            case FOURCC_DXT3:
                format = 0x83F2;
                blockSize = 16;
                compressed = true;
                break;
            case FOURCC_DXT4:
            case FOURCC_DXT5:
                format = 0x83F3;
                blockSize = 16;
                compressed = true;
                break;
            case FOURCC_ATI1:
            case FOURCC_BC4U:
                format = 0x8DBB;
                blockSize = 8;
                compressed = true;
                break;
            case FOURCC_ATI2:
            case FOURCC_BC5U:
                format = 0x8DBD;
                blockSize = 16;
                compressed = true;
                break;
            case FOURCC_DX10:
                std::cout << "      DX10 extended format not fully supported" << std::endl;
                return 0;
            default:
                std::cout << "      Unsupported FourCC: " << fcc << " (0x" << std::hex << fourCC << std::dec << ")" << std::endl;
                return 0;
        }
    } else if (header->pixelFormat.flags & DDPF_RGB) {
        compressed = false;
        if (header->pixelFormat.rgbBitCount == 32) {
            format = GL_RGBA;
            internalFormat = GL_RGBA;
            if (header->pixelFormat.bBitMask == 0x000000FF) {
                format = GL_BGRA;
            }
        } else if (header->pixelFormat.rgbBitCount == 24) {
            format = GL_RGB;
            internalFormat = GL_RGB;
            if (header->pixelFormat.bBitMask == 0x000000FF) {
                format = GL_BGR;
            }
        } else if (header->pixelFormat.rgbBitCount == 16) {
            std::cout << "      16-bit RGB not yet supported" << std::endl;
            return 0;
        } else {
            std::cout << "      Unsupported RGB bit count: " << header->pixelFormat.rgbBitCount << std::endl;
            return 0;
        }
    } else if (header->pixelFormat.flags & DDPF_LUMINANCE) {
        compressed = false;
        if (header->pixelFormat.rgbBitCount == 8) {
            format = GL_RED;
            internalFormat = GL_RED;
        } else {
            std::cout << "      Unsupported luminance bit count: " << header->pixelFormat.rgbBitCount << std::endl;
            return 0;
        }
    } else if (header->pixelFormat.flags & DDPF_ALPHA) {
        compressed = false;
        format = GL_ALPHA;
        internalFormat = GL_ALPHA;
    } else {
        std::cout << "      Unsupported pixel format flags: 0x" << std::hex << header->pixelFormat.flags << std::dec << std::endl;
        return 0;
    }
    uint32_t texId;
    glGenTextures(1, &texId);
    glBindTexture(GL_TEXTURE_2D, texId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipCount > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    const uint8_t* dataPtr = data.data() + headerSize;
    size_t offset = 0;
    for (uint32_t level = 0; level < mipCount && (width || height); level++) {
        if (width == 0) width = 1;
        if (height == 0) height = 1;
        if (compressed) {
            uint32_t size = ((width + 3) / 4) * ((height + 3) / 4) * blockSize;
            if (offset + size > data.size() - headerSize) break;
            glCompressedTexImage2D(GL_TEXTURE_2D, level, format, width, height, 0, size, dataPtr + offset);
            offset += size;
        } else {
            uint32_t bytesPerPixel = header->pixelFormat.rgbBitCount / 8;
            if (bytesPerPixel == 0) bytesPerPixel = 1;
            uint32_t size = width * height * bytesPerPixel;
            if (offset + size > data.size() - headerSize) break;
            glTexImage2D(GL_TEXTURE_2D, level, internalFormat, width, height, 0,
                        format, GL_UNSIGNED_BYTE, dataPtr + offset);
            offset += size;
        }
        width /= 2;
        height /= 2;
    }
    return texId;
}
struct Camera {
    float x = 0.0f, y = 0.0f, z = 5.0f;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float moveSpeed = 5.0f;
    float lookSensitivity = 0.003f;
    void setPosition(float px, float py, float pz) {
        x = px;
        y = py;
        z = pz;
    }
    void lookAt(float tx, float ty, float tz, float dist) {
        x = tx;
        y = tz + dist * 0.5f;
        z = ty + dist;
        yaw = 0.0f;
        pitch = -0.2f;
        moveSpeed = dist * 0.5f;
        if (moveSpeed < 1.0f) moveSpeed = 1.0f;
    }
    void getForward(float& fx, float& fy, float& fz) const {
        fx = -std::sin(yaw) * std::cos(pitch);
        fy = std::sin(pitch);
        fz = -std::cos(yaw) * std::cos(pitch);
    }
    void getRight(float& rx, float& ry, float& rz) const {
        rx = std::cos(yaw);
        ry = 0.0f;
        rz = -std::sin(yaw);
    }
    void moveForward(float amount) {
        float fx, fy, fz;
        getForward(fx, fy, fz);
        x += fx * amount;
        y += fy * amount;
        z += fz * amount;
    }
    void moveRight(float amount) {
        float rx, ry, rz;
        getRight(rx, ry, rz);
        x += rx * amount;
        z += rz * amount;
    }
    void moveUp(float amount) {
        y += amount;
    }
    void rotate(float deltaYaw, float deltaPitch) {
        yaw += deltaYaw;
        pitch += deltaPitch;
        if (pitch > 1.5f) pitch = 1.5f;
        if (pitch < -1.5f) pitch = -1.5f;
    }
};
struct RenderSettings {
    bool wireframe = false;
    bool showAxes = true;
    bool showGrid = true;
    bool showCollision = true;
    bool collisionWireframe = true;
    bool showSkeleton = true;
    bool showBoneNames = false;
    bool showTextures = true;
    std::vector<uint8_t> meshVisible;
    void initMeshVisibility(size_t count) {
        meshVisible.resize(count, 1);
    }
};
struct AppState {
    bool showBrowser = true;
    bool showRenderSettings = false;
    bool showMaoViewer = false;
    bool showUvViewer = false;
    bool showAnimWindow = false;
    std::string maoContent;
    std::string maoFileName;
    int selectedMeshForUv = -1;
    std::string selectedFolder;
    std::vector<std::string> erfFiles;
    int selectedErfIndex = -1;
    std::unique_ptr<ERFFile> currentErf;
    int selectedEntryIndex = -1;
    std::string statusMessage;
    std::string extractPath;
    Model currentModel;
    bool hasModel = false;
    Camera camera;
    RenderSettings renderSettings;
    bool isPanning = false;
    double lastMouseX = 0;
    double lastMouseY = 0;
    std::vector<Animation> animations;
    std::vector<std::string> availableAnimFiles;
    int selectedAnimIndex = -1;
    bool animPlaying = false;
    float animTime = 0.0f;
    float animSpeed = 1.0f;
};
std::string getExeDir() {
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return fs::path(path).parent_path().string();
#else
    return fs::current_path().string();
#endif
}
void ensureExtractDir(const std::string& exeDir) {
    fs::path extractPath = fs::path(exeDir) / "extracted";
    if (!fs::exists(extractPath)) {
        fs::create_directories(extractPath);
    }
}
std::string versionToString(ERFVersion v) {
    switch (v) {
        case ERFVersion::V1_0: return "V1.0";
        case ERFVersion::V1_1: return "V1.1";
        case ERFVersion::V2_0: return "V2.0";
        case ERFVersion::V2_2: return "V2.2";
        case ERFVersion::V3_0: return "V3.0";
        default: return "Unknown";
    }
}
bool isAnimFile(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    size_t dotPos = lower.rfind('.');
    if (dotPos != std::string::npos) {
        std::string ext = lower.substr(dotPos);
        return ext == ".ani";
    }
    return false;
}
uint16_t decompressQuat16(uint16_t val) {
    return val;
}
Animation loadANI(const std::vector<uint8_t>& data, const std::string& filename) {
    Animation anim;
    anim.filename = filename;
    GFFFile gff;
    if (!gff.load(data)) {
        std::cout << "  Failed to load ANI as GFF" << std::endl;
        return anim;
    }
    uint32_t fileType = 0;
    if (data.size() >= 8) {
        memcpy(&fileType, data.data() + 4, 4);
    }
    if (fileType != 541675073) {
        std::cout << "  Not an ANI file (type=" << fileType << ")" << std::endl;
        return anim;
    }
    anim.name = gff.readStringByLabel(0, 4007, 0);
    const GFFField* lenField = gff.findField(0, 4009);
    if (lenField) {
        uint32_t lenOffset = gff.dataOffset() + lenField->dataOffset;
        anim.duration = gff.readFloatAt(lenOffset);
    }
    std::cout << "  ANI: " << anim.name << " duration=" << anim.duration << "s" << std::endl;
    std::vector<GFFStructRef> nodeList = gff.readStructList(0, 4005, 0);
    std::cout << "  Tracks: " << nodeList.size() << std::endl;
    for (const auto& nodeRef : nodeList) {
        AnimTrack track;
        track.boneName = gff.readStringByLabel(nodeRef.structIndex, 4000, nodeRef.offset);
        if (track.boneName.find("_rotation") != std::string::npos) {
            track.isRotation = true;
            size_t pos = track.boneName.find("_rotation");
            track.boneName = track.boneName.substr(0, pos);
        } else if (track.boneName.find("_translation") != std::string::npos) {
            track.isTranslation = true;
            size_t pos = track.boneName.find("_translation");
            track.boneName = track.boneName.substr(0, pos);
        }
        std::vector<GFFStructRef> data1 = gff.readStructList(nodeRef.structIndex, 4004, nodeRef.offset);
        if (!data1.empty()) {
            std::vector<GFFStructRef> keyframes = gff.readStructList(data1[0].structIndex, 4004, data1[0].offset);
            for (const auto& kfRef : keyframes) {
                AnimKeyframe kf;
                const GFFField* timeField = gff.findField(kfRef.structIndex, 4035);
                if (timeField) {
                    uint32_t timeOffset = gff.dataOffset() + timeField->dataOffset + kfRef.offset;
                    uint16_t timeVal = gff.readUInt16At(timeOffset);
                    kf.time = (float)timeVal / 65535.0f * anim.duration;
                }
                const GFFField* d0 = gff.findField(kfRef.structIndex, 4036);
                const GFFField* d1 = gff.findField(kfRef.structIndex, 4037);
                const GFFField* d2 = gff.findField(kfRef.structIndex, 4038);
                const GFFField* d3 = gff.findField(kfRef.structIndex, 4039);
                if (track.isRotation) {
                    if (d0) {
                        uint32_t off = gff.dataOffset() + d0->dataOffset + kfRef.offset;
                        int16_t qx = (int16_t)gff.readUInt16At(off);
                        int16_t qy = (int16_t)gff.readUInt16At(off + 2);
                        int16_t qz = (int16_t)gff.readUInt16At(off + 4);
                        int16_t qw = (int16_t)gff.readUInt16At(off + 6);
                        kf.x = qx / 32767.0f;
                        kf.y = qy / 32767.0f;
                        kf.z = qz / 32767.0f;
                        kf.w = qw / 32767.0f;
                    }
                } else if (track.isTranslation && d0 && d1 && d2) {
                    uint32_t off0 = gff.dataOffset() + d0->dataOffset + kfRef.offset;
                    uint32_t off1 = gff.dataOffset() + d1->dataOffset + kfRef.offset;
                    uint32_t off2 = gff.dataOffset() + d2->dataOffset + kfRef.offset;
                    kf.x = gff.readFloatAt(off0);
                    kf.y = gff.readFloatAt(off1);
                    kf.z = gff.readFloatAt(off2);
                    kf.w = 0;
                }
                track.keyframes.push_back(kf);
            }
        }
        if (!track.keyframes.empty()) {
            anim.tracks.push_back(track);
        }
    }
    return anim;
}
void findAnimationsForModel(AppState& state, const std::string& modelBaseName) {
    state.availableAnimFiles.clear();
    state.animations.clear();
    state.selectedAnimIndex = -1;
    std::string baseNameLower = modelBaseName;
    std::transform(baseNameLower.begin(), baseNameLower.end(), baseNameLower.begin(), ::tolower);
    size_t underscorePos = baseNameLower.find('_');
    std::string prefix = (underscorePos != std::string::npos) ? baseNameLower.substr(0, underscorePos + 1) : baseNameLower;
    std::cout << "Searching for animations with prefix: " << prefix << std::endl;
    for (const auto& erfPath : state.erfFiles) {
        ERFFile erf;
        if (erf.open(erfPath)) {
            for (const auto& entry : erf.entries()) {
                if (isAnimFile(entry.name)) {
                    std::string entryLower = entry.name;
                    std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
                    if (entryLower.find(prefix) == 0) {
                        bool alreadyFound = false;
                        for (const auto& existing : state.availableAnimFiles) {
                            std::string existingLower = existing;
                            std::transform(existingLower.begin(), existingLower.end(), existingLower.begin(), ::tolower);
                            if (existingLower == entryLower) {
                                alreadyFound = true;
                                break;
                            }
                        }
                        if (!alreadyFound) {
                            state.availableAnimFiles.push_back(entry.name);
                        }
                    }
                }
            }
        }
    }
    std::sort(state.availableAnimFiles.begin(), state.availableAnimFiles.end());
    std::cout << "Found " << state.availableAnimFiles.size() << " animation files" << std::endl;
}
bool isModelFile(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    size_t dotPos = lower.rfind('.');
    if (dotPos != std::string::npos) {
        std::string ext = lower.substr(dotPos);
        return ext == ".mmh" || ext == ".msh";
    }
    return false;
}
bool isMaoFile(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    size_t dotPos = lower.rfind('.');
    if (dotPos != std::string::npos) {
        std::string ext = lower.substr(dotPos);
        return ext == ".mao";
    }
    return false;
}
bool isPhyFile(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    size_t dotPos = lower.rfind('.');
    if (dotPos != std::string::npos) {
        std::string ext = lower.substr(dotPos);
        return ext == ".phy";
    }
    return false;
}
bool loadPHY(const std::vector<uint8_t>& data, Model& model) {
    GFFFile gff;
    if (!gff.load(data)) {
        return false;
    }
    auto quatMul = [](float q1x, float q1y, float q1z, float q1w,
                      float q2x, float q2y, float q2z, float q2w,
                      float& rx, float& ry, float& rz, float& rw) {
        rw = q1w*q2w - q1x*q2x - q1y*q2y - q1z*q2z;
        rx = q1w*q2x + q1x*q2w + q1y*q2z - q1z*q2y;
        ry = q1w*q2y - q1x*q2z + q1y*q2w + q1z*q2x;
        rz = q1w*q2z + q1x*q2y - q1y*q2x + q1z*q2w;
    };
    auto quatRotate = [](float qx, float qy, float qz, float qw,
                         float vx, float vy, float vz,
                         float& rx, float& ry, float& rz) {
        float cx = qy*vz - qz*vy;
        float cy = qz*vx - qx*vz;
        float cz = qx*vy - qy*vx;
        float cx2 = qy*cz - qz*cy;
        float cy2 = qz*cx - qx*cz;
        float cz2 = qx*cy - qy*cx;
        rx = vx + 2.0f*(qw*cx + cx2);
        ry = vy + 2.0f*(qw*cy + cy2);
        rz = vz + 2.0f*(qw*cz + cz2);
    };
    std::function<void(size_t, uint32_t, const std::string&)> processStruct =
        [&](size_t structIdx, uint32_t offset, const std::string& parentBoneName) {
        if (structIdx >= gff.structs().size()) return;
        const auto& st = gff.structs()[structIdx];
        std::string structType(st.structType);
        std::string currentBoneName = parentBoneName;
        if (structType == "node") {
            std::string name = gff.readStringByLabel(structIdx, 6000, offset);
            if (!name.empty()) {
                currentBoneName = name;
            }
        }
        if (structType == "shap") {
            CollisionShape shape;
            shape.name = gff.readStringByLabel(structIdx, 6241, offset);
            if (shape.name.empty()) {
                shape.name = "collision_" + std::to_string(model.collisionShapes.size());
            }
            float localPosX = 0, localPosY = 0, localPosZ = 0;
            const GFFField* posField = gff.findField(structIdx, 6061);
            if (posField) {
                uint32_t posOffset = gff.dataOffset() + posField->dataOffset + offset;
                localPosX = gff.readFloatAt(posOffset);
                localPosY = gff.readFloatAt(posOffset + 4);
                localPosZ = gff.readFloatAt(posOffset + 8);
            }
            float localRotX = 0, localRotY = 0, localRotZ = 0, localRotW = 1;
            const GFFField* rotField = gff.findField(structIdx, 6060);
            if (rotField) {
                uint32_t rotOffset = gff.dataOffset() + rotField->dataOffset + offset;
                localRotX = gff.readFloatAt(rotOffset);
                localRotY = gff.readFloatAt(rotOffset + 4);
                localRotZ = gff.readFloatAt(rotOffset + 8);
                localRotW = gff.readFloatAt(rotOffset + 12);
            }
            int boneIdx = model.skeleton.findBone(currentBoneName);
            if (boneIdx >= 0) {
                const Bone& bone = model.skeleton.bones[boneIdx];
                float rotatedPosX, rotatedPosY, rotatedPosZ;
                quatRotate(bone.worldRotX, bone.worldRotY, bone.worldRotZ, bone.worldRotW,
                           localPosX, localPosY, localPosZ,
                           rotatedPosX, rotatedPosY, rotatedPosZ);
                shape.posX = bone.worldPosX + rotatedPosX;
                shape.posY = bone.worldPosY + rotatedPosY;
                shape.posZ = bone.worldPosZ + rotatedPosZ;
                quatMul(bone.worldRotX, bone.worldRotY, bone.worldRotZ, bone.worldRotW,
                        localRotX, localRotY, localRotZ, localRotW,
                        shape.rotX, shape.rotY, shape.rotZ, shape.rotW);
            } else {
                shape.posX = localPosX;
                shape.posY = localPosY;
                shape.posZ = localPosZ;
                shape.rotX = localRotX;
                shape.rotY = localRotY;
                shape.rotZ = localRotZ;
                shape.rotW = localRotW;
                shape.meshVertsWorldSpace = true;
            }
            const GFFField* shapeTypeField = gff.findField(structIdx, 6998);
            GFFStructRef dataRef;
            bool hasShapeData = false;
            if (shapeTypeField) {
                bool isList = (shapeTypeField->flags & 0x8000) != 0;
                bool isStruct = (shapeTypeField->flags & 0x4000) != 0;
                bool isRef = (shapeTypeField->flags & 0x2000) != 0;
                uint32_t dataPos = gff.dataOffset() + shapeTypeField->dataOffset + offset;
                if (isRef && !isList && !isStruct) {
                    uint16_t refStructIdx = gff.readUInt16At(dataPos);
                    uint32_t refOffset = gff.readUInt32At(dataPos + 4);
                    if (refStructIdx < gff.structs().size()) {
                        dataRef.structIndex = refStructIdx;
                        dataRef.offset = refOffset;
                        hasShapeData = true;
                    }
                }
                else if (isStruct && !isList) {
                    int32_t ref = gff.readInt32At(dataPos);
                    if (ref >= 0) {
                        dataRef.structIndex = shapeTypeField->typeId;
                        dataRef.offset = ref;
                        hasShapeData = true;
                    }
                }
                else {
                    std::vector<GFFStructRef> shapeData = gff.readStructList(structIdx, 6998, offset);
                    if (!shapeData.empty()) {
                        dataRef = shapeData[0];
                        hasShapeData = true;
                    }
                }
            }
            bool shapeValid = false;
            if (hasShapeData && dataRef.structIndex < gff.structs().size()) {
                std::string dataType(gff.structs()[dataRef.structIndex].structType);
                if (dataType == "boxs") {
                    shape.type = CollisionShapeType::Box;
                    const GFFField* dimField = gff.findField(dataRef.structIndex, 6071);
                    if (dimField) {
                        uint32_t dimOffset = gff.dataOffset() + dimField->dataOffset + dataRef.offset;
                        shape.boxX = gff.readFloatAt(dimOffset);
                        shape.boxY = gff.readFloatAt(dimOffset + 4);
                        shape.boxZ = gff.readFloatAt(dimOffset + 8);
                        shapeValid = (shape.boxX != 0 || shape.boxY != 0 || shape.boxZ != 0);
                    }
                }
                else if (dataType == "sphs") {
                    shape.type = CollisionShapeType::Sphere;
                    const GFFField* radField = gff.findField(dataRef.structIndex, 6072);
                    if (radField) {
                        uint32_t radOffset = gff.dataOffset() + radField->dataOffset + dataRef.offset;
                        shape.radius = gff.readFloatAt(radOffset);
                    }
                    shapeValid = (shape.radius > 0.0f);
                }
                else if (dataType == "caps") {
                    shape.type = CollisionShapeType::Capsule;
                    const GFFField* radField = gff.findField(dataRef.structIndex, 6072);
                    const GFFField* htField = gff.findField(dataRef.structIndex, 6073);
                    if (radField) {
                        uint32_t radOffset = gff.dataOffset() + radField->dataOffset + dataRef.offset;
                        shape.radius = gff.readFloatAt(radOffset);
                    }
                    if (htField) {
                        uint32_t htOffset = gff.dataOffset() + htField->dataOffset + dataRef.offset;
                        shape.height = gff.readFloatAt(htOffset);
                    }
                    shapeValid = (shape.radius > 0.0f && shape.height > 0.0f);
                }
                else if (dataType == "mshs") {
                    shape.type = CollisionShapeType::Mesh;
                    const GFFField* meshDataField = gff.findField(dataRef.structIndex, 6077);
                    if (meshDataField) {
                        uint32_t meshDataPos = gff.dataOffset() + meshDataField->dataOffset + dataRef.offset;
                        int32_t listRef = gff.readInt32At(meshDataPos);
                        if (listRef >= 0) {
                            uint32_t nxsPos = gff.dataOffset() + listRef + 4;
                            const auto& rawData = gff.rawData();
                            if (nxsPos + 36 < rawData.size()) {
                                nxsPos += 28;
                                uint32_t vertCount = gff.readUInt32At(nxsPos);
                                nxsPos += 4;
                                uint32_t faceCount = gff.readUInt32At(nxsPos);
                                nxsPos += 4;
                                size_t vertsDataSize = vertCount * 3 * sizeof(float);
                                if (nxsPos + vertsDataSize <= rawData.size()) {
                                    shape.meshVerts.reserve(vertCount * 3);
                                    for (uint32_t v = 0; v < vertCount; v++) {
                                        shape.meshVerts.push_back(gff.readFloatAt(nxsPos));
                                        shape.meshVerts.push_back(gff.readFloatAt(nxsPos + 4));
                                        shape.meshVerts.push_back(gff.readFloatAt(nxsPos + 8));
                                        nxsPos += 12;
                                    }
                                    size_t facesDataSize = faceCount * 3;
                                    if (nxsPos + facesDataSize <= rawData.size()) {
                                        shape.meshIndices.reserve(faceCount * 3);
                                        for (uint32_t f = 0; f < faceCount; f++) {
                                            shape.meshIndices.push_back(gff.readUInt8At(nxsPos));
                                            shape.meshIndices.push_back(gff.readUInt8At(nxsPos + 1));
                                            shape.meshIndices.push_back(gff.readUInt8At(nxsPos + 2));
                                            nxsPos += 3;
                                        }
                                        shapeValid = !shape.meshVerts.empty();
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (shapeValid) {
                std::cout << "  Collision: " << shape.name << " type=";
                switch (shape.type) {
                    case CollisionShapeType::Box: std::cout << "Box"; break;
                    case CollisionShapeType::Sphere: std::cout << "Sphere"; break;
                    case CollisionShapeType::Capsule: std::cout << "Capsule"; break;
                    case CollisionShapeType::Mesh: std::cout << "Mesh(" << shape.meshVerts.size()/3 << " verts)"; break;
                }
                std::cout << " pos=(" << shape.posX << "," << shape.posY << "," << shape.posZ << ")";
                std::cout << " rot=(" << shape.rotX << "," << shape.rotY << "," << shape.rotZ << "," << shape.rotW << ")";
                std::cout << " bone=" << currentBoneName << std::endl;
                model.collisionShapes.push_back(shape);
            }
        }
        std::vector<GFFStructRef> children = gff.readStructList(structIdx, 6999, offset);
        for (const auto& child : children) {
            processStruct(child.structIndex, child.offset, currentBoneName);
        }
    };
    processStruct(0, 0, "");
    std::cout << "Loaded " << model.collisionShapes.size() << " collision shapes from PHY" << std::endl;
    return !model.collisionShapes.empty();
}
void quatMulWorld(float ax, float ay, float az, float aw,
                  float bx, float by, float bz, float bw,
                  float& rx, float& ry, float& rz, float& rw) {
    rw = aw*bw - ax*bx - ay*by - az*bz;
    rx = aw*bx + ax*bw + ay*bz - az*by;
    ry = aw*by - ax*bz + ay*bw + az*bx;
    rz = aw*bz + ax*by - ay*bx + az*bw;
}
void quatRotateWorld(float qx, float qy, float qz, float qw,
                     float px, float py, float pz,
                     float& rx, float& ry, float& rz) {
    float tx = 2.0f * (qy * pz - qz * py);
    float ty = 2.0f * (qz * px - qx * pz);
    float tz = 2.0f * (qx * py - qy * px);
    rx = px + qw * tx + (qy * tz - qz * ty);
    ry = py + qw * ty + (qz * tx - qx * tz);
    rz = pz + qw * tz + (qx * ty - qy * tx);
}
void loadMMH(const std::vector<uint8_t>& data, Model& model) {
    std::cout << "--- [DEBUG] Loading MMH ---" << std::endl;
    GFFFile gff;
    if (!gff.load(data)) {
        std::cout << "ERROR: Failed to parse GFF data for MMH." << std::endl;
        return;
    }
    std::cout << "MMH GFF loaded. Total Structs: " << gff.structs().size() << std::endl;
    auto normalizeQuat = [](float& x, float& y, float& z, float& w) {
        float len = std::sqrt(x*x + y*y + z*z + w*w);
        if (len > 0.00001f) {
            float invLen = 1.0f / len;
            x *= invLen; y *= invLen; z *= invLen; w *= invLen;
        } else {
            x = 0; y = 0; z = 0; w = 1;
        }
    };
    std::map<std::string, std::string> meshMaterials;
    std::vector<Bone> tempBones;
    std::function<void(size_t, uint32_t, const std::string&)> findNodes =
        [&](size_t structIdx, uint32_t offset, const std::string& parentName) {
        if (structIdx >= gff.structs().size()) return;
        const auto& s = gff.structs()[structIdx];
        std::string structType(s.structType);
        if (structType == "mshh") {
            std::string meshName = gff.readStringByLabel(structIdx, 6006, offset);
            std::string materialName = gff.readStringByLabel(structIdx, 6001, offset);
            std::cout << "  Found mshh struct: name='" << meshName << "' material='" << materialName << "'" << std::endl;
            if (!meshName.empty() && !materialName.empty()) {
                meshMaterials[meshName] = materialName;
            }
            std::vector<GFFStructRef> children = gff.readStructList(structIdx, 6999, offset);
            for (const auto& child : children) {
                const auto& cs = gff.structs()[child.structIndex];
                std::string childType(cs.structType);
                if (childType == "trsl") {
                    const GFFField* posField = gff.findField(child.structIndex, 6047);
                    if (posField) {
                        uint32_t posOffset = gff.dataOffset() + posField->dataOffset + child.offset;
                        float px = gff.readFloatAt(posOffset);
                        float py = gff.readFloatAt(posOffset + 4);
                        float pz = gff.readFloatAt(posOffset + 8);
                        std::cout << "    mshh trsl: pos=(" << px << "," << py << "," << pz << ")" << std::endl;
                    }
                }
                if (childType == "rota") {
                    const GFFField* rotField = gff.findField(child.structIndex, 6048);
                    if (rotField) {
                        uint32_t rotOffset = gff.dataOffset() + rotField->dataOffset + child.offset;
                        float rx = gff.readFloatAt(rotOffset);
                        float ry = gff.readFloatAt(rotOffset + 4);
                        float rz = gff.readFloatAt(rotOffset + 8);
                        float rw = gff.readFloatAt(rotOffset + 12);
                        std::cout << "    mshh rota: rot=(" << rx << "," << ry << "," << rz << "," << rw << ")" << std::endl;
                    }
                }
            }
        }
        if (structType == "node") {
            Bone bone;
            bone.name = gff.readStringByLabel(structIdx, 6000, offset);
            bone.parentName = parentName;
            std::vector<GFFStructRef> children = gff.readStructList(structIdx, 6999, offset);
            bool foundPos = false;
            bool foundRot = false;
            for (const auto& child : children) {
                const GFFField* posField = gff.findField(child.structIndex, 6047);
                if (posField) {
                     uint32_t posOffset = gff.dataOffset() + posField->dataOffset + child.offset;
                     bone.posX = gff.readFloatAt(posOffset);
                     bone.posY = gff.readFloatAt(posOffset + 4);
                     bone.posZ = gff.readFloatAt(posOffset + 8);
                     foundPos = true;
                }
                const GFFField* rotField = gff.findField(child.structIndex, 6048);
                if (rotField) {
                     uint32_t rotOffset = gff.dataOffset() + rotField->dataOffset + child.offset;
                     bone.rotX = gff.readFloatAt(rotOffset);
                     bone.rotY = gff.readFloatAt(rotOffset + 4);
                     bone.rotZ = gff.readFloatAt(rotOffset + 8);
                     bone.rotW = gff.readFloatAt(rotOffset + 12);
                     normalizeQuat(bone.rotX, bone.rotY, bone.rotZ, bone.rotW);
                     foundRot = true;
                }
            }
            if (foundPos || foundRot) {
            } else {
                std::cout << "  WARNING: No transform data found for bone '" << bone.name << "' in children." << std::endl;
            }
            if (!bone.name.empty()) {
                tempBones.push_back(bone);
            }
            for (const auto& child : children) {
                findNodes(child.structIndex, child.offset, bone.name);
            }
            return;
        }
        std::vector<GFFStructRef> children = gff.readStructList(structIdx, 6999, offset);
        for (const auto& child : children) {
            findNodes(child.structIndex, child.offset, parentName);
        }
    };
    findNodes(0, 0, "");
    std::cout << "Recursive search done. Found " << tempBones.size() << " bones." << std::endl;
    std::cout << "Found " << meshMaterials.size() << " mesh->material mappings:" << std::endl;
    for (const auto& [meshName, matName] : meshMaterials) {
        std::cout << "  '" << meshName << "' -> '" << matName << "'" << std::endl;
    }
    std::cout << "Applying materials to " << model.meshes.size() << " meshes:" << std::endl;
    for (auto& mesh : model.meshes) {
        std::cout << "  Mesh '" << mesh.name << "': ";
        auto it = meshMaterials.find(mesh.name);
        if (it != meshMaterials.end()) {
            mesh.materialName = it->second;
            std::cout << "matched -> '" << it->second << "'" << std::endl;
        } else {
            std::cout << "NO MATCH" << std::endl;
        }
    }
    model.skeleton.bones = tempBones;
    int links = 0;
    for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
        Bone& bone = model.skeleton.bones[i];
        if (!bone.parentName.empty()) {
            bone.parentIndex = model.skeleton.findBone(bone.parentName);
            if (bone.parentIndex >= 0) {
                links++;
            }
        }
    }
    std::cout << "Skeleton linking complete. Valid parent links: " << links << std::endl;
    for (size_t i = 0; i < model.skeleton.bones.size(); i++) {
        Bone& bone = model.skeleton.bones[i];
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
            quatRotateWorld(parent.worldRotX, parent.worldRotY, parent.worldRotZ, parent.worldRotW,
                           bone.posX, bone.posY, bone.posZ,
                           rotatedX, rotatedY, rotatedZ);
            bone.worldPosX = parent.worldPosX + rotatedX;
            bone.worldPosY = parent.worldPosY + rotatedY;
            bone.worldPosZ = parent.worldPosZ + rotatedZ;
            quatMulWorld(parent.worldRotX, parent.worldRotY, parent.worldRotZ, parent.worldRotW,
                        bone.rotX, bone.rotY, bone.rotZ, bone.rotW,
                        bone.worldRotX, bone.worldRotY, bone.worldRotZ, bone.worldRotW);
            normalizeQuat(bone.worldRotX, bone.worldRotY, bone.worldRotZ, bone.worldRotW);
        }
    }
    std::cout << "--- [DEBUG] MMH Load Finished ---" << std::endl;
}
bool loadModelFromEntry(AppState& state, const ERFEntry& entry) {
    if (!state.currentErf) return false;
    std::vector<uint8_t> data = state.currentErf->readEntry(entry);
    if (data.empty()) return false;
    Model model;
    if (!loadMSH(data, model)) {
        state.currentModel = Model();
        state.currentModel.name = entry.name + " (failed to parse)";
        Mesh cube;
        cube.name = "placeholder";
        float s = 1.0f;
        cube.vertices = {
            {-s, -s,  s,  0, 0, 1,  0, 0},
            { s, -s,  s,  0, 0, 1,  1, 0},
            { s,  s,  s,  0, 0, 1,  1, 1},
            {-s,  s,  s,  0, 0, 1,  0, 1},
            { s, -s, -s,  0, 0,-1,  0, 0},
            {-s, -s, -s,  0, 0,-1,  1, 0},
            {-s,  s, -s,  0, 0,-1,  1, 1},
            { s,  s, -s,  0, 0,-1,  0, 1},
            {-s,  s,  s,  0, 1, 0,  0, 0},
            { s,  s,  s,  0, 1, 0,  1, 0},
            { s,  s, -s,  0, 1, 0,  1, 1},
            {-s,  s, -s,  0, 1, 0,  0, 1},
            {-s, -s, -s,  0,-1, 0,  0, 0},
            { s, -s, -s,  0,-1, 0,  1, 0},
            { s, -s,  s,  0,-1, 0,  1, 1},
            {-s, -s,  s,  0,-1, 0,  0, 1},
            { s, -s,  s,  1, 0, 0,  0, 0},
            { s, -s, -s,  1, 0, 0,  1, 0},
            { s,  s, -s,  1, 0, 0,  1, 1},
            { s,  s,  s,  1, 0, 0,  0, 1},
            {-s, -s, -s, -1, 0, 0,  0, 0},
            {-s, -s,  s, -1, 0, 0,  1, 0},
            {-s,  s,  s, -1, 0, 0,  1, 1},
            {-s,  s, -s, -1, 0, 0,  0, 1},
        };
        cube.indices = {
            0,  1,  2,  2,  3,  0,
            4,  5,  6,  6,  7,  4,
            8,  9,  10, 10, 11, 8,
            12, 13, 14, 14, 15, 12,
            16, 17, 18, 18, 19, 16,
            20, 21, 22, 22, 23, 20
        };
        cube.calculateBounds();
        state.currentModel.meshes.push_back(cube);
        state.hasModel = true;
        auto center = cube.center();
        state.camera.lookAt(center[0], center[1], center[2], cube.radius() * 3.0f);
        return false;
    }
    for (const auto& mat : state.currentModel.materials) {
        if (mat.diffuseTexId != 0) {
            glDeleteTextures(1, &mat.diffuseTexId);
        }
        if (mat.normalTexId != 0) {
            glDeleteTextures(1, &mat.normalTexId);
        }
        if (mat.specularTexId != 0) {
            glDeleteTextures(1, &mat.specularTexId);
        }
    }
    state.currentModel = model;
    state.currentModel.name = entry.name;
    state.hasModel = true;
    state.renderSettings.initMeshVisibility(model.meshes.size());
    std::string baseName = entry.name;
    size_t dotPos = baseName.rfind('.');
    if (dotPos != std::string::npos) {
        baseName = baseName.substr(0, dotPos);
    }
    std::vector<std::string> mmhCandidates;
    mmhCandidates.push_back(baseName + ".mmh");
    size_t lastUnderscore = baseName.find_last_of('_');
    if (lastUnderscore != std::string::npos) {
        std::string variantA = baseName;
        variantA.insert(lastUnderscore, "a");
        mmhCandidates.push_back(variantA + ".mmh");
    }
    mmhCandidates.push_back(baseName + "a.mmh");
    bool foundMMH = false;
    for (const auto& erfPath : state.erfFiles) {
        if (state.currentErf && state.currentErf->filename() == erfPath) {
            for (const auto& e : state.currentErf->entries()) {
                std::string eName = e.name;
                std::transform(eName.begin(), eName.end(), eName.begin(), ::tolower);
                for(const auto& candidate : mmhCandidates) {
                    std::string candLower = candidate;
                    std::transform(candLower.begin(), candLower.end(), candLower.begin(), ::tolower);
                    if (eName == candLower) {
                        std::vector<uint8_t> mmhData = state.currentErf->readEntry(e);
                        if (!mmhData.empty()) {
                            loadMMH(mmhData, state.currentModel);
                            foundMMH = true;
                        }
                        break;
                    }
                }
                if (foundMMH) break;
            }
        } else {
            ERFFile searchErf;
            if (searchErf.open(erfPath)) {
                for (const auto& e : searchErf.entries()) {
                    std::string eName = e.name;
                    std::transform(eName.begin(), eName.end(), eName.begin(), ::tolower);
                    for(const auto& candidate : mmhCandidates) {
                        std::string candLower = candidate;
                        std::transform(candLower.begin(), candLower.end(), candLower.begin(), ::tolower);
                        if (eName == candLower) {
                            std::vector<uint8_t> mmhData = searchErf.readEntry(e);
                            if (!mmhData.empty()) {
                                loadMMH(mmhData, state.currentModel);
                                foundMMH = true;
                            }
                            break;
                        }
                    }
                    if (foundMMH) break;
                }
            }
        }
        if (foundMMH) break;
    }
    std::vector<std::string> phyCandidates;
    phyCandidates.push_back(baseName + ".phy");
    phyCandidates.push_back(baseName + "a.phy");
    if (lastUnderscore != std::string::npos) {
        std::string variantA = baseName;
        variantA.insert(lastUnderscore, "a");
        phyCandidates.push_back(variantA + ".phy");
    }
    bool foundPhy = false;
    for (const auto& erfPath : state.erfFiles) {
        ERFFile phyErf;
        if (phyErf.open(erfPath)) {
            for (const auto& e : phyErf.entries()) {
                std::string eName = e.name;
                std::transform(eName.begin(), eName.end(), eName.begin(), ::tolower);
                for (const auto& candidate : phyCandidates) {
                    std::string candLower = candidate;
                    std::transform(candLower.begin(), candLower.end(), candLower.begin(), ::tolower);
                    if (eName == candLower) {
                        std::vector<uint8_t> phyData = phyErf.readEntry(e);
                        if (!phyData.empty()) {
                            loadPHY(phyData, state.currentModel);
                        }
                        foundPhy = true;
                        break;
                    }
                }
                if (foundPhy) break;
            }
        }
        if (foundPhy) break;
    }
    std::set<std::string> materialNames;
    for (const auto& mesh : state.currentModel.meshes) {
        if (!mesh.materialName.empty()) {
            materialNames.insert(mesh.materialName);
        }
    }
    for (const std::string& matName : materialNames) {
        std::string maoName = matName + ".mao";
        std::string maoNameLower = maoName;
        std::transform(maoNameLower.begin(), maoNameLower.end(), maoNameLower.begin(), ::tolower);
        bool foundMao = false;
        for (const auto& erfPath : state.erfFiles) {
            ERFFile maoErf;
            if (maoErf.open(erfPath)) {
                for (const auto& e : maoErf.entries()) {
                    std::string eName = e.name;
                    std::transform(eName.begin(), eName.end(), eName.begin(), ::tolower);
                    if (eName == maoNameLower) {
                        std::vector<uint8_t> maoData = maoErf.readEntry(e);
                        if (!maoData.empty()) {
                            std::string maoContent(maoData.begin(), maoData.end());
                            Material mat = parseMAO(maoContent, matName);
                            mat.maoContent = maoContent;
                            state.currentModel.materials.push_back(mat);
                            foundMao = true;
                        }
                        break;
                    }
                }
            }
            if (foundMao) break;
        }
        if (!foundMao) {
            Material mat;
            mat.name = matName;
            state.currentModel.materials.push_back(mat);
        }
    }
    for (auto& mesh : state.currentModel.meshes) {
        if (!mesh.materialName.empty()) {
            mesh.materialIndex = state.currentModel.findMaterial(mesh.materialName);
        }
    }
    auto loadTextureFromERFs = [&state](const std::string& texName) -> GLuint {
        if (texName.empty()) return 0;
        std::string texNameLower = texName;
        std::transform(texNameLower.begin(), texNameLower.end(), texNameLower.begin(), ::tolower);
        for (const auto& erfPath : state.erfFiles) {
            ERFFile texErf;
            if (texErf.open(erfPath)) {
                for (const auto& e : texErf.entries()) {
                    std::string eName = e.name;
                    std::transform(eName.begin(), eName.end(), eName.begin(), ::tolower);
                    if (eName == texNameLower) {
                        std::vector<uint8_t> texData = texErf.readEntry(e);
                        if (!texData.empty()) {
                            return loadDDSTexture(texData);
                        }
                    }
                }
            }
        }
        return 0;
    };
    for (auto& mat : state.currentModel.materials) {
        if (!mat.diffuseMap.empty() && mat.diffuseTexId == 0) {
            mat.diffuseTexId = loadTextureFromERFs(mat.diffuseMap);
        }
        if (!mat.normalMap.empty() && mat.normalTexId == 0) {
            mat.normalTexId = loadTextureFromERFs(mat.normalMap);
        }
        if (!mat.specularMap.empty() && mat.specularTexId == 0) {
            mat.specularTexId = loadTextureFromERFs(mat.specularMap);
        }
        if (!mat.tintMap.empty() && mat.tintTexId == 0) {
            mat.tintTexId = loadTextureFromERFs(mat.tintMap);
        }
    }
    if (!model.meshes.empty()) {
        float minX = model.meshes[0].minX, maxX = model.meshes[0].maxX;
        float minY = model.meshes[0].minY, maxY = model.meshes[0].maxY;
        float minZ = model.meshes[0].minZ, maxZ = model.meshes[0].maxZ;
        for (const auto& mesh : model.meshes) {
            if (mesh.minX < minX) minX = mesh.minX;
            if (mesh.maxX > maxX) maxX = mesh.maxX;
            if (mesh.minY < minY) minY = mesh.minY;
            if (mesh.maxY > maxY) maxY = mesh.maxY;
            if (mesh.minZ < minZ) minZ = mesh.minZ;
            if (mesh.maxZ > maxZ) maxZ = mesh.maxZ;
        }
        float cx = (minX + maxX) / 2.0f;
        float cy = (minY + maxY) / 2.0f;
        float cz = (minZ + maxZ) / 2.0f;
        float dx = maxX - minX;
        float dy = maxY - minY;
        float dz = maxZ - minZ;
        float radius = std::sqrt(dx*dx + dy*dy + dz*dz) / 2.0f;
        state.camera.lookAt(cx, cy, cz, radius * 2.5f);
    }
    findAnimationsForModel(state, baseName);
    return true;
}

void drawSolidBox(float x, float y, float z) {
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glVertex3f(-x, -y, z); glVertex3f(x, -y, z);
    glVertex3f(x, y, z); glVertex3f(-x, y, z);
    glNormal3f(0, 0, -1);
    glVertex3f(x, -y, -z); glVertex3f(-x, -y, -z);
    glVertex3f(-x, y, -z); glVertex3f(x, y, -z);
    glNormal3f(0, 1, 0);
    glVertex3f(-x, y, -z); glVertex3f(-x, y, z);
    glVertex3f(x, y, z); glVertex3f(x, y, -z);
    glNormal3f(0, -1, 0);
    glVertex3f(-x, -y, -z); glVertex3f(x, -y, -z);
    glVertex3f(x, -y, z); glVertex3f(-x, -y, z);
    glNormal3f(1, 0, 0);
    glVertex3f(x, -y, -z); glVertex3f(x, y, -z);
    glVertex3f(x, y, z); glVertex3f(x, -y, z);
    glNormal3f(-1, 0, 0);
    glVertex3f(-x, -y, z); glVertex3f(-x, y, z);
    glVertex3f(-x, y, -z); glVertex3f(-x, -y, -z);
    glEnd();
}
void drawSolidSphere(float radius, int slices, int stacks) {
    for (int i = 0; i < stacks; i++) {
        float lat0 = 3.14159f * (-0.5f + float(i) / stacks);
        float z0 = radius * std::sin(lat0);
        float zr0 = radius * std::cos(lat0);
        float lat1 = 3.14159f * (-0.5f + float(i + 1) / stacks);
        float z1 = radius * std::sin(lat1);
        float zr1 = radius * std::cos(lat1);
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float lng = 2.0f * 3.14159f * float(j) / slices;
            float x = std::cos(lng);
            float y = std::sin(lng);
            glNormal3f(x * zr0 / radius, y * zr0 / radius, z0 / radius);
            glVertex3f(x * zr0, y * zr0, z0);
            glNormal3f(x * zr1 / radius, y * zr1 / radius, z1 / radius);
            glVertex3f(x * zr1, y * zr1, z1);
        }
        glEnd();
    }
}
void drawSolidCapsule(float radius, float height, int slices, int stacks) {
    float halfHeight = height / 2.0f;
    glBegin(GL_QUAD_STRIP);
    for (int j = 0; j <= slices; j++) {
        float lng = 2.0f * 3.14159f * float(j) / slices;
        float x = std::cos(lng);
        float y = std::sin(lng);
        glNormal3f(x, y, 0);
        glVertex3f(radius * x, radius * y, -halfHeight);
        glVertex3f(radius * x, radius * y, halfHeight);
    }
    glEnd();
    for (int i = 0; i < stacks / 2; i++) {
        float lat0 = 3.14159f * float(i) / stacks;
        float z0 = radius * std::sin(lat0);
        float zr0 = radius * std::cos(lat0);
        float lat1 = 3.14159f * float(i + 1) / stacks;
        float z1 = radius * std::sin(lat1);
        float zr1 = radius * std::cos(lat1);
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float lng = 2.0f * 3.14159f * float(j) / slices;
            float x = std::cos(lng);
            float y = std::sin(lng);
            glNormal3f(x * zr0 / radius, y * zr0 / radius, z0 / radius);
            glVertex3f(x * zr0, y * zr0, z0 + halfHeight);
            glNormal3f(x * zr1 / radius, y * zr1 / radius, z1 / radius);
            glVertex3f(x * zr1, y * zr1, z1 + halfHeight);
        }
        glEnd();
    }
    for (int i = 0; i < stacks / 2; i++) {
        float lat0 = -3.14159f * float(i) / stacks;
        float z0 = radius * std::sin(lat0);
        float zr0 = radius * std::cos(lat0);
        float lat1 = -3.14159f * float(i + 1) / stacks;
        float z1 = radius * std::sin(lat1);
        float zr1 = radius * std::cos(lat1);
        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; j++) {
            float lng = 2.0f * 3.14159f * float(j) / slices;
            float x = std::cos(lng);
            float y = std::sin(lng);
            glNormal3f(x * zr0 / radius, y * zr0 / radius, z0 / radius);
            glVertex3f(x * zr0, y * zr0, z0 - halfHeight);
            glNormal3f(x * zr1 / radius, y * zr1 / radius, z1 / radius);
            glVertex3f(x * zr1, y * zr1, z1 - halfHeight);
        }
        glEnd();
    }
}
void renderModel(const Model& model, const Camera& camera, const RenderSettings& settings, int width, int height) {
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    float fov = 45.0f * 3.14159f / 180.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    float top = nearPlane * std::tan(fov / 2.0f);
    float right = top * aspect;
    glFrustum(-right, right, -top, top, nearPlane, farPlane);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(-camera.pitch * 180.0f / 3.14159f, 1, 0, 0);
    glRotatef(-camera.yaw * 180.0f / 3.14159f, 0, 1, 0);
    glTranslatef(-camera.x, -camera.y, -camera.z);
    glRotatef(-90.0f, 1, 0, 0);
    glRotatef(180.0f, 0, 0, 1);
    if (settings.showGrid) {
        glLineWidth(1.0f);
        glBegin(GL_LINES);
        glColor3f(0.3f, 0.3f, 0.3f);
        float gridSize = 10.0f;
        float gridStep = 1.0f;
        for (float i = -gridSize; i <= gridSize; i += gridStep) {
            glVertex3f(-gridSize, i, 0);
            glVertex3f(gridSize, i, 0);
            glVertex3f(i, -gridSize, 0);
            glVertex3f(i, gridSize, 0);
        }
        glEnd();
    }
    if (settings.showAxes) {
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        glColor3f(1, 0, 0);
        glVertex3f(0, 0, 0); glVertex3f(2, 0, 0);
        glColor3f(0, 1, 0);
        glVertex3f(0, 0, 0); glVertex3f(0, 2, 0);
        glColor3f(0, 0, 1);
        glVertex3f(0, 0, 0); glVertex3f(0, 0, 2);
        glEnd();
        glLineWidth(1.0f);
    }
    if (!model.meshes.empty()) {
        if (settings.wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glDisable(GL_TEXTURE_2D);
            glColor3f(0.8f, 0.8f, 0.8f);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_LIGHTING);
            glEnable(GL_LIGHT0);
            glEnable(GL_COLOR_MATERIAL);
            float lightPos[] = {1.0f, 1.0f, 1.0f, 0.0f};
            float lightAmbient[] = {0.3f, 0.3f, 0.3f, 1.0f};
            float lightDiffuse[] = {0.7f, 0.7f, 0.7f, 1.0f};
            glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
            glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
            glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
            glColor3f(1.0f, 1.0f, 1.0f);
        }
        for (size_t meshIdx = 0; meshIdx < model.meshes.size(); meshIdx++) {
            if (meshIdx < settings.meshVisible.size() && settings.meshVisible[meshIdx] == 0) {
                continue;
            }
            const auto& mesh = model.meshes[meshIdx];
            uint32_t texId = 0;
            if (!settings.wireframe && settings.showTextures && mesh.materialIndex >= 0 &&
                mesh.materialIndex < (int)model.materials.size()) {
                texId = model.materials[mesh.materialIndex].diffuseTexId;
            }
            if (texId != 0) {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, texId);
            } else {
                glDisable(GL_TEXTURE_2D);
                if (!settings.wireframe) {
                    glColor3f(0.7f, 0.7f, 0.7f);
                }
            }
            glBegin(GL_TRIANGLES);
            for (size_t i = 0; i < mesh.indices.size(); i += 3) {
                for (int j = 0; j < 3; j++) {
                    const auto& v = mesh.vertices[mesh.indices[i + j]];
                    if (texId != 0) {
                        glTexCoord2f(v.u, 1.0f - v.v);
                    }
                    glNormal3f(v.nx, v.ny, v.nz);
                    glVertex3f(v.x, v.y, v.z);
                }
            }
            glEnd();
        }
        glDisable(GL_TEXTURE_2D);
        if (!settings.wireframe) {
            glDisable(GL_LIGHTING);
            glDisable(GL_LIGHT0);
            glDisable(GL_COLOR_MATERIAL);
        }
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    if (settings.showCollision && !model.collisionShapes.empty()) {
        bool wireframe = settings.collisionWireframe;
        if (wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glColor3f(0.0f, 1.0f, 1.0f);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(0.0f, 1.0f, 1.0f, 0.3f);
        }
        glLineWidth(2.0f);
        glDisable(GL_LIGHTING);
        for (const auto& shape : model.collisionShapes) {
            glPushMatrix();
            glTranslatef(shape.posX, shape.posY, shape.posZ);
            float rotW = shape.rotW;
            if (rotW > 1.0f) rotW = 1.0f;
            if (rotW < -1.0f) rotW = -1.0f;
            if (rotW < 0.9999f && rotW > -0.9999f) {
                float angle = 2.0f * std::acos(rotW) * 180.0f / 3.14159f;
                float s = std::sqrt(1.0f - rotW * rotW);
                if (s > 0.001f) {
                    glRotatef(angle, shape.rotX / s, shape.rotY / s, shape.rotZ / s);
                }
            }
            switch (shape.type) {
                case CollisionShapeType::Box: {
                    float x = shape.boxX, y = shape.boxY, z = shape.boxZ;
                    if (wireframe) {
                        glBegin(GL_LINE_LOOP);
                        glVertex3f(-x, -y, -z); glVertex3f(x, -y, -z);
                        glVertex3f(x, y, -z); glVertex3f(-x, y, -z);
                        glEnd();
                        glBegin(GL_LINE_LOOP);
                        glVertex3f(-x, -y, z); glVertex3f(x, -y, z);
                        glVertex3f(x, y, z); glVertex3f(-x, y, z);
                        glEnd();
                        glBegin(GL_LINES);
                        glVertex3f(-x, -y, -z); glVertex3f(-x, -y, z);
                        glVertex3f(x, -y, -z); glVertex3f(x, -y, z);
                        glVertex3f(x, y, -z); glVertex3f(x, y, z);
                        glVertex3f(-x, y, -z); glVertex3f(-x, y, z);
                        glEnd();
                    } else {
                        drawSolidBox(x, y, z);
                    }
                    break;
                }
                case CollisionShapeType::Sphere: {
                    float r = shape.radius;
                    if (wireframe) {
                        int segments = 24;
                        for (int plane = 0; plane < 3; plane++) {
                            glBegin(GL_LINE_LOOP);
                            for (int i = 0; i < segments; i++) {
                                float a = 2.0f * 3.14159f * float(i) / segments;
                                float c = r * std::cos(a);
                                float s = r * std::sin(a);
                                if (plane == 0) glVertex3f(c, s, 0);
                                else if (plane == 1) glVertex3f(c, 0, s);
                                else glVertex3f(0, c, s);
                            }
                            glEnd();
                        }
                    } else {
                        drawSolidSphere(r, 16, 12);
                    }
                    break;
                }
                case CollisionShapeType::Capsule: {
                    int segments = 24;
                    float r = shape.radius;
                    float h = shape.height / 2.0f;
                    if (wireframe) {
                        for (float zOff : {-h, h}) {
                            glBegin(GL_LINE_LOOP);
                            for (int i = 0; i < segments; i++) {
                                float a = 2.0f * 3.14159f * float(i) / segments;
                                glVertex3f(r * std::cos(a), r * std::sin(a), zOff);
                            }
                            glEnd();
                        }
                        glBegin(GL_LINES);
                        for (int i = 0; i < 4; i++) {
                            float a = 2.0f * 3.14159f * float(i) / 4;
                            glVertex3f(r * std::cos(a), r * std::sin(a), -h);
                            glVertex3f(r * std::cos(a), r * std::sin(a), h);
                        }
                        glEnd();
                        for (float zSign : {-1.0f, 1.0f}) {
                            for (int j = 1; j <= 4; j++) {
                                float lat = (3.14159f / 2.0f) * float(j) / 4;
                                float zOff = r * std::sin(lat) * zSign + h * zSign;
                                float rOff = r * std::cos(lat);
                                glBegin(GL_LINE_LOOP);
                                for (int i = 0; i < segments; i++) {
                                    float a = 2.0f * 3.14159f * float(i) / segments;
                                    glVertex3f(rOff * std::cos(a), rOff * std::sin(a), zOff);
                                }
                                glEnd();
                            }
                        }
                    } else {
                        drawSolidCapsule(r, shape.height, 16, 12);
                    }
                    break;
                }
                case CollisionShapeType::Mesh: {
                    if (!shape.meshVerts.empty() && !shape.meshIndices.empty()) {
                        if (wireframe) {
                            for (size_t i = 0; i + 2 < shape.meshIndices.size(); i += 3) {
                                uint32_t i0 = shape.meshIndices[i];
                                uint32_t i1 = shape.meshIndices[i + 1];
                                uint32_t i2 = shape.meshIndices[i + 2];
                                if (i0 * 3 + 2 < shape.meshVerts.size() &&
                                    i1 * 3 + 2 < shape.meshVerts.size() &&
                                    i2 * 3 + 2 < shape.meshVerts.size()) {
                                    glBegin(GL_LINE_LOOP);
                                    glVertex3f(shape.meshVerts[i0 * 3],
                                              shape.meshVerts[i0 * 3 + 1],
                                              shape.meshVerts[i0 * 3 + 2]);
                                    glVertex3f(shape.meshVerts[i1 * 3],
                                              shape.meshVerts[i1 * 3 + 1],
                                              shape.meshVerts[i1 * 3 + 2]);
                                    glVertex3f(shape.meshVerts[i2 * 3],
                                              shape.meshVerts[i2 * 3 + 1],
                                              shape.meshVerts[i2 * 3 + 2]);
                                    glEnd();
                                }
                            }
                        } else {
                            glBegin(GL_TRIANGLES);
                            for (size_t i = 0; i + 2 < shape.meshIndices.size(); i += 3) {
                                uint32_t i0 = shape.meshIndices[i];
                                uint32_t i1 = shape.meshIndices[i + 1];
                                uint32_t i2 = shape.meshIndices[i + 2];
                                if (i0 * 3 + 2 < shape.meshVerts.size() &&
                                    i1 * 3 + 2 < shape.meshVerts.size() &&
                                    i2 * 3 + 2 < shape.meshVerts.size()) {
                                    float v0x = shape.meshVerts[i0 * 3];
                                    float v0y = shape.meshVerts[i0 * 3 + 1];
                                    float v0z = shape.meshVerts[i0 * 3 + 2];
                                    float v1x = shape.meshVerts[i1 * 3];
                                    float v1y = shape.meshVerts[i1 * 3 + 1];
                                    float v1z = shape.meshVerts[i1 * 3 + 2];
                                    float v2x = shape.meshVerts[i2 * 3];
                                    float v2y = shape.meshVerts[i2 * 3 + 1];
                                    float v2z = shape.meshVerts[i2 * 3 + 2];
                                    float e1x = v1x - v0x, e1y = v1y - v0y, e1z = v1z - v0z;
                                    float e2x = v2x - v0x, e2y = v2y - v0y, e2z = v2z - v0z;
                                    float nx = e1y * e2z - e1z * e2y;
                                    float ny = e1z * e2x - e1x * e2z;
                                    float nz = e1x * e2y - e1y * e2x;
                                    float len = std::sqrt(nx*nx + ny*ny + nz*nz);
                                    if (len > 0.0001f) {
                                        nx /= len; ny /= len; nz /= len;
                                    }
                                    glNormal3f(nx, ny, nz);
                                    glVertex3f(v0x, v0y, v0z);
                                    glVertex3f(v1x, v1y, v1z);
                                    glVertex3f(v2x, v2y, v2z);
                                }
                            }
                            glEnd();
                        }
                    }
                    break;
                }
            }
            glPopMatrix();
        }
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glLineWidth(1.0f);
        glDisable(GL_BLEND);
    }
    if (settings.showSkeleton && !model.skeleton.bones.empty()) {
        glDisable(GL_LIGHTING);
        glDisable(GL_DEPTH_TEST);
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        for (const auto& bone : model.skeleton.bones) {
            if (bone.parentIndex >= 0) {
                const Bone& parent = model.skeleton.bones[bone.parentIndex];
                glColor3f(0.0f, 1.0f, 0.0f);
                glVertex3f(parent.worldPosX, parent.worldPosY, parent.worldPosZ);
                glColor3f(1.0f, 1.0f, 0.0f);
                glVertex3f(bone.worldPosX, bone.worldPosY, bone.worldPosZ);
            }
        }
        glEnd();
        glPointSize(6.0f);
        glBegin(GL_POINTS);
        for (const auto& bone : model.skeleton.bones) {
            if (bone.parentIndex < 0) {
                glColor3f(1.0f, 0.0f, 0.0f);
            } else {
                glColor3f(1.0f, 1.0f, 0.0f);
            }
            glVertex3f(bone.worldPosX, bone.worldPosY, bone.worldPosZ);
        }
        glEnd();
        glPointSize(1.0f);
        glLineWidth(1.0f);
        glEnable(GL_DEPTH_TEST);
    }
    glDisable(GL_DEPTH_TEST);
}
int main() {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Dragon Age Model Browser", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    loadGLExtensions();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 120");
    AppState state;
    state.extractPath = (fs::path(getExeDir()) / "extracted").string();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (!io.WantCaptureMouse) {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                ImGui::SetWindowFocus(nullptr);
                if (state.isPanning) {
                    float dx = static_cast<float>(mx - state.lastMouseX);
                    float dy = static_cast<float>(my - state.lastMouseY);
                    state.camera.rotate(-dx * state.camera.lookSensitivity, -dy * state.camera.lookSensitivity);
                }
                state.isPanning = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                float scroll = io.MouseWheel;
                if (scroll != 0.0f) {
                    state.camera.moveSpeed *= (scroll > 0) ? 1.2f : 0.8f;
                    if (state.camera.moveSpeed < 0.1f) state.camera.moveSpeed = 0.1f;
                    if (state.camera.moveSpeed > 100.0f) state.camera.moveSpeed = 100.0f;
                }
            } else {
                if (state.isPanning) {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                }
                state.isPanning = false;
            }
            state.lastMouseX = mx;
            state.lastMouseY = my;
        }
        if (!io.WantCaptureKeyboard) {
            float deltaTime = io.DeltaTime;
            float speed = state.camera.moveSpeed * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
                speed *= 3.0f;
            }
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                state.camera.moveForward(speed);
            }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                state.camera.moveForward(-speed);
            }
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                state.camera.moveRight(-speed);
            }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                state.camera.moveRight(speed);
            }
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
                state.camera.moveUp(speed);
            }
            if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
                state.camera.moveUp(-speed);
            }
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        if (state.showBrowser) {
            ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
            ImGui::Begin("ERF Browser", &state.showBrowser, ImGuiWindowFlags_MenuBar);
            if (ImGui::BeginMenuBar()) {
                if (ImGui::Button("Open Folder")) {
                    IGFD::FileDialogConfig config;
                    config.path = state.selectedFolder.empty() ? "." : state.selectedFolder;
                    ImGuiFileDialog::Instance()->OpenDialog("ChooseFolder", "Choose Folder", nullptr, config);
                }
                if (!state.statusMessage.empty()) {
                    ImGui::SameLine();
                    ImGui::Text("%s", state.statusMessage.c_str());
                }
                ImGui::EndMenuBar();
            }
            ImGui::Columns(2, "browser_columns");
            ImGui::Text("ERF Files (%zu)", state.erfFiles.size());
            ImGui::Separator();
            ImGui::BeginChild("ERFList", ImVec2(0, 0), true);
            for (int i = 0; i < static_cast<int>(state.erfFiles.size()); i++) {
                std::string displayName = fs::path(state.erfFiles[i]).filename().string();
                bool selected = (i == state.selectedErfIndex);
                if (ImGui::Selectable(displayName.c_str(), selected)) {
                    if (state.selectedErfIndex != i) {
                        state.selectedErfIndex = i;
                        state.selectedEntryIndex = -1;
                        state.currentErf = std::make_unique<ERFFile>();
                        if (!state.currentErf->open(state.erfFiles[i])) {
                            state.statusMessage = "Failed to open";
                            state.currentErf.reset();
                        } else {
                            state.statusMessage = versionToString(state.currentErf->version());
                        }
                    }
                }
            }
            ImGui::EndChild();
            ImGui::NextColumn();
            if (state.currentErf) {
                ImGui::Text("Contents (%zu)", state.currentErf->entries().size());
                if (state.currentErf->encryption() != 0) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "[Enc]");
                }
                if (state.currentErf->compression() != 0) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 1, 1), "[Comp]");
                }
                ImGui::Separator();
                ImGui::BeginChild("EntryList", ImVec2(0, 0), true);
                for (int i = 0; i < static_cast<int>(state.currentErf->entries().size()); i++) {
                    const auto& entry = state.currentErf->entries()[i];
                    bool selected = (i == state.selectedEntryIndex);
                    bool isModel = isModelFile(entry.name);
                    bool isMao = isMaoFile(entry.name);
                    bool isPhy = isPhyFile(entry.name);
                    if (isModel) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
                    } else if (isMao) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.4f, 1.0f));
                    } else if (isPhy) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 1.0f, 1.0f));
                    }
                    char label[256];
                    snprintf(label, sizeof(label), "%s##%d", entry.name.c_str(), i);
                    if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                        state.selectedEntryIndex = i;
                        if (ImGui::IsMouseDoubleClicked(0)) {
                            if (isModel) {
                                if (loadModelFromEntry(state, entry)) {
                                    state.statusMessage = "Loaded: " + entry.name + " (" +
                                        std::to_string(state.currentModel.meshes.size()) + " meshes)";
                                    state.showRenderSettings = true;
                                } else {
                                    state.statusMessage = "Failed to parse: " + entry.name;
                                    state.showRenderSettings = true;
                                }
                            } else if (isMao) {
                                std::vector<uint8_t> data = state.currentErf->readEntry(entry);
                                if (!data.empty()) {
                                    state.maoContent = std::string(data.begin(), data.end());
                                    state.maoFileName = entry.name;
                                    state.showMaoViewer = true;
                                    state.statusMessage = "Opened: " + entry.name;
                                }
                            }
                        }
                    }
                    if (isModel || isMao || isPhy) {
                        ImGui::PopStyleColor();
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("Size: %u bytes", entry.length);
                        if (entry.packed_length != entry.length) {
                            ImGui::Text("Packed: %u bytes", entry.packed_length);
                        }
                        if (isModel) {
                            ImGui::Text("Double-click to load model");
                        } else if (isMao) {
                            ImGui::Text("Double-click to view material");
                        } else if (isPhy) {
                            ImGui::Text("Collision data (auto-loaded with model)");
                        }
                        ImGui::EndTooltip();
                    }
                }
                ImGui::EndChild();
            } else {
                ImGui::Text("Select an ERF file");
            }
            ImGui::Columns(1);
            ImGui::End();
        }
        if (ImGuiFileDialog::Instance()->Display("ChooseFolder", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                state.selectedFolder = ImGuiFileDialog::Instance()->GetCurrentPath();
                state.erfFiles = scanForERFFiles(state.selectedFolder);
                state.selectedErfIndex = -1;
                state.currentErf.reset();
                state.selectedEntryIndex = -1;
                state.statusMessage = "Found " + std::to_string(state.erfFiles.size()) + " ERF files";
            }
            ImGuiFileDialog::Instance()->Close();
        }
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Browser", nullptr, &state.showBrowser);
                ImGui::MenuItem("Render Settings", nullptr, &state.showRenderSettings);
                ImGui::MenuItem("Animation", nullptr, &state.showAnimWindow);
                ImGui::EndMenu();
            }
            if (state.hasModel) {
                ImGui::SameLine(ImGui::GetWindowWidth() - 500);
                ImGui::Text("Model: %s | RMB+Mouse: Look | WASD: Move | Space/Ctrl: Up/Down | Shift: Fast", state.currentModel.name.c_str());
            }
            ImGui::EndMainMenuBar();
        }
        if (state.showRenderSettings) {
            ImGui::SetNextWindowSizeConstraints(ImVec2(300, 100), ImVec2(500, 800));
            ImGui::Begin("Render Settings", &state.showRenderSettings, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Checkbox("Wireframe", &state.renderSettings.wireframe);
            ImGui::Checkbox("Show Axes", &state.renderSettings.showAxes);
            ImGui::Checkbox("Show Grid", &state.renderSettings.showGrid);
            ImGui::Checkbox("Show Collision", &state.renderSettings.showCollision);
            if (state.renderSettings.showCollision) {
                ImGui::SameLine();
                ImGui::Checkbox("Wireframe##coll", &state.renderSettings.collisionWireframe);
            }
            ImGui::Checkbox("Show Skeleton", &state.renderSettings.showSkeleton);
            if (state.renderSettings.showSkeleton && !state.currentModel.skeleton.bones.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%zu bones)", state.currentModel.skeleton.bones.size());
            }
            ImGui::Checkbox("Show Textures", &state.renderSettings.showTextures);
            if (!state.currentModel.materials.empty()) {
                int diffCount = 0, normCount = 0, specCount = 0, tintCount = 0;
                for (const auto& mat : state.currentModel.materials) {
                    if (mat.diffuseTexId != 0) diffCount++;
                    if (mat.normalTexId != 0) normCount++;
                    if (mat.specularTexId != 0) specCount++;
                    if (mat.tintTexId != 0) tintCount++;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(D:%d N:%d S:%d T:%d)", diffCount, normCount, specCount, tintCount);
            }
            ImGui::Separator();
            ImGui::Text("Camera Speed: %.1f", state.camera.moveSpeed);
            ImGui::SliderFloat("##speed", &state.camera.moveSpeed, 0.1f, 100.0f, "%.1f");
            ImGui::TextDisabled("(RMB + Scroll to adjust)");
            if (state.hasModel) {
                ImGui::Separator();
                size_t totalVerts = 0, totalTris = 0;
                for (const auto& m : state.currentModel.meshes) {
                    totalVerts += m.vertices.size();
                    totalTris += m.indices.size() / 3;
                }
                ImGui::Text("Total: %zu meshes, %zu verts, %zu tris",
                    state.currentModel.meshes.size(), totalVerts, totalTris);
                if (state.currentModel.meshes.size() >= 1) {
                    ImGui::Separator();
                    ImGui::Text("Meshes:");
                    if (state.renderSettings.meshVisible.size() != state.currentModel.meshes.size()) {
                        state.renderSettings.initMeshVisibility(state.currentModel.meshes.size());
                    }
                    float listHeight = std::min(300.0f, state.currentModel.meshes.size() * 70.0f + 20.0f);
                    ImGui::BeginChild("MeshList", ImVec2(0, listHeight), true);
                    for (size_t i = 0; i < state.currentModel.meshes.size(); i++) {
                        const auto& mesh = state.currentModel.meshes[i];
                        ImGui::PushID(static_cast<int>(i));
                        bool visible = state.renderSettings.meshVisible[i] != 0;
                        if (ImGui::Checkbox("##vis", &visible)) {
                            state.renderSettings.meshVisible[i] = visible ? 1 : 0;
                        }
                        ImGui::SameLine();
                        std::string meshLabel = mesh.name.empty() ?
                            ("Mesh " + std::to_string(i)) : mesh.name;
                        ImGui::Text("%s", meshLabel.c_str());
                        ImGui::Indent();
                        ImGui::TextDisabled("%zu verts, %zu tris",
                            mesh.vertices.size(), mesh.indices.size() / 3);
                        if (!mesh.materialName.empty()) {
                            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f),
                                "Material: %s.mao", mesh.materialName.c_str());
                        } else {
                            ImGui::TextDisabled("Material: (none)");
                        }
                        if (ImGui::SmallButton("View UVs")) {
                            state.selectedMeshForUv = static_cast<int>(i);
                            state.showUvViewer = true;
                        }
                        if (!mesh.materialName.empty()) {
                            ImGui::SameLine();
                            if (ImGui::SmallButton("View MAO")) {
                                if (mesh.materialIndex >= 0 && mesh.materialIndex < (int)state.currentModel.materials.size()) {
                                    const Material& mat = state.currentModel.materials[mesh.materialIndex];
                                    if (!mat.maoContent.empty()) {
                                        state.maoContent = mat.maoContent;
                                        state.maoFileName = mesh.materialName + ".mao";
                                        state.showMaoViewer = true;
                                    } else {
                                        state.maoContent = "(MAO file not found)";
                                        state.maoFileName = mesh.materialName + ".mao";
                                        state.showMaoViewer = true;
                                    }
                                }
                            }
                        }
                        if (mesh.materialIndex >= 0 && mesh.materialIndex < (int)state.currentModel.materials.size()) {
                            const Material& mat = state.currentModel.materials[mesh.materialIndex];
                            if (mat.diffuseTexId != 0 || mat.normalTexId != 0 || mat.specularTexId != 0 || mat.tintTexId != 0) {
                                ImGui::Text("Textures:");
                                if (mat.diffuseTexId != 0) {
                                    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "  [D] %s", mat.diffuseMap.c_str());
                                }
                                if (mat.normalTexId != 0) {
                                    ImGui::TextColored(ImVec4(0.5f, 0.7f, 1.0f, 1.0f), "  [N] %s", mat.normalMap.c_str());
                                }
                                if (mat.specularTexId != 0) {
                                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "  [S] %s", mat.specularMap.c_str());
                                }
                                if (mat.tintTexId != 0) {
                                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 1.0f, 1.0f), "  [T] %s", mat.tintMap.c_str());
                                }
                            }
                        }
                        ImGui::Unindent();
                        ImGui::PopID();
                        if (i < state.currentModel.meshes.size() - 1) {
                            ImGui::Spacing();
                        }
                    }
                    ImGui::EndChild();
                    if (state.currentModel.meshes.size() > 1) {
                        if (ImGui::Button("Show All")) {
                            for (size_t i = 0; i < state.renderSettings.meshVisible.size(); i++) {
                                state.renderSettings.meshVisible[i] = 1;
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Hide All")) {
                            for (size_t i = 0; i < state.renderSettings.meshVisible.size(); i++) {
                                state.renderSettings.meshVisible[i] = 0;
                            }
                        }
                    }
                }
                if (!state.currentModel.collisionShapes.empty()) {
                    ImGui::Separator();
                    if (ImGui::TreeNode("CollisionShapes", "Collision Shapes (%zu)", state.currentModel.collisionShapes.size())) {
                        for (const auto& shape : state.currentModel.collisionShapes) {
                            const char* typeStr = "Unknown";
                            switch (shape.type) {
                                case CollisionShapeType::Box: typeStr = "Box"; break;
                                case CollisionShapeType::Sphere: typeStr = "Sphere"; break;
                                case CollisionShapeType::Capsule: typeStr = "Capsule"; break;
                                case CollisionShapeType::Mesh: typeStr = "Mesh"; break;
                            }
                            ImGui::BulletText("%s: %s", shape.name.c_str(), typeStr);
                        }
                        ImGui::TreePop();
                    }
                }
                if (!state.currentModel.skeleton.bones.empty()) {
                    ImGui::Separator();
                    if (ImGui::TreeNode("Skeleton", "Skeleton (%zu bones)", state.currentModel.skeleton.bones.size())) {
                        float boneListHeight = std::min(300.0f, state.currentModel.skeleton.bones.size() * 20.0f + 20.0f);
                        ImGui::BeginChild("BoneList", ImVec2(0, boneListHeight), true);
                        for (size_t i = 0; i < state.currentModel.skeleton.bones.size(); i++) {
                            const auto& bone = state.currentModel.skeleton.bones[i];
                            if (bone.parentIndex < 0) {
                                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "%s (root)", bone.name.c_str());
                            } else {
                                ImGui::Text("%s", bone.name.c_str());
                                ImGui::SameLine();
                                ImGui::TextDisabled("-> %s", bone.parentName.c_str());
                            }
                        }
                        ImGui::EndChild();
                        ImGui::TreePop();
                    }
                }
            }
            ImGui::End();
        }
        if (state.showMaoViewer) {
            ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
            ImGui::Begin(("MAO Viewer - " + state.maoFileName).c_str(), &state.showMaoViewer);
            if (ImGui::Button("Copy to Clipboard")) {
                ImGui::SetClipboardText(state.maoContent.c_str());
            }
            ImGui::Separator();
            ImGui::BeginChild("MaoContent", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted(state.maoContent.c_str());
            ImGui::EndChild();
            ImGui::End();
        }
        if (state.showUvViewer && state.hasModel && state.selectedMeshForUv >= 0 &&
            state.selectedMeshForUv < static_cast<int>(state.currentModel.meshes.size())) {
            const auto& mesh = state.currentModel.meshes[state.selectedMeshForUv];
            std::string title = "UV Viewer - " + (mesh.name.empty() ? "Mesh " + std::to_string(state.selectedMeshForUv) : mesh.name);
            ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
            ImGui::Begin(title.c_str(), &state.showUvViewer);
            ImVec2 canvasSize = ImGui::GetContentRegionAvail();
            float size = std::min(canvasSize.x, canvasSize.y - 20);
            if (size < 100) size = 100;
            ImVec2 canvasPos = ImGui::GetCursorScreenPos();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + size, canvasPos.y + size),
                IM_COL32(40, 40, 40, 255));
            int gridLines = 8;
            for (int i = 0; i <= gridLines; i++) {
                float t = float(i) / gridLines;
                ImU32 col = (i == 0 || i == gridLines) ? IM_COL32(100, 100, 100, 255) : IM_COL32(60, 60, 60, 255);
                drawList->AddLine(
                    ImVec2(canvasPos.x + t * size, canvasPos.y),
                    ImVec2(canvasPos.x + t * size, canvasPos.y + size), col);
                drawList->AddLine(
                    ImVec2(canvasPos.x, canvasPos.y + t * size),
                    ImVec2(canvasPos.x + size, canvasPos.y + t * size), col);
            }
            for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
                const auto& v0 = mesh.vertices[mesh.indices[i]];
                const auto& v1 = mesh.vertices[mesh.indices[i + 1]];
                const auto& v2 = mesh.vertices[mesh.indices[i + 2]];
                ImVec2 p0(canvasPos.x + v0.u * size, canvasPos.y + (1.0f - v0.v) * size);
                ImVec2 p1(canvasPos.x + v1.u * size, canvasPos.y + (1.0f - v1.v) * size);
                ImVec2 p2(canvasPos.x + v2.u * size, canvasPos.y + (1.0f - v2.v) * size);
                drawList->AddTriangle(p0, p1, p2, IM_COL32(0, 200, 255, 200), 1.0f);
            }
            ImGui::Dummy(ImVec2(size, size));
            ImGui::Text("Triangles: %zu", mesh.indices.size() / 3);
            ImGui::End();
        }
        if (state.showAnimWindow) {
            ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);
            ImGui::Begin("Animation", &state.showAnimWindow);
            if (!state.hasModel) {
                ImGui::TextDisabled("Load a model to see animations");
            } else {
                ImGui::Text("Model: %s", state.currentModel.name.c_str());
                ImGui::Text("Available Animations: %zu", state.availableAnimFiles.size());
                ImGui::Separator();
                if (state.availableAnimFiles.empty()) {
                    ImGui::TextDisabled("No animations found for this model");
                } else {
                    ImGui::Text("Animation Files:");
                    float listHeight = std::min(200.0f, state.availableAnimFiles.size() * 20.0f + 10.0f);
                    ImGui::BeginChild("AnimList", ImVec2(0, listHeight), true);
                    for (size_t i = 0; i < state.availableAnimFiles.size(); i++) {
                        bool isSelected = (state.selectedAnimIndex == (int)i);
                        if (ImGui::Selectable(state.availableAnimFiles[i].c_str(), isSelected)) {
                            state.selectedAnimIndex = (int)i;
                            state.animPlaying = false;
                            state.animTime = 0.0f;
                            for (const auto& erfPath : state.erfFiles) {
                                ERFFile erf;
                                if (erf.open(erfPath)) {
                                    for (const auto& entry : erf.entries()) {
                                        std::string entryLower = entry.name;
                                        std::string targetLower = state.availableAnimFiles[i];
                                        std::transform(entryLower.begin(), entryLower.end(), entryLower.begin(), ::tolower);
                                        std::transform(targetLower.begin(), targetLower.end(), targetLower.begin(), ::tolower);
                                        if (entryLower == targetLower) {
                                            std::vector<uint8_t> aniData = erf.readEntry(entry);
                                            if (!aniData.empty()) {
                                                Animation anim = loadANI(aniData, entry.name);
                                                if (!anim.tracks.empty()) {
                                                    state.animations.clear();
                                                    state.animations.push_back(anim);
                                                    for (auto& track : state.animations[0].tracks) {
                                                        track.boneIndex = state.currentModel.skeleton.findBone(track.boneName);
                                                    }
                                                }
                                            }
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    ImGui::EndChild();
                    ImGui::Separator();
                    if (state.selectedAnimIndex >= 0 && !state.animations.empty()) {
                        const Animation& anim = state.animations[0];
                        ImGui::Text("Name: %s", anim.name.c_str());
                        ImGui::Text("Duration: %.2fs", anim.duration);
                        ImGui::Text("Tracks: %zu", anim.tracks.size());
                        ImGui::Separator();
                        if (ImGui::Button(state.animPlaying ? "Pause" : "Play")) {
                            state.animPlaying = !state.animPlaying;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Stop")) {
                            state.animPlaying = false;
                            state.animTime = 0.0f;
                        }
                        ImGui::SliderFloat("Time", &state.animTime, 0.0f, anim.duration, "%.2fs");
                        ImGui::SliderFloat("Speed", &state.animSpeed, 0.1f, 2.0f, "%.1fx");
                        if (state.animPlaying) {
                            state.animTime += io.DeltaTime * state.animSpeed;
                            if (state.animTime > anim.duration) {
                                state.animTime = 0.0f;
                            }
                        }
                        if (ImGui::TreeNode("Tracks")) {
                            for (const auto& track : anim.tracks) {
                                const char* typeStr = track.isRotation ? "Rot" : (track.isTranslation ? "Pos" : "?");
                                ImVec4 col = (track.boneIndex >= 0) ? ImVec4(0.5f, 1.0f, 0.5f, 1.0f) : ImVec4(1.0f, 0.5f, 0.5f, 1.0f);
                                ImGui::TextColored(col, "[%s] %s (%zu keys)", typeStr, track.boneName.c_str(), track.keyframes.size());
                            }
                            ImGui::TreePop();
                        }
                    }
                }
            }
            ImGui::End();
        }
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (state.hasModel) {
            renderModel(state.currentModel, state.camera, state.renderSettings, display_w, display_h);
        } else {
            Model empty;
            renderModel(empty, state.camera, state.renderSettings, display_w, display_h);
        }
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}