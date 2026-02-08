#pragma once
#include "ui.h"
#include "types.h"
#include "mmh_loader.h"
#include "animation.h"
#include "erf.h"
#include "export.h"
#include "dds_loader.h"
#include "Shaders/d3d_context.h"
#include "Gff.h"
#include "Gff32.h"
#include "GffViewer.h"
#include "gda.h"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <mmsystem.h>
#endif
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "Fonts/IconsFontAwesome6.h"
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
void saveSettings(const AppState& state);
void loadSettings(AppState& state);
void scanAudioFiles(AppState& state);
bool extractFSB4toMP3(const std::string& fsbPath, const std::string& outPath);
std::vector<uint8_t> extractFSB4toMP3Data(const std::string& fsbPath);
std::string getFSB4SampleName(const std::string& fsbPath);
std::vector<FSBSampleInfo> parseFSB4Samples(const std::string& fsbPath);
std::vector<uint8_t> extractFSB4SampleToWav(const std::string& fsbPath, int sampleIndex);
void buildMaterialCache(AppState& state, float startProgress = 0.0f, float endProgress = 1.0f);
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
bool isModelFile(const std::string& name);
bool isMaoFile(const std::string& name);
bool isPhyFile(const std::string& name);
std::vector<std::pair<std::string, std::string>> findAssociatedHeads(AppState& state, const std::string& bodyMsh);
std::vector<std::pair<std::string, std::string>> findAssociatedEyes(AppState& state, const std::string& bodyMsh);
void loadMeshDatabase(AppState& state);
bool loadModelFromEntry(AppState& state, const ERFEntry& entry);
bool mergeModelEntry(AppState& state, const ERFEntry& entry);
bool mergeModelByName(AppState& state, const std::string& modelName,
                      float px, float py, float pz,
                      float qx, float qy, float qz, float qw,
                      float scale);
void finalizeLevelMaterials(AppState& state);
void buildErfIndex(AppState& state);
void clearPropCache();
void ensureBaseErfsLoaded(AppState& state);
std::vector<uint8_t> readFromErfs(const std::vector<std::unique_ptr<ERFFile>>& erfs, const std::string& name);
std::vector<uint8_t> readFromCache(AppState& state, const std::string& name, const std::string& ext);
uint32_t loadTexByNameCached(AppState& state, const std::string& texName,
                             std::vector<uint8_t>* rgbaOut = nullptr, int* wOut = nullptr, int* hOut = nullptr);
uint32_t loadTexByName(AppState& state, const std::string& texName,
                       std::vector<uint8_t>* rgbaOut = nullptr, int* wOut = nullptr, int* hOut = nullptr);
std::vector<uint8_t> loadTextureData(AppState& state, const std::string& texName);
void loadAndMergeHead(AppState& state, const std::string& headMshFile);
void drawVirtualList(int itemCount, std::function<void(int)> renderItem);
void drawMeshBrowserWindow(AppState& state);
void drawImportMenu(AppState& state);
void drawBrowserWindow(AppState& state);
void markModelAsImported(const std::string& modelName);
void drawRenderSettingsWindow(AppState& state);
void drawKeybindsWindow(AppState& state);
void drawMaoViewer(AppState& state);
void drawAudioPlayer(AppState& state);
void drawTexturePreview(AppState& state);
void drawUvViewer(AppState& state);
void drawAnimWindow(AppState& state, ImGuiIO& io);
void drawHeightmapViewer(AppState& state);
void drawGffViewerWindow(GffViewerState& state);
void filterEncryptedErfs(AppState& state);
void buildCharacterLists(AppState& state);
void loadCharacterModel(AppState& state);
void drawCharacterDesigner(AppState& state, ImGuiIO& io);
void preloadCharacterData(AppState& state);
void draw2DAEditorWindow(AppState& state);