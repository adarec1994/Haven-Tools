#include <filesystem>
#include "types.h"
#include "renderer.h"
#include "animation.h"
#include "ui.h"
#include "version.h"
#include "update/update.h"
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (Update::HandleUpdaterMode(argc, argv)) return 0;

    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Haven Tools", nullptr, nullptr);
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

    Update::StartAutoCheckAndUpdate();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        handleInput(state, window, io);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        drawUI(state, window, io);
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        if (state.hasModel) {
            if (state.animPlaying && state.currentAnim.duration > 0)
                applyAnimation(state.currentModel, state.currentAnim, state.animTime, state.basePoseBones);
            renderModel(state.currentModel, state.camera, state.renderSettings, display_w, display_h, state.animPlaying, state.selectedBoneIndex);
        } else {
            Model empty;
            renderModel(empty, state.camera, state.renderSettings, display_w, display_h, false, -1);
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
