#include "QuizRandomizer.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <limits>
#include <random>

namespace QuizRandomizer {
namespace {
void keepOrderFresh(std::vector<std::size_t> &values,
                    const std::vector<std::size_t> &previous) {
    if (values.size() > 1 && values == previous)
        std::rotate(values.begin(), values.begin() + 1, values.end());
}

void avoidPreviousFirst(std::vector<std::size_t> &values, std::size_t previousQuestion) {
    if (values.size() < 2 || values.front() != previousQuestion)
        return;
    const auto different = std::find_if(values.begin() + 1, values.end(),
                                        [previousQuestion](std::size_t q) {
                                            return q != previousQuestion;
                                        });
    if (different != values.end())
        std::iter_swap(values.begin(), different);
}

bool orderDependent(const std::array<std::string, 4> &choices) {
    for (std::string choice : choices) {
        std::transform(choice.begin(), choice.end(), choice.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (choice.find("all of the above") != std::string::npos ||
            (choice.rfind("both ", 0) == 0 && choice.find(" and ") != std::string::npos))
            return true;
    }
    return false;
}

void refill(QuestionBag &bag, std::mt19937 &random) {
    bag.remaining = bag.pool;
    fisherYates(bag.remaining, random);
    keepOrderFresh(bag.remaining, bag.previousFullOrder);
    avoidPreviousFirst(bag.remaining, bag.lastQuestion);
    // The no-repeat-first swap can turn a different shuffle back into the previous order.
    if (bag.remaining.size() > 2 && bag.remaining == bag.previousFullOrder) {
        if (bag.remaining.front() == bag.lastQuestion)
            std::iter_swap(bag.remaining.begin(), bag.remaining.begin() + 1);
        else
            std::iter_swap(bag.remaining.begin() + 1, bag.remaining.begin() + 2);
    }
    bag.previousFullOrder = bag.remaining;
}
} // namespace

std::uint32_t nextSeed(std::uint32_t &sequence) {
    ++sequence;
    if constexpr (kQuizFixedSeed != 0) {
        return kQuizFixedSeed + (sequence - 1u) * 0x9e3779b9u;
    }
    try {
        std::random_device device;
        const std::uint32_t seed = (static_cast<std::uint32_t>(device()) << 16u) ^
                                   static_cast<std::uint32_t>(device());
        if (seed != 0u)
            return seed;
    } catch (...) {
        // Some platforms do not provide random_device; use the clock fallback below.
    }
    const auto ticks = static_cast<std::uint32_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    return ticks ^ (sequence * 0x9e3779b9u);
}

void fisherYates(std::vector<std::size_t> &values, std::mt19937 &random) {
    for (std::size_t remaining = values.size(); remaining > 1; --remaining) {
        std::uniform_int_distribution<std::size_t> pick(0, remaining - 1);
        std::swap(values[remaining - 1], values[pick(random)]);
    }
}

void startMissionBag(QuestionBag &bag, std::mt19937 &random) {
    refill(bag, random);
}

void prepareQuizRound(QuestionBag &bag, std::mt19937 &random, std::size_t count,
                      std::vector<std::size_t> &round) {
    fisherYates(bag.remaining, random);
    keepOrderFresh(bag.remaining, bag.previousQuizOrder);
    bag.previousQuizOrder = bag.remaining;
    round.clear();
    round.reserve(count);
    for (std::size_t i = 0; i < count && !bag.pool.empty(); ++i) {
        if (bag.remaining.empty())
            refill(bag, random);
        avoidPreviousFirst(bag.remaining, bag.lastQuestion);
        round.push_back(bag.remaining.front());
        bag.lastQuestion = bag.remaining.front();
        bag.remaining.erase(bag.remaining.begin());
    }
}

std::array<std::string, 4> shuffleOptions(const std::array<std::string, 4> &choices,
                                         std::mt19937 &random) {
    if (orderDependent(choices))
        return choices;
    std::array<std::string, 4> shuffled = choices;
    for (std::size_t remaining = shuffled.size(); remaining > 1; --remaining) {
        std::uniform_int_distribution<std::size_t> pick(0, remaining - 1);
        std::swap(shuffled[remaining - 1], shuffled[pick(random)]);
    }
    return shuffled;
}
} // namespace QuizRandomizer
