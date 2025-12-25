#pragma once

struct GLFWwindow;
struct AppState;
struct ImGuiIO;

// Draw all ImGui UI elements
void drawUI(AppState& state, GLFWwindow* window, ImGuiIO& io);

// Handle input (mouse/keyboard) for camera control
void handleInput(AppState& state, GLFWwindow* window, ImGuiIO& io);