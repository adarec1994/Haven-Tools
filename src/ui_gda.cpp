#include "ui_internal.h"

GDAEditorState::~GDAEditorState() {
    delete editor;
    editor = nullptr;
}

void draw2DAEditorWindow(AppState& state) {
    if (!state.gdaEditor.showWindow) return;

    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("2DA/GDA Editor", &state.gdaEditor.showWindow, ImGuiWindowFlags_MenuBar)) {

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Close", nullptr, false, state.gdaEditor.editor != nullptr)) {
                    delete state.gdaEditor.editor;
                    state.gdaEditor.editor = nullptr;
                    state.gdaEditor.currentFile.clear();
                    state.gdaEditor.selectedRow = -1;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if (state.gdaEditor.editor && state.gdaEditor.editor->isLoaded()) {
            const auto& columns = state.gdaEditor.editor->columns();
            const auto& rows = state.gdaEditor.editor->rows();

            ImGui::InputText("Filter", state.gdaEditor.rowFilter, sizeof(state.gdaEditor.rowFilter));
            ImGui::SameLine();
            ImGui::Text("%zu rows", rows.size());

            std::string filterLower = state.gdaEditor.rowFilter;
            std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);

            ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                         ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX |
                                         ImGuiTableFlags_ScrollY | ImGuiTableFlags_Hideable;

            int numCols = static_cast<int>(columns.size());
            if (numCols == 0) numCols = 1;

            if (ImGui::BeginTable("GDATable", numCols, tableFlags)) {
                for (const auto& col : columns) {
                    ImGui::TableSetupColumn(col.name.c_str());
                }
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableHeadersRow();

                for (size_t rowIdx = 0; rowIdx < rows.size(); rowIdx++) {
                    const auto& row = rows[rowIdx];

                    if (!filterLower.empty()) {
                        bool match = false;
                        for (const auto& val : row.values) {
                            if (std::holds_alternative<std::string>(val)) {
                                std::string strLower = std::get<std::string>(val);
                                std::transform(strLower.begin(), strLower.end(), strLower.begin(), ::tolower);
                                if (strLower.find(filterLower) != std::string::npos) {
                                    match = true;
                                    break;
                                }
                            } else if (std::holds_alternative<int32_t>(val)) {
                                if (std::to_string(std::get<int32_t>(val)).find(filterLower) != std::string::npos) {
                                    match = true;
                                    break;
                                }
                            }
                        }
                        if (!match) continue;
                    }

                    ImGui::TableNextRow();

                    for (size_t colIdx = 0; colIdx < columns.size(); colIdx++) {
                        ImGui::TableNextColumn();

                        if (colIdx < row.values.size()) {
                            const auto& val = row.values[colIdx];

                            if (std::holds_alternative<int32_t>(val)) {
                                ImGui::Text("%d", std::get<int32_t>(val));
                            } else if (std::holds_alternative<float>(val)) {
                                ImGui::Text("%.4f", std::get<float>(val));
                            } else if (std::holds_alternative<std::string>(val)) {
                                ImGui::TextUnformatted(std::get<std::string>(val).c_str());
                            } else if (std::holds_alternative<bool>(val)) {
                                ImGui::Text("%s", std::get<bool>(val) ? "1" : "0");
                            }
                        }
                    }
                }

                ImGui::EndTable();
            }
        } else {
            ImGui::TextWrapped(
                "No GDA file loaded.\n\n"
                "To view a GDA:\n"
                "1. Select 2da.erf in the ERF Browser\n"
                "2. Double-click any .gda file"
            );
        }
    }
    ImGui::End();
}