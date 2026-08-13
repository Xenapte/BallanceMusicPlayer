#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include "imgui.h"
#include "Common.hpp"

class FilePicker {
public:
    enum Mode {
        MODE_FILE,
        MODE_DIRECTORY
    };

    FilePicker() : m_IsOpen(false), m_Mode(MODE_FILE) {}

    void Open(Mode mode, const std::string& startPath = "") {
        m_Mode = mode;
        m_IsOpen = true;
        m_SelectedPath.clear();
        m_SearchFilter[0] = '\0';
        
        std::filesystem::path defaultStart = std::filesystem::current_path().parent_path();
        
        if (!startPath.empty()) {
            try {
                std::filesystem::path p = Utf8ToPath(startPath);
                if (std::filesystem::exists(p)) {
                    if (std::filesystem::is_directory(p)) {
                        m_CurrentPath = p;
                    } else {
                        m_CurrentPath = p.parent_path();
                        m_SelectedPath = p;
                    }
                } else {
                    m_CurrentPath = defaultStart;
                }
            } catch (...) {
                m_CurrentPath = defaultStart;
            }
        } else {
            m_CurrentPath = defaultStart;
        }
        UpdateList();
    }

    void Close() {
        m_IsOpen = false;
    }

    bool IsOpen() const { return m_IsOpen; }
    Mode GetMode() const { return m_Mode; }
    void SetFontScale(float scale) { m_FontScale = scale; }

    bool Draw(const char* title, std::string& outPath) {
        if (!m_IsOpen) return false;

        bool confirmed = false;
        ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(title, &m_IsOpen, ImGuiWindowFlags_NoCollapse)) {
            ImGui::SetWindowFontScale(m_FontScale);
            // Address bar and Navigation Up
            char pathBuf[1024];
            std::string pathStr = PathToUtf8(m_CurrentPath);
            strncpy(pathBuf, pathStr.c_str(), sizeof(pathBuf));
            pathBuf[sizeof(pathBuf) - 1] = '\0';
            
            ImGui::Text("Current Folder:");
            ImGui::PushItemWidth(-70.0f);
            if (ImGui::InputText("##PathInput", pathBuf, sizeof(pathBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                std::filesystem::path newPath = Utf8ToPath(pathBuf);
                if (std::filesystem::exists(newPath) && std::filesystem::is_directory(newPath)) {
                    m_CurrentPath = std::filesystem::canonical(newPath);
                    UpdateList();
                }
            }
            ImGui::PopItemWidth();
            
            ImGui::SameLine();
            if (ImGui::Button("Up")) {
                if (m_CurrentPath.has_parent_path() && m_CurrentPath != m_CurrentPath.root_path()) {
                    m_CurrentPath = m_CurrentPath.parent_path();
                    UpdateList();
                }
            }

            // Search filter
            ImGui::Text("Filter:");
            ImGui::PushItemWidth(-10.0f);
            ImGui::InputText("##SearchFilter", m_SearchFilter, sizeof(m_SearchFilter));
            ImGui::PopItemWidth();

            ImGui::Separator();

            // List child window
            float listHeight = ImGui::GetContentRegionAvail().y - 45.0f;
            if (ImGui::BeginChild("FilesChildList", ImVec2(0, listHeight), true)) {
                ImGui::SetWindowFontScale(m_FontScale * 0.8f);
                std::string filterStr(m_SearchFilter);
                std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

                // Directories list
                for (const auto& dir : m_Directories) {
                    std::string dirName = dir.filename().string();
                    std::string dirLower = dirName;
                    std::transform(dirLower.begin(), dirLower.end(), dirLower.begin(), ::tolower);

                    if (!filterStr.empty() && dirLower.find(filterStr) == std::string::npos) {
                        continue;
                    }

                    std::string label = "[Folder] " + dirName;
                    bool isSelected = (m_SelectedPath == dir);
                    if (ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                        m_SelectedPath = dir;
                        if (ImGui::IsMouseDoubleClicked(0)) {
                            m_CurrentPath = dir;
                            m_SelectedPath.clear();
                            UpdateList();
                            break;
                        }
                    }
                }

                // Files list
                for (const auto& file : m_Files) {
                    std::string fileName = file.filename().string();
                    std::string fileLower = fileName;
                    std::transform(fileLower.begin(), fileLower.end(), fileLower.begin(), ::tolower);

                    if (!filterStr.empty() && fileLower.find(filterStr) == std::string::npos) {
                        continue;
                    }

                    std::string label = "[File] " + fileName;
                    bool isSelected = (m_SelectedPath == file);
                    if (ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                        m_SelectedPath = file;
                        if (ImGui::IsMouseDoubleClicked(0) && m_Mode == MODE_FILE) {
                            outPath = PathToUtf8(m_SelectedPath);
                            confirmed = true;
                            m_IsOpen = false;
                            break;
                        }
                    }
                }
            }
            ImGui::EndChild();

            ImGui::Separator();

            // Footer controls
            if (m_Mode == MODE_DIRECTORY) {
                if (ImGui::Button("Select This Directory")) {
                    outPath = PathToUtf8(m_CurrentPath);
                    confirmed = true;
                    m_IsOpen = false;
                }
                ImGui::SameLine();
            }

            bool canSelect = false;
            if (m_Mode == MODE_FILE) {
                canSelect = !m_SelectedPath.empty() && std::filesystem::is_regular_file(m_SelectedPath);
            } else {
                canSelect = !m_SelectedPath.empty() && std::filesystem::is_directory(m_SelectedPath);
            }

            if (!canSelect) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("OK")) {
                if (m_Mode == MODE_FILE) {
                    outPath = PathToUtf8(m_SelectedPath);
                } else {
                    outPath = PathToUtf8(m_SelectedPath);
                }
                confirmed = true;
                m_IsOpen = false;
            }
            if (!canSelect) {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                m_IsOpen = false;
            }

            ImGui::SameLine();
            if (!m_SelectedPath.empty()) {
                ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "Selected: %s", m_SelectedPath.filename().string().c_str());
            } else {
                ImGui::Text("No Selection");
            }
        }
        ImGui::End();

        return confirmed;
    }

private:
    void UpdateList() {
        m_Directories.clear();
        m_Files.clear();
        try {
            if (std::filesystem::exists(m_CurrentPath) && std::filesystem::is_directory(m_CurrentPath)) {
                for (const auto& entry : std::filesystem::directory_iterator(m_CurrentPath)) {
                    if (entry.is_directory()) {
                        m_Directories.push_back(entry.path());
                    } else if (entry.is_regular_file()) {
                        std::string ext = entry.path().extension().string();
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        if (ext == ".mp3" || ext == ".wav" || ext == ".wma" || ext == ".mid" || ext == ".midi") {
                            m_Files.push_back(entry.path());
                        }
                    }
                }
                // Sort
                std::sort(m_Directories.begin(), m_Directories.end());
                std::sort(m_Files.begin(), m_Files.end());
            }
        } catch (...) {}
    }

    bool m_IsOpen;
    Mode m_Mode;
    std::filesystem::path m_CurrentPath;
    std::filesystem::path m_SelectedPath;
    std::vector<std::filesystem::path> m_Directories;
    std::vector<std::filesystem::path> m_Files;
    char m_SearchFilter[128];
    float m_FontScale = 1.0f;
};
