#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <cmath>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

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

// Fly camera (game-style)
struct Camera {
    float x = 0.0f, y = 0.0f, z = 5.0f;  // Position
    float yaw = 0.0f;
    float pitch = 0.0f;
    float moveSpeed = 5.0f;
    float lookSensitivity = 0.003f;

    void setPosition(float px, float py, float pz) {
        x = px;
        y = py;
        z = pz;
    }

    void lookAt(float tx, float ty, float tz, float dist) {
        // Position camera to look at target (remembering Z is up after rotation)
        // Camera is in Y-up space, model is rotated to Z-up
        // So we position in Y-up space: target's Z becomes our Y
        x = tx;
        y = tz + dist * 0.5f;  // tz is the model's "up", position camera above
        z = ty + dist;          // ty is the model's "forward", position camera back
        yaw = 0.0f;
        pitch = -0.2f;  // Look slightly down
        moveSpeed = dist * 0.5f;
        if (moveSpeed < 1.0f) moveSpeed = 1.0f;
    }

    void getForward(float& fx, float& fy, float& fz) const {
        // Forward direction based on yaw and pitch
        fx = -std::sin(yaw) * std::cos(pitch);
        fy = std::sin(pitch);
        fz = -std::cos(yaw) * std::cos(pitch);
    }

    void getRight(float& rx, float& ry, float& rz) const {
        // Right is perpendicular to forward on XZ plane
        rx = std::cos(yaw);
        ry = 0.0f;
        rz = -std::sin(yaw);
    }

    void moveForward(float amount) {
        // Move in the direction we're looking
        float fx, fy, fz;
        getForward(fx, fy, fz);
        x += fx * amount;
        y += fy * amount;
        z += fz * amount;
    }

    void moveRight(float amount) {
        // Strafe left/right
        float rx, ry, rz;
        getRight(rx, ry, rz);
        x += rx * amount;
        z += rz * amount;
    }

    void moveUp(float amount) {
        // Move along world Y axis
        y += amount;
    }

    void rotate(float deltaYaw, float deltaPitch) {
        yaw += deltaYaw;
        pitch += deltaPitch;
        // Clamp pitch
        if (pitch > 1.5f) pitch = 1.5f;
        if (pitch < -1.5f) pitch = -1.5f;
    }
};

// Render settings
struct RenderSettings {
    bool wireframe = false;
    bool showAxes = true;
    bool showGrid = true;
    bool showCollision = false;
    bool collisionWireframe = true;  // Draw collision as wireframe or solid
    std::vector<uint8_t> meshVisible;  // Per-mesh visibility (using uint8_t because vector<bool> is special)

    void initMeshVisibility(size_t count) {
        meshVisible.resize(count, 1);
    }
};

struct AppState {
    // Browser state
    bool showBrowser = true;
    bool showRenderSettings = false;
    bool showMaoViewer = false;
    std::string maoContent;
    std::string maoFileName;
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
    RenderSettings renderSettings;

    // Mouse state for camera control
    bool isPanning = false;       // Right mouse - look around
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

// Check if entry is a MAO (material) file
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

