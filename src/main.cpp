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
#include <atomic>

#include "Common.hpp"
#include "SecondaryConfig.hpp"
#include "MusicPlayerCore.hpp"
#include "FilePicker.hpp"
#include "BallSpeedTracker.hpp"

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
    explicit BallanceMusicPlayer(IBML *bml) : IMod(bml), m_SpeedTracker(bml) {}

    const char *GetID() override { return "MusicPlayer"; }
    const char *GetVersion() override { return "0.2.0"; }
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
        internalCfg["Speed"] = std::to_string(m_ManualSpeed);
        internalCfg["KeepPitch"] = m_Player.GetKeepPitch() ? "true" : "false";
        
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
        pHotkey->SetDefaultKey(CKKEY_BACKSLASH); // Default: Backslash key (scan code 43)
        
        CKKEYBOARD keyVal = static_cast<CKKEYBOARD>(0);
        if (pHotkey->GetType() == IProperty::KEY) {
            keyVal = pHotkey->GetKey();
        } else if (pHotkey->GetType() == IProperty::INTEGER) {
            keyVal = static_cast<CKKEYBOARD>(pHotkey->GetInteger());
        }
        if (keyVal == static_cast<CKKEYBOARD>(0)) {
            keyVal = CKKEY_BACKSLASH;
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

        // Register SlidingRheostat config category
        GetConfig()->SetCategoryComment("SlidingRheostat", "Easter egg: dynamic playback speed based on ball speed.");
        
        IProperty* pRheostatMode = GetConfig()->GetProperty("SlidingRheostat", "Mode");
        char commentBuf[256];
        snprintf(commentBuf, sizeof(commentBuf), "%d = Disabled, %d = Linear model, %d = Sliding window model, %d = Square root model, %d = Logarithmic model, %d = Squared model (maximum sensitivity)",
                 RHEOSTAT_DISABLED, RHEOSTAT_LINEAR, RHEOSTAT_SLIDING_WINDOW, RHEOSTAT_SQUARE_ROOT, RHEOSTAT_LOGARITHMIC, RHEOSTAT_SQUARED);
        pRheostatMode->SetComment(commentBuf);
        pRheostatMode->SetDefaultInteger(RHEOSTAT_DISABLED);
        m_RheostatMode = static_cast<RheostatMode>(pRheostatMode->GetInteger());

        IProperty* pTestInterval = GetConfig()->GetProperty("SlidingRheostat", "SpeedTestInterval");
        pTestInterval->SetComment("Minimum update interval for speed calculation (milliseconds)");
        pTestInterval->SetDefaultFloat(100.0f);
        float testInterval = pTestInterval->GetFloat();

        IProperty* pEasingRate = GetConfig()->GetProperty("SlidingRheostat", "EasingRate");
        pEasingRate->SetComment("Easing rate for speed changes (0 = instant change, 1 = maximum easing so speed never changes)");
        pEasingRate->SetDefaultFloat(0.92f);
        m_EasingRate = std::clamp(pEasingRate->GetFloat(), 0.0f, 1.0f);

        IProperty* pParam1 = GetConfig()->GetProperty("SlidingRheostat", "ExtraParameter1");
        pParam1->SetComment("For linear model: reference/default speed of the ball (units/second, default 25.0). For sliding window model: history average window size/weight (default 15.0)");
        pParam1->SetDefaultFloat(25.0f);
        float param1 = pParam1->GetFloat();

        IProperty* pParam2 = GetConfig()->GetProperty("SlidingRheostat", "ExtraParameter2");
        pParam2->SetComment("For linear model: slope value for speed scaling (default 0.02). For sliding window model: sensitivity/slope for acceleration scaling (default 0.02)");
        pParam2->SetDefaultFloat(0.02f);
        float param2 = pParam2->GetFloat();

        m_SpeedTracker.SetConfigValues(m_RheostatMode, testInterval, param1, param2);

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
        if (internalCfg.count("KeepPitch")) {
            m_Player.SetKeepPitch(internalCfg["KeepPitch"] == "true");
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
            m_ManualSpeed = std::stof(internalCfg["Speed"]);
        } else {
            m_ManualSpeed = 1.0f;
        }
        m_Player.SetSpeed(m_ManualSpeed);

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

    void OnPostStartMenu() override {
        if (m_Init) return;
        // fix for cursor glitch by delaying the player
        m_BML->AddTimer(1024.0f, [this]() {
            m_Init = true;
        });
    }

    void OnModifyConfig(const char *category, const char *key, IProperty *prop) override {
        if (strcmp(category, "SlidingRheostat") == 0) {
            m_RheostatMode = static_cast<RheostatMode>(GetConfig()->GetProperty("SlidingRheostat", "Mode")->GetInteger());
            float testInterval = GetConfig()->GetProperty("SlidingRheostat", "SpeedTestInterval")->GetFloat();
            m_EasingRate = std::clamp(GetConfig()->GetProperty("SlidingRheostat", "EasingRate")->GetFloat(), 0.0f, 1.0f);
            float param1 = GetConfig()->GetProperty("SlidingRheostat", "ExtraParameter1")->GetFloat();
            float param2 = GetConfig()->GetProperty("SlidingRheostat", "ExtraParameter2")->GetFloat();
            m_SpeedTracker.SetConfigValues(m_RheostatMode, testInterval, param1, param2);
        } else {
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
    }

    void OnProcess() override {
        if (!g_MusicPlayerEnabled || !m_Init) return;

        // Detect toggle hotkey press
        if (m_BML->GetInputManager()->oIsKeyPressed(m_Hotkey)) {
            g_MusicPlayerOpen = !g_MusicPlayerOpen;
        }

        float targetSpeed = m_ManualSpeed;
        if (m_RheostatMode != RHEOSTAT_DISABLED) {
            m_SpeedTracker.Update();
            float rawTarget = m_SpeedTracker.GetTargetPlaybackSpeed(m_ManualSpeed);
            // EasingRate of 0 means k = 1 (instant), EasingRate of 1 means k = 0 (never changes)
            float k = 1.0f - m_EasingRate;
            m_TargetPlaybackSpeed = m_TargetPlaybackSpeed + k * (rawTarget - m_TargetPlaybackSpeed);
            targetSpeed = m_TargetPlaybackSpeed;
        } else {
            m_TargetPlaybackSpeed = m_ManualSpeed;
            targetSpeed = m_ManualSpeed;
        }

        // Only call SetSpeed if it changes by more than a threshold
        if (std::abs(targetSpeed - m_LastSetSpeed) > 0.0001f) {
            m_Player.SetSpeed(targetSpeed);
            m_LastSetSpeed = targetSpeed;
        }

        DrawUI();
    }

    void DrawUI() {
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
            bool pickerDisabled = m_BML->IsPlaying();
            if (pickerDisabled) ImGui::BeginDisabled();
            if (m_FilePicker.Draw(pickerTitle.c_str(), m_PickerTempPath)) {
                if (m_FilePicker.GetMode() == FilePicker::MODE_FILE) {
                    m_Player.LoadSingleFile(m_PickerTempPath, true);
                } else {
                    m_Player.LoadDirectory(m_PickerTempPath, true);
                }
                SaveConfig();
            }
            if (pickerDisabled) ImGui::EndDisabled();
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

        bool uiDisabled = m_BML->IsPlaying();
        float currentOpacity = uiDisabled ? (m_Opacity * 0.6f) : m_Opacity;

        if (uiDisabled) {
            ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 0.60f); // make text/items more transparent when disabled
        }

        float textAlpha = (currentOpacity * 1.5f > 1.0f) ? 1.0f : (currentOpacity * 1.5f);
        // Push transparency style colors for child elements and title bar text
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.96f, 0.98f, textAlpha));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.20f, 0.20f, 0.25f, currentOpacity * 0.8f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.30f, 0.30f, 0.40f, currentOpacity * 0.9f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.35f, 0.35f, 0.45f, currentOpacity * 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.18f, 0.18f, 0.22f, currentOpacity * 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.25f, 0.25f, 0.35f, currentOpacity * 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.35f, currentOpacity * 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.48f, currentOpacity * 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.45f, 0.45f, 0.60f, currentOpacity * 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.25f, 0.30f, currentOpacity * 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.18f, currentOpacity * 0.7f));

        // Floating music player panel
        ImGui::SetNextWindowSize(ImVec2(400.0f * scale, 0), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(currentOpacity);
        if (ImGui::Begin("Music Player", &g_MusicPlayerOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::SetWindowFontScale(m_FontScale);

            if (uiDisabled) {
                ImGui::BeginDisabled();
            }

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
            float speed = m_ManualSpeed;
            int speedIdx = GetSpeedIndex(speed);
            if (ComboWithFontScale("##SpeedCombo", &speedIdx, SPEED_LABELS, SPEED_COUNT, m_FontScale * 0.8f)) {
                m_ManualSpeed = SPEED_VALUES[speedIdx];
                SaveConfig();
            }
            ImGui::PopItemWidth();

            // Volume
            float volume = m_Player.GetVolume() * 100.0f;
            if (m_RheostatMode != RHEOSTAT_DISABLED) {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Rheostat: %.1f m/s -> %.2fx", m_SpeedTracker.GetCurrentSpeed(), m_TargetPlaybackSpeed);
            }
            ImGui::Text("Vol:");
            ImGui::SameLine();
            ImGui::PushItemWidth(120.0f * scale);
            if (ImGui::SliderFloat("##Volume", &volume, 0.0f, 100.0f, "%.0f%%")) {
                m_Player.SetVolume(volume / 100.0f);
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                SaveConfig();
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            bool keepPitch = m_Player.GetKeepPitch();
            if (ImGui::Checkbox("Keep Pitch", &keepPitch)) {
                m_Player.SetKeepPitch(keepPitch);
                SaveConfig();
            }

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
            if (uiDisabled) {
                ImGui::EndDisabled();
                ImGui::PopStyleVar();
            }
        }
        ImGui::End();
        ImGui::PopStyleColor(11);
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

    CKKEYBOARD m_Hotkey = CKKEY_BACKSLASH;
    float m_FontScale = 0.85f;
    float m_Opacity = 0.60f;

    std::atomic_bool m_Init = false;
    float m_ManualSpeed = 1.0f;
    float m_TargetPlaybackSpeed = 1.0f;
    float m_LastSetSpeed = -1.0f;
    float m_EasingRate = 0.05f;
    RheostatMode m_RheostatMode = RHEOSTAT_DISABLED;
    BallSpeedTracker m_SpeedTracker;
};

MOD_EXPORT IMod *BMLEntry(IBML *bml) { return new BallanceMusicPlayer(bml); }
MOD_EXPORT void BMLExit(IMod *mod) { delete mod; }
