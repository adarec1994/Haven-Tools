#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ImGuiFileDialog.h"
#include "erf.h"

namespace fs = std::filesystem;

struct AppState {
    bool showSplash = true;
    std::string selectedFolder;
    std::vector<std::string> erfFiles;
    int selectedErfIndex = -1;
    std::unique_ptr<ERFFile> currentErf;
    int selectedEntryIndex = -1;
    std::string statusMessage;
    std::string extractPath;
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

int main() {
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1280, 720, "ERF Browser", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    AppState state;
    state.extractPath = (fs::path(getExeDir()) / "extracted").string();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (state.showSplash) {
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(300, 150));
            ImGui::Begin("ERF Browser", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

            ImGui::Text("ERF Archive Browser");
            ImGui::Separator();
            ImGui::Spacing();

            float buttonWidth = 200;
            ImGui::SetCursorPosX((300 - buttonWidth) * 0.5f);
            if (ImGui::Button("Open Folder...", ImVec2(buttonWidth, 40))) {
                IGFD::FileDialogConfig config;
                config.path = ".";
                ImGuiFileDialog::Instance()->OpenDialog("ChooseFolder", "Choose Folder", nullptr, config);
            }

            ImGui::End();
        } else {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(io.DisplaySize);
            ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_MenuBar);

            if (ImGui::BeginMenuBar()) {
                if (ImGui::Button("Open Folder")) {
                    IGFD::FileDialogConfig config;
                    config.path = state.selectedFolder.empty() ? "." : state.selectedFolder;
                    ImGuiFileDialog::Instance()->OpenDialog("ChooseFolder", "Choose Folder", nullptr, config);
                }

                ImGui::SameLine();
                if (ImGui::Button("Dump All") && state.currentErf) {
                    ensureExtractDir(getExeDir());
                    std::string erfName = state.currentErf->filename();
                    fs::path outDir = fs::path(state.extractPath) / erfName;
                    fs::create_directories(outDir);

                    int success = 0, fail = 0;
                    for (const auto& entry : state.currentErf->entries()) {
                        std::string outPath = (outDir / entry.name).string();
                        if (state.currentErf->extractEntry(entry, outPath)) {
                            success++;
                        } else {
                            fail++;
                        }
                    }
                    state.statusMessage = "Extracted " + std::to_string(success) + " files, " + std::to_string(fail) + " failed";
                }

                ImGui::SameLine();
                if (!state.statusMessage.empty()) {
                    ImGui::Text("%s", state.statusMessage.c_str());
                }

                ImGui::EndMenuBar();
            }

            float panelWidth = io.DisplaySize.x * 0.3f;

            ImGui::BeginChild("LeftPanel", ImVec2(panelWidth, 0), true);
            ImGui::Text("ERF Files (%zu)", state.erfFiles.size());
            ImGui::Separator();

            for (int i = 0; i < static_cast<int>(state.erfFiles.size()); i++) {
                std::string displayName = fs::path(state.erfFiles[i]).filename().string();
                bool selected = (i == state.selectedErfIndex);

                if (ImGui::Selectable(displayName.c_str(), selected)) {
                    if (state.selectedErfIndex != i) {
                        state.selectedErfIndex = i;
                        state.selectedEntryIndex = -1;
                        state.currentErf = std::make_unique<ERFFile>();
                        if (!state.currentErf->open(state.erfFiles[i])) {
                            state.statusMessage = "Failed to open: " + displayName;
                            state.currentErf.reset();
                        } else {
                            state.statusMessage = "Opened: " + displayName + " (" + versionToString(state.currentErf->version()) + ")";
                        }
                    }
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);
            if (state.currentErf) {
                ImGui::Text("Contents: %s (%zu entries)", state.currentErf->filename().c_str(), state.currentErf->entries().size());

                if (state.currentErf->encryption() != 0) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "[Encrypted]");
                }
                if (state.currentErf->compression() != 0) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 1, 1), "[Compressed]");
                }

                ImGui::Separator();

                ImGui::BeginChild("EntryList");
                for (int i = 0; i < static_cast<int>(state.currentErf->entries().size()); i++) {
                    const auto& entry = state.currentErf->entries()[i];
                    bool selected = (i == state.selectedEntryIndex);

                    char label[512];
                    snprintf(label, sizeof(label), "%s (%u bytes)##%d", entry.name.c_str(), entry.length, i);

                    if (ImGui::Selectable(label, selected)) {
                        state.selectedEntryIndex = i;
                    }

                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem("Export")) {
                            ensureExtractDir(getExeDir());
                            std::string outPath = (fs::path(state.extractPath) / entry.name).string();
                            if (state.currentErf->extractEntry(entry, outPath)) {
                                state.statusMessage = "Exported: " + entry.name;
                            } else {
                                state.statusMessage = "Failed to export: " + entry.name;
                            }
                        }
                        ImGui::EndPopup();
                    }
                }
                ImGui::EndChild();
            } else {
                ImGui::Text("Select an ERF file to view contents");
            }
            ImGui::EndChild();

            ImGui::End();
        }

        if (ImGuiFileDialog::Instance()->Display("ChooseFolder", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                state.selectedFolder = ImGuiFileDialog::Instance()->GetCurrentPath();
                state.erfFiles = scanForERFFiles(state.selectedFolder);
                state.selectedErfIndex = -1;
                state.currentErf.reset();
                state.selectedEntryIndex = -1;
                state.showSplash = false;
                state.statusMessage = "Found " + std::to_string(state.erfFiles.size()) + " ERF files";
            }
            ImGuiFileDialog::Instance()->Close();
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
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