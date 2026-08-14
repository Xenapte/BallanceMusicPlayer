#pragma once

#include <BML/IMod.h>
#include <BML/Bui.h>
#include <string>
#include <vector>
#include <functional>

#include "Common.hpp"
#include "MusicPlayerCore.hpp"
#include "FilePicker.hpp"
#include "BallSpeedTracker.hpp"

class MusicPlayerUI {
public:
    MusicPlayerUI();

    void Draw(IBML* bml, MusicPlayerCore& player, BallSpeedTracker& speedTracker,
              float& manualSpeed, float targetPlaybackSpeed, RheostatMode rheostatMode,
              float fontScale, float opacity, bool blockMouseIngame,
              std::function<void()> saveConfigCallback);

    bool IsHovered() const { return m_WindowHovered; }

private:
    FilePicker m_FilePicker;
    std::string m_PickerTempPath;
    bool m_FirstRender = true;
    bool m_Seeking = false;
    int m_SeekTargetMs = 0;
    bool m_WindowHovered = false;
};
