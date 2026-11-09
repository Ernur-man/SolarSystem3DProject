#include "Audio.h"

#ifdef SOLAR_NO_AUDIO

struct Audio::Impl {};

Audio::Audio() : impl(nullptr) {}
Audio::~Audio() = default;
bool Audio::initialize() { return false; }
void Audio::update(int, float, float, float) {}
void Audio::setMaster(float) {}
void Audio::toggleMute() {}
bool Audio::available() const { return false; }
bool Audio::muted() const { return false; }
float Audio::master() const { return volume; }
const char *Audio::status() const { return "Sound: OFF (not built)"; }
void Audio::playBlip() {}

#else

#define MINIAUDIO_IMPLEMENTATION
#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include "miniaudio.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include "SoundSynth.h"
#include <algorithm>
#include <chrono>
#include <future>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr float kDefaultMasterVolume = 0.35f;
constexpr float kDistanceNear = 4.0f;
constexpr float kDistanceFar = 24.0f;
constexpr float kGainFadeSeconds = 1.0f;
constexpr float kMasterFadeSeconds = 1.0f;
constexpr float kBlipGain = 0.25f;
constexpr std::array<const char *, 10> kWorldNames = {"mercury", "venus", "earth", "moon", "mars", "jupiter", "saturn", "uranus", "neptune", "sun"};
}

struct Audio::Impl {
    ma_engine engine{};
    bool ready = false;
    std::array<ma_audio_buffer, 11> buffers{};
    std::array<std::vector<float>, 11> samples{};
    std::array<ma_sound, 11> sounds{};
    std::array<bool, 11> bufferReady{};
    std::array<bool, 11> soundReady{};
    ma_audio_buffer blipBuffer{};
    std::vector<float> blipSamples;
    ma_sound blip{};
    bool blipBufferReady = false, blipReady = false;
    std::array<float, 11> gains{};
    float appliedMaster = 0.0f;
    std::future<std::array<std::vector<float>, 11>> synthesis;
    std::array<bool, 11> needsSynthesis{};
    bool synthesisStarted = false;
};

Audio::Audio() : impl(new Impl), volume(kDefaultMasterVolume) {}
Audio::~Audio() {
    if (!impl) return;
    if (impl->synthesis.valid()) impl->synthesis.wait();
    if (impl->blipReady) ma_sound_uninit(&impl->blip);
    if (impl->blipBufferReady) ma_audio_buffer_uninit(&impl->blipBuffer);
    for (std::size_t i = 0; i < impl->sounds.size(); ++i) {
        if (impl->soundReady[i]) ma_sound_uninit(&impl->sounds[i]);
        if (impl->bufferReady[i]) ma_audio_buffer_uninit(&impl->buffers[i]);
    }
    if (impl->ready) ma_engine_uninit(&impl->engine);
}

bool Audio::initialize() {
    if (ma_engine_init(nullptr, &impl->engine) != MA_SUCCESS) {
        std::cerr << "Warning: audio output device unavailable; sound disabled.\n";
        return false;
    }
    impl->ready = true;
    for (int i = 0; i < 11; ++i) {
        const std::string name = i == 10 ? "deep-space" : kWorldNames[static_cast<std::size_t>(i)];
        const std::array<const char *, 3> extensions = {"wav", "mp3", "flac"};
        bool loaded = false;
        if (i < 10) for (const char *extension : extensions) {
            const std::string path = "assets/audio/" + name + "." + extension;
            std::error_code fileError;
            if (!std::filesystem::exists(path, fileError)) continue;
            if (ma_sound_init_from_file(&impl->engine, path.c_str(), MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION,
                                        nullptr, nullptr, &impl->sounds[i]) == MA_SUCCESS) {
                impl->soundReady[i] = true;
                ma_sound_set_looping(&impl->sounds[i], MA_TRUE);
                ma_sound_set_volume(&impl->sounds[i], 0.0f);
                ma_sound_start(&impl->sounds[i]);
                std::cout << "Loaded audio override: " << path << '\n';
                loaded = true;
                break;
            }
        }
        impl->needsSynthesis[static_cast<std::size_t>(i)] = !loaded;
    }
    const auto needsSynthesis = impl->needsSynthesis;
    try {
        impl->synthesis = std::async(std::launch::async, [needsSynthesis] {
            std::array<std::vector<float>, 11> generated{};
            for (int i = 0; i < 11; ++i)
                if (needsSynthesis[static_cast<std::size_t>(i)])
                    generated[static_cast<std::size_t>(i)] = SoundSynth::makeWorldLoop(i, i == 10);
            return generated;
        });
    } catch (...) {
        std::cerr << "Warning: audio synthesis thread unavailable; sound disabled.\n";
        impl->ready = false;
        return false;
    }
    impl->synthesisStarted = true;
    impl->blipSamples = SoundSynth::makeBlip();
    auto blipConfig = ma_audio_buffer_config_init(ma_format_f32, 2, impl->blipSamples.size() / 2, impl->blipSamples.data(), nullptr);
    if (ma_audio_buffer_init(&blipConfig, &impl->blipBuffer) == MA_SUCCESS) {
        impl->blipBufferReady = true;
        impl->blipReady = ma_sound_init_from_data_source(&impl->engine, &impl->blipBuffer,
            MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &impl->blip) == MA_SUCCESS;
    }
    return true;
}

