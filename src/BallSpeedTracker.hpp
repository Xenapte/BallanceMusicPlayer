#pragma once

#include <BML/IMod.h>
#include <BML/IConfig.h>
#include <BML/Defines.h>

enum RheostatMode {
    RHEOSTAT_DISABLED = 0,
    RHEOSTAT_LINEAR = 1,
    RHEOSTAT_SLIDING_WINDOW = 2,
    RHEOSTAT_SQUARE_ROOT = 3,
    RHEOSTAT_LOGARITHMIC = 4,
    RHEOSTAT_SQUARED = 5
};

class BallSpeedTracker {
public:
    explicit BallSpeedTracker(IBML* bml);

    void Update();

    float GetTargetPlaybackSpeed(float manualSpeed);

    void SetConfigValues(RheostatMode mode, float testIntervalMs, float refSpeed, float slope);

    float GetCurrentSpeed() const { return m_CurrentSpeed; }

private:
    IBML* m_BML = nullptr;
    CKDataArray* m_CurrentLevelArray = nullptr;

    // Tracking states
    VxVector m_LastPosition = {0.0f, 0.0f, 0.0f};
    float m_LastTimestamp = 0.0f;
    float m_CurrentSpeed = 0.0f;
    float m_AvgSpeed = 0.0f;
    bool m_Initialized = false;

    // Configuration values
    RheostatMode m_Mode = RHEOSTAT_DISABLED;
    float m_SpeedTestInterval = 100.0f; // ms
    float m_RefSpeed = 30.0f;
    float m_Slope = 0.02f;
    float m_LastPlaybackSpeed = 1.0f;
    float m_MaxDistance = 100.0f;
};
