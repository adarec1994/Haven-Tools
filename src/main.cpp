#include <filesystem>
#include "types.h"
#include "renderer.h"
#include "Shaders/d3d_context.h"
#include "animation.h"
#include "ui.h"
#include "version.h"
#include "update/update.h"
#include "spt.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_dx11.h"
#include "Fonts/IconsFontAwesome6.h"
#include "fa_solid_embedded.h"
#include "roboto_embedded.h"

namespace fs = std::filesystem;

static D3DContext g_d3d;

ImFont* g_audioFont = nullptr;

static float fontSizeWithOffset(float baseSize, float offset, float minSize) {
    float size = baseSize + offset;
    return size < minSize ? minSize : size;
}

static void buildUIFontAtlas(ImGuiIO& io, float dpiScale, float uiFontSize) {
    uiFontSize = clampUIFontSize(uiFontSize);
    io.Fonts->Clear();
    io.FontDefault = nullptr;

    // DPI-aware font setup. On a scaled Windows display the framebuffer is
    // larger than the logical window, so a font rasterized at 15px and drawn
    // at 15 logical units gets bilinearly upsampled at the GPU = blurry text.
    // Fix: rasterize the atlas at PHYSICAL pixel size, then counter-scale via
    // io.FontGlobalScale so glyphs draw at the requested logical size.
    if (roboto_ttf_len > 1) {
        ImFontConfig roboto_cfg;
        roboto_cfg.FontDataOwnedByAtlas = false;
        roboto_cfg.OversampleH = 3;
        roboto_cfg.OversampleV = 1;
        roboto_cfg.PixelSnapH = true;
        io.Fonts->AddFontFromMemoryTTF(
            (void*)roboto_ttf, (int)roboto_ttf_len,
            uiFontSize * dpiScale, &roboto_cfg);
    } else {
        ImFontConfig def_cfg;
        def_cfg.SizePixels = fontSizeWithOffset(uiFontSize, -2.0f, 8.0f) * dpiScale;
        io.Fonts->AddFontDefault(&def_cfg);
    }

    float supportFontSize = fontSizeWithOffset(uiFontSize, -1.0f, 8.0f);
    auto tryMergeFont = [&](const char* path, const ImWchar* ranges) {
        if (std::filesystem::exists(path)) {
            ImFontConfig cfg;
            cfg.MergeMode = true;
            cfg.PixelSnapH = true;
            cfg.OversampleH = 2;
            cfg.OversampleV = 1;
            io.Fonts->AddFontFromFileTTF(path, supportFontSize * dpiScale, &cfg, ranges);
        }
    };
    tryMergeFont("C:\\Windows\\Fonts\\segoeui.ttf", io.Fonts->GetGlyphRangesCyrillic());
    tryMergeFont("C:\\Windows\\Fonts\\malgun.ttf",  io.Fonts->GetGlyphRangesKorean());
    tryMergeFont("C:\\Windows\\Fonts\\msyh.ttc",    io.Fonts->GetGlyphRangesChineseFull());
    tryMergeFont("C:\\Windows\\Fonts\\YuGothM.ttc", io.Fonts->GetGlyphRangesJapanese());
    tryMergeFont("C:\\Windows\\Fonts\\segoeui.ttf", io.Fonts->GetGlyphRangesGreek());

    ImFontConfig fa_cfg;
    fa_cfg.MergeMode = true;
    fa_cfg.PixelSnapH = true;
    fa_cfg.GlyphMinAdvanceX = fontSizeWithOffset(uiFontSize, -2.0f, 8.0f) * dpiScale;
    fa_cfg.FontDataOwnedByAtlas = false;
    static const ImWchar fa_range[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    if (fa_solid_otf_len > 1) {
        io.Fonts->AddFontFromMemoryTTF(
            (void*)fa_solid_otf, fa_solid_otf_len,
            fontSizeWithOffset(uiFontSize, -2.0f, 8.0f) * dpiScale, &fa_cfg, fa_range
        );
    }

    ImFontConfig audio_cfg;
    audio_cfg.SizePixels = fontSizeWithOffset(uiFontSize, -2.0f, 8.0f) * dpiScale;
    g_audioFont = io.Fonts->AddFontDefault(&audio_cfg);

    io.Fonts->Build();
    io.FontGlobalScale = 1.0f / dpiScale;
}

static void framebufferSizeCallback(GLFWwindow*, int width, int height) {
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

    AppState state;
    loadSettings(state);

    float dpiScale = 1.0f;
    {
        float xscale = 1.0f, yscale = 1.0f;
        glfwGetWindowContentScale(window, &xscale, &yscale);
        dpiScale = xscale > 1.0f ? xscale : 1.0f;
    }

    buildUIFontAtlas(io, dpiScale, state.uiFontSize);

    ImGui_ImplGlfw_InitForOther(window, true);
    ImGui_ImplDX11_Init(g_d3d.device, g_d3d.context);

    state.extractPath = (fs::path(getExeDir()) / "extracted").string();
    initSpeedTree();

    Update::StartAutoCheckAndUpdate();

    float appliedUIFontSize = clampUIFontSize(state.uiFontSize);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (state.levelLoad.stage == 0)
            handleInput(state, window, io);

        state.uiFontSize = clampUIFontSize(state.uiFontSize);
        float fontSizeDiff = appliedUIFontSize - state.uiFontSize;
        if (fontSizeDiff < 0.0f) fontSizeDiff = -fontSizeDiff;
        if (fontSizeDiff > 0.01f) {
            buildUIFontAtlas(io, dpiScale, state.uiFontSize);
            ImGui_ImplDX11_InvalidateDeviceObjects();
            ImGui_ImplDX11_CreateDeviceObjects();
            appliedUIFontSize = state.uiFontSize;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        drawUI(state, window, io);
        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        if (state.envSettings.loaded) {
            beginFrame(g_d3d, state.envSettings.atmoFogColor[0] * 0.3f,
                              state.envSettings.atmoFogColor[1] * 0.3f,
                              state.envSettings.atmoFogColor[2] * 0.3f, 1.0f);
        } else {
            beginFrame(g_d3d, 0.15f, 0.15f, 0.15f, 1.0f);
        }

        if (state.hasModel) {
            if (state.animPlaying && state.currentAnim.duration > 0)
                applyAnimation(state.currentModel, state.currentAnim, state.animTime, state.basePoseBones);
            renderModel(state.currentModel, state.camera, state.renderSettings, display_w, display_h, state.animPlaying, state.selectedBoneIndex, state.selectedLevelChunk, &state.envSettings, state.skyboxLoaded ? &state.skyboxModel : nullptr, state.selectedLevelInstance);
        } else {
            Model empty;
            renderModel(empty, state.camera, state.renderSettings, display_w, display_h, false, -1, -1, &state.envSettings, state.skyboxLoaded ? &state.skyboxModel : nullptr);
        }

        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        endFrame(g_d3d);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    shutdownSpeedTree();
    cleanupRenderer();
    cleanupD3D(g_d3d);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
