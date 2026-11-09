#pragma once
#include <array>
#include <memory>

class Audio {
public:
    Audio();
    ~Audio();
    bool initialize();
    void update(int world, float distance, float blackHoleGain, float deltaTime);
    void setMaster(float volume);
    void toggleMute();
    bool available() const;
    bool muted() const;
    float master() const;
    const char *status() const;
    void playBlip();
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    float volume = 0.35f;
    bool isMuted = false;
};
