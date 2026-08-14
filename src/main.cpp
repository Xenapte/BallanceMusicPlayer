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
#include "MusicPlayerUI.hpp"

// Globals for external toggle / API access
static bool g_MusicPlayerEnabled = true;
bool g_MusicPlayerOpen = true;

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

class BallanceMusicPlayer final : public IMod {
public:
    explicit BallanceMusicPlayer(IBML *bml)
        : IMod(bml)
        , m_SpeedTracker(bml)
        , m_BlockKeyboardIngame(true)
        , m_BlockMouseIngame(false) {}

    const char *GetID() override { return "MusicPlayer"; }
    const char *GetVersion() override { return "0.2.0"; }
    const char *GetName() override { return "Music Player"; }
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
        // Save user-facing mod configs
        GetConfig()->GetProperty("Core", "BlockKeyboardIngame")->SetBoolean(m_BlockKeyboardIngame);
        GetConfig()->GetProperty("Core", "BlockMouseIngame")->SetBoolean(m_BlockMouseIngame);

        // Save internal play state configs to secondary config
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

    void LoadConfig() {
        // Load internal play state configs
        auto internalPath = GetInternalConfigPath();
        auto internalCfg = SecondaryConfig::Load(internalPath);

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
        m_TargetPlaybackSpeed = m_ManualSpeed;
        m_LastSetSpeed = m_ManualSpeed;
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

        IProperty* pBlockKeyboard = GetConfig()->GetProperty("Core", "BlockKeyboardIngame");
        pBlockKeyboard->SetComment("Block game keyboard input when the music player UI is open in-game");
        pBlockKeyboard->SetDefaultBoolean(true);
        m_BlockKeyboardIngame = pBlockKeyboard->GetBoolean();

        IProperty* pBlockMouse = GetConfig()->GetProperty("Core", "BlockMouseIngame");
        pBlockMouse->SetComment("Block game mouse input (camera/ball rotation) when the music player UI is open in-game");
        pBlockMouse->SetDefaultBoolean(false);
        m_BlockMouseIngame = pBlockMouse->GetBoolean();

        // Register SlidingRheostat config category
        GetConfig()->SetCategoryComment("SlidingRheostat", "Easter egg: dynamic playback speed based on ball speed.");
        
        IProperty* pRheostatMode = GetConfig()->GetProperty("SlidingRheostat", "Mode");
        char commentBuf[512];
        snprintf(commentBuf, sizeof(commentBuf),
                 "Dynamic playback speed mode:\n"
                 "%d = Disabled\n"
                 "%d = Linear: rate = 1.0 + (speed - RefSpeed) * Slope\n"
                 "%d = Sliding Window: rate = 1.0 + (speed - AvgSpeed) * Slope\n"
                 "%d = Square Root: rate = 1.0 + (sqrt(speed) - sqrt(RefSpeed)) * Slope * 2*sqrt(RefSpeed)\n"
                 "%d = Logarithmic: rate = 1.0 + (ln(speed+1) - ln(RefSpeed+1)) * Slope * (RefSpeed+1)\n"
                 "%d = Squared: rate = 1.0 + (speed^2 - RefSpeed^2) * Slope / (2*RefSpeed)",
                 RHEOSTAT_DISABLED, RHEOSTAT_LINEAR, RHEOSTAT_SLIDING_WINDOW,
                 RHEOSTAT_SQUARE_ROOT, RHEOSTAT_LOGARITHMIC, RHEOSTAT_SQUARED);
        pRheostatMode->SetComment(commentBuf);
        pRheostatMode->SetDefaultInteger(RHEOSTAT_DISABLED);
        m_RheostatMode = static_cast<RheostatMode>(pRheostatMode->GetInteger());

        IProperty* pTestInterval = GetConfig()->GetProperty("SlidingRheostat", "SpeedTestInterval");
        pTestInterval->SetComment("Minimum update interval for speed calculation (milliseconds)");
        pTestInterval->SetDefaultFloat(100.0f);
        float testInterval = pTestInterval->GetFloat();

        IProperty* pEasingRate = GetConfig()->GetProperty("SlidingRheostat", "EasingRate");
        pEasingRate->SetComment("Easing rate for speed changes (0 = instant change, 1 = maximum easing so speed never changes)");
        pEasingRate->SetDefaultFloat(0.05f);
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

        // Load internal configurations
        LoadConfig();
        m_Init = true;
    }

    void OnModifyConfig(const char* category, const char* key, IProperty* prop) override {
        if (!m_Init) return;

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
            } else if (strcmp(key, "BlockKeyboardIngame") == 0) {
                m_BlockKeyboardIngame = prop->GetBoolean();
            } else if (strcmp(key, "BlockMouseIngame") == 0) {
                m_BlockMouseIngame = prop->GetBoolean();
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
        bool keyboardBlocked = m_BlockKeyboardIngame && m_BML->IsPlaying();
        if (!keyboardBlocked) {
            if (m_BML->GetInputManager()->oIsKeyPressed(m_Hotkey)) {
                g_MusicPlayerOpen = !g_MusicPlayerOpen;
            }
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

        m_UI.Draw(m_BML, m_Player, m_SpeedTracker, m_ManualSpeed, m_TargetPlaybackSpeed, m_RheostatMode, m_FontScale, m_Opacity, m_BlockMouseIngame, [this]() { SaveConfig(); });
    }

    void OnRender(CK_RENDER_FLAGS flags) override {
        // ImGui draw calls must occur in OnProcess() during BML's active frame
    }

    MusicPlayerCore m_Player;
    MusicPlayerUI m_UI;

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

    bool m_BlockKeyboardIngame = true;
    bool m_BlockMouseIngame = false;
};

MOD_EXPORT IMod *BMLEntry(IBML *bml) { return new BallanceMusicPlayer(bml); }
MOD_EXPORT void BMLExit(IMod *mod) { delete mod; }
