#pragma once

#include <BML/IMod.h>
#include <BML/IConfig.h>
#include <BML/Defines.h>

class BallSpeedTracker {
public:
    explicit BallSpeedTracker(IBML* bml);

    void Update();

    float GetTargetPlaybackSpeed(float manualSpeed);

    void SetConfigValues(int mode, float testIntervalMs, float refSpeed, float slope);

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
    int m_Mode = 0;
    float m_SpeedTestInterval = 100.0f; // ms
    float m_RefSpeed = 30.0f;
    float m_Slope = 0.02f;
    float m_LastPlaybackSpeed = 1.0f;
};
