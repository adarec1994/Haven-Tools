#include "import.h"
#include <iostream>
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
        std::cerr << "[DAOTools] Failed to extract tools" << std::endl;
        return false;
    }

    m_initialized = true;
    std::cout << "[DAOTools] Initialized at: " << m_workDir << std::endl;
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
        std::cerr << "[DAOTools] Processor not found: " << exePath << std::endl;
        return false;
    }

#ifdef _WIN32
    std::cout << "[DAOTools] Running: " << cmdLine << std::endl;

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

        if (!stdoutStr.empty()) std::cout << "[DAOTools] STDOUT: " << stdoutStr << std::endl;
        if (!stderrStr.empty()) std::cout << "[DAOTools] STDERR: " << stderrStr << std::endl;

        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        std::cout << "[DAOTools] Exit code: " << exitCode << std::endl;

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return exitCode == 0;
    }

    DWORD err = GetLastError();
    std::cerr << "[DAOTools] CreateProcess failed: " << err << std::endl;
    CloseHandle(hStdOutRead);
    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrRead);
    CloseHandle(hStdErrWrite);
    return false;
#else
    std::cerr << "[DAOTools] Graphics processors only supported on Windows" << std::endl;
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
        std::cerr << "[DAOTools] MSH output not created: " << outPath << std::endl;
        return {};
    }

    auto result = ReadBinaryFile(outPath);
    std::cout << "[DAOTools] Generated MSH: " << result.size() << " bytes" << std::endl;
    return result;
}

std::vector<uint8_t> DAOGraphicsTools::ProcessMMH(const fs::path& xmlPath) {
    fs::path exePath = m_mmhDir / "GraphicsProcessorMMH.exe";
    fs::path localXml = m_mmhDir / xmlPath.filename();
    fs::copy_file(xmlPath, localXml, fs::copy_options::overwrite_existing);

    std::string outName = xmlPath.stem().string();
    if (outName.size() > 4 && outName.substr(outName.size() - 4) == ".mmh") {
        outName = outName.substr(0, outName.size() - 4);
    }
    fs::path outPath = m_mmhDir / (outName + ".mmh");
    fs::remove(outPath);

    if (!RunProcessor(exePath, localXml)) return {};

    if (!fs::exists(outPath)) {
        std::cerr << "[DAOTools] MMH output not created: " << outPath << std::endl;
        return {};
    }

    auto result = ReadBinaryFile(outPath);
    std::cout << "[DAOTools] Generated MMH: " << result.size() << " bytes" << std::endl;
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
        std::cerr << "[Import] Failed to initialize graphics tools" << std::endl;
        return false;
    }

    ReportProgress(0.05f, "Loading GLB...");
    std::cout << "[Import] Processing: " << fs::path(glbPath).filename().string() << std::endl;

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
        std::cerr << "[Import] Error: Required ERFs not found." << std::endl;
        return false;
    }

    std::cout << "[Import] ERF locations:" << std::endl;
    std::cout << "  mesh: " << meshErf << std::endl;
    std::cout << "  hier: " << hierErf << std::endl;
    std::cout << "  mat:  " << matErf << std::endl;
    std::cout << "  tex:  " << (texErf.empty() ? "NOT FOUND" : texErf) << std::endl;

    std::string baseName = modelData.name;
    std::map<std::string, std::vector<uint8_t>> meshFiles, hierFiles, matFiles, texFiles;

    ReportProgress(0.2f, "Generating MSH XML...");
    fs::path mshXmlPath = m_tools.GetWorkDir() / (baseName + ".msh.xml");
    if (!WriteMSHXml(mshXmlPath, modelData)) {
        std::cerr << "[Import] Failed to write MSH XML" << std::endl;
        return false;
    }

    ReportProgress(0.3f, "Converting MSH...");
    std::string mshFile = baseName + ".msh";
    meshFiles[mshFile] = m_tools.ProcessMSH(mshXmlPath);
    if (meshFiles[mshFile].empty()) {
        std::cerr << "[Import] MSH conversion failed" << std::endl;
        return false;
    }
    std::cout << "  + Generated: " << mshFile << " (" << meshFiles[mshFile].size() << " bytes)" << std::endl;

    ReportProgress(0.4f, "Generating MMH XML...");
    fs::path mmhXmlPath = m_tools.GetWorkDir() / (baseName + ".mmh.xml");
    if (!WriteMMHXml(mmhXmlPath, modelData, mshFile)) {
        std::cerr << "[Import] Failed to write MMH XML" << std::endl;
        return false;
    }

    ReportProgress(0.5f, "Converting MMH...");
    std::string mmhFile = baseName + ".mmh";
    hierFiles[mmhFile] = m_tools.ProcessMMH(mmhXmlPath);
    if (hierFiles[mmhFile].empty()) {
        std::cerr << "[Import] MMH conversion failed" << std::endl;
        return false;
    }
    std::cout << "  + Generated: " << mmhFile << " (" << hierFiles[mmhFile].size() << " bytes)" << std::endl;

    ReportProgress(0.6f, "Converting textures...");
    for (const auto& tex : modelData.textures) {
        if (tex.width > 0 && tex.height > 0 && !tex.data.empty() && !tex.ddsName.empty()) {
            std::vector<uint8_t> ddsData = ConvertToDDS(tex.data, tex.width, tex.height, tex.channels);
            texFiles[tex.ddsName] = std::move(ddsData);
            std::cout << "  + Generated texture: " << tex.ddsName << " (" << tex.width << "x" << tex.height << ")" << std::endl;
        }
    }

    ReportProgress(0.65f, "Generating MAO files...");
    for (const auto& mat : modelData.materials) {
        std::string maoFile = mat.name + ".mao";
        std::string xml = GenerateMAO(mat.name, mat.diffuseMap, mat.normalMap, mat.specularMap);
        matFiles[maoFile].assign(xml.begin(), xml.end());
        std::cout << "  + Generated: " << maoFile << std::endl;
    }

    // Generate PHY file if collision shapes exist
    if (!modelData.collisionShapes.empty()) {
        ReportProgress(0.67f, "Generating PHY file...");
        std::string phyFile = baseName + ".phy";
        std::vector<uint8_t> phyData = GeneratePHY(modelData);
        if (!phyData.empty()) {
            hierFiles[phyFile] = std::move(phyData);
            std::cout << "  + Generated: " << phyFile << " (" << modelData.collisionShapes.size() << " shapes)" << std::endl;
        }
    }

    std::cout << "\n[Import] Updating ERF files..." << std::endl;

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
    ReportProgress(1.0f, success ? "Import complete!" : "Import failed!");

    std::cout << "\n[Import] " << (success ? "SUCCESS" : "FAILED") << std::endl;
    return success;
}