void Audio::update(int world, float distance, float blackHoleGain, float deltaTime) {
    if (!available()) return;
    if (impl->synthesisStarted && impl->synthesis.valid() &&
        impl->synthesis.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        auto generated = impl->synthesis.get();
        for (std::size_t i = 0; i < generated.size(); ++i) {
            if (generated[i].empty()) continue;
            impl->samples[i] = std::move(generated[i]);
            auto &samples = impl->samples[i];
            auto config = ma_audio_buffer_config_init(ma_format_f32, 2, samples.size() / 2,
                                                      samples.data(), nullptr);
            if (ma_audio_buffer_init(&config, &impl->buffers[i]) != MA_SUCCESS) continue;
            impl->bufferReady[i] = true;
            if (ma_sound_init_from_data_source(&impl->engine, &impl->buffers[i],
                    MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &impl->sounds[i]) != MA_SUCCESS)
                continue;
            impl->soundReady[i] = true;
            ma_sound_set_looping(&impl->sounds[i], MA_TRUE);
            ma_sound_set_volume(&impl->sounds[i], 0.0f);
            ma_sound_start(&impl->sounds[i]);
        }
        impl->synthesisStarted = false;
    }
    const float distanceT = std::clamp((distance - kDistanceNear) / (kDistanceFar - kDistanceNear), 0.0f, 1.0f);
    const float fade = 1.0f - distanceT * distanceT * (3.0f - 2.0f * distanceT);
    const float targetMaster = isMuted ? 0.0f : volume;
    const float masterStep = std::clamp(deltaTime, 0.0f, 0.05f) / kMasterFadeSeconds;
    impl->appliedMaster += std::clamp(targetMaster - impl->appliedMaster, -masterStep, masterStep);
    for (int i = 0; i < 11; ++i) {
        float gain = i == 10 ? (world < 0 ? 1.0f : 0.0f) : (world == i ? fade * blackHoleGain : 0.0f);
        const float step = std::clamp(deltaTime, 0.0f, 0.05f) / kGainFadeSeconds;
        float &current = impl->gains[static_cast<std::size_t>(i)];
        current += std::clamp(gain - current, -step, step);
        if (impl->soundReady[static_cast<std::size_t>(i)])
            ma_sound_set_volume(&impl->sounds[static_cast<std::size_t>(i)], impl->appliedMaster * current);
    }
}
void Audio::setMaster(float value) { volume = std::clamp(value, 0.0f, 1.0f); }
void Audio::toggleMute() { isMuted = !isMuted; }
bool Audio::available() const { return impl && impl->ready; }
bool Audio::muted() const { return isMuted; }
float Audio::master() const { return volume; }
const char *Audio::status() const { return !available() ? "Sound: OFF (no device)" : isMuted ? "Sound: MUTED" : "Sound: ON"; }
void Audio::playBlip() {
    if (!available() || !impl->blipReady || isMuted) return;
    ma_sound_set_volume(&impl->blip, volume * kBlipGain);
    ma_sound_seek_to_pcm_frame(&impl->blip, 0);
    ma_sound_start(&impl->blip);
}

#endif