    // Init per-mesh visibility
    state.renderSettings.initMeshVisibility(model.meshes.size());

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
void renderModel(const Model& model, const Camera& camera, const RenderSettings& settings, int width, int height) {
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

    // Set up modelview matrix for fly camera
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Apply camera rotation (pitch then yaw)
    glRotatef(-camera.pitch * 180.0f / 3.14159f, 1, 0, 0);
    glRotatef(-camera.yaw * 180.0f / 3.14159f, 0, 1, 0);
    // Then translate
    glTranslatef(-camera.x, -camera.y, -camera.z);

    // Rotate world so Z is up (Dragon Age uses Z-up, OpenGL uses Y-up)
    glRotatef(-90.0f, 1, 0, 0);
    // Flip model to face correct direction (rotate 180 around Z)
    glRotatef(180.0f, 0, 0, 1);

    // Draw grid if enabled (on XY plane since Z is up)
    if (settings.showGrid) {
        glLineWidth(1.0f);
        glBegin(GL_LINES);
        glColor3f(0.3f, 0.3f, 0.3f);
        float gridSize = 10.0f;
        float gridStep = 1.0f;
        for (float i = -gridSize; i <= gridSize; i += gridStep) {
            // Lines along X
            glVertex3f(-gridSize, i, 0);
            glVertex3f(gridSize, i, 0);
            // Lines along Y
            glVertex3f(i, -gridSize, 0);
            glVertex3f(i, gridSize, 0);
        }
        glEnd();
    }

    // Draw axes if enabled (X=red, Y=green, Z=blue pointing up)
    if (settings.showAxes) {
        glLineWidth(2.0f);
        glBegin(GL_LINES);
        // X axis - red
        glColor3f(1, 0, 0);
        glVertex3f(0, 0, 0); glVertex3f(2, 0, 0);
        // Y axis - green
        glColor3f(0, 1, 0);
        glVertex3f(0, 0, 0); glVertex3f(0, 2, 0);
        // Z axis - blue (pointing UP)
        glColor3f(0, 0, 1);
        glVertex3f(0, 0, 0); glVertex3f(0, 0, 2);
        glEnd();
        glLineWidth(1.0f);
    }

    // Render meshes
    if (!model.meshes.empty()) {
        if (settings.wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glColor3f(0.8f, 0.8f, 0.8f);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_LIGHTING);
            glEnable(GL_LIGHT0);
            glEnable(GL_COLOR_MATERIAL);

            // Set up light
            float lightPos[] = {1.0f, 1.0f, 1.0f, 0.0f};
            float lightAmbient[] = {0.3f, 0.3f, 0.3f, 1.0f};
            float lightDiffuse[] = {0.7f, 0.7f, 0.7f, 1.0f};
            glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
            glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
            glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);

            glColor3f(0.7f, 0.7f, 0.7f);
        }

