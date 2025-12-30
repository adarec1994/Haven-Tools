#pragma once
#include "ui.h"
#include "types.h"
#include "mmh_loader.h"
#include "animation.h"
#include "erf.h"
#include "export.h"
#include "dds_loader.h"
#include "Gff.h"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <mmsystem.h>
#endif
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "ImGuiFileDialog.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <set>
#include <map>
#include <functional>

namespace fs = std::filesystem;

// Settings (ui_settings.cpp)
void saveSettings(const AppState& state);
void loadSettings(AppState& state);

// Audio (ui_audio.cpp)
void scanAudioFiles(AppState& state);
bool extractFSB4toMP3(const std::string& fsbPath, const std::string& outPath);
std::vector<uint8_t> extractFSB4toMP3Data(const std::string& fsbPath);
std::string getFSB4SampleName(const std::string& fsbPath);
std::vector<FSBSampleInfo> parseFSB4Samples(const std::string& fsbPath);
std::vector<uint8_t> extractFSB4SampleToWav(const std::string& fsbPath, int sampleIndex);
bool saveFSB4SampleToWav(const std::string& fsbPath, int sampleIndex, const std::string& outPath);
void stopAudio();
bool playAudioFromMemory(const std::vector<uint8_t>& mp3Data);
bool playWavFromMemory(const std::vector<uint8_t>& wavData);
bool playAudio(const std::string& mp3Path);
bool isAudioPlaying();
int getAudioLength();
int getAudioPosition();
void setAudioPosition(int ms);
void pauseAudio();
void resumeAudio();

// Helpers (ui_helpers.cpp)
void loadMeshDatabase(AppState& state);
std::vector<std::pair<std::string, std::string>> findAssociatedHeads(AppState& state, const std::string& bodyMsh);
std::vector<std::pair<std::string, std::string>> findAssociatedEyes(AppState& state, const std::string& bodyMsh);
std::vector<uint8_t> readFromCache(AppState& state, const std::string& name, const std::string& ext);
std::vector<uint8_t> readFromErfs(const std::vector<std::unique_ptr<ERFFile>>& erfs, const std::string& name);
uint32_t loadTexByNameCached(AppState& state, const std::string& texName, 
                             std::vector<uint8_t>* rgbaOut = nullptr, int* wOut = nullptr, int* hOut = nullptr);
uint32_t loadTexByName(AppState& state, const std::string& texName, 
                       std::vector<uint8_t>* rgbaOut = nullptr, int* wOut = nullptr, int* hOut = nullptr);
std::vector<uint8_t> loadTextureData(AppState& state, const std::string& texName);
void loadAndMergeHead(AppState& state, const std::string& headMshFile);

// Browser windows (ui_browser.cpp)
void drawMeshBrowserWindow(AppState& state);
void drawBrowserWindow(AppState& state);

// UI Windows (ui_windows.cpp)
void drawRenderSettingsWindow(AppState& state);
void drawMaoViewer(AppState& state);
void drawAudioPlayer(AppState& state);
void drawTexturePreview(AppState& state);
void drawUvViewer(AppState& state);
void drawAnimWindow(AppState& state, ImGuiIO& io);
void drawFSBBrowserWindow(AppState& state);

// Character Designer (ui_character.cpp)
void filterEncryptedErfs(AppState& state);
void buildCharacterLists(AppState& state);
void loadCharacterModel(AppState& state);
void drawCharacterDesigner(AppState& state, ImGuiIO& io);

// Main UI (ui_main.cpp)
void drawSplashScreen(AppState& state, int displayW, int displayH);
void preloadErfs(AppState& state);

// Splash state
extern bool showSplash;
