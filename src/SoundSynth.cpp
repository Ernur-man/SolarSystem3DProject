#include "SoundSynth.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <string>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr int kSourceLoopSeconds = 10;
constexpr float kCrossfadeSeconds = 0.5f;
constexpr std::size_t kLoopFrames = SoundSynth::SampleRate * kSourceLoopSeconds;
constexpr std::size_t kCrossfadeFrames = static_cast<std::size_t>(SoundSynth::SampleRate * kCrossfadeSeconds);
constexpr float kPeakLimit = 0.72f;
constexpr bool kPrintSynthDiagnostics = false;
constexpr float kFilterBase = 0.003f, kFilterRange = 0.025f;
constexpr float kNoiseLfoBaseHz = 0.08f, kNoiseLfoWorldStepHz = 0.011f;
constexpr float kPulseLfoBaseHz = 0.06f, kPulseLfoWorldStepHz = 0.009f;
constexpr float kPulseFloor = 0.55f, kPulseDepth = 0.45f;
constexpr float kSecondPartialGain = 0.35f, kThirdPartialGain = 0.16f;
constexpr float kMercuryToneGain = 0.075f, kMercurySecondGain = 0.035f;
constexpr float kMercuryTickHz = 2400.0f, kMercuryTickSeconds = 0.012f;
constexpr float kVenusHarmonicGain = 0.025f;
constexpr float kEarthChordGain = 0.045f, kEarthNoiseGain = 0.11f;
constexpr float kMoonToneGain = 0.045f, kMoonGateStart = 0.72f, kMoonGateWidth = 0.18f;
constexpr float kMarsToneGain = 0.065f, kMarsNoiseGain = 0.16f, kMarsGustHz = 0.13f;
constexpr float kJupiterBeatGain = 0.11f, kJupiterBeatHz = 0.22f, kJupiterRumbleGain = 0.055f;
constexpr float kSaturnHarmonicGain = 0.022f;
constexpr float kUranusVibratoDepth = 0.002f, kUranusVibratoHz = 0.18f;
constexpr float kUranusToneGain = 0.085f, kUranusDetuneRatio = 1.012f;
constexpr float kNeptuneSweepDepth = 0.08f, kNeptuneSweepHz = 0.07f;
constexpr float kNeptuneToneGain = 0.11f, kNeptuneNoiseGain = 0.13f;
constexpr float kSunHumGain = 0.12f, kSunNoiseGain = 0.09f, kSunCrackleGain = 0.05f;
constexpr float kWorldFundamentalsHz[10] = {880.0f, 55.0f, 196.0f, 220.0f, 392.0f,
                                             36.0f, 330.0f, 262.0f, 65.0f, 46.0f};
float sine(float hz, std::size_t frame) {
    return std::sin(2.0f * kPi * hz * static_cast<float>(frame) / SoundSynth::SampleRate);
}
}

