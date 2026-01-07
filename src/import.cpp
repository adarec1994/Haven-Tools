#include "import.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <iomanip>
#include <cmath>

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

DAOGraphicsTools::~DAOGraphicsTools() {
    Cleanup();
}

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

    std::cout << "[DAOTools] Exe exists: " << fs::exists(exePath) << std::endl;
    std::cout << "[DAOTools] Exe size: " << fs::file_size(exePath) << std::endl;

#ifdef _WIN32
    std::cout << "[DAOTools] Running: " << cmdLine << std::endl;

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hStdOutRead, hStdOutWrite;
    HANDLE hStdErrRead, hStdErrWrite;
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
    std::cout << "[DAOTools] Working dir: " << workDir << std::endl;

    std::cout << "[DAOTools] Files in working dir:" << std::endl;
    for (const auto& entry : fs::directory_iterator(workDir)) {
        std::cout << "  " << entry.path().filename() << " (" << entry.file_size() << " bytes)" << std::endl;
    }

    if (CreateProcessA(NULL, &cmd[0], NULL, NULL, TRUE,
                       0, NULL, workDir.string().c_str(), &si, &pi)) {
        CloseHandle(hStdOutWrite);
        CloseHandle(hStdErrWrite);

        DWORD waitResult = WaitForSingleObject(pi.hProcess, 30000);
        std::cout << "[DAOTools] Wait result: " << waitResult << " (0=WAIT_OBJECT_0, 258=TIMEOUT)" << std::endl;

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

        if (!stdoutStr.empty()) {
            std::cout << "[DAOTools] STDOUT: " << stdoutStr << std::endl;
        }
        if (!stderrStr.empty()) {
            std::cout << "[DAOTools] STDERR: " << stderrStr << std::endl;
        }

        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        std::cout << "[DAOTools] Exit code: " << exitCode << std::endl;

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        std::cout << "[DAOTools] Files after processing:" << std::endl;
        for (const auto& entry : fs::directory_iterator(workDir)) {
            std::cout << "  " << entry.path().filename() << " (" << entry.file_size() << " bytes)" << std::endl;
        }

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

    // FIXED: Use correct command line from MaxScript:
    // GraphicsProcessorMSH.exe -platform pc mmdtogff <filename>
    std::string cmdLine = "\"" + exePath.string() + "\" -platform pc mmdtogff \"" + localXml.string() + "\"";

    if (!RunProcessorWithCmd(exePath, cmdLine)) {
        return {};
    }

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

    if (!RunProcessor(exePath, localXml)) {
        return {};
    }

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
    if (m_progressCallback) {
        m_progressCallback(progress, status);
    }
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

    std::cout << "\n[Import] Updating ERF files..." << std::endl;

    ReportProgress(0.7f, "Updating modelmeshdata.erf...");
    bool ok1 = RepackERF(meshErf, meshFiles);

    ReportProgress(0.8f, "Updating modelhierarchies.erf...");
    bool ok2 = RepackERF(hierErf, hierFiles);

    ReportProgress(0.85f, "Updating materialobjects.erf...");
    bool ok3 = RepackERF(matErf, matFiles);

    bool ok4 = true;
    if (!texErf.empty() && !texFiles.empty()) {
        ReportProgress(0.9f, "Updating texturepack.erf...");
        ok4 = RepackERF(texErf, texFiles);
    }

    ReportProgress(1.0f, ok1 && ok2 && ok3 && ok4 ? "Import complete!" : "Import failed!");
    std::cout << "\n[Import] " << (ok1 && ok2 && ok3 && ok4 ? "SUCCESS" : "FAILED") << std::endl;
    return ok1 && ok2 && ok3 && ok4;
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

    for (const auto& mesh : model.meshes) {
        for (const auto& prim : mesh.primitives) {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES) continue;

            DAOModelData::MeshPart part;
            part.name = mesh.name.empty() ? outData.name : CleanName(mesh.name);

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

            for (const auto& attr : prim.attributes) {
                if (attr.first == "POSITION") posAcc = getAccessor(attr.second);
                else if (attr.first == "NORMAL") normAcc = getAccessor(attr.second);
                else if (attr.first == "TEXCOORD_0") uvAcc = getAccessor(attr.second);
                else if (attr.first == "TANGENT") tanAcc = getAccessor(attr.second);
            }

            if (!posAcc) continue;

            const float* positions = reinterpret_cast<const float*>(getBufferData(posAcc));
            const float* normals = normAcc ? reinterpret_cast<const float*>(getBufferData(normAcc)) : nullptr;
            const float* uvs = uvAcc ? reinterpret_cast<const float*>(getBufferData(uvAcc)) : nullptr;
            const float* tangents = tanAcc ? reinterpret_cast<const float*>(getBufferData(tanAcc)) : nullptr;

            size_t vertexCount = posAcc->count;
            part.vertices.resize(vertexCount);

            for (size_t v = 0; v < vertexCount; ++v) {
                part.vertices[v].x = positions[v * 3];
                part.vertices[v].y = positions[v * 3 + 1];
                part.vertices[v].z = positions[v * 3 + 2];

                if (normals) {
                    part.vertices[v].nx = normals[v * 3];
                    part.vertices[v].ny = normals[v * 3 + 1];
                    part.vertices[v].nz = normals[v * 3 + 2];
                } else {
                    part.vertices[v].nx = 0;
                    part.vertices[v].ny = 1;
                    part.vertices[v].nz = 0;
                }

                if (uvs) {
                    part.vertices[v].u = uvs[v * 2];
                    part.vertices[v].v = uvs[v * 2 + 1];
                } else {
                    part.vertices[v].u = 0;
                    part.vertices[v].v = 0;
                }

                if (tangents) {
                    part.vertices[v].tx = tangents[v * 4];
                    part.vertices[v].ty = tangents[v * 4 + 1];
                    part.vertices[v].tz = tangents[v * 4 + 2];
                    part.vertices[v].tw = tangents[v * 4 + 3];
                } else {
                    // Generate a default tangent perpendicular to normal
                    // Use the cross product of normal with an arbitrary axis
                    float nx = part.vertices[v].nx;
                    float ny = part.vertices[v].ny;
                    float nz = part.vertices[v].nz;

                    // Choose axis least aligned with normal
                    float ax = 1.0f, ay = 0.0f, az = 0.0f;
                    if (std::abs(nx) > 0.9f) {
                        ax = 0.0f; ay = 1.0f; az = 0.0f;
                    }

                    // Cross product: tangent = axis x normal
                    float tx = ay * nz - az * ny;
                    float ty = az * nx - ax * nz;
                    float tz = ax * ny - ay * nx;

                    // Normalize
                    float len = std::sqrt(tx*tx + ty*ty + tz*tz);
                    if (len > 0.0001f) {
                        tx /= len; ty /= len; tz /= len;
                    } else {
                        tx = 1.0f; ty = 0.0f; tz = 0.0f;
                    }

                    part.vertices[v].tx = tx;
                    part.vertices[v].ty = ty;
                    part.vertices[v].tz = tz;
                    part.vertices[v].tw = 1.0f;
                }
            }

            if (prim.indices >= 0) {
                const auto* idxAcc = getAccessor(prim.indices);
                const uint8_t* idxData = getBufferData(idxAcc);
                size_t idxCount = idxAcc->count;
                part.indices.resize(idxCount);

                if (idxAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    const uint16_t* indices16 = reinterpret_cast<const uint16_t*>(idxData);
                    for (size_t i = 0; i < idxCount; ++i) {
                        part.indices[i] = indices16[i];
                    }
                } else if (idxAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    const uint32_t* indices32 = reinterpret_cast<const uint32_t*>(idxData);
                    for (size_t i = 0; i < idxCount; ++i) {
                        part.indices[i] = indices32[i];
                    }
                } else if (idxAcc->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                    for (size_t i = 0; i < idxCount; ++i) {
                        part.indices[i] = idxData[i];
                    }
                }
            }

            outData.parts.push_back(part);
        }
    }

    return !outData.parts.empty();
}

