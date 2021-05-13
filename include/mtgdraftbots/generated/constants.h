#ifndef MTGDRAFTBOTS_CONSTANTS_H
#define MTGDRAFTBOTS_CONSTANTS_H
#include <array>
#include <bits/c++config.h>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <string_view>

#include <frozen/unordered_map.h>
#include <frozen/string.h>
#include <utility>

namespace mtgdraftbots::constants {
    constexpr std::size_t COUNT_DIMS_EXP = 5;
    // This is slightly misleading since these are actually 1 greater than the max value
    // since they are the size of that dimension.
    constexpr std::size_t MAX_COUNT_A  = 1 << COUNT_DIMS_EXP;
    constexpr std::size_t MAX_COUNT_B  = 1 << COUNT_DIMS_EXP;
    constexpr std::size_t MAX_COUNT_AB = 1 << COUNT_DIMS_EXP;
    constexpr std::size_t MAX_REQUIRED_B = 4;
    constexpr std::size_t MAX_REQUIRED_A = 7;
    constexpr std::size_t MAX_CMC = 11;
    constexpr std::size_t PROB_TABLE_SIZE =  MAX_COUNT_A * MAX_COUNT_B * MAX_COUNT_AB\
                                           * MAX_REQUIRED_B * MAX_REQUIRED_A * MAX_CMC;
    // It's okay to sacrifice precision here, 16 fixed point is really good enough, could probably even use 8 bit.
    // TODO: Find way to initialize this from a file, maybe just include a generated file.
    constexpr std::array<std::uint16_t, PROB_TABLE_SIZE> PROB_TABLE{};
    // Multiplying a 16 bit fixed point number by this converts it into the [0, 1] inclusive range.
    constexpr float PROB_SCALING_FACTOR = 1 / static_cast<float>(0xFFFF);

    using Embedding = std::array<float, 64>;

    constexpr std::size_t NUM_CARDS = 1;

    // TODO: Populate these constants with the trained values.
    constexpr frozen::unordered_map<frozen::string, std::uint16_t, NUM_CARDS> CARD_INDICES{
        {"Lightning Bolt", 0},
    };
    constexpr std::array<std::array<std::string_view, 11>, NUM_CARDS> CARD_COST_SYMBOLS{};
    constexpr std::array<std::uint8_t, NUM_CARDS> CARD_CMCS{};
    constexpr std::array<Embedding, NUM_CARDS> CARD_EMBEDDINGS{};
    constexpr std::array<float, NUM_CARDS> CARD_RATINGS{};
    constexpr std::array<std::optional<std::uint8_t>, NUM_CARDS> CARD_PRODUCES{};

    constexpr std::size_t WEIGHT_X_DIM = 15;
    constexpr std::size_t WEIGHT_Y_DIM = 3;
    using Weights = std::array<std::array<float, WEIGHT_Y_DIM>, WEIGHT_X_DIM>;
    // TODO: Move into separate constants file.
    constexpr Weights RATING_WEIGHTS{};
    constexpr Weights PICK_SYNERGY_WEIGHTS{};
    constexpr Weights INTERNAL_SYNERGY_WEIGHTS{};
    constexpr Weights COLORS_WEIGHTS{};
    constexpr Weights OPENNESS_WEIGHTS{};
}
#endif