bool DAOImporter::LoadGLB(const std::string& path, DAOModelData& outData) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    if (!loader.LoadBinaryFromFile(&model, &err, &warn, path)) {
        std::cerr << "GLB Load Error: " << err << std::endl;
        return false;
    }
    outData.name = ToLower(fs::path(path).stem().string());

    // Debug: Print scene info
    std::cout << "\n=== GLB DEBUG ===" << std::endl;
    std::cout << "[GLB] File: " << path << std::endl;
    std::cout << "[GLB] Scenes: " << model.scenes.size() << std::endl;
    std::cout << "[GLB] Nodes: " << model.nodes.size() << std::endl;
    std::cout << "[GLB] Meshes: " << model.meshes.size() << std::endl;
    std::cout << "[GLB] Skins: " << model.skins.size() << std::endl;

    // Debug: Print all nodes and their transforms
    std::cout << "\n[GLB] Node hierarchy:" << std::endl;
    for (size_t i = 0; i < model.nodes.size(); ++i) {
        const auto& node = model.nodes[i];
        std::cout << "  Node " << i << ": \"" << node.name << "\"";
        if (node.mesh >= 0) std::cout << " [mesh=" << node.mesh << "]";
        if (node.skin >= 0) std::cout << " [skin=" << node.skin << "]";
        if (!node.children.empty()) {
            std::cout << " children=[";
            for (size_t c = 0; c < node.children.size(); ++c) {
                if (c > 0) std::cout << ",";
                std::cout << node.children[c];
            }
            std::cout << "]";
        }
        std::cout << std::endl;

        if (!node.translation.empty()) {
            std::cout << "    translation: [" << node.translation[0] << ", "
                      << node.translation[1] << ", " << node.translation[2] << "]" << std::endl;
        }
        if (!node.rotation.empty()) {
            std::cout << "    rotation: [" << node.rotation[0] << ", " << node.rotation[1]
                      << ", " << node.rotation[2] << ", " << node.rotation[3] << "]" << std::endl;
        }
        if (!node.scale.empty()) {
            std::cout << "    scale: [" << node.scale[0] << ", "
                      << node.scale[1] << ", " << node.scale[2] << "]" << std::endl;
        }
        if (!node.matrix.empty()) {
            std::cout << "    HAS MATRIX TRANSFORM (16 values)" << std::endl;
        }
    }

    if (!model.skins.empty()) {
        const auto& skin = model.skins[0];
        outData.skeleton.hasSkeleton = true;
        std::cout << "\n[GLB] Found skeleton with " << skin.joints.size() << " bones" << std::endl;
        std::cout << "[GLB] Skin skeleton root node: " << skin.skeleton << std::endl;

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

        // Debug: Print first few bones
        std::cout << "\n[GLB] Bone data (first 5):" << std::endl;
        for (size_t i = 0; i < std::min(size_t(5), outData.skeleton.bones.size()); ++i) {
            const auto& bone = outData.skeleton.bones[i];
            std::cout << "  Bone " << i << ": \"" << bone.name << "\" parent=" << bone.parentIndex << std::endl;
            std::cout << "    trans: [" << bone.translation[0] << ", " << bone.translation[1] << ", " << bone.translation[2] << "]" << std::endl;
            std::cout << "    rot: [" << bone.rotation[0] << ", " << bone.rotation[1] << ", " << bone.rotation[2] << ", " << bone.rotation[3] << "]" << std::endl;
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

    // Helper to apply quaternion rotation to a vector
    auto rotateByQuat = [](float& x, float& y, float& z, const float* q) {
        // q = [x, y, z, w]
        float qx = q[0], qy = q[1], qz = q[2], qw = q[3];

        // v' = q * v * q^-1
        // Optimized formula:
        float tx = 2.0f * (qy * z - qz * y);
        float ty = 2.0f * (qz * x - qx * z);
        float tz = 2.0f * (qx * y - qy * x);

        float nx = x + qw * tx + (qy * tz - qz * ty);
        float ny = y + qw * ty + (qz * tx - qx * tz);
        float nz = z + qw * tz + (qx * ty - qy * tx);

        x = nx; y = ny; z = nz;
    };

    // Helper to check if mesh name is a collision mesh (UE naming convention)
    auto isCollisionMesh = [](const std::string& name) -> int {
        // Returns: 0=not collision, 1=box, 2=sphere, 3=capsule, 4=convex mesh
        if (name.substr(0, 4) == "UBX_") return 1;
        if (name.substr(0, 4) == "USP_") return 2;
        if (name.substr(0, 4) == "UCP_") return 3;
        if (name.substr(0, 4) == "UCX_") return 4;
        return 0;
    };

    // Helper to compute bounding box from vertices
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

    // Process each mesh - keep them as separate parts with their own materials
    for (size_t meshIdx = 0; meshIdx < model.meshes.size(); ++meshIdx) {
        const auto& mesh = model.meshes[meshIdx];

        // Check if this is a collision mesh
        int collisionType = isCollisionMesh(mesh.name);

        for (size_t primIdx = 0; primIdx < mesh.primitives.size(); ++primIdx) {
            const auto& prim = mesh.primitives[primIdx];
            if (prim.mode != TINYGLTF_MODE_TRIANGLES) continue;

            // Handle collision mesh
            if (collisionType > 0) {
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

                // Read all vertices
                std::vector<float> verts(vertCount * 3);
                for (size_t v = 0; v < vertCount; ++v) {
                    verts[v*3] = positions[v*3];
                    verts[v*3+1] = positions[v*3+1];
                    verts[v*3+2] = positions[v*3+2];
                }

                // Read indices
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

                // Compute bounds for shape estimation
                float minX, maxX, minY, maxY, minZ, maxZ;
                computeBounds(verts, minX, maxX, minY, maxY, minZ, maxZ);
                float cx = (minX + maxX) / 2.0f;
                float cy = (minY + maxY) / 2.0f;
                float cz = (minZ + maxZ) / 2.0f;

                DAOModelData::CollisionShape shape;
                shape.name = mesh.name;
                shape.posX = cx; shape.posY = cy; shape.posZ = cz;

                switch (collisionType) {
                    case 1: // Box - UBX_
                        shape.type = DAOModelData::CollisionType::Box;
                        shape.boxX = (maxX - minX) / 2.0f;
                        shape.boxY = (maxY - minY) / 2.0f;
                        shape.boxZ = (maxZ - minZ) / 2.0f;
                        std::cout << "[GLB] Collision box: " << mesh.name << " size=("
                                  << shape.boxX*2 << ", " << shape.boxY*2 << ", " << shape.boxZ*2 << ")" << std::endl;
                        break;
                    case 2: // Sphere - USP_
                        shape.type = DAOModelData::CollisionType::Sphere;
                        shape.radius = std::max({maxX-minX, maxY-minY, maxZ-minZ}) / 2.0f;
                        std::cout << "[GLB] Collision sphere: " << mesh.name << " radius=" << shape.radius << std::endl;
                        break;
                    case 3: // Capsule - UCP_
                        shape.type = DAOModelData::CollisionType::Capsule;
                        // Capsule: height is along Z, radius from X/Y
                        shape.height = maxZ - minZ;
                        shape.radius = std::max(maxX-minX, maxY-minY) / 2.0f;
                        std::cout << "[GLB] Collision capsule: " << mesh.name << " radius=" << shape.radius
                                  << " height=" << shape.height << std::endl;
                        break;
                    case 4: // Convex mesh - UCX_
                        shape.type = DAOModelData::CollisionType::Mesh;
                        // Store raw mesh data, centered at origin
                        for (size_t i = 0; i < verts.size(); i += 3) {
                            shape.meshVerts.push_back(verts[i] - cx);
                            shape.meshVerts.push_back(verts[i+1] - cy);
                            shape.meshVerts.push_back(verts[i+2] - cz);
                        }
                        shape.meshIndices = indices;
                        std::cout << "[GLB] Collision mesh: " << mesh.name << " verts=" << vertCount
                                  << " tris=" << indices.size()/3 << std::endl;
                        break;
                }

                outData.collisionShapes.push_back(shape);
                continue; // Skip adding to regular mesh parts
            }

            DAOModelData::MeshPart part;
            // Use mesh name, add primitive index only if multiple primitives in same mesh
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

            std::cout << "[GLB] Mesh " << meshIdx << " part " << primIdx << ": \"" << part.name
                      << "\" material: \"" << part.materialName << "\"" << std::endl;

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

            const float* positions = reinterpret_cast<const float*>(getBufferData(posAcc));
            const float* normals = normAcc ? reinterpret_cast<const float*>(getBufferData(normAcc)) : nullptr;
            const float* uvs = uvAcc ? reinterpret_cast<const float*>(getBufferData(uvAcc)) : nullptr;
            const float* tangents = tanAcc ? reinterpret_cast<const float*>(getBufferData(tanAcc)) : nullptr;
            const uint8_t* jointsData = jointsAcc ? getBufferData(jointsAcc) : nullptr;
            const float* weightsData = weightsAcc ? reinterpret_cast<const float*>(getBufferData(weightsAcc)) : nullptr;

            size_t vertexCount = posAcc->count;
            part.vertices.resize(vertexCount);

            for (size_t v = 0; v < vertexCount; ++v) {
                ImportVertex& vert = part.vertices[v];

                vert.x = positions[v * 3];
                vert.y = positions[v * 3 + 1];
                vert.z = positions[v * 3 + 2];

                if (normals) {
                    vert.nx = normals[v * 3];
                    vert.ny = normals[v * 3 + 1];
                    vert.nz = normals[v * 3 + 2];
                } else {
                    vert.nx = 0; vert.ny = 1; vert.nz = 0;
                }

                if (uvs) {
                    vert.u = uvs[v * 2];
                    vert.v = uvs[v * 2 + 1];
                } else {
                    vert.u = 0; vert.v = 0;
                }

                if (tangents) {
                    vert.tx = tangents[v * 4];
                    vert.ty = tangents[v * 4 + 1];
                    vert.tz = tangents[v * 4 + 2];
                    vert.tw = tangents[v * 4 + 3];
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

                if (part.hasSkinning && jointsData && weightsData) {
                    if (jointsAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        vert.boneIndices[0] = jointsData[v * 4 + 0];
                        vert.boneIndices[1] = jointsData[v * 4 + 1];
                        vert.boneIndices[2] = jointsData[v * 4 + 2];
                        vert.boneIndices[3] = jointsData[v * 4 + 3];
                    } else if (jointsAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        const uint16_t* joints16 = reinterpret_cast<const uint16_t*>(jointsData);
                        vert.boneIndices[0] = joints16[v * 4 + 0];
                        vert.boneIndices[1] = joints16[v * 4 + 1];
                        vert.boneIndices[2] = joints16[v * 4 + 2];
                        vert.boneIndices[3] = joints16[v * 4 + 3];
                    }

                    vert.boneWeights[0] = weightsData[v * 4 + 0];
                    vert.boneWeights[1] = weightsData[v * 4 + 1];
                    vert.boneWeights[2] = weightsData[v * 4 + 2];
                    vert.boneWeights[3] = weightsData[v * 4 + 3];
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
                std::cout << "[GLB] Mesh '" << part.name << "' uses " << part.bonesUsed.size() << " bones" << std::endl;
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

    // Debug: Print mesh bounding box and check orientation
    float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
    float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;
    for (const auto& part : outData.parts) {
        for (const auto& v : part.vertices) {
            minX = std::min(minX, v.x); maxX = std::max(maxX, v.x);
            minY = std::min(minY, v.y); maxY = std::max(maxY, v.y);
            minZ = std::min(minZ, v.z); maxZ = std::max(maxZ, v.z);
        }
    }

    float xSize = maxX - minX;
    float ySize = maxY - minY;
    float zSize = maxZ - minZ;

    std::cout << "\n[GLB] Mesh bounding box:" << std::endl;
    std::cout << "  X: [" << minX << " to " << maxX << "] size=" << xSize << std::endl;
    std::cout << "  Y: [" << minY << " to " << maxY << "] size=" << ySize << std::endl;
    std::cout << "  Z: [" << minZ << " to " << maxZ << "] size=" << zSize << std::endl;

    // Dragon Age expects Z-up coordinate system
    // Check if model needs rotation correction
    bool needsRotation = false;
    float correctionQuat[4] = {0, 0, 0, 1}; // Identity

    if (zSize > xSize * 1.5f && zSize > ySize * 1.5f) {
        // Z is tallest - model is already Z-up (correct for DAO)
        std::cout << "  [OK] Model is Z-up (correct orientation)" << std::endl;
    } else if (ySize > xSize * 1.5f && ySize > zSize * 1.5f) {
        // Y is tallest - model is Y-up (typical GLB from Blender)
        // Need to rotate +90 degrees around X to convert Y-up to Z-up
        std::cout << "  [FIX] Model is Y-up, applying +90° X rotation for Z-up" << std::endl;
        // Quaternion for +90° around X: [sin(45°), 0, 0, cos(45°)] = [0.707, 0, 0, 0.707]
        correctionQuat[0] = 0.7071068f; // x
        correctionQuat[1] = 0.0f;       // y
        correctionQuat[2] = 0.0f;       // z
        correctionQuat[3] = 0.7071068f; // w
        needsRotation = true;
    } else if (xSize > ySize * 1.5f && xSize > zSize * 1.5f) {
        // X is tallest - model is lying on its side
        std::cout << "  [WARNING] Model has X as tallest axis (unusual orientation)" << std::endl;
    } else {
        // No dominant axis - might be a prop or non-humanoid, leave as-is
        std::cout << "  [INFO] No dominant vertical axis, keeping original orientation" << std::endl;
    }

    // Apply rotation correction if needed
    if (needsRotation) {
        for (auto& part : outData.parts) {
            for (auto& v : part.vertices) {
                rotateByQuat(v.x, v.y, v.z, correctionQuat);
                rotateByQuat(v.nx, v.ny, v.nz, correctionQuat);
                rotateByQuat(v.tx, v.ty, v.tz, correctionQuat);
            }
        }
        std::cout << "  [APPLIED] Rotation correction applied to all vertices" << std::endl;
    }

    std::cout << "=== END GLB DEBUG ===\n" << std::endl;

    return !outData.parts.empty();
}

bool DAOImporter::WriteMSHXml(const fs::path& outputPath, const DAOModelData& model) {
    std::ofstream out(outputPath);
    if (!out) return false;

    out << std::fixed << std::setprecision(6);

    std::cout << "[MSH XML] Writing " << model.parts.size() << " mesh groups" << std::endl;

    out << "<?xml version=\"1.0\" ?>\n";
    out << "<ModelMeshData Name=\"" << model.name << ".MSH\" Version=\"1\">\n";

    // Write each part as a separate MeshGroup
    for (size_t partIdx = 0; partIdx < model.parts.size(); ++partIdx) {
        const auto& part = model.parts[partIdx];
        size_t vertCount = part.vertices.size();
        size_t idxCount = part.indices.size();

        std::cout << "[MSH XML] Part " << partIdx << ": \"" << part.name << "\" verts=" << vertCount
                  << " indices=" << idxCount << " skinning=" << (part.hasSkinning ? "yes" : "no") << std::endl;

        out << "<MeshGroup Name=\"" << part.name << "\" Optimize=\"All\">\n";

        out << "<Data ElementCount=\"" << vertCount << "\" Semantic=\"POSITION\" Type=\"Float4\">\n<![CDATA[\n";
        for (const auto& v : part.vertices)
            out << v.x << " " << v.y << " " << v.z << " 1.0\n";
        out << "]]>\n</Data>\n";

        out << "<Data ElementCount=\"" << vertCount << "\" Semantic=\"TEXCOORD\" Type=\"Float2\">\n<![CDATA[\n";
        for (const auto& v : part.vertices)
            out << v.u << " " << (1.0f - v.v) << "\n";
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

            out << indent << "<Node Name=\"" << bone.name << "\" BoneIndex=\"" << bone.index << "\">\n";
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

            // Children nodes are nested directly inside parent node
            for (int child : children) writeBone(child, depth + 1);

            out << indent << "</Node>\n";
        };

        // Check if model already has a root GOB bone
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
            // Model has GOB - write skeleton starting from GOB
            writeBone(gobBoneIdx, 1);
        } else {
            // No GOB - create wrapper
            out << "  <Node Name=\"GOB\" SoundMaterialType=\"0\">\n";
            out << "    <Translation>0 0 0</Translation>\n";
            out << "    <Rotation>0 0 0 1</Rotation>\n";

            for (size_t i = 0; i < model.skeleton.bones.size(); ++i) {
                if (model.skeleton.bones[i].parentIndex == -1) {
                    writeBone(static_cast<int>(i), 2);
                }
            }
        }

        int meshIndent = hasGOB ? 2 : 4;
        std::string meshIndentStr(meshIndent, ' ');

        // Output one NodeMesh per mesh part (each with its own material)
        for (size_t partIdx = 0; partIdx < model.parts.size(); ++partIdx) {
            const auto& part = model.parts[partIdx];

            // Get bones used by this specific part
            std::set<int> partBonesUsed(part.bonesUsed.begin(), part.bonesUsed.end());

            out << meshIndentStr << "<NodeMesh Name=\"" << part.name << "\" ";
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
            out << meshIndentStr << "  <Translation>0 0 0</Translation>\n";
            out << meshIndentStr << "  <Rotation>0 0 0 1</Rotation>\n";
            out << meshIndentStr << "</NodeMesh>\n";
        }

        if (!hasGOB) {
            out << "  </Node>\n";
        }
    } else {
        // Non-skinned mesh - output one NodeMesh per part
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

enum class ERFVersion { Unknown, V20, V22, V30 };

bool DAOImporter::RepackERF(const std::string& erfPath, const std::map<std::string, std::vector<uint8_t>>& newFiles) {
    std::cout << "\n[RepackERF] === " << fs::path(erfPath).filename().string() << " ===" << std::endl;
    std::cout << "[RepackERF] Adding " << newFiles.size() << " files" << std::endl;

    if (!fs::exists(erfPath)) {
        std::cerr << "[RepackERF] ERROR: File does not exist!" << std::endl;
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
        std::cerr << "[RepackERF] ERROR: Invalid magic" << std::endl;
        return false;
    }

    ERFVersion erfVer = ERFVersion::Unknown;
    if (version == "V2.0") erfVer = ERFVersion::V20;
    else if (version == "V2.2") erfVer = ERFVersion::V22;

    if (erfVer != ERFVersion::V20 && erfVer != ERFVersion::V22) {
        std::cerr << "[RepackERF] ERROR: Unsupported version: " << version << std::endl;
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

    std::cout << "[RepackERF] SUCCESS (" << newErfData.size() << " bytes)" << std::endl;
    return true;
}

// Generate PHY file (GFF format) from collision shapes
std::vector<uint8_t> DAOImporter::GeneratePHY(const DAOModelData& model) {
    if (model.collisionShapes.empty()) return {};

    // GFF V4.0 format helper functions
    auto writeU32 = [](std::vector<uint8_t>& buf, uint32_t val) {
        buf.push_back(val & 0xFF);
        buf.push_back((val >> 8) & 0xFF);
        buf.push_back((val >> 16) & 0xFF);
        buf.push_back((val >> 24) & 0xFF);
    };
    auto writeU16 = [](std::vector<uint8_t>& buf, uint16_t val) {
        buf.push_back(val & 0xFF);
        buf.push_back((val >> 8) & 0xFF);
    };
    auto writeFloat = [](std::vector<uint8_t>& buf, float val) {
        uint32_t bits;
        std::memcpy(&bits, &val, 4);
        buf.push_back(bits & 0xFF);
        buf.push_back((bits >> 8) & 0xFF);
        buf.push_back((bits >> 16) & 0xFF);
        buf.push_back((bits >> 24) & 0xFF);
    };

    std::vector<uint8_t> data;

    // GFF V4.0 Header (16 bytes)
    data.push_back('G'); data.push_back('F'); data.push_back('F'); data.push_back(' ');
    data.push_back('V'); data.push_back('4'); data.push_back('.'); data.push_back('0');
    data.push_back('P'); data.push_back('C'); data.push_back(' '); data.push_back(' ');
    writeU32(data, 0); // Platform flags

    // For a minimal PHY, we need struct/field definitions
    // This is a simplified version - full GFF is complex

    // For now, just log that we're generating collision data
    std::cout << "[PHY] Generating collision data for " << model.collisionShapes.size() << " shapes" << std::endl;
    for (const auto& shape : model.collisionShapes) {
        switch (shape.type) {
            case DAOModelData::CollisionType::Box:
                std::cout << "  - Box: " << shape.name << " at (" << shape.posX << "," << shape.posY << "," << shape.posZ
                          << ") size=(" << shape.boxX*2 << "," << shape.boxY*2 << "," << shape.boxZ*2 << ")" << std::endl;
                break;
            case DAOModelData::CollisionType::Sphere:
                std::cout << "  - Sphere: " << shape.name << " at (" << shape.posX << "," << shape.posY << "," << shape.posZ
                          << ") radius=" << shape.radius << std::endl;
                break;
            case DAOModelData::CollisionType::Capsule:
                std::cout << "  - Capsule: " << shape.name << " at (" << shape.posX << "," << shape.posY << "," << shape.posZ
                          << ") radius=" << shape.radius << " height=" << shape.height << std::endl;
                break;
            case DAOModelData::CollisionType::Mesh:
                std::cout << "  - Mesh: " << shape.name << " at (" << shape.posX << "," << shape.posY << "," << shape.posZ
                          << ") verts=" << shape.meshVerts.size()/3 << " tris=" << shape.meshIndices.size()/3 << std::endl;
                break;
        }
    }

    // TODO: Full GFF PHY generation requires proper struct/field encoding
    // For now, return empty - this is a placeholder for future implementation
    // The collision shapes are stored in modelData.collisionShapes and can be
    // used by the game's physics system if a proper PHY writer is implemented

    return {}; // Return empty for now - PHY generation is complex
}