bool DAOImporter::WriteMSHXml(const fs::path& outputPath, const DAOModelData& model) {
    std::ofstream out(outputPath);
    if (!out) return false;

    // Set up consistent floating-point formatting - write directly to file
    out << std::fixed << std::setprecision(6);

    size_t totalVerts = 0;
    size_t totalIndices = 0;
    for (const auto& part : model.parts) {
        totalVerts += part.vertices.size();
        totalIndices += part.indices.size();
    }

    std::cout << "[MSH XML] Writing to: " << outputPath << std::endl;
    std::cout << "[MSH XML] Model name: " << model.name << std::endl;
    std::cout << "[MSH XML] Total vertices: " << totalVerts << std::endl;
    std::cout << "[MSH XML] Total indices: " << totalIndices << std::endl;
    std::cout << "[MSH XML] Parts: " << model.parts.size() << std::endl;

    // Write XML header - match MaxScript format exactly (line 936-941 of DAOModelExport.ms)
    out << "<?xml version=\"1.0\" ?>\n";
    out << "<ModelMeshData Name=\"" << model.name << ".MSH\" Version=\"1\">\n";
    out << "<MeshGroup Name=\"" << model.name << "\" Optimize=\"All\">\n";

    // POSITION - format: "X Y Z 1.0\n" per MaxScript line 1369
    out << "<Data ElementCount=\"" << totalVerts << "\" Semantic=\"POSITION\" Type=\"Float4\">\n";
    out << "<![CDATA[\n";
    for (const auto& part : model.parts) {
        for (const auto& v : part.vertices) {
            out << v.x << " " << v.y << " " << v.z << " 1.0\n";
        }
    }
    out << "]]>\n";
    out << "</Data>\n";

    // TEXCOORD - format: "U V\n" per MaxScript line 1388 (V is flipped: 1-V)
    out << "<Data ElementCount=\"" << totalVerts << "\" Semantic=\"TEXCOORD\" Type=\"Float2\">\n";
    out << "<![CDATA[\n";
    for (const auto& part : model.parts) {
        for (const auto& v : part.vertices) {
            float flippedV = 1.0f - v.v;
            out << v.u << " " << flippedV << "\n";
        }
    }
    out << "]]>\n";
    out << "</Data>\n";

    // TANGENT - format: "X Y Z 1.0\n" per MaxScript line 1441
    out << "<Data ElementCount=\"" << totalVerts << "\" Semantic=\"TANGENT\" Type=\"Float4\">\n";
    out << "<![CDATA[\n";
    for (const auto& part : model.parts) {
        for (const auto& v : part.vertices) {
            out << v.tx << " " << v.ty << " " << v.tz << " 1.0\n";
        }
    }
    out << "]]>\n";
    out << "</Data>\n";

    // BINORMAL - format: "X Y Z 1.0\n" per MaxScript line 1473
    // Calculated as cross(normal, tangent) * handedness
    out << "<Data ElementCount=\"" << totalVerts << "\" Semantic=\"BINORMAL\" Type=\"Float4\">\n";
    out << "<![CDATA[\n";
    for (const auto& part : model.parts) {
        for (const auto& v : part.vertices) {
            // binormal = cross(normal, tangent) * handedness
            float bx = v.ny * v.tz - v.nz * v.ty;
            float by = v.nz * v.tx - v.nx * v.tz;
            float bz = v.nx * v.ty - v.ny * v.tx;
            float hand = v.tw; // handedness from tangent w component
            out << (bx * hand) << " " << (by * hand) << " " << (bz * hand) << " 1.0\n";
        }
    }
    out << "]]>\n";
    out << "</Data>\n";

    // NORMAL - format: "X Y Z 1.0\n" per MaxScript line 1497
    out << "<Data ElementCount=\"" << totalVerts << "\" Semantic=\"NORMAL\" Type=\"Float4\">\n";
    out << "<![CDATA[\n";
    for (const auto& part : model.parts) {
        for (const auto& v : part.vertices) {
            out << v.nx << " " << v.ny << " " << v.nz << " 1.0\n";
        }
    }
    out << "]]>\n";
    out << "</Data>\n";

    // Indices - format: "I0 I1 I2\n" per MaxScript line 1610 (one triangle per line)
    out << "<Data IndexCount=\"" << totalIndices << "\" IndexType=\"Index32\" Semantic=\"Indices\">\n";
    out << "<![CDATA[\n";
    uint32_t indexOffset = 0;
    for (const auto& part : model.parts) {
        for (size_t i = 0; i + 2 < part.indices.size(); i += 3) {
            uint32_t i0 = part.indices[i] + indexOffset;
            uint32_t i1 = part.indices[i + 1] + indexOffset;
            uint32_t i2 = part.indices[i + 2] + indexOffset;
            out << i0 << " " << i1 << " " << i2 << "\n";
        }
        indexOffset += static_cast<uint32_t>(part.vertices.size());
    }
    out << "]]>\n";
    out << "</Data>\n";

    out << "</MeshGroup>\n";
    out << "</ModelMeshData>\n";

    out.flush();
    bool success = out.good();
    out.close();

    // Verify the file was written correctly
    if (success) {
        std::ifstream verify(outputPath);
        if (verify) {
            verify.seekg(0, std::ios::end);
            size_t fileSize = verify.tellg();
            std::cout << "[MSH XML] Written " << fileSize << " bytes" << std::endl;

            // Show first few lines for debugging
            verify.seekg(0, std::ios::beg);
            std::string line;
            int lineCount = 0;
            std::cout << "[MSH XML] First 15 lines:" << std::endl;
            while (std::getline(verify, line) && lineCount < 15) {
                std::cout << "  " << line << std::endl;
                lineCount++;
            }
        }
    }

    return success;
}

