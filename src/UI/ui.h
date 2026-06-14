#pragma once

struct GLFWwindow;
struct AppState;
struct ImGuiIO;

void drawUI(AppState& state, GLFWwindow* window, ImGuiIO& io);

void handleInput(AppState& state, GLFWwindow* window, ImGuiIO& io);

void saveSettings(const AppState& state);
void loadSettings(AppState& state);
