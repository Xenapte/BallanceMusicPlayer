#include "MusicPlayerUI.hpp"
#include <cmath>

extern bool g_MusicPlayerOpen;

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

MusicPlayerUI::MusicPlayerUI() {}

void MusicPlayerUI::Draw(IBML* bml, MusicPlayerCore& player, BallSpeedTracker& speedTracker,
                         float& manualSpeed, float targetPlaybackSpeed, RheostatMode rheostatMode,
                         float fontScale, float opacity, bool blockKeyboardIngame, bool blockMouseIngame,
                         std::function<void()> saveConfigCallback) {
    player.Update();

    Bui::ImGuiContextScope scope;

    bool isPlaying = bml->IsPlaying();
    bool keyboardBlocked = blockKeyboardIngame && isPlaying;

    ImGuiIO& io = ImGui::GetIO();
    if (keyboardBlocked) {
        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
        io.ClearInputKeys();
    } else {
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    }

    if (m_FirstRender) {
        ApplyDarkTheme();
        m_FirstRender = false;
    }

    // Calculate dynamic scaling factor based on game window width and custom font scale
    float scale = (ImGui::GetIO().DisplaySize.x / 1600.0f) * (fontScale / 0.85f);

    // Draw file picker window if open
    if (m_FilePicker.IsOpen()) {
        m_FilePicker.SetFontScale(fontScale);
        std::string pickerTitle = (m_FilePicker.GetMode() == FilePicker::MODE_FILE) ? "Select Music File" : "Select Music Folder";
        bool pickerDisabled = blockMouseIngame && bml->IsPlaying();
        if (pickerDisabled) ImGui::BeginDisabled();
        if (m_FilePicker.Draw(pickerTitle.c_str(), m_PickerTempPath)) {
            if (m_FilePicker.GetMode() == FilePicker::MODE_FILE) {
                player.LoadSingleFile(m_PickerTempPath, true);
            } else {
                player.LoadDirectory(m_PickerTempPath, true);
            }
            saveConfigCallback();
        }
        if (pickerDisabled) ImGui::EndDisabled();
    }

    if (!g_MusicPlayerOpen) return;

    if (!player.IsEngineInitialized()) {
        ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Music Player", &g_MusicPlayerOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::SetWindowFontScale(fontScale);
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Audio Engine init failed!");
        }
        ImGui::End();
        return;
    }

    bool mouseBlocked = blockMouseIngame && isPlaying;
    bool useDynamicTrans = isPlaying && !m_WindowHovered;
    float currentOpacity = useDynamicTrans ? (opacity * 0.6f) : opacity;

    if (useDynamicTrans) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.50f);
    } else if (mouseBlocked) {
        ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 0.60f);
    }

    float textAlpha = (currentOpacity * 1.5f > 1.0f) ? 1.0f : (currentOpacity * 1.5f);
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

    ImGui::SetNextWindowSize(ImVec2(400.0f * scale, 0), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(currentOpacity);
    if (ImGui::Begin("Music Player", &g_MusicPlayerOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetWindowFontScale(fontScale);

        if (mouseBlocked) {
            ImGui::BeginDisabled();
        }

        // Track info
        ImGui::Text("Now Playing:");
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", player.GetCurrentSongName().c_str());

        ImGui::Separator();

        // Progress Slider
        int curPos = m_Seeking ? m_SeekTargetMs : player.GetPositionMs();
        int duration = player.GetDurationMs();
        float progress = duration > 0 ? static_cast<float>(curPos) / duration : 0.0f;

        char timeStr[64];
        snprintf(timeStr, sizeof(timeStr), "%s / %s", FormatTime(curPos).c_str(), FormatTime(duration).c_str());
        
        ImGui::PushItemWidth(-1.0f);
        if (ImGui::SliderFloat("##Progress", &progress, 0.0f, 1.0f, timeStr)) {
            m_Seeking = true;
            m_SeekTargetMs = static_cast<int>(progress * duration);
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            player.SeekTo(m_SeekTargetMs);
            m_Seeking = false;
        }
        ImGui::PopItemWidth();

        // Controls
        if (ImGui::Button("|<")) {
            player.PlayPrev();
        }
        ImGui::SameLine();
        
        if (player.IsPlaying() && !player.IsPaused()) {
            if (ImGui::Button("Pause")) {
                player.Pause();
            }
        } else {
            if (ImGui::Button("Play")) {
                player.Play();
            }
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Stop")) {
            player.Stop();
        }
        ImGui::SameLine();
        
        if (ImGui::Button(">|")) {
            player.PlayNext();
        }

        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();

        bool shuffle = player.GetShuffle();
        if (ImGui::Checkbox("Shuffle", &shuffle)) {
            player.SetShuffle(shuffle);
            saveConfigCallback();
        }

        // Loop Modes & Speed Selection
        int repeatMode = player.GetRepeatMode();
        const char* repeatModes[] = { "Repeat Off", "Repeat One", "Repeat All" };
        ImGui::Text("Loop:");
        ImGui::SameLine();
        ImGui::PushItemWidth(110.0f * scale);
        if (ComboWithFontScale("##RepeatMode", &repeatMode, repeatModes, 3, fontScale * 0.8f)) {
            player.SetRepeatMode(repeatMode);
            saveConfigCallback();
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();
        ImGui::Text(" Speed:");
        ImGui::SameLine();
        ImGui::PushItemWidth(90.0f * scale);
        float speed = manualSpeed;
        int speedIdx = GetSpeedIndex(speed);
        if (ComboWithFontScale("##SpeedCombo", &speedIdx, SPEED_LABELS, SPEED_COUNT, fontScale * 0.8f)) {
            manualSpeed = SPEED_VALUES[speedIdx];
            saveConfigCallback();
        }
        ImGui::PopItemWidth();

        // Volume
        float volume = player.GetVolume() * 100.0f;
        if (rheostatMode != RHEOSTAT_DISABLED) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Rheostat: %.1f m/s -> %.2fx", speedTracker.GetCurrentSpeed(), targetPlaybackSpeed);
        }
        ImGui::Text("Vol:");
        ImGui::SameLine();
        ImGui::PushItemWidth(120.0f * scale);
        if (ImGui::SliderFloat("##Volume", &volume, 0.0f, 100.0f, "%.0f%%")) {
            player.SetVolume(volume / 100.0f);
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            saveConfigCallback();
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();
        bool keepPitch = player.GetKeepPitch();
        if (ImGui::Checkbox("Keep Pitch", &keepPitch)) {
            player.SetKeepPitch(keepPitch);
            saveConfigCallback();
        }

        ImGui::Separator();

        // Directory / File Loading
        if (ImGui::Button("Load Song...")) {
            m_FilePicker.Open(FilePicker::MODE_FILE, player.GetLastPath());
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Folder...")) {
            m_FilePicker.Open(FilePicker::MODE_DIRECTORY, player.GetLastPath());
        }

        // Playlist Selection list
        const auto& playlist = player.GetPlaylist();
        const auto& playlistNames = player.GetPlaylistNames();
        if (!playlist.empty()) {
            ImGui::Separator();
            ImGui::Text("Playlist (%d tracks):", (int)playlist.size());
            if (ImGui::BeginChild("PlaylistListChild", ImVec2(0, 110.0f * scale), true)) {
                ImGui::SetWindowFontScale(fontScale * 0.8f);
                
                int activeIdx = player.GetCurrentPlaylistIndex();
                for (int i = 0; i < (int)playlist.size(); ++i) {
                    const std::string& name = playlistNames[i];
                    bool isCurrent = (i == activeIdx);
                    if (isCurrent) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
                    }
                    if (ImGui::Selectable(name.c_str(), isCurrent)) {
                        player.PlaySequentialIndex(i);
                        saveConfigCallback();
                    }
                    if (isCurrent) {
                        ImGui::PopStyleColor();
                    }
                }
            }
            ImGui::EndChild();
        }
        m_WindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

        if (mouseBlocked) {
            ImGui::EndDisabled();
        }
        if (useDynamicTrans || mouseBlocked) {
            ImGui::PopStyleVar();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(11);
}
