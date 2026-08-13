#include "BallSpeedTracker.hpp"
#include <algorithm>
#include <cmath>

BallSpeedTracker::BallSpeedTracker(IBML* bml) : m_BML(bml) {}

void BallSpeedTracker::SetConfigValues(int mode, float testIntervalMs, float refSpeed, float slope) {
    m_Mode = mode;
    m_SpeedTestInterval = std::max(10.0f, testIntervalMs); // clamp interval to at least 10ms
    m_RefSpeed = refSpeed;
    m_Slope = slope;
}

void BallSpeedTracker::Update() {
    if (!m_BML || m_Mode == 0) {
        m_CurrentLevelArray = nullptr;
        m_Initialized = false;
        m_CurrentSpeed = 0.0f;
        return;
    }

    if (!m_BML->IsIngame()) {
        m_CurrentLevelArray = nullptr;
        m_Initialized = false;
        m_CurrentSpeed = 0.0f;
        return;
    }

    if (!m_BML->IsPlaying()) {
        m_Initialized = false;
        m_CurrentSpeed = 0.0f;
        return;
    }

    if (!m_CurrentLevelArray) {
        m_CurrentLevelArray = m_BML->GetArrayByName("CurrentLevel");
        if (!m_CurrentLevelArray) return;
    }

    if (m_CurrentLevelArray->GetRowCount() <= 0) {
        return;
    }

    CK3dObject* ball = static_cast<CK3dObject*>(m_CurrentLevelArray->GetElementObject(0, 1));
    if (!ball) {
        return;
    }

    VxVector currentPos;
    ball->GetPosition(&currentPos);
    float currentTime = m_BML->GetTimeManager()->GetTime();

    if (!m_Initialized) {
        m_LastPosition = currentPos;
        m_LastTimestamp = currentTime;
        m_CurrentSpeed = 0.0f;
        m_AvgSpeed = 0.0f;
        m_Initialized = true;
        return;
    }

    float dt = currentTime - m_LastTimestamp;
    if (dt >= m_SpeedTestInterval) {
        float distance = Magnitude(currentPos - m_LastPosition);
        m_CurrentSpeed = (distance / dt) * 1000.0f;
        
        if (m_Mode == 2) {
            float weight = m_RefSpeed;
            if (weight < 0.0f) weight = 0.0f;
            if (m_AvgSpeed == 0.0f && m_CurrentSpeed > 0.0f) {
                m_AvgSpeed = m_CurrentSpeed;
            } else {
                m_AvgSpeed = m_AvgSpeed + (m_CurrentSpeed - m_AvgSpeed) / (weight + 1.0f);
            }
        }

        m_LastPosition = currentPos;
        m_LastTimestamp = currentTime;
    }
}

float BallSpeedTracker::GetTargetPlaybackSpeed(float manualSpeed) {
    if (m_Mode == 0) {
        return manualSpeed;
    }
    if (m_Mode == 1) { // Linear model
        if (!m_BML->IsIngame()) {
            m_LastPlaybackSpeed = 1.0f;
            return 1.0f;
        }
        if (!m_BML->IsPlaying()) {
            return m_LastPlaybackSpeed; // Keep previous rate when paused
        }
        if (!m_Initialized) {
            return m_LastPlaybackSpeed;
        }
        float speed = 1.0f + (m_CurrentSpeed - m_RefSpeed) * m_Slope;
        m_LastPlaybackSpeed = std::clamp(speed, 0.1f, 10.0f);
        return m_LastPlaybackSpeed;
    }
    if (m_Mode == 2) { // Sliding window mode (acceleration-based)
        if (!m_BML->IsIngame()) {
            m_LastPlaybackSpeed = 1.0f;
            return 1.0f;
        }
        if (!m_BML->IsPlaying()) {
            return m_LastPlaybackSpeed; // Keep previous rate when paused
        }
        if (!m_Initialized) {
            return m_LastPlaybackSpeed;
        }
        float speed = 1.0f + (m_CurrentSpeed - m_AvgSpeed) * m_Slope;
        m_LastPlaybackSpeed = std::clamp(speed, 0.1f, 10.0f);
        return m_LastPlaybackSpeed;
    }
    return manualSpeed;
}