bool DAOImporter::WriteMMHXml(const fs::path& outputPath, const DAOModelData& model, const std::string& mshFilename) {
    std::ofstream out(outputPath);
    if (!out) return false;

    std::string materialName = model.materials.empty() ? model.name : model.materials[0].name;

    out << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    out << "<ModelHierarchy Name=\"" << model.name << ".mmh\" ModelDataName=\"" << mshFilename << "\">\n";
    out << "  <Node Name=\"GOB\" SoundMaterialType=\"0\">\n";
    out << "    <Translation>0 0 0</Translation>\n";
    out << "    <Rotation>0 0 0 1</Rotation>\n";
    out << "    <Children>\n";
    out << "      <MeshNode Name=\"" << model.name << "\" MeshName=\"" << model.name << "\"";
    out << " MaterialObject=\"" << materialName << "\" CastShadow=\"1\" ReceiveShadow=\"1\">\n";
    out << "        <Translation>0 0 0</Translation>\n";
    out << "        <Rotation>0 0 0 1</Rotation>\n";
    out << "      </MeshNode>\n";
    out << "    </Children>\n";
    out << "  </Node>\n";
    out << "</ModelHierarchy>\n";

    return out.good();
}

std::string DAOImporter::GenerateMAO(const std::string& matName, const std::string& diffuse, const std::string& normal, const std::string& specular) {
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
    for (const auto& [name, data] : newFiles) {
        std::cout << "  + " << name << " (" << data.size() << " bytes)" << std::endl;
    }

    if (!fs::exists(erfPath)) {
        std::cerr << "[RepackERF] ERROR: File does not exist!" << std::endl;
        return false;
    }

    std::vector<uint8_t> erfData;
    {
        std::ifstream in(erfPath, std::ios::binary | std::ios::ate);
        if (!in) {
            std::cerr << "[RepackERF] ERROR: Cannot open file" << std::endl;
            return false;
        }
        size_t size = in.tellg();
        in.seekg(0);
        erfData.resize(size);
        in.read(reinterpret_cast<char*>(erfData.data()), size);
    }
    std::cout << "[RepackERF] Current size: " << erfData.size() << " bytes" << std::endl;

    if (erfData.size() < 16) {
        std::cerr << "[RepackERF] ERROR: File too small" << std::endl;
        return false;
    }

    auto readU32 = [&](size_t offset) -> uint32_t {
        if (offset + 4 > erfData.size()) return 0;
        return *reinterpret_cast<uint32_t*>(&erfData[offset]);
    };

    std::string magic(reinterpret_cast<char*>(erfData.data()), 4);
    std::string version(reinterpret_cast<char*>(erfData.data() + 4), 4);

    if (magic != "ERF ") {
        std::cerr << "[RepackERF] ERROR: Invalid magic: " << magic << std::endl;
        return false;
    }

    ERFVersion erfVer = ERFVersion::Unknown;
    if (version == "V2.0") erfVer = ERFVersion::V20;
    else if (version == "V2.2") erfVer = ERFVersion::V22;
    else if (version == "V3.0") erfVer = ERFVersion::V30;

    std::cout << "[RepackERF] Format: " << version << std::endl;

    if (erfVer != ERFVersion::V20 && erfVer != ERFVersion::V22) {
        std::cerr << "[RepackERF] ERROR: Unsupported version: " << version << std::endl;
        return false;
    }

    uint32_t fileCount = readU32(8);
    std::cout << "[RepackERF] Current entries: " << fileCount << std::endl;

    size_t headerSize = (erfVer == ERFVersion::V22) ? 24 : 16;
    size_t entrySize = (erfVer == ERFVersion::V22) ? 72 : 8;

    struct FileEntry {
        std::string name;
        uint32_t offset;
        uint32_t size;
        uint64_t nameHash;
    };
    std::vector<FileEntry> entries;

    size_t tableOffset = headerSize;
    for (uint32_t i = 0; i < fileCount; ++i) {
        size_t entryOff = tableOffset + i * entrySize;
        if (entryOff + entrySize > erfData.size()) break;

        FileEntry e;
        if (erfVer == ERFVersion::V22) {
            char nameBuf[65] = {0};
            memcpy(nameBuf, &erfData[entryOff], 64);
            e.name = nameBuf;
            e.offset = readU32(entryOff + 64);
            e.size = readU32(entryOff + 68);
        } else {
            e.nameHash = *reinterpret_cast<uint64_t*>(&erfData[entryOff]);
            e.offset = 0;
            e.size = 0;
            e.name = "";
        }
        entries.push_back(e);
    }

    if (erfVer == ERFVersion::V20) {
        size_t offsetTableStart = tableOffset + fileCount * 8;
        for (uint32_t i = 0; i < fileCount; ++i) {
            entries[i].offset = readU32(offsetTableStart + i * 8);
            entries[i].size = readU32(offsetTableStart + i * 8 + 4);
        }
    }

    for (const auto& [name, data] : newFiles) {
        bool found = false;
        std::string lowerName = ToLower(name);
        for (auto& e : entries) {
            if (ToLower(e.name) == lowerName) {
                found = true;
                break;
            }
        }
        if (!found) {
            FileEntry newEntry;
            newEntry.name = name;
            newEntry.offset = 0;
            newEntry.size = (uint32_t)data.size();
            newEntry.nameHash = 0;
            entries.push_back(newEntry);
        }
    }

    std::cout << "[RepackERF] Total after merge: " << entries.size() << std::endl;

    fs::path backupDir = fs::current_path() / "backups";
    fs::create_directories(backupDir);
    fs::path backupPath = backupDir / (fs::path(erfPath).filename().string() + ".bak");

    if (!fs::exists(backupPath)) {
        bool doBackup = true;
        if (m_backupCallback) {
            doBackup = m_backupCallback(fs::path(erfPath).filename().string(), backupDir.string());
        }
        if (doBackup) {
            fs::copy_file(erfPath, backupPath);
            std::cout << "[RepackERF] Created backup: " << backupPath << std::endl;
        }
    } else {
        std::cout << "[RepackERF] Backup exists (skipping)" << std::endl;
    }

    std::vector<uint8_t> newErfData;
    auto writeU32 = [&](uint32_t v) {
        newErfData.push_back(v & 0xFF);
        newErfData.push_back((v >> 8) & 0xFF);
        newErfData.push_back((v >> 16) & 0xFF);
        newErfData.push_back((v >> 24) & 0xFF);
    };
    auto writeU64 = [&](uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            newErfData.push_back((v >> (i * 8)) & 0xFF);
        }
    };

    newErfData.insert(newErfData.end(), {'E', 'R', 'F', ' '});
    newErfData.insert(newErfData.end(), version.begin(), version.end());
    writeU32((uint32_t)entries.size());
    if (erfVer == ERFVersion::V22) {
        writeU32(readU32(12));
        writeU32(readU32(16));
        writeU32(readU32(20));
    } else {
        writeU32(readU32(12));
    }

    size_t tableStart = newErfData.size();
    size_t newEntrySize = (erfVer == ERFVersion::V22) ? 72 : 8;
    size_t tableSize = entries.size() * newEntrySize;
    if (erfVer == ERFVersion::V20) {
        tableSize += entries.size() * 8;
    }

    size_t dataStart = tableStart + tableSize;
    while (dataStart % 16 != 0) dataStart++;

    newErfData.resize(dataStart, 0);

    std::vector<std::pair<uint32_t, uint32_t>> offsets;

    std::cout << "[RepackERF] Final file list (first 10 new entries):" << std::endl;
    int newCount = 0;

    for (auto& e : entries) {
        uint32_t offset = (uint32_t)newErfData.size();
        uint32_t size = 0;

        std::string lowerName = ToLower(e.name);
        auto it = std::find_if(newFiles.begin(), newFiles.end(),
            [&](const auto& p) { return ToLower(p.first) == lowerName; });

        if (it != newFiles.end()) {
            size = (uint32_t)it->second.size();
            newErfData.insert(newErfData.end(), it->second.begin(), it->second.end());
            if (newCount < 10) {
                std::cout << "  [NEW] " << e.name << " (" << size << " bytes)" << std::endl;
                newCount++;
            }
        } else {
            size = e.size;
            if (e.offset + e.size <= erfData.size()) {
                newErfData.insert(newErfData.end(),
                    erfData.begin() + e.offset,
                    erfData.begin() + e.offset + e.size);
            }
        }

        offsets.push_back({offset, size});
    }

    if (erfVer == ERFVersion::V22) {
        for (size_t i = 0; i < entries.size(); ++i) {
            size_t entryOff = tableStart + i * 72;
            char nameBuf[64] = {0};
            strncpy(nameBuf, entries[i].name.c_str(), 63);
            memcpy(&newErfData[entryOff], nameBuf, 64);
            memcpy(&newErfData[entryOff + 64], &offsets[i].first, 4);
            memcpy(&newErfData[entryOff + 68], &offsets[i].second, 4);
        }
    } else {
        for (size_t i = 0; i < entries.size(); ++i) {
            size_t hashOff = tableStart + i * 8;
            memcpy(&newErfData[hashOff], &entries[i].nameHash, 8);

            size_t offsetOff = tableStart + entries.size() * 8 + i * 8;
            memcpy(&newErfData[offsetOff], &offsets[i].first, 4);
            memcpy(&newErfData[offsetOff + 4], &offsets[i].second, 4);
        }
    }

    {
        std::ofstream out(erfPath, std::ios::binary);
        if (!out) {
            std::cerr << "[RepackERF] ERROR: Cannot write output" << std::endl;
            return false;
        }
        out.write(reinterpret_cast<char*>(newErfData.data()), newErfData.size());
    }

    std::cout << "[RepackERF] SUCCESS (" << newErfData.size() << " bytes)" << std::endl;
    return true;
}