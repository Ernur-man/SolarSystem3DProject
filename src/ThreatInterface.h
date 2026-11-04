#pragma once

#include <algorithm>

class ThreatInterface {
public:
    virtual ~ThreatInterface() = default;
    virtual void setThreatSpeedMultiplier(float multiplier) = 0;
    virtual float getThreatProgress() const = 0;
    virtual bool isSunSwallowed() const = 0;
    virtual void update(float deltaTime) = 0;
    virtual void reset() = 0;
};

class FallbackThreat final : public ThreatInterface {
public:
    void setThreatSpeedMultiplier(float multiplier) override {
        speedMultiplier = std::clamp(multiplier, 0.25f, 4.0f);
    }

    float getThreatProgress() const override {
        return progress;
    }
    bool isSunSwallowed() const override {
        return progress >= 1.0f;
    }

    void update(float deltaTime) override {
        progress = std::min(progress + deltaTime * speedMultiplier / 120.0f, 1.0f);
    }

    void reset() override {
        progress = 0.0f;
        speedMultiplier = 1.0f;
    }

private:
    float progress = 0.0f;
    float speedMultiplier = 1.0f;
};
