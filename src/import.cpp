#include "import.h"
#include "ui_internal.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <cfloat>
#include <set>
#include <functional>
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"
#include "msh_embedded.h"
#include "mmh_embedded.h"
#include "ani_embedded.h"
#ifdef _WIN32
#include <windows.h>
#endif
static std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static std::string CleanName(const std::string& input) {
    std::string result = input;
    size_t lastSlash = result.find_last_of("/\\");
    if (lastSlash != std::string::npos) result = result.substr(lastSlash + 1);
    size_t lastDot = result.find_last_of('.');
    if (lastDot != std::string::npos) result = result.substr(0, lastDot);
    return ToLower(result);
}

DAOGraphicsTools::DAOGraphicsTools() {}
DAOGraphicsTools::~DAOGraphicsTools() { Cleanup(); }
bool DAOGraphicsTools::Initialize() {
    if (m_initialized) return true;
    m_workDir = fs::temp_directory_path() / "haven_tools";
    m_mshDir = m_workDir / "msh";
    m_mmhDir = m_workDir / "mmh";
    m_aniDir = m_workDir / "ani";
    fs::create_directories(m_mshDir);
    fs::create_directories(m_mmhDir);
    fs::create_directories(m_aniDir);
    if (!ExtractTools()) {
        return false;
    }
    m_initialized = true;
    return true;
}

bool DAOGraphicsTools::ExtractTools() {
#ifdef _WIN32
    auto writeFile = [](const fs::path& path, const unsigned char* data, unsigned int len) {
        std::ofstream out(path, std::ios::binary);
        if (!out) return false;
        out.write(reinterpret_cast<const char*>(data), len);
        return out.good();
    };
    bool ok = true;
    ok &= writeFile(m_mshDir / "GraphicsProcessorMSH.exe", msh_GraphicsProcessorMSH_exe, msh_GraphicsProcessorMSH_exe_len);
    ok &= writeFile(m_mshDir / "IlmImf.dll", msh_IlmImf_dll, msh_IlmImf_dll_len);
    ok &= writeFile(m_mshDir / "NxCharacter.dll", msh_NxCharacter_dll, msh_NxCharacter_dll_len);
    ok &= writeFile(m_mshDir / "NxCooking.dll", msh_NxCooking_dll, msh_NxCooking_dll_len);
    ok &= writeFile(m_mshDir / "SpeedTreeRT.dll", msh_SpeedTreeRT_dll, msh_SpeedTreeRT_dll_len);
    ok &= writeFile(m_mmhDir / "GraphicsProcessorMMH.exe", mmh_GraphicsProcessorMMH_exe, mmh_GraphicsProcessorMMH_exe_len);
    ok &= writeFile(m_mmhDir / "NxCharacter.dll", mmh_NxCharacter_dll, mmh_NxCharacter_dll_len);
    ok &= writeFile(m_mmhDir / "nxcooking.dll", mmh_nxcooking_dll, mmh_nxcooking_dll_len);
    ok &= writeFile(m_mmhDir / "SpeedTreeRT.dll", mmh_SpeedTreeRT_dll, mmh_SpeedTreeRT_dll_len);
    ok &= writeFile(m_mmhDir / "umbra.dll", mmh_umbra_dll, mmh_umbra_dll_len);
    return ok;
#else
    return true;
#endif
}

bool DAOGraphicsTools::RunProcessor(const fs::path& exePath, const fs::path& xmlPath) {
    std::string cmdLine = "\"" + exePath.string() + "\" \"" + xmlPath.string() + "\"";
    return RunProcessorWithCmd(exePath, cmdLine);
}

bool DAOGraphicsTools::RunProcessorWithCmd(const fs::path& exePath, const std::string& cmdLine) {
    if (!fs::exists(exePath)) {
        return false;
    }
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hStdOutRead, hStdOutWrite, hStdErrRead, hStdErrWrite;
    CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0);
    CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0);
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdErrWrite;
    si.hStdInput = NULL;
    PROCESS_INFORMATION pi = {};
    std::string cmd = cmdLine;
    fs::path workDir = exePath.parent_path();
    if (CreateProcessA(NULL, &cmd[0], NULL, NULL, TRUE, 0, NULL, workDir.string().c_str(), &si, &pi)) {
        CloseHandle(hStdOutWrite);
        CloseHandle(hStdErrWrite);
        WaitForSingleObject(pi.hProcess, 30000);
        char buffer[4096];
        DWORD bytesRead;
        std::string stdoutStr, stderrStr;
        while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = 0;
            stdoutStr += buffer;
        }
        while (ReadFile(hStdErrRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = 0;
            stderrStr += buffer;
        }
        CloseHandle(hStdOutRead);
        CloseHandle(hStdErrRead);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exitCode == 0;
    }
    DWORD err = GetLastError();
    CloseHandle(hStdOutRead);
    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrRead);
    CloseHandle(hStdErrWrite);
    return false;
#else
    return false;
#endif
}

std::vector<uint8_t> DAOGraphicsTools::ReadBinaryFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};
    size_t size = file.tellg();
    file.seekg(0);
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

std::vector<uint8_t> DAOGraphicsTools::ProcessMSH(const fs::path& xmlPath) {
    fs::path exePath = m_mshDir / "GraphicsProcessorMSH.exe";
    fs::path localXml = m_mshDir / xmlPath.filename();
    fs::copy_file(xmlPath, localXml, fs::copy_options::overwrite_existing);
    std::string outName = xmlPath.stem().string();
    if (outName.size() > 4 && outName.substr(outName.size() - 4) == ".msh") {
        outName = outName.substr(0, outName.size() - 4);
    }
    fs::path outPath = m_mshDir / (outName + ".msh");
    fs::remove(outPath);
    std::string cmdLine = "\"" + exePath.string() + "\" -platform pc mmdtogff \"" + localXml.string() + "\"";
    if (!RunProcessorWithCmd(exePath, cmdLine)) return {};
    if (!fs::exists(outPath)) {
        return {};
    }
    auto result = ReadBinaryFile(outPath);
    return result;
}

std::vector<uint8_t> DAOGraphicsTools::ProcessMMH(const fs::path& xmlPath) {
    m_lastPHY.clear();
    fs::path exePath = m_mmhDir / "GraphicsProcessorMMH.exe";
    fs::path localXml = m_mmhDir / xmlPath.filename();
    fs::copy_file(xmlPath, localXml, fs::copy_options::overwrite_existing);
    std::string outName = xmlPath.stem().string();
    if (outName.size() > 4 && outName.substr(outName.size() - 4) == ".mmh") {
        outName = outName.substr(0, outName.size() - 4);
    }
    fs::path outPath = m_mmhDir / (outName + ".mmh");
    fs::path phyPath = m_mmhDir / (outName + ".phy");
    fs::remove(outPath);
    fs::remove(phyPath);
    if (!RunProcessor(exePath, localXml)) return {};
    if (!fs::exists(outPath)) {
        return {};
    }
    if (fs::exists(phyPath)) {
        m_lastPHY = ReadBinaryFile(phyPath);
    }
    auto result = ReadBinaryFile(outPath);
    return result;
}

void DAOGraphicsTools::Cleanup() {
    if (!m_workDir.empty() && fs::exists(m_workDir)) {
        std::error_code ec;
        fs::remove_all(m_workDir, ec);
    }
}

DAOImporter::DAOImporter() : m_backupCallback(nullptr), m_progressCallback(nullptr) {}
DAOImporter::~DAOImporter() {}
bool DAOImporter::BackupExists(const std::string& erfPath) {
    fs::path backupDir = fs::current_path() / "backups";
    std::string erfName = fs::path(erfPath).filename().string();
    return fs::exists(backupDir / (erfName + ".bak"));
}

std::string DAOImporter::GetBackupDir() {
    return (fs::current_path() / "backups").string();
}

void DAOImporter::ReportProgress(float progress, const std::string& status) {
    if (m_progressCallback) m_progressCallback(progress, status);
}

