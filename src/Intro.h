#pragma once

#include <imgui.h>

class Intro {
public:
    static constexpr bool kShowIntro = true; // Enables the one-time startup splash by default.
    static constexpr float kIntroSeconds = 5.2f; // Total duration; the splash is capped below 6 seconds.

    void start(bool enabled);
    bool active() const { return running; }
    void update(float deltaTime, bool skipRequested);
    void draw(float width, float height, float dpiScale) const;

private:
    bool running = false;
    bool skipping = false;
    float elapsed = 0.0f;
    float skipElapsed = 0.0f;
};
