#pragma once
#include <cstdint>
#include <vector>

namespace SoundSynth {
constexpr std::uint32_t SampleRate = 44100;
std::vector<float> makeWorldLoop(int world, bool ambience = false);
std::vector<float> makeBlip();
}