static std::string FindErfPath(const fs::path& root, const std::string& filename) {
    if (fs::exists(root / filename)) return (root / filename).string();
    try {
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (entry.is_regular_file() && ToLower(entry.path().filename().string()) == ToLower(filename)) {
                return entry.path().string();
            }
        }
    } catch (...) {}
    return "";
}

static std::vector<uint8_t> ConvertToDDS(const std::vector<uint8_t>& imageData, int width, int height, int channels) {
    std::vector<uint8_t> dds;
    auto writeU32 = [&](uint32_t v) {
        dds.push_back(v & 0xFF);
        dds.push_back((v >> 8) & 0xFF);
        dds.push_back((v >> 16) & 0xFF);
        dds.push_back((v >> 24) & 0xFF);
    };
    dds.push_back('D'); dds.push_back('D'); dds.push_back('S'); dds.push_back(' ');
    writeU32(124);
    writeU32(0x1 | 0x2 | 0x4 | 0x1000);
    writeU32(height);
    writeU32(width);
    writeU32(width * 4);
    writeU32(0);
    writeU32(1);
    for (int i = 0; i < 11; i++) writeU32(0);
    writeU32(32);
    writeU32(0x41);
    writeU32(0);
    writeU32(32);
    writeU32(0x00FF0000);
    writeU32(0x0000FF00);
    writeU32(0x000000FF);
    writeU32(0xFF000000);
    writeU32(0x1000);
    for (int i = 0; i < 4; i++) writeU32(0);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int srcIdx = (y * width + x) * channels;
            uint8_t r = imageData[srcIdx];
            uint8_t g = (channels > 1) ? imageData[srcIdx + 1] : r;
            uint8_t b = (channels > 2) ? imageData[srcIdx + 2] : r;
            uint8_t a = (channels > 3) ? imageData[srcIdx + 3] : 255;
            dds.push_back(b);
            dds.push_back(g);
            dds.push_back(r);
            dds.push_back(a);
        }
    }
    return dds;
}

bool DAOImporter::ImportToDirectory(const std::string& glbPath, const std::string& targetDir) {
    ReportProgress(0.0f, "Initializing tools...");
    if (!m_tools.Initialize()) {
        return false;
    }
    ReportProgress(0.05f, "Loading GLB...");
    DAOModelData modelData;
    if (!LoadGLB(glbPath, modelData)) return false;
    ReportProgress(0.1f, "Locating ERF files...");
    const fs::path baseDir(targetDir);
    fs::path corePath = baseDir / "packages" / "core" / "data";
    fs::path texturePath = baseDir / "packages" / "core" / "textures" / "high";
    auto resolvePath = [&](const std::string& filename) -> std::string {
        if (fs::exists(corePath / filename)) return (corePath / filename).string();
        return FindErfPath(baseDir, filename);
    };
    std::string meshErf = resolvePath("modelmeshdata.erf");
    std::string hierErf = resolvePath("modelhierarchies.erf");
    std::string matErf  = resolvePath("materialobjects.erf");
    std::string texErf;
    if (fs::exists(texturePath / "texturepack.erf")) {
        texErf = (texturePath / "texturepack.erf").string();
    } else {
        texErf = FindErfPath(baseDir, "texturepack.erf");
    }
    if (meshErf.empty() || hierErf.empty() || matErf.empty()) {
        return false;
    }
    std::string baseName = modelData.name;
    std::map<std::string, std::vector<uint8_t>> meshFiles, hierFiles, matFiles, texFiles;
    ReportProgress(0.2f, "Generating MSH XML...");
    fs::path mshXmlPath = m_tools.GetWorkDir() / (baseName + ".msh.xml");
    if (!WriteMSHXml(mshXmlPath, modelData)) {
        return false;
    }
    ReportProgress(0.3f, "Converting MSH...");
    std::string mshFile = baseName + ".msh";
    meshFiles[mshFile] = m_tools.ProcessMSH(mshXmlPath);
    if (meshFiles[mshFile].empty()) {
        return false;
    }
    ReportProgress(0.4f, "Generating MMH XML...");
    fs::path mmhXmlPath = m_tools.GetWorkDir() / (baseName + ".mmh.xml");
    if (!WriteMMHXml(mmhXmlPath, modelData, mshFile)) {
        return false;
    }
    ReportProgress(0.5f, "Converting MMH...");
    std::string mmhFile = baseName + ".mmh";
    hierFiles[mmhFile] = m_tools.ProcessMMH(mmhXmlPath);
    if (hierFiles[mmhFile].empty()) {
        return false;
    }
    auto phyData = m_tools.GetLastPHY();
    if (!phyData.empty()) {
        std::string phyFile = baseName + ".phy";
        hierFiles[phyFile] = std::move(phyData);
    }
    ReportProgress(0.6f, "Converting textures...");
    for (const auto& tex : modelData.textures) {
        if (tex.width > 0 && tex.height > 0 && !tex.data.empty() && !tex.ddsName.empty()) {
            std::vector<uint8_t> ddsData = ConvertToDDS(tex.data, tex.width, tex.height, tex.channels);
            texFiles[tex.ddsName] = std::move(ddsData);
        }
    }
    ReportProgress(0.65f, "Generating MAO files...");
    for (const auto& mat : modelData.materials) {
        std::string maoFile = mat.name + ".mao";
        std::string xml = GenerateMAO(mat.name, mat.diffuseMap, mat.normalMap, mat.specularMap);
        matFiles[maoFile].assign(xml.begin(), xml.end());
    }
    ReportProgress(0.7f, "Refreshing modelmeshdata.erf...");
    bool ok1 = RepackERF(meshErf, meshFiles);
    ReportProgress(0.8f, "Refreshing modelhierarchies.erf...");
    bool ok2 = RepackERF(hierErf, hierFiles);
    ReportProgress(0.85f, "Refreshing materialobjects.erf...");
    bool ok3 = RepackERF(matErf, matFiles);
    bool ok4 = true;
    if (!texErf.empty() && !texFiles.empty()) {
        ReportProgress(0.9f, "Refreshing texturepack.erf...");
        ok4 = RepackERF(texErf, texFiles);
    }
    bool success = ok1 && ok2 && ok3 && ok4;
    if (success) {
        markModelAsImported(baseName + ".mmh");
        markModelAsImported(baseName + ".msh");
        if (!modelData.collisionShapes.empty()) {
            markModelAsImported(baseName + ".phy");
        }
    }
    ReportProgress(1.0f, success ? "Import complete!" : "Import failed!");
    return success;
}