        for (size_t meshIdx = 0; meshIdx < model.meshes.size(); meshIdx++) {
            // Check per-mesh visibility
            if (meshIdx < settings.meshVisible.size() && settings.meshVisible[meshIdx] == 0) {
                continue;
            }

            const auto& mesh = model.meshes[meshIdx];
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

        if (!settings.wireframe) {
            glDisable(GL_LIGHTING);
            glDisable(GL_LIGHT0);
            glDisable(GL_COLOR_MATERIAL);
        }

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // Render collision shapes (cyan color)
    if (settings.showCollision && !model.collisionShapes.empty()) {
        if (settings.collisionWireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        if (settings.collisionWireframe) {
            glColor3f(0.0f, 1.0f, 1.0f);  // Cyan solid for wireframe
        } else {
            glColor4f(0.0f, 1.0f, 1.0f, 0.3f);  // Cyan transparent for solid
        }
        glLineWidth(1.5f);
        glDisable(GL_LIGHTING);

        for (const auto& shape : model.collisionShapes) {
            glPushMatrix();

            // Apply position
            glTranslatef(shape.posX, shape.posY, shape.posZ);

            // Apply rotation (quaternion to axis-angle)
            float angle = 2.0f * std::acos(shape.rotW) * 180.0f / 3.14159f;
            float s = std::sqrt(1.0f - shape.rotW * shape.rotW);
            if (s > 0.001f) {
                glRotatef(angle, shape.rotX / s, shape.rotY / s, shape.rotZ / s);
            }

            switch (shape.type) {
                case CollisionShapeType::Box: {
                    // Draw wireframe box
                    float x = shape.dimX, y = shape.dimY, z = shape.dimZ;
                    glBegin(GL_LINE_LOOP); // Bottom
                    glVertex3f(-x, -y, -z); glVertex3f(x, -y, -z);
                    glVertex3f(x, y, -z); glVertex3f(-x, y, -z);
                    glEnd();
                    glBegin(GL_LINE_LOOP); // Top
                    glVertex3f(-x, -y, z); glVertex3f(x, -y, z);
                    glVertex3f(x, y, z); glVertex3f(-x, y, z);
                    glEnd();
                    glBegin(GL_LINES); // Verticals
                    glVertex3f(-x, -y, -z); glVertex3f(-x, -y, z);
                    glVertex3f(x, -y, -z); glVertex3f(x, -y, z);
                    glVertex3f(x, y, -z); glVertex3f(x, y, z);
                    glVertex3f(-x, y, -z); glVertex3f(-x, y, z);
                    glEnd();
                    break;
                }
                case CollisionShapeType::Sphere: {
                    // Draw wireframe sphere (latitude/longitude lines)
                    int segments = 16;
                    float r = shape.radius;
                    // Latitude circles
                    for (int i = 0; i <= segments / 2; i++) {
                        float lat = 3.14159f * (-0.5f + float(i) / (segments / 2));
                        float z1 = r * std::sin(lat);
                        float r1 = r * std::cos(lat);
                        glBegin(GL_LINE_LOOP);
                        for (int j = 0; j < segments; j++) {
                            float lng = 2.0f * 3.14159f * float(j) / segments;
                            glVertex3f(r1 * std::cos(lng), r1 * std::sin(lng), z1);
                        }
                        glEnd();
                    }
                    // Longitude circles
                    for (int j = 0; j < segments / 2; j++) {
                        float lng = 3.14159f * float(j) / (segments / 2);
                        glBegin(GL_LINE_LOOP);
                        for (int i = 0; i < segments; i++) {
                            float lat = 2.0f * 3.14159f * float(i) / segments;
                            glVertex3f(r * std::cos(lat) * std::cos(lng), r * std::cos(lat) * std::sin(lng), r * std::sin(lat));
                        }
                        glEnd();
                    }
                    break;
                }
                case CollisionShapeType::Capsule:
                case CollisionShapeType::Cylinder: {
                    // Draw wireframe cylinder/capsule
                    int segments = 16;
                    float r = shape.radius;
                    float h = shape.height / 2.0f;
                    // Top and bottom circles
                    for (float zOff : {-h, h}) {
                        glBegin(GL_LINE_LOOP);
                        for (int i = 0; i < segments; i++) {
                            float a = 2.0f * 3.14159f * float(i) / segments;
                            glVertex3f(r * std::cos(a), r * std::sin(a), zOff);
                        }
                        glEnd();
                    }
                    // Vertical lines
                    glBegin(GL_LINES);
                    for (int i = 0; i < segments; i += segments / 4) {
                        float a = 2.0f * 3.14159f * float(i) / segments;
                        glVertex3f(r * std::cos(a), r * std::sin(a), -h);
                        glVertex3f(r * std::cos(a), r * std::sin(a), h);
                    }
                    glEnd();
                    // For capsule, add hemisphere caps
                    if (shape.type == CollisionShapeType::Capsule) {
                        for (float zSign : {-1.0f, 1.0f}) {
                            for (int i = 1; i <= 4; i++) {
                                float lat = (3.14159f / 2.0f) * float(i) / 4;
                                float z1 = r * std::sin(lat) * zSign + h * zSign;
                                float r1 = r * std::cos(lat);
                                glBegin(GL_LINE_LOOP);
                                for (int j = 0; j < segments; j++) {
                                    float lng = 2.0f * 3.14159f * float(j) / segments;
                                    glVertex3f(r1 * std::cos(lng), r1 * std::sin(lng), z1);
                                }
                                glEnd();
                            }
                        }
                    }
                    break;
                }
                case CollisionShapeType::Mesh: {
                    // Draw collision mesh
                    glBegin(GL_TRIANGLES);
                    for (size_t i = 0; i < shape.mesh.indices.size(); i += 3) {
                        for (int j = 0; j < 3; j++) {
                            const auto& v = shape.mesh.vertices[shape.mesh.indices[i + j]];
                            glVertex3f(v.x, v.y, v.z);
                        }
                    }
                    glEnd();
                    break;
                }
            }

            glPopMatrix();
        }

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glLineWidth(1.0f);
        glDisable(GL_BLEND);
    }

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

        // Handle mouse input for camera - RIGHT CLICK to look around
        if (!io.WantCaptureMouse) {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);

            // Right mouse - look around (fly cam style)
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                // Clear ImGui focus so WASD works immediately
                ImGui::SetWindowFocus(nullptr);

                if (state.isPanning) {
                    float dx = static_cast<float>(mx - state.lastMouseX);
                    float dy = static_cast<float>(my - state.lastMouseY);
                    state.camera.rotate(-dx * state.camera.lookSensitivity, -dy * state.camera.lookSensitivity);
                }
                state.isPanning = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

                // RMB + scroll to adjust speed
                float scroll = io.MouseWheel;
                if (scroll != 0.0f) {
                    state.camera.moveSpeed *= (scroll > 0) ? 1.2f : 0.8f;
                    if (state.camera.moveSpeed < 0.1f) state.camera.moveSpeed = 0.1f;
                    if (state.camera.moveSpeed > 100.0f) state.camera.moveSpeed = 100.0f;
                }
            } else {
                if (state.isPanning) {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                }
                state.isPanning = false;
            }

            state.lastMouseX = mx;
            state.lastMouseY = my;
        }

