#include "BallSpeedTracker.hpp"
#include <algorithm>
#include <cmath>

BallSpeedTracker::BallSpeedTracker(IBML* bml) : m_BML(bml) {}

void BallSpeedTracker::SetConfigValues(RheostatMode mode, float testIntervalMs, float refSpeed, float slope) {
    m_Mode = mode;
    m_SpeedTestInterval = std::max(10.0f, testIntervalMs); // clamp interval to at least 10ms
    m_RefSpeed = refSpeed;
    m_Slope = slope;
}

void BallSpeedTracker::Update() {
    if (!m_BML || m_Mode == RHEOSTAT_DISABLED) {
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
        
        // If movement exceeds 100 units (meters) between updates, treat it as an outlier (e.g. level reset)
        if (distance > 100.0f) {
            m_LastPosition = currentPos;
            m_LastTimestamp = currentTime;
            return;
        }

        m_CurrentSpeed = (distance / dt) * 1000.0f;
        
        if (m_Mode == RHEOSTAT_SLIDING_WINDOW) {
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
    if (m_Mode == RHEOSTAT_DISABLED) {
        return manualSpeed;
    }
    if (m_Mode == RHEOSTAT_LINEAR) { // Linear model
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
    if (m_Mode == RHEOSTAT_SLIDING_WINDOW) { // Sliding window mode (acceleration-based)
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
    if (m_Mode == RHEOSTAT_SQUARE_ROOT) { // Square root model
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
        float refSpeed = std::max(0.0f, m_RefSpeed);
        float currentSpeed = std::max(0.0f, m_CurrentSpeed);
        float scaledSlope = m_Slope * 2.0f * std::sqrt(refSpeed);
        float speed = 1.0f + (std::sqrt(currentSpeed) - std::sqrt(refSpeed)) * scaledSlope;
        m_LastPlaybackSpeed = std::clamp(speed, 0.1f, 10.0f);
        return m_LastPlaybackSpeed;
    }
    if (m_Mode == RHEOSTAT_LOGARITHMIC) { // Logarithmic model
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
        float refSpeed = std::max(0.0f, m_RefSpeed);
        float currentSpeed = std::max(0.0f, m_CurrentSpeed);
        float scaledSlope = m_Slope * (refSpeed + 1.0f);
        float speed = 1.0f + (std::log(currentSpeed + 1.0f) - std::log(refSpeed + 1.0f)) * scaledSlope;
        m_LastPlaybackSpeed = std::clamp(speed, 0.1f, 10.0f);
        return m_LastPlaybackSpeed;
    }
    if (m_Mode == RHEOSTAT_SQUARED) { // Squared model (maximum sensitivity)
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
        float refSpeed = std::max(0.0f, m_RefSpeed);
        float currentSpeed = std::max(0.0f, m_CurrentSpeed);
        float divisor = 2.0f * refSpeed;
        float scaledSlope = (divisor > 0.001f) ? (m_Slope / divisor) : m_Slope;
        float speed = 1.0f + (currentSpeed * currentSpeed - refSpeed * refSpeed) * scaledSlope;
        m_LastPlaybackSpeed = std::clamp(speed, 0.1f, 10.0f);
        return m_LastPlaybackSpeed;
    }
    return manualSpeed;
}