bool DAOImporter::ImportToOverride(const std::string& glbPath, const std::string& targetDir) {
    ReportProgress(0.0f, "Initializing tools...");
    if (!m_tools.Initialize()) {
        return false;
    }
    ReportProgress(0.05f, "Loading GLB...");
    DAOModelData modelData;
    if (!LoadGLB(glbPath, modelData)) return false;

    const fs::path baseDir(targetDir);
    fs::path overrideDir = baseDir / "packages" / "core" / "override";
    if (!fs::exists(overrideDir)) {
        fs::create_directories(overrideDir);
    }

    std::string baseName = modelData.name;

    ReportProgress(0.2f, "Generating MSH XML...");
    fs::path mshXmlPath = m_tools.GetWorkDir() / (baseName + ".msh.xml");
    if (!WriteMSHXml(mshXmlPath, modelData)) return false;

    ReportProgress(0.3f, "Converting MSH...");
    std::vector<uint8_t> mshData = m_tools.ProcessMSH(mshXmlPath);
    if (mshData.empty()) return false;

    ReportProgress(0.4f, "Generating MMH XML...");
    fs::path mmhXmlPath = m_tools.GetWorkDir() / (baseName + ".mmh.xml");
    if (!WriteMMHXml(mmhXmlPath, modelData, baseName + ".msh")) return false;

    ReportProgress(0.5f, "Converting MMH...");
    std::vector<uint8_t> mmhData = m_tools.ProcessMMH(mmhXmlPath);
    if (mmhData.empty()) return false;

    auto phyData = m_tools.GetLastPHY();

    ReportProgress(0.6f, "Converting textures...");
    std::map<std::string, std::vector<uint8_t>> texFiles;
    for (const auto& tex : modelData.textures) {
        if (tex.width > 0 && tex.height > 0 && !tex.data.empty() && !tex.ddsName.empty()) {
            texFiles[tex.ddsName] = ConvertToDDS(tex.data, tex.width, tex.height, tex.channels);
        }
    }

    ReportProgress(0.65f, "Generating MAO files...");
    std::map<std::string, std::vector<uint8_t>> matFiles;
    for (const auto& mat : modelData.materials) {
        std::string xml = GenerateMAO(mat.name, mat.diffuseMap, mat.normalMap, mat.specularMap);
        matFiles[mat.name + ".mao"].assign(xml.begin(), xml.end());
    }

    ReportProgress(0.8f, "Writing to override folder...");
    auto writeFile = [&](const fs::path& path, const std::vector<uint8_t>& data) -> bool {
        std::ofstream out(path, std::ios::binary);
        if (!out) return false;
        out.write(reinterpret_cast<const char*>(data.data()), data.size());
        return out.good();
    };

    bool ok = true;
    ok &= writeFile(overrideDir / (baseName + ".msh"), mshData);
    ok &= writeFile(overrideDir / (baseName + ".mmh"), mmhData);
    if (!phyData.empty()) {
        ok &= writeFile(overrideDir / (baseName + ".phy"), phyData);
    }
    for (const auto& [name, data] : matFiles) {
        ok &= writeFile(overrideDir / name, data);
    }
    for (const auto& [name, data] : texFiles) {
        ok &= writeFile(overrideDir / name, data);
    }

    if (ok) {
        markModelAsImported(baseName + ".mmh");
        markModelAsImported(baseName + ".msh");
        if (!modelData.collisionShapes.empty()) {
            markModelAsImported(baseName + ".phy");
        }
    }
    ReportProgress(1.0f, ok ? "Import complete!" : "Import failed!");
    return ok;
}

