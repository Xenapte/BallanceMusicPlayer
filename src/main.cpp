#include <BML/IMod.h>
#include <BML/Bui.h>
#include <BML/IConfig.h>
#include <BML/Defines.h>

#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <map>

#include "Common.hpp"
#include "SecondaryConfig.hpp"
#include "MusicPlayerCore.hpp"
#include "FilePicker.hpp"

// Globals for external toggle / API access
static bool g_MusicPlayerEnabled = true;
static bool g_MusicPlayerOpen = true;

MOD_EXPORT void SetMusicPlayerEnabled(bool enabled) {
    g_MusicPlayerEnabled = enabled;
}

MOD_EXPORT bool IsMusicPlayerEnabled() {
    return g_MusicPlayerEnabled;
}

MOD_EXPORT void SetMusicPlayerOpen(bool open) {
    g_MusicPlayerOpen = open;
}

MOD_EXPORT bool IsMusicPlayerOpen() {
    return g_MusicPlayerOpen;
}

MOD_EXPORT void ToggleMusicPlayerOpen() {
    g_MusicPlayerOpen = !g_MusicPlayerOpen;
}

static const float SPEED_VALUES[] = { 0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f, 3.0f, 4.0f };
static const char* SPEED_LABELS[] = { "0.25x", "0.50x", "0.75x", "1.00x", "1.25x", "1.50x", "2.00x", "3.00x", "4.00x" };
constexpr int SPEED_COUNT = sizeof(SPEED_VALUES) / sizeof(SPEED_VALUES[0]);

static int GetSpeedIndex(float speed) {
    int closestIdx = 3; // Default to 1.0x (index 3)
    float minDiff = 999.0f;
    for (int i = 0; i < SPEED_COUNT; ++i) {
        float diff = std::abs(SPEED_VALUES[i] - speed);
        if (diff < minDiff) {
            minDiff = diff;
            closestIdx = i;
        }
    }
    return closestIdx;
}

static std::string FormatTime(int ms) {
    int totalSec = ms / 1000;
    int min = totalSec / 60;
    int sec = totalSec % 60;
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d", min, sec);
    return buf;
}

static void ApplyDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.11f, 0.11f, 0.13f, 0.95f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.11f, 0.11f, 0.13f, 0.98f);
    colors[ImGuiCol_Border]                 = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.30f, 0.30f, 0.40f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.35f, 0.35f, 0.45f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.25f, 0.25f, 0.35f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.30f, 0.30f, 0.40f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.40f, 0.40f, 0.50f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.50f, 0.50f, 0.60f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.50f, 0.55f, 0.90f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.45f, 0.50f, 0.85f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.55f, 0.60f, 0.95f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.25f, 0.25f, 0.35f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.35f, 0.35f, 0.48f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.45f, 0.45f, 0.60f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.25f, 0.25f, 0.35f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.35f, 0.35f, 0.48f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.45f, 0.45f, 0.60f, 1.00f);
    colors[ImGuiCol_Separator]              = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.35f, 0.35f, 0.45f, 1.00f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.45f, 0.45f, 0.60f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.35f, 0.35f, 0.45f, 1.00f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.45f, 0.45f, 0.60f, 1.00f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.35f, 0.35f, 0.48f, 1.00f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.30f, 0.30f, 0.45f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
}

