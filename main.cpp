#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <cmath>

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ImGuiFileDialog.h"
#include "erf.h"
#include "Gff.h"
#include "Mesh.h"
#include "model_loader.h"

namespace fs = std::filesystem;

// Simple orbit camera
struct Camera {
    float distance = 5.0f;
    float yaw = 0.0f;
    float pitch = 0.3f;
    float targetX = 0.0f;
    float targetY = 0.0f;
    float targetZ = 0.0f;

    void lookAt(float x, float y, float z, float dist) {
        targetX = x;
        targetY = y;
        targetZ = z;
        distance = dist;
    }

    void getPosition(float& x, float& y, float& z) const {
        x = targetX + distance * std::cos(pitch) * std::sin(yaw);
        y = targetY + distance * std::sin(pitch);
        z = targetZ + distance * std::cos(pitch) * std::cos(yaw);
    }
};

struct AppState {
    // Browser state
    bool showBrowser = true;
    std::string selectedFolder;
    std::vector<std::string> erfFiles;
    int selectedErfIndex = -1;
    std::unique_ptr<ERFFile> currentErf;
    int selectedEntryIndex = -1;
    std::string statusMessage;
    std::string extractPath;

    // Model state
    Model currentModel;
    bool hasModel = false;
    Camera camera;

    // Mouse state for camera control
    bool isDragging = false;
    double lastMouseX = 0;
    double lastMouseY = 0;
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

// Check if entry is a model file
bool isModelFile(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Check for MMH (model) or MSH (mesh) files
    size_t dotPos = lower.rfind('.');
    if (dotPos != std::string::npos) {
        std::string ext = lower.substr(dotPos);
        return ext == ".mmh" || ext == ".msh";
    }
    return false;
}

// Try to load model from ERF entry
bool loadModelFromEntry(AppState& state, const ERFEntry& entry) {
    if (!state.currentErf) return false;

    std::vector<uint8_t> data = state.currentErf->readEntry(entry);
    if (data.empty()) return false;

    // Try to load as MSH
    Model model;
    if (!loadMSH(data, model)) {
        // If MSH loading fails, show a placeholder
        state.currentModel = Model();
        state.currentModel.name = entry.name + " (failed to parse)";

        // Create a simple cube as placeholder
        Mesh cube;
        cube.name = "placeholder";

        float s = 1.0f;
        cube.vertices = {
            {-s, -s,  s,  0, 0, 1,  0, 0},
            { s, -s,  s,  0, 0, 1,  1, 0},
            { s,  s,  s,  0, 0, 1,  1, 1},
            {-s,  s,  s,  0, 0, 1,  0, 1},
            { s, -s, -s,  0, 0,-1,  0, 0},
            {-s, -s, -s,  0, 0,-1,  1, 0},
            {-s,  s, -s,  0, 0,-1,  1, 1},
            { s,  s, -s,  0, 0,-1,  0, 1},
            {-s,  s,  s,  0, 1, 0,  0, 0},
            { s,  s,  s,  0, 1, 0,  1, 0},
            { s,  s, -s,  0, 1, 0,  1, 1},
            {-s,  s, -s,  0, 1, 0,  0, 1},
            {-s, -s, -s,  0,-1, 0,  0, 0},
            { s, -s, -s,  0,-1, 0,  1, 0},
            { s, -s,  s,  0,-1, 0,  1, 1},
            {-s, -s,  s,  0,-1, 0,  0, 1},
            { s, -s,  s,  1, 0, 0,  0, 0},
            { s, -s, -s,  1, 0, 0,  1, 0},
            { s,  s, -s,  1, 0, 0,  1, 1},
            { s,  s,  s,  1, 0, 0,  0, 1},
            {-s, -s, -s, -1, 0, 0,  0, 0},
            {-s, -s,  s, -1, 0, 0,  1, 0},
            {-s,  s,  s, -1, 0, 0,  1, 1},
            {-s,  s, -s, -1, 0, 0,  0, 1},
        };

        cube.indices = {
            0,  1,  2,  2,  3,  0,
            4,  5,  6,  6,  7,  4,
            8,  9,  10, 10, 11, 8,
            12, 13, 14, 14, 15, 12,
            16, 17, 18, 18, 19, 16,
            20, 21, 22, 22, 23, 20
        };

        cube.calculateBounds();
        state.currentModel.meshes.push_back(cube);
        state.hasModel = true;

        auto center = cube.center();
        state.camera.lookAt(center[0], center[1], center[2], cube.radius() * 3.0f);

        return false; // Indicate parsing failed but we have placeholder
    }

    // Successfully loaded
    state.currentModel = model;
    state.currentModel.name = entry.name;
    state.hasModel = true;

    // Calculate bounds and position camera
    if (!model.meshes.empty()) {
        // Find overall bounds
        float minX = model.meshes[0].minX, maxX = model.meshes[0].maxX;
        float minY = model.meshes[0].minY, maxY = model.meshes[0].maxY;
        float minZ = model.meshes[0].minZ, maxZ = model.meshes[0].maxZ;

        for (const auto& mesh : model.meshes) {
            if (mesh.minX < minX) minX = mesh.minX;
            if (mesh.maxX > maxX) maxX = mesh.maxX;
            if (mesh.minY < minY) minY = mesh.minY;
            if (mesh.maxY > maxY) maxY = mesh.maxY;
            if (mesh.minZ < minZ) minZ = mesh.minZ;
            if (mesh.maxZ > maxZ) maxZ = mesh.maxZ;
        }

        float cx = (minX + maxX) / 2.0f;
        float cy = (minY + maxY) / 2.0f;
        float cz = (minZ + maxZ) / 2.0f;
        float dx = maxX - minX;
        float dy = maxY - minY;
        float dz = maxZ - minZ;
        float radius = std::sqrt(dx*dx + dy*dy + dz*dz) / 2.0f;

        state.camera.lookAt(cx, cy, cz, radius * 2.5f);
    }

    return true;
}

// Render model using immediate mode OpenGL
void renderModel(const Model& model, const Camera& camera, int width, int height) {
    if (model.meshes.empty()) return;

    glEnable(GL_DEPTH_TEST);

    // Set up projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    float fov = 45.0f * 3.14159f / 180.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;
    float top = nearPlane * std::tan(fov / 2.0f);
    float right = top * aspect;
    glFrustum(-right, right, -top, top, nearPlane, farPlane);

    // Set up modelview matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float camX, camY, camZ;
    camera.getPosition(camX, camY, camZ);

    // Translate to camera position
    glTranslatef(0, 0, -camera.distance);
    glRotatef(camera.pitch * 180.0f / 3.14159f, 1, 0, 0);
    glRotatef(camera.yaw * 180.0f / 3.14159f, 0, 1, 0);
    glTranslatef(-camera.targetX, -camera.targetY, -camera.targetZ);

    // Render meshes as wireframe
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glColor3f(0.8f, 0.8f, 0.8f);

    for (const auto& mesh : model.meshes) {
        glBegin(GL_TRIANGLES);
        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            for (int j = 0; j < 3; j++) {
                const auto& v = mesh.vertices[mesh.indices[i + j]];
                glNormal3f(v.nx, v.ny, v.nz);
                glVertex3f(v.x, v.y, v.z);
            }
        }
        glEnd();
    }

