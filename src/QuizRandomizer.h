#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace QuizRandomizer {
// Zero selects random_device (with a time fallback); nonzero seeds a repeatable seed sequence.
inline constexpr std::uint32_t kQuizFixedSeed = 0;
inline constexpr std::size_t kNoQuestion = static_cast<std::size_t>(-1);

struct QuestionBag {
    std::vector<std::size_t> pool;
    std::vector<std::size_t> remaining;
    std::vector<std::size_t> previousFullOrder;
    std::vector<std::size_t> previousQuizOrder;
    std::size_t lastQuestion = kNoQuestion;
};

std::uint32_t nextSeed(std::uint32_t &sequence);
void fisherYates(std::vector<std::size_t> &values, std::mt19937 &random);
void startMissionBag(QuestionBag &bag, std::mt19937 &random);
void prepareQuizRound(QuestionBag &bag, std::mt19937 &random, std::size_t count,
                      std::vector<std::size_t> &round);
std::array<std::string, 4> shuffleOptions(const std::array<std::string, 4> &choices,
                                         std::mt19937 &random);
} // namespace QuizRandomizer