static bool ComboWithFontScale(const char* label, int* current_item, const char* const items[], int items_count, float scale) {
    bool value_changed = false;
    if (ImGui::BeginCombo(label, items[*current_item])) {
        ImGui::SetWindowFontScale(scale);
        for (int i = 0; i < items_count; i++) {
            const bool is_selected = (*current_item == i);
            if (ImGui::Selectable(items[i], is_selected)) {
                *current_item = i;
                value_changed = true;
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    return value_changed;
}

class BallanceMusicPlayer final : public IMod {
public:
    explicit BallanceMusicPlayer(IBML *bml) : IMod(bml) {}

    const char *GetID() override { return "MusicPlayer"; }
    const char *GetVersion() override { return "0.1.1"; }
    const char* GetName() override { return "Music Player"; }
    const char *GetAuthor() override { return "BallanceBug"; }
    const char *GetDescription() override { return "Standalone Music player mod based on ImGui and Miniaudio."; }
    DECLARE_BML_VERSION;

private:
    std::string GetInternalConfigPath() {
        auto configDir = std::filesystem::current_path().parent_path() / "ModLoader" / "Configs";
        std::filesystem::create_directories(configDir);
        return PathToUtf8(configDir / "MusicPlayer_Internal.ini");
    }

    void SaveConfig() {
        // Save Native BML config properties
        GetConfig()->GetProperty("Core", "WindowToggleHotkey")->SetKey(m_Hotkey);
        GetConfig()->GetProperty("Aesthetics", "FontScale")->SetFloat(m_FontScale);
        GetConfig()->GetProperty("Aesthetics", "Opacity")->SetFloat(m_Opacity);

        // Save internal states to secondary config
        std::map<std::string, std::string> internalCfg;
        internalCfg["Volume"] = std::to_string(m_Player.GetVolume());
        internalCfg["Shuffle"] = m_Player.GetShuffle() ? "true" : "false";
        internalCfg["RepeatMode"] = std::to_string(m_Player.GetRepeatMode());
        internalCfg["WindowOpen"] = g_MusicPlayerOpen ? "true" : "false";
        internalCfg["LastPath"] = m_Player.GetLastPath();
        internalCfg["LastSongPath"] = m_Player.GetCurrentSongPath();
        internalCfg["IsDirMode"] = m_Player.IsDirectoryMode() ? "true" : "false";
        internalCfg["Speed"] = std::to_string(m_Player.GetSpeed());
        
        SecondaryConfig::Save(GetInternalConfigPath(), internalCfg);
    }

    void OnLoad() override {
        // Register Native BML config properties
        IProperty* pEnabled = GetConfig()->GetProperty("Core", "Enabled");
        pEnabled->SetComment("Whether the music player mod is enabled");
        pEnabled->SetDefaultBoolean(true);
        g_MusicPlayerEnabled = pEnabled->GetBoolean();

        IProperty* pHotkey = GetConfig()->GetProperty("Core", "WindowToggleHotkey");
        pHotkey->SetComment("Hotkey to toggle the music player UI");
        pHotkey->SetDefaultKey(CKKEY_M); // Default: M key (scan code 50)
        
        CKKEYBOARD keyVal = static_cast<CKKEYBOARD>(0);
        if (pHotkey->GetType() == IProperty::KEY) {
            keyVal = pHotkey->GetKey();
        } else if (pHotkey->GetType() == IProperty::INTEGER) {
            keyVal = static_cast<CKKEYBOARD>(pHotkey->GetInteger());
        }
        if (keyVal == static_cast<CKKEYBOARD>(0)) {
            keyVal = CKKEY_M;
        }
        m_Hotkey = keyVal;

        IProperty* pFontScale = GetConfig()->GetProperty("Aesthetics", "FontScale");
        pFontScale->SetComment("Font scale for the music player UI");
        pFontScale->SetDefaultFloat(0.85f);
        m_FontScale = pFontScale->GetFloat();

        IProperty* pOpacity = GetConfig()->GetProperty("Aesthetics", "Opacity");
        pOpacity->SetComment("Opacity (background alpha) of the music player window");
        pOpacity->SetDefaultFloat(0.80f);
        m_Opacity = pOpacity->GetFloat();

        // Load internal secondary configurations
        std::string configPath = GetInternalConfigPath();
        auto internalCfg = SecondaryConfig::Load(configPath);
        
        if (internalCfg.count("Volume")) {
            m_Player.SetVolume(std::stof(internalCfg["Volume"]));
        } else {
            m_Player.SetVolume(1.0f); // Default volume 100%
        }
        if (internalCfg.count("Shuffle")) {
            m_Player.SetShuffle(internalCfg["Shuffle"] == "true");
        }
        if (internalCfg.count("RepeatMode")) {
            m_Player.SetRepeatMode(std::stoi(internalCfg["RepeatMode"]));
        }
        if (internalCfg.count("WindowOpen")) {
            g_MusicPlayerOpen = (internalCfg["WindowOpen"] == "true");
        } else {
            g_MusicPlayerOpen = true; // Default open to true
        }
        if (internalCfg.count("Speed")) {
            m_Player.SetSpeed(std::stof(internalCfg["Speed"]));
        }

        std::string lastPath = internalCfg["LastPath"];
        std::string lastSongPath = internalCfg["LastSongPath"];
        bool isDir = (internalCfg["IsDirMode"] == "true");
        if (!lastPath.empty()) {
            if (isDir) {
                if (!lastSongPath.empty()) {
                    m_Player.LoadDirectoryAndSetSong(lastPath, lastSongPath, false); // Restore exact active track
                } else {
                    m_Player.LoadDirectory(lastPath, false);
                }
            } else {
                m_Player.LoadSingleFile(lastPath, false);
            }
        }
    }

    void OnUnload() override {
        m_Player.Stop();
        SaveConfig();
    }

    void OnModifyConfig(const char *category, const char *key, IProperty *prop) override {
        if (strcmp(key, "Enabled") == 0) {
            g_MusicPlayerEnabled = prop->GetBoolean();
        } else if (strcmp(key, "WindowToggleHotkey") == 0) {
            if (prop->GetType() == IProperty::KEY) {
                m_Hotkey = prop->GetKey();
            } else if (prop->GetType() == IProperty::INTEGER) {
                m_Hotkey = static_cast<CKKEYBOARD>(prop->GetInteger());
            }
        } else if (strcmp(key, "FontScale") == 0) {
            m_FontScale = prop->GetFloat();
        } else if (strcmp(key, "Opacity") == 0) {
            m_Opacity = prop->GetFloat();
        }
    }

    void OnProcess() override {
        if (!g_MusicPlayerEnabled) return;

        InputHook* input = m_BML->GetInputManager();

        // Detect toggle hotkey press
        if (m_Hotkey != static_cast<CKKEYBOARD>(0)) {
            if (input->IsKeyPressed(m_Hotkey)) {
                g_MusicPlayerOpen = !g_MusicPlayerOpen;
            }
        }

        m_Player.Update();

        // RENDER IMGUI DIRECTLY IN ONPROCESS (BMLPlus ImGui frame is active here)
        Bui::ImGuiContextScope scope;

        if (m_FirstRender) {
            ApplyDarkTheme();
            m_FirstRender = false;
        }

        // Calculate dynamic scaling factor based on game window width and custom font scale
        float scale = (ImGui::GetIO().DisplaySize.x / 1600.0f) * (m_FontScale / 0.85f);

        // Draw file picker window if open
        if (m_FilePicker.IsOpen()) {
            m_FilePicker.SetFontScale(m_FontScale);
            std::string pickerTitle = (m_FilePicker.GetMode() == FilePicker::MODE_FILE) ? "Select Music File" : "Select Music Folder";
            if (m_FilePicker.Draw(pickerTitle.c_str(), m_PickerTempPath)) {
                if (m_FilePicker.GetMode() == FilePicker::MODE_FILE) {
                    m_Player.LoadSingleFile(m_PickerTempPath, true);
                } else {
                    m_Player.LoadDirectory(m_PickerTempPath, true);
                }
                SaveConfig();
            }
        }

        if (!g_MusicPlayerOpen) return;

        if (!m_Player.IsEngineInitialized()) {
            ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Music Player", &g_MusicPlayerOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::SetWindowFontScale(m_FontScale);
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Audio Engine init failed!");
            }
            ImGui::End();
            return;
        }

        // Push transparency style colors for child elements
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.20f, 0.20f, 0.25f, m_Opacity * 0.8f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.30f, 0.30f, 0.40f, m_Opacity * 0.9f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.35f, 0.35f, 0.45f, m_Opacity * 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.18f, 0.18f, 0.22f, m_Opacity * 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.25f, 0.25f, 0.35f, m_Opacity * 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.35f, m_Opacity * 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.48f, m_Opacity * 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.45f, 0.45f, 0.60f, m_Opacity * 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.25f, 0.30f, m_Opacity * 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.18f, m_Opacity * 0.7f));

        // Floating music player panel
        ImGui::SetNextWindowSize(ImVec2(400.0f * scale, 0), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(m_Opacity);
        if (ImGui::Begin("Music Player", &g_MusicPlayerOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::SetWindowFontScale(m_FontScale);

            // Track info
            ImGui::Text("Now Playing:");
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", m_Player.GetCurrentSongName().c_str());

            ImGui::Separator();

            // Progress Slider
            int curPos = m_Seeking ? m_SeekTargetMs : m_Player.GetPositionMs();
            int duration = m_Player.GetDurationMs();
            float progress = duration > 0 ? static_cast<float>(curPos) / duration : 0.0f;

            char timeStr[64];
            snprintf(timeStr, sizeof(timeStr), "%s / %s", FormatTime(curPos).c_str(), FormatTime(duration).c_str());
            
            ImGui::PushItemWidth(-1.0f);
            if (ImGui::SliderFloat("##Progress", &progress, 0.0f, 1.0f, timeStr)) {
                m_Seeking = true;
                m_SeekTargetMs = static_cast<int>(progress * duration);
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                m_Player.SeekTo(m_SeekTargetMs);
                m_Seeking = false;
            }
            ImGui::PopItemWidth();

            // Controls
            if (ImGui::Button("|<")) {
                m_Player.PlayPrev();
            }
            ImGui::SameLine();
            
            if (m_Player.IsPlaying() && !m_Player.IsPaused()) {
                if (ImGui::Button("Pause")) {
                    m_Player.Pause();
                }
            } else {
                if (ImGui::Button("Play")) {
                    m_Player.Play();
                }
            }
            ImGui::SameLine();
            
            if (ImGui::Button("Stop")) {
                m_Player.Stop();
            }
            ImGui::SameLine();
            
            if (ImGui::Button(">|")) {
                m_Player.PlayNext();
            }

            ImGui::SameLine();
            ImGui::Spacing();
            ImGui::SameLine();

            bool shuffle = m_Player.GetShuffle();
            if (ImGui::Checkbox("Shuffle", &shuffle)) {
                m_Player.SetShuffle(shuffle);
                SaveConfig();
            }

            // Loop Modes & Speed Selection (on the same line)
            int repeatMode = m_Player.GetRepeatMode();
            const char* repeatModes[] = { "Repeat Off", "Repeat One", "Repeat All" };
            ImGui::Text("Loop:");
            ImGui::SameLine();
            ImGui::PushItemWidth(110.0f * scale);
            if (ComboWithFontScale("##RepeatMode", &repeatMode, repeatModes, 3, m_FontScale * 0.8f)) {
                m_Player.SetRepeatMode(repeatMode);
                SaveConfig();
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            ImGui::Text(" Speed:");
            ImGui::SameLine();
            ImGui::PushItemWidth(90.0f * scale);
            float speed = m_Player.GetSpeed();
            int speedIdx = GetSpeedIndex(speed);
            if (ComboWithFontScale("##SpeedCombo", &speedIdx, SPEED_LABELS, SPEED_COUNT, m_FontScale * 0.8f)) {
                m_Player.SetSpeed(SPEED_VALUES[speedIdx]);
                SaveConfig();
            }
            ImGui::PopItemWidth();

            // Volume
            float volume = m_Player.GetVolume() * 100.0f;
            ImGui::Text("Volume:");
            ImGui::SameLine();
            ImGui::PushItemWidth(-1.0f);
            if (ImGui::SliderFloat("##Volume", &volume, 0.0f, 100.0f, "%.0f%%")) {
                m_Player.SetVolume(volume / 100.0f);
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                SaveConfig();
            }
            ImGui::PopItemWidth();

            ImGui::Separator();

            // Directory / File Loading
            if (ImGui::Button("Load Song...")) {
                m_FilePicker.Open(FilePicker::MODE_FILE, m_Player.GetLastPath());
            }
            ImGui::SameLine();
            if (ImGui::Button("Load Folder...")) {
                m_FilePicker.Open(FilePicker::MODE_DIRECTORY, m_Player.GetLastPath());
            }

            // Playlist Selection list
            const auto& playlist = m_Player.GetPlaylist();
            if (!playlist.empty()) {
                ImGui::Separator();
                ImGui::Text("Playlist (%d tracks):", (int)playlist.size());
                if (ImGui::BeginChild("PlaylistListChild", ImVec2(0, 110.0f * scale), true)) {
                    ImGui::SetWindowFontScale(m_FontScale * 0.8f); // Make list font size smaller
                    
                    int activeIdx = m_Player.GetCurrentPlaylistIndex();
                    for (int i = 0; i < (int)playlist.size(); ++i) {
                        std::string name = std::filesystem::path(Utf8ToPath(playlist[i])).filename().string();
                        bool isCurrent = (i == activeIdx);
                        if (isCurrent) {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
                        }
                        if (ImGui::Selectable(name.c_str(), isCurrent)) {
                            m_Player.PlaySequentialIndex(i);
                            SaveConfig();
                        }
                        if (isCurrent) {
                            ImGui::PopStyleColor();
                        }
                    }
                }
                ImGui::EndChild();
            }
        }
        ImGui::End();
        ImGui::PopStyleColor(10);
    }

    void OnRender(CK_RENDER_FLAGS flags) override {
        // ImGui draw calls must occur in OnProcess() during BML's active frame
    }

    MusicPlayerCore m_Player;
    FilePicker m_FilePicker;
    std::string m_PickerTempPath;
    
    bool m_FirstRender = true;
    bool m_Seeking = false;
    int m_SeekTargetMs = 0;

    CKKEYBOARD m_Hotkey = CKKEY_M;
    float m_FontScale = 0.85f;
    float m_Opacity = 0.60f;
};

MOD_EXPORT IMod *BMLEntry(IBML *bml) { return new BallanceMusicPlayer(bml); }
MOD_EXPORT void BMLExit(IMod *mod) { delete mod; }
