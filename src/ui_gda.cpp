#include "ui_internal.h"

static const char* GDA_BACKUP_DIR = "gda_backups";

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

            if (ImGui::BeginMenu("Backup")) {
                if (ImGui::MenuItem("Create Backup", nullptr, false,
                                    state.gdaEditor.editor != nullptr && !state.gdaEditor.currentFile.empty())) {
                    fs::path backupDir = fs::current_path() / GDA_BACKUP_DIR;
                    GDAFile::createBackup(state.gdaEditor.currentFile, backupDir.string());
                }

                if (ImGui::MenuItem("Restore from Backup", nullptr, false,
                                    !state.gdaEditor.currentFile.empty() &&
                                    GDAFile::backupExists(state.gdaEditor.currentFile,
                                                          (fs::current_path() / GDA_BACKUP_DIR).string()))) {
                    state.gdaEditor.showRestoreDialog = true;
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Open Backup Folder")) {
                    fs::path backupDir = fs::current_path() / GDA_BACKUP_DIR;
                    fs::create_directories(backupDir);
#ifdef _WIN32
                    ShellExecuteA(NULL, "open", backupDir.string().c_str(), NULL, NULL, SW_SHOWDEFAULT);
#endif
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Add Row", nullptr, false, state.gdaEditor.editor != nullptr)) {
                    if (state.gdaEditor.editor) {
                        int32_t newId = state.gdaEditor.editor->table().getNextAvailableId();
                        int newRow = state.gdaEditor.editor->table().addRow(newId);
                        if (newRow >= 0) {
                            state.gdaEditor.selectedRow = newRow;
                            state.gdaEditor.editor->setModified(true);
                        }
                    }
                }

                if (ImGui::MenuItem("Delete Row", nullptr, false,
                                    state.gdaEditor.editor != nullptr && state.gdaEditor.selectedRow >= 0)) {
                    if (state.gdaEditor.editor) {
                        state.gdaEditor.editor->table().removeRow(state.gdaEditor.selectedRow);
                        state.gdaEditor.editor->setModified(true);
                        state.gdaEditor.selectedRow = -1;
                    }
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        if (state.gdaEditor.editor && state.gdaEditor.editor->isLoaded()) {
            const auto& table = state.gdaEditor.editor->table();

            ImGui::InputText("Filter", state.gdaEditor.rowFilter, sizeof(state.gdaEditor.rowFilter));
            std::string filterLower = state.gdaEditor.rowFilter;
            std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);

            ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                         ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX |
                                         ImGuiTableFlags_ScrollY;

            int numCols = 1 + static_cast<int>(table.columns.size());

            float tableHeight = ImGui::GetContentRegionAvail().y - 200;
            if (tableHeight < 100) tableHeight = 100;

            if (ImGui::BeginTable("GDATable", numCols, tableFlags, ImVec2(0, tableHeight))) {
                ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 60);
                for (const auto& col : table.columns) {
                    ImGui::TableSetupColumn(col.name.c_str());
                }
                ImGui::TableSetupScrollFreeze(1, 1);
                ImGui::TableHeadersRow();

                for (size_t rowIdx = 0; rowIdx < table.rows.size(); rowIdx++) {
                    const auto& row = table.rows[rowIdx];

                    if (!filterLower.empty()) {
                        bool match = false;
                        std::string idStr = std::to_string(row.id);
                        if (idStr.find(filterLower) != std::string::npos) match = true;

                        if (!match) {
                            for (const auto& val : row.values) {
                                if (std::holds_alternative<std::string>(val)) {
                                    std::string strLower = std::get<std::string>(val);
                                    std::transform(strLower.begin(), strLower.end(), strLower.begin(), ::tolower);
                                    if (strLower.find(filterLower) != std::string::npos) {
                                        match = true;
                                        break;
                                    }
                                }
                            }
                        }
                        if (!match) continue;
                    }

                    ImGui::TableNextRow();
                    bool isSelected = (state.gdaEditor.selectedRow == (int)rowIdx);

                    ImGui::TableNextColumn();
                    char idBuf[32];
                    snprintf(idBuf, sizeof(idBuf), "%d##%zu", row.id, rowIdx);
                    if (ImGui::Selectable(idBuf, isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                        state.gdaEditor.selectedRow = static_cast<int>(rowIdx);
                    }

                    for (size_t colIdx = 0; colIdx < table.columns.size(); colIdx++) {
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
                                ImGui::Text("%s", std::get<bool>(val) ? "Yes" : "No");
                            }
                        }
                    }
                }

                ImGui::EndTable();
            }

            if (state.gdaEditor.selectedRow >= 0 &&
                state.gdaEditor.selectedRow < (int)table.rows.size()) {

                ImGui::Separator();

                auto& editTable = state.gdaEditor.editor->table();
                auto& editRow = editTable.rows[state.gdaEditor.selectedRow];

                bool modified = false;

                ImGui::BeginChild("RowEditor", ImVec2(0, 0), true);

                for (size_t colIdx = 0; colIdx < editTable.columns.size(); colIdx++) {
                    if (colIdx >= editRow.values.size()) continue;

                    const auto& col = editTable.columns[colIdx];
                    auto& val = editRow.values[colIdx];

                    ImGui::PushID(static_cast<int>(colIdx));

                    if (std::holds_alternative<int32_t>(val)) {
                        int intVal = std::get<int32_t>(val);
                        if (ImGui::InputInt(col.name.c_str(), &intVal)) {
                            val = static_cast<int32_t>(intVal);
                            modified = true;
                        }
                    } else if (std::holds_alternative<float>(val)) {
                        float floatVal = std::get<float>(val);
                        if (ImGui::InputFloat(col.name.c_str(), &floatVal)) {
                            val = floatVal;
                            modified = true;
                        }
                    } else if (std::holds_alternative<std::string>(val)) {
                        std::string strVal = std::get<std::string>(val);
                        char buf[256];
                        strncpy(buf, strVal.c_str(), sizeof(buf) - 1);
                        buf[sizeof(buf) - 1] = 0;
                        if (ImGui::InputText(col.name.c_str(), buf, sizeof(buf))) {
                            val = std::string(buf);
                            modified = true;
                        }
                    } else if (std::holds_alternative<bool>(val)) {
                        bool boolVal = std::get<bool>(val);
                        if (ImGui::Checkbox(col.name.c_str(), &boolVal)) {
                            val = boolVal;
                            modified = true;
                        }
                    }

                    ImGui::PopID();
                }

                ImGui::EndChild();

                if (modified) {
                    state.gdaEditor.editor->setModified(true);
                }
            }
        } else {
            ImGui::TextWrapped(
                "No GDA file loaded.\n\n"
                "To open a GDA file:\n"
                "1. Select 2da.erf in the ERF Browser\n"
                "2. Double-click any .gda file to open it here"
            );
        }
    }
    ImGui::End();

    if (state.gdaEditor.showRestoreDialog) {
        ImGui::OpenPopup("RestoreBackup?");
        state.gdaEditor.showRestoreDialog = false;
    }

    if (ImGui::BeginPopupModal("RestoreBackup?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Warning!");
        ImGui::Text("This will overwrite the current file with the backup.");
        ImGui::Separator();

        if (ImGui::Button("Restore", ImVec2(100, 0))) {
            fs::path backupDir = fs::current_path() / GDA_BACKUP_DIR;
            if (GDAFile::restoreBackup(state.gdaEditor.currentFile, backupDir.string())) {
                if (state.gdaEditor.editor->load(state.gdaEditor.currentFile)) {
                    state.gdaEditor.selectedRow = -1;
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}