std::vector<float> SoundSynth::makeWorldLoop(int world, bool ambience) {
    const auto started = std::chrono::steady_clock::now();
    std::vector<float> result(kLoopFrames * 2);
    std::mt19937 random(static_cast<std::uint32_t>(0x51A7u + world * 991u));
    std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
    float filtered = 0.0f;
    for (std::size_t i = 0; i < kLoopFrames; ++i) {
        const float t = static_cast<float>(i) / SoundSynth::SampleRate;
        const float lfo = 0.5f + 0.5f * std::sin(2.0f * kPi * (kNoiseLfoBaseHz + world * kNoiseLfoWorldStepHz) * t);
        const float cutoff = kFilterBase + kFilterRange * lfo;
        filtered += cutoff * (noise(random) - filtered);
        float value = 0.0f;
        if (ambience) {
            value = 0.025f * filtered + 0.012f * sine(55.0f, i);
        } else {
            const float f = kWorldFundamentalsHz[std::clamp(world, 0, 9)];
            const float pulse = kPulseFloor + kPulseDepth * std::sin(2.0f * kPi * (kPulseLfoBaseHz + world * kPulseLfoWorldStepHz) * t);
            const float harmonics = sine(f, i) + kSecondPartialGain * sine(f * 2.0f, i) + kThirdPartialGain * sine(f * 3.0f, i);
            value = 0.12f * harmonics;
            if (world == 0) {
                value = kMercuryToneGain * sine(f, i) + kMercurySecondGain * sine(f * 2.0f, i) +
                        (t > 2.0f && t < 2.0f + kMercuryTickSeconds
                            ? 0.055f * std::exp(-(t - 2.0f) * 520.0f) * sine(kMercuryTickHz, i) : 0.0f);
            } else if (world == 1) {
                value = kVenusHarmonicGain * (sine(f, i) + sine(f * 2.0f, i) + sine(f * 3.0f, i) +
                                  sine(f * 4.0f, i) + sine(f * 5.0f, i)) * pulse;
            } else if (world == 2) {
                value = kEarthChordGain * (sine(196.0f, i) + sine(247.0f, i) + sine(294.0f, i)) + kEarthNoiseGain * filtered;
            } else if (world == 3) {
                const float sparseLfo = 0.5f + 0.5f * std::sin(2.0f * kPi * 0.05f * t);
                const float gate = std::clamp((sparseLfo - kMoonGateStart) / kMoonGateWidth, 0.0f, 1.0f);
                const float sparse = gate * gate * (3.0f - 2.0f * gate);
                value = kMoonToneGain * sparse * (sine(f, i) + 0.5f * sine(f * 1.5f, i));
            } else if (world == 4) {
                const float gust = 0.2f + 0.8f * (0.5f + 0.5f * std::sin(2.0f * kPi * kMarsGustHz * t));
                value = kMarsToneGain * sine(f, i) + kMarsNoiseGain * filtered * gust;
            } else if (world == 5) {
                value = kJupiterBeatGain * (sine(f, i) + sine(f + kJupiterBeatHz, i)) + kJupiterRumbleGain * filtered;
            } else if (world == 6) {
                value = kSaturnHarmonicGain * pulse * (sine(f, i) + sine(f * 2.0f, i) + sine(f * 3.0f, i) +
                    sine(f * 4.0f, i) + sine(f * 5.0f, i) + sine(f * 6.0f, i) + sine(f * 7.0f, i));
            } else if (world == 7) {
                const float vibrato = 1.0f + kUranusVibratoDepth * std::sin(2.0f * kPi * kUranusVibratoHz * t);
                value = kUranusToneGain * sine(f * vibrato, i) + kUranusToneGain * sine(f * kUranusDetuneRatio * vibrato, i);
            } else if (world == 8) {
                const float sweep = 1.0f + kNeptuneSweepDepth * std::sin(2.0f * kPi * kNeptuneSweepHz * t);
                value = kNeptuneToneGain * sine(f * sweep, i) + kNeptuneNoiseGain * filtered;
            } else {
                const float crackleGate = std::clamp((lfo - 0.94f) / 0.06f, 0.0f, 1.0f);
                const float crackle = noise(random) * crackleGate * crackleGate;
                value = kSunHumGain * sine(f, i) + kSunNoiseGain * filtered + kSunCrackleGain * crackle;
            }
        }
        result[i * 2] = value;
        result[i * 2 + 1] = value;
    }
    float peak = 0.0f;
    for (float sample : result) peak = std::max(peak, std::abs(sample));
    if (peak > kPeakLimit) for (float &sample : result) sample *= kPeakLimit / peak;
    std::vector<float> loop;
    loop.reserve((kLoopFrames - kCrossfadeFrames) * 2);
    const std::size_t firstKeptFrame = kCrossfadeFrames;
    const std::size_t fadeStartFrame = kLoopFrames - kCrossfadeFrames;
    loop.insert(loop.end(), result.begin() + static_cast<std::ptrdiff_t>(firstKeptFrame * 2),
                result.begin() + static_cast<std::ptrdiff_t>(fadeStartFrame * 2));
    for (std::size_t i = 0; i < kCrossfadeFrames; ++i) {
        const float fade = static_cast<float>(i) / static_cast<float>(kCrossfadeFrames - 1);
        const std::size_t tail = (fadeStartFrame + i) * 2;
        const std::size_t head = i * 2;
        for (std::size_t channel = 0; channel < 2; ++channel)
            loop.push_back(result[tail + channel] * (1.0f - fade) + result[head + channel] * fade);
    }
    if (kPrintSynthDiagnostics) {
        float peak = 0.0f, sumSquares = 0.0f;
        bool finite = true;
        for (float sample : loop) {
            finite = finite && std::isfinite(sample);
            peak = std::max(peak, std::abs(sample));
            sumSquares += sample * sample;
        }
        const float rms = std::sqrt(sumSquares / static_cast<float>(loop.size()));
        const float seam = std::abs(loop.front() - loop.back());
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started).count();
        std::cout << "Synth " << (ambience ? "deep-space" : std::to_string(world))
                  << ": peak=" << peak << " RMS=" << rms << " seam=" << seam
                  << " finite=" << (finite ? "yes" : "no") << " time=" << elapsed << " us\n";
    }
    return loop;
}

std::vector<float> SoundSynth::makeBlip() {
    constexpr std::size_t frames = SampleRate * 12 / 100;
    std::vector<float> result(frames * 2);
    for (std::size_t i = 0; i < frames; ++i) {
        const float t = static_cast<float>(i) / SampleRate;
        const float env = std::exp(-t * 32.0f);
        const float value = 0.12f * env * std::sin(2.0f * kPi * (660.0f - 250.0f * t) * t);
        result[2 * i] = result[2 * i + 1] = value;
    }
    return result;
}