bool DAOImporter::LoadGLB(const std::string& path, DAOModelData& outData) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    if (!loader.LoadBinaryFromFile(&model, &err, &warn, path)) {
        return false;
    }
    outData.name = ToLower(fs::path(path).stem().string());
    for (size_t i = 0; i < model.nodes.size(); ++i) {
        const auto& node = model.nodes[i];
        if (!node.children.empty()) {
            for (size_t c = 0; c < node.children.size(); ++c) {
            }
        }
        if (!node.translation.empty()) {
        }
        if (!node.rotation.empty()) {
        }
        if (!node.scale.empty()) {
        }
        if (!node.matrix.empty()) {
        }
    }
    if (!model.skins.empty()) {
        const auto& skin = model.skins[0];
        outData.skeleton.hasSkeleton = true;
        std::vector<float> inverseBindMatrices;
        if (skin.inverseBindMatrices >= 0) {
            const auto& accessor = model.accessors[skin.inverseBindMatrices];
            const auto& bufferView = model.bufferViews[accessor.bufferView];
            const auto& buffer = model.buffers[bufferView.buffer];
            const float* data = reinterpret_cast<const float*>(
                buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);
            inverseBindMatrices.assign(data, data + accessor.count * 16);
        }
        outData.skeleton.bones.resize(skin.joints.size());
        for (size_t i = 0; i < skin.joints.size(); ++i) {
            int nodeIdx = skin.joints[i];
            const auto& node = model.nodes[nodeIdx];
            ImportBone& bone = outData.skeleton.bones[i];
            bone.name = node.name.empty() ? "bone_" + std::to_string(i) : node.name;
            bone.index = static_cast<int>(i);
            bone.parentIndex = -1;
            bool hasTRS = !node.translation.empty() || !node.rotation.empty() || !node.scale.empty();
            if (!node.matrix.empty() && !hasTRS) {
                const auto& m = node.matrix;
                bone.translation[0] = (float)m[12];
                bone.translation[1] = (float)m[13];
                bone.translation[2] = (float)m[14];
                float sx = std::sqrt((float)(m[0]*m[0] + m[1]*m[1] + m[2]*m[2]));
                float sy = std::sqrt((float)(m[4]*m[4] + m[5]*m[5] + m[6]*m[6]));
                float sz = std::sqrt((float)(m[8]*m[8] + m[9]*m[9] + m[10]*m[10]));
                bone.scale[0] = sx; bone.scale[1] = sy; bone.scale[2] = sz;
                float r00 = (float)(m[0]/sx), r01 = (float)(m[4]/sy), r02 = (float)(m[8]/sz);
                float r10 = (float)(m[1]/sx), r11 = (float)(m[5]/sy), r12 = (float)(m[9]/sz);
                float r20 = (float)(m[2]/sx), r21 = (float)(m[6]/sy), r22 = (float)(m[10]/sz);
                float tr = r00 + r11 + r22;
                if (tr > 0) {
                    float s = 0.5f / std::sqrt(tr + 1.0f);
                    bone.rotation[3] = 0.25f / s;
                    bone.rotation[0] = (r21 - r12) * s;
                    bone.rotation[1] = (r02 - r20) * s;
                    bone.rotation[2] = (r10 - r01) * s;
                } else if (r00 > r11 && r00 > r22) {
                    float s = 2.0f * std::sqrt(1.0f + r00 - r11 - r22);
                    bone.rotation[3] = (r21 - r12) / s;
                    bone.rotation[0] = 0.25f * s;
                    bone.rotation[1] = (r01 + r10) / s;
                    bone.rotation[2] = (r02 + r20) / s;
                } else if (r11 > r22) {
                    float s = 2.0f * std::sqrt(1.0f + r11 - r00 - r22);
                    bone.rotation[3] = (r02 - r20) / s;
                    bone.rotation[0] = (r01 + r10) / s;
                    bone.rotation[1] = 0.25f * s;
                    bone.rotation[2] = (r12 + r21) / s;
                } else {
                    float s = 2.0f * std::sqrt(1.0f + r22 - r00 - r11);
                    bone.rotation[3] = (r10 - r01) / s;
                    bone.rotation[0] = (r02 + r20) / s;
                    bone.rotation[1] = (r12 + r21) / s;
                    bone.rotation[2] = 0.25f * s;
                }
            } else {
            if (!node.translation.empty()) {
                bone.translation[0] = static_cast<float>(node.translation[0]);
                bone.translation[1] = static_cast<float>(node.translation[1]);
                bone.translation[2] = static_cast<float>(node.translation[2]);
            } else {
                bone.translation[0] = bone.translation[1] = bone.translation[2] = 0.0f;
            }
            if (!node.rotation.empty()) {
                bone.rotation[0] = static_cast<float>(node.rotation[0]);
                bone.rotation[1] = static_cast<float>(node.rotation[1]);
                bone.rotation[2] = static_cast<float>(node.rotation[2]);
                bone.rotation[3] = static_cast<float>(node.rotation[3]);
            } else {
                bone.rotation[0] = bone.rotation[1] = bone.rotation[2] = 0.0f;
                bone.rotation[3] = 1.0f;
            }
            if (!node.scale.empty()) {
                bone.scale[0] = static_cast<float>(node.scale[0]);
                bone.scale[1] = static_cast<float>(node.scale[1]);
                bone.scale[2] = static_cast<float>(node.scale[2]);
            } else {
                bone.scale[0] = bone.scale[1] = bone.scale[2] = 1.0f;
            }
            }
            if (i * 16 + 15 < inverseBindMatrices.size()) {
                for (int j = 0; j < 16; ++j) {
                    bone.inverseBindMatrix[j] = inverseBindMatrices[i * 16 + j];
                }
            }
        }
        for (size_t i = 0; i < skin.joints.size(); ++i) {
            int nodeIdx = skin.joints[i];
            for (size_t n = 0; n < model.nodes.size(); ++n) {
                const auto& parentNode = model.nodes[n];
                for (int childIdx : parentNode.children) {
                    if (childIdx == nodeIdx) {
                        for (size_t j = 0; j < skin.joints.size(); ++j) {
                            if (skin.joints[j] == static_cast<int>(n)) {
                                outData.skeleton.bones[i].parentIndex = static_cast<int>(j);
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }
        for (size_t i = 0; i < std::min(size_t(5), outData.skeleton.bones.size()); ++i) {
            const auto& bone = outData.skeleton.bones[i];
        }
    }
    for (size_t i = 0; i < model.images.size(); ++i) {
        const auto& img = model.images[i];
        if (img.width > 0 && img.height > 0 && !img.image.empty()) {
            DAOModelData::Texture tex;
            tex.originalName = img.uri.empty() ? (img.name.empty() ? "texture_" + std::to_string(i) : img.name) : img.uri;
            tex.ddsName = "";
            tex.width = img.width;
            tex.height = img.height;
            tex.channels = img.component;
            tex.data = img.image;
            outData.textures.push_back(tex);
        }
    }
    auto getTextureImageIndex = [&](int textureIndex) -> int {
        if (textureIndex < 0 || textureIndex >= (int)model.textures.size()) return -1;
        return model.textures[textureIndex].source;
    };
    for (size_t i = 0; i < model.materials.size(); ++i) {
        const auto& gltfMat = model.materials[i];
        DAOModelData::Material mat;
        mat.name = CleanName(gltfMat.name.empty() ? "material_" + std::to_string(i) : gltfMat.name);
        mat.diffuseMap = mat.name + "_d.dds";
        mat.normalMap = mat.name + "_n.dds";
        mat.specularMap = mat.name + "_spec.dds";
        int diffuseIdx = getTextureImageIndex(gltfMat.pbrMetallicRoughness.baseColorTexture.index);
        int normalIdx = getTextureImageIndex(gltfMat.normalTexture.index);
        int specIdx = getTextureImageIndex(gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index);
        if (specIdx < 0) specIdx = getTextureImageIndex(gltfMat.occlusionTexture.index);
        if (diffuseIdx >= 0 && diffuseIdx < (int)outData.textures.size()) {
            outData.textures[diffuseIdx].ddsName = mat.diffuseMap;
        } else {
            mat.diffuseMap = "default_d.dds";
        }
        if (normalIdx >= 0 && normalIdx < (int)outData.textures.size()) {
            outData.textures[normalIdx].ddsName = mat.normalMap;
        } else {
            mat.normalMap = "default_n.dds";
        }
        if (specIdx >= 0 && specIdx < (int)outData.textures.size()) {
            outData.textures[specIdx].ddsName = mat.specularMap;
        } else {
            mat.specularMap = "default_spec.dds";
        }
        outData.materials.push_back(mat);
    }
    if (outData.materials.empty()) {
        DAOModelData::Material defaultMat;
        defaultMat.name = outData.name;
        defaultMat.diffuseMap = "default_d.dds";
        defaultMat.normalMap = "default_n.dds";
        defaultMat.specularMap = "default_spec.dds";
        outData.materials.push_back(defaultMat);
    }
    auto isCollisionMesh = [](const std::string& name) -> bool {
        if (name.size() < 4) return false;
        char c0 = std::tolower((unsigned char)name[0]);
        char c1 = std::tolower((unsigned char)name[1]);
        char c2 = std::tolower((unsigned char)name[2]);
        char c3 = name[3];
        return (c0 == 'u' && c1 == 'c' && c2 == 'x' && c3 == '_');
    };
    auto extractBoneName = [](const std::string& name, const std::string& modelName) -> std::string {
        std::string prefix = "UCX_" + modelName + "_";
        if (name.size() > prefix.size() && name.substr(0, prefix.size()) == prefix) {
            return name.substr(prefix.size());
        }
        if (name.size() > 4) {
            return name.substr(4);
        }
        return "";
    };
    auto computeBounds = [](const std::vector<float>& verts, float& minX, float& maxX,
                           float& minY, float& maxY, float& minZ, float& maxZ) {
        minX = minY = minZ = 1e30f;
        maxX = maxY = maxZ = -1e30f;
        for (size_t i = 0; i < verts.size(); i += 3) {
            if (verts[i] < minX) minX = verts[i];
            if (verts[i] > maxX) maxX = verts[i];
            if (verts[i+1] < minY) minY = verts[i+1];
            if (verts[i+1] > maxY) maxY = verts[i+1];
            if (verts[i+2] < minZ) minZ = verts[i+2];
            if (verts[i+2] > maxZ) maxZ = verts[i+2];
        }
    };
    std::map<int, std::string> meshNodeNames;
    std::map<int, int> meshNodeIndices;
    for (size_t ni = 0; ni < model.nodes.size(); ++ni) {
        const auto& node = model.nodes[ni];
        if (node.mesh >= 0 && !node.name.empty()) {
            meshNodeNames[node.mesh] = node.name;
            meshNodeIndices[node.mesh] = (int)ni;
        }
    }
    std::map<int, int> nodeParent;
    for (size_t ni = 0; ni < model.nodes.size(); ++ni) {
        for (int childIdx : model.nodes[ni].children) {
            nodeParent[childIdx] = (int)ni;
        }
    }
    std::set<int> skinJoints;
    for (const auto& skin : model.skins) {
        for (int j : skin.joints) {
            skinJoints.insert(j);
        }
    }
    auto findParentBone = [&](int nodeIdx) -> std::string {
        int current = nodeIdx;
        while (nodeParent.count(current)) {
            int parent = nodeParent[current];
            if (skinJoints.count(parent)) {
                return model.nodes[parent].name;
            }
            current = parent;
        }
        return "";
    };
    for (size_t meshIdx = 0; meshIdx < model.meshes.size(); ++meshIdx) {
        const auto& mesh = model.meshes[meshIdx];
        std::string meshName = mesh.name;
        if (meshNodeNames.count((int)meshIdx)) {
            meshName = meshNodeNames[(int)meshIdx];
        }
        bool isCollision = isCollisionMesh(meshName);
        for (size_t primIdx = 0; primIdx < mesh.primitives.size(); ++primIdx) {
            const auto& prim = mesh.primitives[primIdx];
            if (prim.mode != TINYGLTF_MODE_TRIANGLES) continue;
            if (isCollision) {
                auto getAccessor = [&](int idx) -> const tinygltf::Accessor* {
                    if (idx < 0 || idx >= (int)model.accessors.size()) return nullptr;
                    return &model.accessors[idx];
                };
                auto getBufferData = [&](const tinygltf::Accessor* acc) -> const uint8_t* {
                    if (!acc) return nullptr;
                    const auto& bv = model.bufferViews[acc->bufferView];
                    return model.buffers[bv.buffer].data.data() + bv.byteOffset + acc->byteOffset;
                };
                const tinygltf::Accessor* posAcc = nullptr;
                for (const auto& attr : prim.attributes) {
                    if (attr.first == "POSITION") posAcc = getAccessor(attr.second);
                }
                if (!posAcc) continue;
                const float* positions = reinterpret_cast<const float*>(getBufferData(posAcc));
                size_t vertCount = posAcc->count;
                std::vector<float> verts(vertCount * 3);
                for (size_t v = 0; v < vertCount; ++v) {
                    verts[v*3] = positions[v*3];
                    verts[v*3+1] = positions[v*3+1];
                    verts[v*3+2] = positions[v*3+2];
                }
                std::vector<uint32_t> indices;
                if (prim.indices >= 0) {
                    const tinygltf::Accessor* idxAcc = getAccessor(prim.indices);
                    const uint8_t* idxData = getBufferData(idxAcc);
                    size_t idxCount = idxAcc->count;
                    indices.resize(idxCount);
                    if (idxAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        const uint16_t* idx16 = reinterpret_cast<const uint16_t*>(idxData);
                        for (size_t i = 0; i < idxCount; ++i) indices[i] = idx16[i];
                    } else if (idxAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        const uint32_t* idx32 = reinterpret_cast<const uint32_t*>(idxData);
                        for (size_t i = 0; i < idxCount; ++i) indices[i] = idx32[i];
                    } else if (idxAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        for (size_t i = 0; i < idxCount; ++i) indices[i] = idxData[i];
                    }
                }
                float minX, maxX, minY, maxY, minZ, maxZ;
                computeBounds(verts, minX, maxX, minY, maxY, minZ, maxZ);
                float cx = (minX + maxX) / 2.0f;
                float cy = (minY + maxY) / 2.0f;
                float cz = (minZ + maxZ) / 2.0f;
                DAOModelData::CollisionShape shape;
                shape.name = meshName;
                std::string boneName = "";
                if (meshNodeIndices.count((int)meshIdx)) {
                    boneName = findParentBone(meshNodeIndices[(int)meshIdx]);
                }
                if (boneName.empty()) {
                    boneName = extractBoneName(meshName, outData.name);
                }
                shape.boneName = boneName;
                shape.type = DAOModelData::CollisionType::Mesh;
                shape.posX = cx; shape.posY = cy; shape.posZ = cz;
                for (size_t i = 0; i < verts.size(); i += 3) {
                    shape.meshVerts.push_back(verts[i] - cx);
                    shape.meshVerts.push_back(verts[i+1] - cy);
                    shape.meshVerts.push_back(verts[i+2] - cz);
                }
                shape.meshIndices = indices;
                outData.collisionShapes.push_back(shape);
                continue;
            }
            DAOModelData::MeshPart part;
            std::string baseName = mesh.name.empty() ? outData.name : CleanName(mesh.name);
            if (mesh.primitives.size() > 1) {
                part.name = baseName + "_" + std::to_string(primIdx);
            } else {
                part.name = baseName;
            }
            if (prim.material >= 0 && prim.material < (int)outData.materials.size()) {
                part.materialName = outData.materials[prim.material].name;
            } else if (!outData.materials.empty()) {
                part.materialName = outData.materials[0].name;
            }
            auto getAccessor = [&](int idx) -> const tinygltf::Accessor* {
                if (idx < 0 || idx >= (int)model.accessors.size()) return nullptr;
                return &model.accessors[idx];
            };
            auto getBufferData = [&](const tinygltf::Accessor* acc) -> const uint8_t* {
                if (!acc) return nullptr;
                const auto& bv = model.bufferViews[acc->bufferView];
                return model.buffers[bv.buffer].data.data() + bv.byteOffset + acc->byteOffset;
            };
            const tinygltf::Accessor* posAcc = nullptr;
            const tinygltf::Accessor* normAcc = nullptr;
            const tinygltf::Accessor* uvAcc = nullptr;
            const tinygltf::Accessor* tanAcc = nullptr;
            const tinygltf::Accessor* jointsAcc = nullptr;
            const tinygltf::Accessor* weightsAcc = nullptr;
            for (const auto& attr : prim.attributes) {
                if (attr.first == "POSITION") posAcc = getAccessor(attr.second);
                else if (attr.first == "NORMAL") normAcc = getAccessor(attr.second);
                else if (attr.first == "TEXCOORD_0") uvAcc = getAccessor(attr.second);
                else if (attr.first == "TANGENT") tanAcc = getAccessor(attr.second);
                else if (attr.first == "JOINTS_0") jointsAcc = getAccessor(attr.second);
                else if (attr.first == "WEIGHTS_0") weightsAcc = getAccessor(attr.second);
            }
            if (!posAcc) continue;
            part.hasSkinning = (jointsAcc != nullptr && weightsAcc != nullptr);

            auto getStride = [&](const tinygltf::Accessor* acc, int defaultSize) -> int {
                const auto& bv = model.bufferViews[acc->bufferView];
                return bv.byteStride > 0 ? (int)bv.byteStride : defaultSize;
            };
            auto getElemPtr = [&](const tinygltf::Accessor* acc, size_t idx, int stride) -> const uint8_t* {
                const auto& bv = model.bufferViews[acc->bufferView];
                return model.buffers[bv.buffer].data.data() + bv.byteOffset + acc->byteOffset + idx * stride;
            };

            int posStride = getStride(posAcc, 12);
            int normStride = normAcc ? getStride(normAcc, 12) : 0;
            int uvStride = uvAcc ? getStride(uvAcc, 8) : 0;
            int tanStride = tanAcc ? getStride(tanAcc, 16) : 0;

            size_t vertexCount = posAcc->count;
            part.vertices.resize(vertexCount);
            for (size_t v = 0; v < vertexCount; ++v) {
                ImportVertex& vert = part.vertices[v];
                const float* pos = reinterpret_cast<const float*>(getElemPtr(posAcc, v, posStride));
                vert.x = pos[0];
                vert.y = pos[1];
                vert.z = pos[2];
                if (normAcc) {
                    const float* n = reinterpret_cast<const float*>(getElemPtr(normAcc, v, normStride));
                    vert.nx = n[0]; vert.ny = n[1]; vert.nz = n[2];
                } else {
                    vert.nx = 0; vert.ny = 1; vert.nz = 0;
                }
                if (uvAcc) {
                    const float* uv = reinterpret_cast<const float*>(getElemPtr(uvAcc, v, uvStride));
                    vert.u = uv[0]; vert.v = uv[1];
                } else {
                    vert.u = 0; vert.v = 0;
                }
                if (tanAcc) {
                    const float* t = reinterpret_cast<const float*>(getElemPtr(tanAcc, v, tanStride));
                    vert.tx = t[0]; vert.ty = t[1]; vert.tz = t[2]; vert.tw = t[3];
                } else {
                    float ax = 1.0f, ay = 0.0f, az = 0.0f;
                    if (std::abs(vert.nx) > 0.9f) { ax = 0.0f; ay = 1.0f; az = 0.0f; }
                    float tx = ay * vert.nz - az * vert.ny;
                    float ty = az * vert.nx - ax * vert.nz;
                    float tz = ax * vert.ny - ay * vert.nx;
                    float len = std::sqrt(tx*tx + ty*ty + tz*tz);
                    if (len > 0.0001f) { tx /= len; ty /= len; tz /= len; }
                    else { tx = 1.0f; ty = 0.0f; tz = 0.0f; }
                    vert.tx = tx; vert.ty = ty; vert.tz = tz; vert.tw = 1.0f;
                }
                if (part.hasSkinning && jointsAcc && weightsAcc) {
                    int jointStride = getStride(jointsAcc, jointsAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ? 8 :
                                                           jointsAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT ? 16 : 4);
                    int weightStride = getStride(weightsAcc, weightsAcc->componentType == TINYGLTF_COMPONENT_TYPE_FLOAT ? 16 :
                                                              weightsAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ? 8 : 4);
                    const uint8_t* jp = getElemPtr(jointsAcc, v, jointStride);
                    const uint8_t* wp = getElemPtr(weightsAcc, v, weightStride);
                    if (jointsAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        vert.boneIndices[0] = jp[0]; vert.boneIndices[1] = jp[1];
                        vert.boneIndices[2] = jp[2]; vert.boneIndices[3] = jp[3];
                    } else if (jointsAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        const uint16_t* j16 = reinterpret_cast<const uint16_t*>(jp);
                        vert.boneIndices[0] = j16[0]; vert.boneIndices[1] = j16[1];
                        vert.boneIndices[2] = j16[2]; vert.boneIndices[3] = j16[3];
                    } else if (jointsAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        const uint32_t* j32 = reinterpret_cast<const uint32_t*>(jp);
                        vert.boneIndices[0] = (int)j32[0]; vert.boneIndices[1] = (int)j32[1];
                        vert.boneIndices[2] = (int)j32[2]; vert.boneIndices[3] = (int)j32[3];
                    }
                    if (weightsAcc->componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                        const float* wf = reinterpret_cast<const float*>(wp);
                        vert.boneWeights[0] = wf[0]; vert.boneWeights[1] = wf[1];
                        vert.boneWeights[2] = wf[2]; vert.boneWeights[3] = wf[3];
                    } else if (weightsAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        vert.boneWeights[0] = wp[0] / 255.0f; vert.boneWeights[1] = wp[1] / 255.0f;
                        vert.boneWeights[2] = wp[2] / 255.0f; vert.boneWeights[3] = wp[3] / 255.0f;
                    } else if (weightsAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        const uint16_t* w16 = reinterpret_cast<const uint16_t*>(wp);
                        vert.boneWeights[0] = w16[0] / 65535.0f; vert.boneWeights[1] = w16[1] / 65535.0f;
                        vert.boneWeights[2] = w16[2] / 65535.0f; vert.boneWeights[3] = w16[3] / 65535.0f;
                    }
                    float wsum = vert.boneWeights[0] + vert.boneWeights[1] +
                                 vert.boneWeights[2] + vert.boneWeights[3];
                    if (wsum > 0.0001f && std::abs(wsum - 1.0f) > 0.001f) {
                        float inv = 1.0f / wsum;
                        vert.boneWeights[0] *= inv;
                        vert.boneWeights[1] *= inv;
                        vert.boneWeights[2] *= inv;
                        vert.boneWeights[3] *= inv;
                    }
                }
            }
            if (part.hasSkinning) {
                std::set<int> usedBones;
                for (const auto& v : part.vertices) {
                    for (int i = 0; i < 4; ++i) {
                        if (v.boneWeights[i] > 0.0f) {
                            usedBones.insert(v.boneIndices[i]);
                        }
                    }
                }
                part.bonesUsed.assign(usedBones.begin(), usedBones.end());
                std::map<int, int> boneRemap;
                for (size_t bi = 0; bi < part.bonesUsed.size(); ++bi)
                    boneRemap[part.bonesUsed[bi]] = (int)bi;
                for (auto& v : part.vertices) {
                    for (int i = 0; i < 4; ++i) {
                        auto it = boneRemap.find(v.boneIndices[i]);
                        if (it != boneRemap.end())
                            v.boneIndices[i] = it->second;
                        else if (v.boneWeights[i] > 0.0f)
                            v.boneIndices[i] = 0;
                    }
                }
            }
            if (prim.indices >= 0) {
                const auto* idxAcc = getAccessor(prim.indices);
                const uint8_t* idxData = getBufferData(idxAcc);
                size_t idxCount = idxAcc->count;
                part.indices.resize(idxCount);
                if (idxAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    const uint16_t* idx16 = reinterpret_cast<const uint16_t*>(idxData);
                    for (size_t i = 0; i < idxCount; ++i) part.indices[i] = idx16[i];
                } else if (idxAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    const uint32_t* idx32 = reinterpret_cast<const uint32_t*>(idxData);
                    for (size_t i = 0; i < idxCount; ++i) part.indices[i] = idx32[i];
                } else if (idxAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    for (size_t i = 0; i < idxCount; ++i) part.indices[i] = idxData[i];
                }
            }
            outData.parts.push_back(part);
        }
    }
    // GLB mesh data from Blender is in Z-up local space.
    // DAO uses Y-up. Convert: (x, y, z) -> (x, -z, y)
    for (auto& part : outData.parts) {
        for (auto& v : part.vertices) {
            float oy = v.y;  v.y = -v.z;  v.z = oy;
            float ny = v.ny; v.ny = -v.nz; v.nz = ny;
            float ty = v.ty; v.ty = -v.tz; v.tz = ty;
        }
    }
    // Convert root bone transforms
    if (outData.skeleton.hasSkeleton) {
        for (auto& bone : outData.skeleton.bones) {
            if (bone.parentIndex < 0) {
                float oy = bone.translation[1];
                bone.translation[1] = -bone.translation[2];
                bone.translation[2] = oy;
                // Prepend +90° X rotation to root bone quaternion
                // Q_x(+90) = (0.7071068, 0, 0, 0.7071068)
                float rqx = 0.7071068f, rqy = 0.0f, rqz = 0.0f, rqw = 0.7071068f;
                float bq[4] = {bone.rotation[0], bone.rotation[1], bone.rotation[2], bone.rotation[3]};
                bone.rotation[3] = rqw*bq[3] - rqx*bq[0] - rqy*bq[1] - rqz*bq[2];
                bone.rotation[0] = rqw*bq[0] + rqx*bq[3] + rqy*bq[2] - rqz*bq[1];
                bone.rotation[1] = rqw*bq[1] - rqx*bq[2] + rqy*bq[3] + rqz*bq[0];
                bone.rotation[2] = rqw*bq[2] + rqx*bq[1] - rqy*bq[0] + rqz*bq[3];
            }
        }
    }
    // Convert collision shapes
    for (auto& shape : outData.collisionShapes) {
        float oy = shape.posY; shape.posY = -shape.posZ; shape.posZ = oy;
    }

    return !outData.parts.empty() || !outData.collisionShapes.empty();
}

bool DAOImporter::WriteMSHXml(const fs::path& outputPath, const DAOModelData& model) {
    std::ofstream out(outputPath);
    if (!out) return false;
    out << std::fixed << std::setprecision(6);
    out << "<?xml version=\"1.0\" ?>\n";
    out << "<ModelMeshData Name=\"" << model.name << ".MSH\" Version=\"1\">\n";
    for (size_t partIdx = 0; partIdx < model.parts.size(); ++partIdx) {
        const auto& part = model.parts[partIdx];
        size_t vertCount = part.vertices.size();
        size_t idxCount = part.indices.size();
        out << "<MeshGroup Name=\"" << part.name << "\" Optimize=\"All\">\n";
        out << "<Data ElementCount=\"" << vertCount << "\" Semantic=\"POSITION\" Type=\"Float4\">\n<![CDATA[\n";
        for (const auto& v : part.vertices)
            out << v.x << " " << v.y << " " << v.z << " 1.0\n";
        out << "]]>\n</Data>\n";
        out << "<Data ElementCount=\"" << vertCount << "\" Semantic=\"TEXCOORD\" Type=\"Float2\">\n<![CDATA[\n";
        for (const auto& v : part.vertices)
            out << v.u << " " << v.v << "\n";
        out << "]]>\n</Data>\n";
        out << "<Data ElementCount=\"" << vertCount << "\" Semantic=\"TANGENT\" Type=\"Float4\">\n<![CDATA[\n";
        for (const auto& v : part.vertices)
            out << v.tx << " " << v.ty << " " << v.tz << " 1.0\n";
        out << "]]>\n</Data>\n";
        out << "<Data ElementCount=\"" << vertCount << "\" Semantic=\"BINORMAL\" Type=\"Float4\">\n<![CDATA[\n";
        for (const auto& v : part.vertices) {
            float bx = v.ny * v.tz - v.nz * v.ty;
            float by = v.nz * v.tx - v.nx * v.tz;
            float bz = v.nx * v.ty - v.ny * v.tx;
            out << (bx * v.tw) << " " << (by * v.tw) << " " << (bz * v.tw) << " 1.0\n";
        }
        out << "]]>\n</Data>\n";
        out << "<Data ElementCount=\"" << vertCount << "\" Semantic=\"NORMAL\" Type=\"Float4\">\n<![CDATA[\n";
        for (const auto& v : part.vertices)
            out << v.nx << " " << v.ny << " " << v.nz << " 1.0\n";
        out << "]]>\n</Data>\n";
        if (part.hasSkinning) {
            out << "<Data ElementCount=\"" << vertCount << "\" Semantic=\"BLENDWEIGHT\" Type=\"Float4\">\n<![CDATA[\n";
            for (const auto& v : part.vertices)
                out << v.boneWeights[0] << " " << v.boneWeights[1] << " "
                    << v.boneWeights[2] << " " << v.boneWeights[3] << "\n";
            out << "]]>\n</Data>\n";
            out << "<Data ElementCount=\"" << vertCount << "\" Semantic=\"BLENDINDICES\" Type=\"Short4\">\n<![CDATA[\n";
            for (const auto& v : part.vertices)
                out << v.boneIndices[0] << " " << v.boneIndices[1] << " "
                    << v.boneIndices[2] << " " << v.boneIndices[3] << "\n";
            out << "]]>\n</Data>\n";
        }
        out << "<Data IndexCount=\"" << idxCount << "\" IndexType=\"Index32\" Semantic=\"Indices\">\n<![CDATA[\n";
        for (size_t i = 0; i + 2 < part.indices.size(); i += 3) {
            out << part.indices[i] << " " << part.indices[i + 1] << " " << part.indices[i + 2] << "\n";
        }
        out << "]]>\n</Data>\n";
        out << "</MeshGroup>\n";
    }
    out << "</ModelMeshData>\n";
    out.flush();
    return out.good();
}

bool DAOImporter::WriteMMHXml(const fs::path& outputPath, const DAOModelData& model, const std::string& mshFilename) {
    std::ofstream out(outputPath);
    if (!out) return false;
    out << std::fixed << std::setprecision(6);
    std::string materialName = model.materials.empty() ? model.name : model.materials[0].name;
    bool hasSkeleton = model.skeleton.hasSkeleton && !model.skeleton.bones.empty();
    out << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    out << "<ModelHierarchy Name=\"" << model.name << "\" ModelDataName=\"" << model.name << "\">\n";
    if (hasSkeleton) {
        std::function<void(int, int)> writeBone = [&](int boneIdx, int depth) {
            const ImportBone& bone = model.skeleton.bones[boneIdx];
            std::string indent(depth * 2, ' ');
            out << indent << "<Node Name=\"" << bone.name << "\"";
            if (bone.index >= 0) {
                out << " BoneIndex=\"" << bone.index << "\"";
            }
            out << ">\n";
            out << indent << "  <Translation>" << bone.translation[0] << " "
                << bone.translation[1] << " " << bone.translation[2] << "</Translation>\n";
            out << indent << "  <Rotation>" << bone.rotation[0] << " " << bone.rotation[1] << " "
                << bone.rotation[2] << " " << bone.rotation[3] << "</Rotation>\n";
            std::vector<int> children;
            for (size_t i = 0; i < model.skeleton.bones.size(); ++i) {
                if (model.skeleton.bones[i].parentIndex == boneIdx) {
                    children.push_back(static_cast<int>(i));
                }
            }
            for (int child : children) writeBone(child, depth + 1);
            out << indent << "</Node>\n";
        };
        auto writeBoneChildrenOnly = [&](int boneIdx, int depth) {
            std::vector<int> children;
            for (size_t i = 0; i < model.skeleton.bones.size(); ++i) {
                if (model.skeleton.bones[i].parentIndex == boneIdx) {
                    children.push_back(static_cast<int>(i));
                }
            }
            for (int child : children) writeBone(child, depth);
        };
        bool hasGOB = false;
        int gobBoneIdx = -1;
        for (size_t i = 0; i < model.skeleton.bones.size(); ++i) {
            if (model.skeleton.bones[i].name == "GOB" && model.skeleton.bones[i].parentIndex == -1) {
                hasGOB = true;
                gobBoneIdx = static_cast<int>(i);
                break;
            }
        }
        if (hasGOB) {
            const ImportBone& gob = model.skeleton.bones[gobBoneIdx];
            out << "  <Node Name=\"GOB\" SoundMaterialType=\"0\">\n";
            out << "    <Translation>" << gob.translation[0] << " "
                << gob.translation[1] << " " << gob.translation[2] << "</Translation>\n";
            out << "    <Rotation>" << gob.rotation[0] << " " << gob.rotation[1] << " "
                << gob.rotation[2] << " " << gob.rotation[3] << "</Rotation>\n";
            writeBoneChildrenOnly(gobBoneIdx, 2);
            for (size_t partIdx = 0; partIdx < model.parts.size(); ++partIdx) {
                const auto& part = model.parts[partIdx];
                std::set<int> partBonesUsed(part.bonesUsed.begin(), part.bonesUsed.end());
                out << "    <NodeMesh Name=\"" << part.name << "\" ";
                if (!partBonesUsed.empty()) {
                    out << "BonesUsed=\"";
                    bool first = true;
                    for (int bi : partBonesUsed) {
                        if (!first) out << " ";
                        out << bi;
                        first = false;
                    }
                    out << "\" ";
                }
                out << "MeshGroupName=\"" << part.name << "\" ";
                out << "MaterialObject=\"" << part.materialName << "\" ";
                out << "CastRuntimeShadow=\"1\" ReceiveRuntimeShadow=\"1\">\n";
                out << "      <Translation>0 0 0</Translation>\n";
                out << "      <Rotation>0 0 0 1</Rotation>\n";
                out << "    </NodeMesh>\n";
            }
            out << "  </Node>\n";
        } else {
            out << "  <Node Name=\"GOB\" SoundMaterialType=\"0\">\n";
            out << "    <Translation>0 0 0</Translation>\n";
            out << "    <Rotation>0 0 0 1</Rotation>\n";
            for (size_t i = 0; i < model.skeleton.bones.size(); ++i) {
                if (model.skeleton.bones[i].parentIndex == -1) {
                    writeBone(static_cast<int>(i), 2);
                }
            }
            for (size_t partIdx = 0; partIdx < model.parts.size(); ++partIdx) {
                const auto& part = model.parts[partIdx];
                std::set<int> partBonesUsed(part.bonesUsed.begin(), part.bonesUsed.end());
                out << "    <NodeMesh Name=\"" << part.name << "\" ";
                if (!partBonesUsed.empty()) {
                    out << "BonesUsed=\"";
                    bool first = true;
                    for (int bi : partBonesUsed) {
                        if (!first) out << " ";
                        out << bi;
                        first = false;
                    }
                    out << "\" ";
                }
                out << "MeshGroupName=\"" << part.name << "\" ";
                out << "MaterialObject=\"" << part.materialName << "\" ";
                out << "CastRuntimeShadow=\"1\" ReceiveRuntimeShadow=\"1\">\n";
                out << "      <Translation>0 0 0</Translation>\n";
                out << "      <Rotation>0 0 0 1</Rotation>\n";
                out << "    </NodeMesh>\n";
            }
            out << "  </Node>\n";
        }
    } else {
        for (size_t partIdx = 0; partIdx < model.parts.size(); ++partIdx) {
            const auto& part = model.parts[partIdx];
            out << "  <NodeMesh Name=\"" << part.name << "\" ";
            out << "MeshGroupName=\"" << part.name << "\" ";
            out << "MaterialObject=\"" << part.materialName << "\" ";
            out << "CastRuntimeShadow=\"1\" ReceiveRuntimeShadow=\"1\">\n";
            out << "    <Translation>0 0 0</Translation>\n";
            out << "    <Rotation>0 0 0 1</Rotation>\n";
            out << "  </NodeMesh>\n";
        }
    }
    if (!model.collisionShapes.empty()) {
        out << "  <CollisionObject Static=\"true\">\n";
        for (const auto& shape : model.collisionShapes) {
            out << "    <Shape Name=\"" << shape.name << "\" Type=\"Mesh\" ";
            out << "AllowEmitterSpawn=\"1\" Fadeable=\"false\" ";
            out << "GROUP_MASK_WALKABLE=\"false\" ";
            out << "GROUP_MASK_NONWALKABLE=\"false\" ";
            out << "GROUP_MASK_ITEMS=\"false\" ";
            out << "GROUP_MASK_CREATURES=\"false\" ";
            out << "GROUP_MASK_PLACEABLES=\"false\" ";
            out << "GROUP_MASK_STATICGEOMETRY=\"true\" ";
            out << "GROUP_MASK_TRIGGERS=\"false\" ";
            out << "GROUP_MASK_TERRAIN_WALL=\"false\" ";
            out << "Rotation=\"" << shape.rotX << " " << shape.rotY << " " << shape.rotZ << " " << shape.rotW << "\" ";
            out << "Position=\"" << shape.posX << " " << shape.posY << " " << shape.posZ << " 1.0\" >\n";
            size_t vertCount = shape.meshVerts.size() / 3;
            out << "      <VertexData length=\"" << vertCount << "\">\n";
            for (size_t i = 0; i < shape.meshVerts.size(); i += 3) {
                out << "        " << shape.meshVerts[i] << " " << shape.meshVerts[i+1] << " " << shape.meshVerts[i+2] << "\n";
            }
            out << "      </VertexData>\n";
            out << "      <IndexData>\n";
            for (size_t i = 0; i + 2 < shape.meshIndices.size(); i += 3) {
                out << "        " << shape.meshIndices[i] << " " << shape.meshIndices[i+1] << " " << shape.meshIndices[i+2] << "\n";
            }
            out << "      </IndexData>\n";
            out << "    </Shape>\n";
        }
        out << "  </CollisionObject>\n";
    }
    out << "</ModelHierarchy>\n";
    return out.good();
}

std::string DAOImporter::GenerateMAO(const std::string& matName, const std::string& diffuse,
                                      const std::string& normal, const std::string& specular) {
    std::stringstream ss;
    ss << "<?xml version=\"1.0\" ?>\n<MaterialObject Name=\"" << matName << "\">\n";
    ss << "    <Material Name=\"Prop.mat\" />\n";
    ss << "    <DefaultSemantic Name=\"Default\" />\n";
    ss << "    <Texture Name=\"mml_tDiffuse\" ResName=\"" << diffuse << "\" />\n";
    ss << "    <Texture Name=\"mml_tNormalMap\" ResName=\"" << normal << "\" />\n";
    ss << "    <Texture Name=\"mml_tSpecularMask\" ResName=\"" << specular << "\" />\n";
    ss << "</MaterialObject>";
    return ss.str();
}

bool DAOImporter::RepackERF(const std::string& erfPath, const std::map<std::string, std::vector<uint8_t>>& newFiles) {
    if (!fs::exists(erfPath)) {
        return false;
    }
    std::vector<uint8_t> erfData;
    {
        std::ifstream in(erfPath, std::ios::binary | std::ios::ate);
        if (!in) return false;
        size_t size = in.tellg();
        in.seekg(0);
        erfData.resize(size);
        in.read(reinterpret_cast<char*>(erfData.data()), size);
    }
    if (erfData.size() < 32) return false;
    auto readU32 = [&](size_t offset) -> uint32_t {
        if (offset + 4 > erfData.size()) return 0;
        return *reinterpret_cast<uint32_t*>(&erfData[offset]);
    };
    auto readUtf16String = [&](size_t offset, size_t charCount) -> std::string {
        std::string result;
        for (size_t i = 0; i < charCount; ++i) {
            size_t pos = offset + i * 2;
            if (pos + 2 > erfData.size()) break;
            uint16_t ch = *reinterpret_cast<uint16_t*>(&erfData[pos]);
            if (ch == 0) break;
            if (ch < 128) result += static_cast<char>(ch);
        }
        return result;
    };
    std::string magic = readUtf16String(0, 4);
    std::string version = readUtf16String(8, 4);
    if (magic != "ERF ") {
        return false;
    }
    ERFVersion erfVer = ERFVersion::Unknown;
    if (version == "V2.0") erfVer = ERFVersion::V2_0;
    else if (version == "V2.2") erfVer = ERFVersion::V2_2;
    if (erfVer != ERFVersion::V2_0 && erfVer != ERFVersion::V2_2) {
        return false;
    }
    uint32_t fileCount = readU32(16);
    uint32_t year = readU32(20);
    uint32_t day = readU32(24);
    uint32_t unknown = readU32(28);
    struct FileEntry { std::string name; uint32_t offset; uint32_t size; };
    std::vector<FileEntry> entries;
    size_t tableOffset = 32;
    for (uint32_t i = 0; i < fileCount; ++i) {
        size_t entryOff = tableOffset + i * 72;
        if (entryOff + 72 > erfData.size()) break;
        FileEntry e;
        e.name = readUtf16String(entryOff, 32);
        e.offset = readU32(entryOff + 64);
        e.size = readU32(entryOff + 68);
        entries.push_back(e);
    }
    for (const auto& [name, data] : newFiles) {
        std::string lowerName = ToLower(name);
        bool found = false;
        for (auto& e : entries) {
            if (ToLower(e.name) == lowerName) { found = true; break; }
        }
        if (!found) {
            entries.push_back({name, 0, static_cast<uint32_t>(data.size())});
        }
    }
    fs::path backupDir = fs::current_path() / "backups";
    fs::create_directories(backupDir);
    fs::path backupPath = backupDir / (fs::path(erfPath).filename().string() + ".bak");
    if (!fs::exists(backupPath)) {
        bool doBackup = true;
        if (m_backupCallback) doBackup = m_backupCallback(fs::path(erfPath).filename().string(), backupDir.string());
        if (doBackup) fs::copy_file(erfPath, backupPath);
    }
    std::vector<uint8_t> newErfData;
    auto writeU16 = [&](uint16_t v) {
        newErfData.push_back(v & 0xFF);
        newErfData.push_back((v >> 8) & 0xFF);
    };
    auto writeU32 = [&](uint32_t v) {
        newErfData.push_back(v & 0xFF);
        newErfData.push_back((v >> 8) & 0xFF);
        newErfData.push_back((v >> 16) & 0xFF);
        newErfData.push_back((v >> 24) & 0xFF);
    };
    auto writeUtf16Fixed = [&](const std::string& str, size_t charCount) {
        for (size_t i = 0; i < charCount; ++i) {
            writeU16(i < str.size() ? static_cast<uint16_t>(static_cast<unsigned char>(str[i])) : 0);
        }
    };
    writeUtf16Fixed("ERF ", 4);
    writeUtf16Fixed(version, 4);
    writeU32(static_cast<uint32_t>(entries.size()));
    writeU32(year);
    writeU32(day);
    writeU32(unknown);
    size_t tableStart = newErfData.size();
    size_t dataStart = tableStart + entries.size() * 72;
    while (dataStart % 16 != 0) dataStart++;
    newErfData.resize(dataStart, 0);
    std::vector<std::pair<uint32_t, uint32_t>> offsets;
    for (auto& e : entries) {
        uint32_t offset = static_cast<uint32_t>(newErfData.size());
        uint32_t size = 0;
        std::string lowerName = ToLower(e.name);
        auto it = std::find_if(newFiles.begin(), newFiles.end(),
            [&](const auto& p) { return ToLower(p.first) == lowerName; });
        if (it != newFiles.end()) {
            size = static_cast<uint32_t>(it->second.size());
            newErfData.insert(newErfData.end(), it->second.begin(), it->second.end());
        } else {
            size = e.size;
            if (e.offset + e.size <= erfData.size()) {
                newErfData.insert(newErfData.end(), erfData.begin() + e.offset, erfData.begin() + e.offset + e.size);
            }
        }
        offsets.push_back({offset, size});
    }
    for (size_t i = 0; i < entries.size(); ++i) {
        size_t entryOff = tableStart + i * 72;
        for (size_t c = 0; c < 32; ++c) {
            uint16_t ch = c < entries[i].name.size() ? static_cast<uint16_t>(static_cast<unsigned char>(entries[i].name[c])) : 0;
            newErfData[entryOff + c * 2] = ch & 0xFF;
            newErfData[entryOff + c * 2 + 1] = (ch >> 8) & 0xFF;
        }
        memcpy(&newErfData[entryOff + 64], &offsets[i].first, 4);
        memcpy(&newErfData[entryOff + 68], &offsets[i].second, 4);
    }
    std::ofstream out(erfPath, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<char*>(newErfData.data()), newErfData.size());
    return true;
}