        // WASD movement (fly cam style)
        if (!io.WantCaptureKeyboard) {
            float deltaTime = io.DeltaTime;
            float speed = state.camera.moveSpeed * deltaTime;

            // Move faster with shift
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
                speed *= 3.0f;
            }

            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                state.camera.moveForward(speed);
            }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                state.camera.moveForward(-speed);
            }
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                state.camera.moveRight(-speed);
            }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                state.camera.moveRight(speed);
            }
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
                state.camera.moveUp(speed);
            }
            if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
                state.camera.moveUp(-speed);
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

                    // Highlight model and material files
                    bool isModel = isModelFile(entry.name);
                    bool isMao = isMaoFile(entry.name);

                    if (isModel) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));  // Green
                    } else if (isMao) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.4f, 1.0f));  // Orange
                    }

                    char label[256];
                    snprintf(label, sizeof(label), "%s##%d", entry.name.c_str(), i);

                    if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                        state.selectedEntryIndex = i;

                        // Double-click to load model
                        if (ImGui::IsMouseDoubleClicked(0)) {
                            if (isModel) {
                                if (loadModelFromEntry(state, entry)) {
                                    state.statusMessage = "Loaded: " + entry.name + " (" +
                                        std::to_string(state.currentModel.meshes.size()) + " meshes)";
                                    state.showRenderSettings = true;
                                } else {
                                    state.statusMessage = "Failed to parse: " + entry.name;
                                    state.showRenderSettings = true;
                                }
                            } else if (isMao) {
                                // Load MAO as plaintext
                                std::vector<uint8_t> data = state.currentErf->readEntry(entry);
                                if (!data.empty()) {
                                    state.maoContent = std::string(data.begin(), data.end());
                                    state.maoFileName = entry.name;
                                    state.showMaoViewer = true;
                                    state.statusMessage = "Opened: " + entry.name;
                                }
                            }
                        }
                    }

                    if (isModel || isMao) {
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
                            ImGui::Text("Double-click to load model");
                        } else if (isMao) {
                            ImGui::Text("Double-click to view material");
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
                ImGui::MenuItem("Render Settings", nullptr, &state.showRenderSettings);
                ImGui::EndMenu();
            }

            if (state.hasModel) {
                ImGui::SameLine(ImGui::GetWindowWidth() - 500);
                ImGui::Text("Model: %s | RMB+Mouse: Look | WASD: Move | Space/Ctrl: Up/Down | Shift: Fast", state.currentModel.name.c_str());
            }

            ImGui::EndMainMenuBar();
        }

        // Render Settings window
        if (state.showRenderSettings) {
            ImGui::SetNextWindowSize(ImVec2(350, 500), ImGuiCond_FirstUseEver);
            ImGui::Begin("Render Settings", &state.showRenderSettings);

            ImGui::Checkbox("Wireframe", &state.renderSettings.wireframe);
            ImGui::Checkbox("Show Axes", &state.renderSettings.showAxes);
            ImGui::Checkbox("Show Grid", &state.renderSettings.showGrid);
            ImGui::Checkbox("Show Collision", &state.renderSettings.showCollision);
            if (state.renderSettings.showCollision) {
                ImGui::SameLine();
                ImGui::Checkbox("Wireframe##coll", &state.renderSettings.collisionWireframe);
            }

            ImGui::Separator();
            ImGui::Text("Camera Speed: %.1f", state.camera.moveSpeed);
            ImGui::SliderFloat("##speed", &state.camera.moveSpeed, 0.1f, 100.0f, "%.1f");
            ImGui::TextDisabled("(RMB + Scroll to adjust)");

            if (state.hasModel) {
                ImGui::Separator();
                size_t totalVerts = 0, totalTris = 0;
                for (const auto& m : state.currentModel.meshes) {
                    totalVerts += m.vertices.size();
                    totalTris += m.indices.size() / 3;
                }
                ImGui::Text("Total: %zu meshes, %zu verts, %zu tris",
                    state.currentModel.meshes.size(), totalVerts, totalTris);

                // Mesh list with visibility and material names
                if (state.currentModel.meshes.size() >= 1) {
                    ImGui::Separator();
                    if (state.currentModel.meshes.size() > 1) {
                        ImGui::Text("Mesh Visibility:");
                    } else {
                        ImGui::Text("Mesh Info:");
                    }

                    // Ensure visibility array is correct size
                    if (state.renderSettings.meshVisible.size() != state.currentModel.meshes.size()) {
                        state.renderSettings.initMeshVisibility(state.currentModel.meshes.size());
                    }

                    ImGui::BeginChild("MeshList", ImVec2(0, 200), true);
                    for (size_t i = 0; i < state.currentModel.meshes.size(); i++) {
                        const auto& mesh = state.currentModel.meshes[i];

                        // Visibility checkbox (only if multiple meshes)
                        if (state.currentModel.meshes.size() > 1) {
                            bool visible = state.renderSettings.meshVisible[i] != 0;
                            if (ImGui::Checkbox(("##mesh" + std::to_string(i)).c_str(), &visible)) {
                                state.renderSettings.meshVisible[i] = visible ? 1 : 0;
                            }
                            ImGui::SameLine();
                        }

                        // Mesh name
                        std::string meshLabel = mesh.name.empty() ?
                            ("Mesh " + std::to_string(i)) : mesh.name;
                        ImGui::Text("%s", meshLabel.c_str());

                        // Mesh details indented
                        ImGui::Indent();
                        ImGui::TextDisabled("%zu verts, %zu tris",
                            mesh.vertices.size(), mesh.indices.size() / 3);
                        if (!mesh.materialName.empty()) {
                            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f),
                                "Material: %s", mesh.materialName.c_str());
                        }
                        ImGui::Unindent();

                        if (i < state.currentModel.meshes.size() - 1) {
                            ImGui::Spacing();
                        }
                    }
                    ImGui::EndChild();

                    // Show all / Hide all buttons (only if multiple meshes)
                    if (state.currentModel.meshes.size() > 1) {
                        if (ImGui::Button("Show All")) {
                            for (size_t i = 0; i < state.renderSettings.meshVisible.size(); i++) {
                                state.renderSettings.meshVisible[i] = 1;
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Hide All")) {
                            for (size_t i = 0; i < state.renderSettings.meshVisible.size(); i++) {
                                state.renderSettings.meshVisible[i] = 0;
                            }
                        }
                    }
                }

                // Collision mesh info
                if (!state.currentModel.collisionShapes.empty()) {
                    ImGui::Separator();
                    ImGui::Text("Collision shapes: %zu", state.currentModel.collisionShapes.size());
                }
            }

            ImGui::End();
        }

        // MAO Viewer window
        if (state.showMaoViewer) {
            ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
            ImGui::Begin(("MAO Viewer - " + state.maoFileName).c_str(), &state.showMaoViewer);

            // Copy button
            if (ImGui::Button("Copy to Clipboard")) {
                ImGui::SetClipboardText(state.maoContent.c_str());
            }

            ImGui::Separator();

            // Scrollable text area
            ImGui::BeginChild("MaoContent", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
            ImGui::TextUnformatted(state.maoContent.c_str());
            ImGui::EndChild();

            ImGui::End();
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
            renderModel(state.currentModel, state.camera, state.renderSettings, display_w, display_h);
        } else {
            // Render empty scene with just grid/axes
            Model empty;
            renderModel(empty, state.camera, state.renderSettings, display_w, display_h);
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