    // Draw axes
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    // X axis - red
    glColor3f(1, 0, 0);
    glVertex3f(0, 0, 0); glVertex3f(2, 0, 0);
    // Y axis - green
    glColor3f(0, 1, 0);
    glVertex3f(0, 0, 0); glVertex3f(0, 2, 0);
    // Z axis - blue
    glColor3f(0, 0, 1);
    glVertex3f(0, 0, 0); glVertex3f(0, 0, 2);
    glEnd();
    glLineWidth(1.0f);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_DEPTH_TEST);
}

int main() {
    if (!glfwInit()) return -1;

    // Use OpenGL 2.1 for compatibility with immediate mode rendering
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Dragon Age Model Browser", nullptr, nullptr);
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
    ImGui_ImplOpenGL3_Init("#version 120");

    AppState state;
    state.extractPath = (fs::path(getExeDir()) / "extracted").string();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Handle mouse input for camera
        if (!io.WantCaptureMouse) {
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
                double mx, my;
                glfwGetCursorPos(window, &mx, &my);

                if (state.isDragging) {
                    float dx = static_cast<float>(mx - state.lastMouseX);
                    float dy = static_cast<float>(my - state.lastMouseY);
                    state.camera.yaw += dx * 0.01f;
                    state.camera.pitch += dy * 0.01f;
                    state.camera.pitch = std::max(-1.5f, std::min(1.5f, state.camera.pitch));
                }

                state.isDragging = true;
                state.lastMouseX = mx;
                state.lastMouseY = my;
            } else {
                state.isDragging = false;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Browser window (popup style)
        if (state.showBrowser) {
            ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
            ImGui::Begin("ERF Browser", &state.showBrowser, ImGuiWindowFlags_MenuBar);

            if (ImGui::BeginMenuBar()) {
                if (ImGui::Button("Open Folder")) {
                    IGFD::FileDialogConfig config;
                    config.path = state.selectedFolder.empty() ? "." : state.selectedFolder;
                    ImGuiFileDialog::Instance()->OpenDialog("ChooseFolder", "Choose Folder", nullptr, config);
                }

                if (!state.statusMessage.empty()) {
                    ImGui::SameLine();
                    ImGui::Text("%s", state.statusMessage.c_str());
                }
                ImGui::EndMenuBar();
            }

            // Two-column layout
            ImGui::Columns(2, "browser_columns");

            // Left: ERF file list
            ImGui::Text("ERF Files (%zu)", state.erfFiles.size());
            ImGui::Separator();

            ImGui::BeginChild("ERFList", ImVec2(0, 0), true);
            for (int i = 0; i < static_cast<int>(state.erfFiles.size()); i++) {
                std::string displayName = fs::path(state.erfFiles[i]).filename().string();
                bool selected = (i == state.selectedErfIndex);

                if (ImGui::Selectable(displayName.c_str(), selected)) {
                    if (state.selectedErfIndex != i) {
                        state.selectedErfIndex = i;
                        state.selectedEntryIndex = -1;
                        state.currentErf = std::make_unique<ERFFile>();
                        if (!state.currentErf->open(state.erfFiles[i])) {
                            state.statusMessage = "Failed to open";
                            state.currentErf.reset();
                        } else {
                            state.statusMessage = versionToString(state.currentErf->version());
                        }
                    }
                }
            }
            ImGui::EndChild();

            ImGui::NextColumn();

            // Right: Entry list
            if (state.currentErf) {
                ImGui::Text("Contents (%zu)", state.currentErf->entries().size());

                if (state.currentErf->encryption() != 0) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "[Enc]");
                }
                if (state.currentErf->compression() != 0) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 1, 1), "[Comp]");
                }

                ImGui::Separator();

                ImGui::BeginChild("EntryList", ImVec2(0, 0), true);
                for (int i = 0; i < static_cast<int>(state.currentErf->entries().size()); i++) {
                    const auto& entry = state.currentErf->entries()[i];
                    bool selected = (i == state.selectedEntryIndex);

                    // Highlight model files
                    bool isModel = isModelFile(entry.name);
                    if (isModel) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
                    }

                    char label[256];
                    snprintf(label, sizeof(label), "%s##%d", entry.name.c_str(), i);

                    if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                        state.selectedEntryIndex = i;

                        // Double-click to load model
                        if (ImGui::IsMouseDoubleClicked(0) && isModel) {
                            if (loadModelFromEntry(state, entry)) {
                                state.statusMessage = "Loaded: " + entry.name + " (" +
                                    std::to_string(state.currentModel.meshes.size()) + " meshes)";
                            } else {
                                state.statusMessage = "Failed to parse: " + entry.name;
                            }
                        }
                    }

                    if (isModel) {
                        ImGui::PopStyleColor();
                    }

                    // Tooltip with more info
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("Size: %u bytes", entry.length);
                        if (entry.packed_length != entry.length) {
                            ImGui::Text("Packed: %u bytes", entry.packed_length);
                        }
                        if (isModel) {
                            ImGui::Text("Double-click to load");
                        }
                        ImGui::EndTooltip();
                    }
                }
                ImGui::EndChild();
            } else {
                ImGui::Text("Select an ERF file");
            }

            ImGui::Columns(1);
            ImGui::End();
        }

        // File dialog
        if (ImGuiFileDialog::Instance()->Display("ChooseFolder", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                state.selectedFolder = ImGuiFileDialog::Instance()->GetCurrentPath();
                state.erfFiles = scanForERFFiles(state.selectedFolder);
                state.selectedErfIndex = -1;
                state.currentErf.reset();
                state.selectedEntryIndex = -1;
                state.statusMessage = "Found " + std::to_string(state.erfFiles.size()) + " ERF files";
            }
            ImGuiFileDialog::Instance()->Close();
        }

        // Main menu bar
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Browser", nullptr, &state.showBrowser);
                ImGui::EndMenu();
            }

            if (state.hasModel) {
                ImGui::SameLine(ImGui::GetWindowWidth() - 300);
                ImGui::Text("Model: %s | Drag to rotate", state.currentModel.name.c_str());
            }

            ImGui::EndMainMenuBar();
        }

        // Render
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render model in main area
        if (state.hasModel) {
            renderModel(state.currentModel, state.camera, display_w, display_h);
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