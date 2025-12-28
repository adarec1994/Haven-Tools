#include "types.h"
#include <filesystem>
#include <algorithm>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace fs = std::filesystem;

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

bool isMshFile(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    size_t dotPos = lower.rfind('.');
    if (dotPos != std::string::npos) {
        std::string ext = lower.substr(dotPos);
        return ext == ".msh";
    }
    return false;
}

#include "erf.h"
#include <iostream>
#include <set>

void dumpAllMshFileNames(const std::vector<std::string>& erfFiles) {
    std::cout << "\n=== ALL MSH FILES ===" << std::endl;
    std::set<std::string> allMsh;
    for (const auto& erfPath : erfFiles) {
        ERFFile erf;
        if (erf.open(erfPath)) {
            for (const auto& entry : erf.entries()) {
                if (isMshFile(entry.name)) {
                    allMsh.insert(entry.name);
                }
            }
        }
    }
    for (const auto& name : allMsh) {
        std::cout << name << std::endl;
    }
    std::cout << "=== TOTAL: " << allMsh.size() << " MSH files ===" << std::endl;
}