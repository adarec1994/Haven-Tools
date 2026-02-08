#include <filesystem>
#include "types.h"
#include "renderer.h"
#include "Shaders/d3d_context.h"
#include "animation.h"
#include "ui.h"
#include "version.h"
#include "update/update.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_dx11.h"

namespace fs = std::filesystem;

static D3DContext g_d3d;

static void framebufferSizeCallback(GLFWwindow* /*window*/, int width, int height) {
    if (width > 0 && height > 0) {
        resizeD3D(g_d3d, width, height);
    }
}

int main(int argc, char** argv) {
    if (Update::HandleUpdaterMode(argc, argv)) return 0;

    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Haven Tools", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    if (!initD3D(window, g_d3d)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    initRenderer();

    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOther(window, true);
    ImGui_ImplDX11_Init(g_d3d.device, g_d3d.context);

    AppState state;
    state.extractPath = (fs::path(getExeDir()) / "extracted").string();

    Update::StartAutoCheckAndUpdate();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        handleInput(state, window, io);

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        drawUI(state, window, io);
        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        beginFrame(g_d3d, 0.15f, 0.15f, 0.15f, 1.0f);

        if (state.hasModel) {
            if (state.animPlaying && state.currentAnim.duration > 0)
                applyAnimation(state.currentModel, state.currentAnim, state.animTime, state.basePoseBones);
            renderModel(state.currentModel, state.camera, state.renderSettings, display_w, display_h, state.animPlaying, state.selectedBoneIndex);
        } else {
            Model empty;
            renderModel(empty, state.camera, state.renderSettings, display_w, display_h, false, -1);
        }

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        endFrame(g_d3d);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    cleanupRenderer();
    cleanupD3D(g_d3d);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}