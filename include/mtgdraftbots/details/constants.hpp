#ifndef MTGDRAFTBOTS_CONSTANTS_H
#define MTGDRAFTBOTS_CONSTANTS_H
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
#include <string_view>
#include <utility>

#include <frozen/unordered_map.h>

namespace mtgdraftbots::constants {
    constexpr std::size_t COUNT_DIMS_EXP = 5;
    // This is slightly misleading since these are actually 1 greater than the max value
    // since they are the size of that dimension.
    constexpr std::size_t NUM_COUNT_A  = 1 << COUNT_DIMS_EXP;
    constexpr std::size_t NUM_COUNT_B  = 1 << COUNT_DIMS_EXP;
    constexpr std::size_t NUM_COUNT_AB = 1 << COUNT_DIMS_EXP;
    constexpr std::size_t NUM_REQUIRED_B = 4;
    constexpr std::size_t NUM_REQUIRED_A = 7;
    constexpr std::size_t NUM_CMC = 9;
    constexpr std::size_t PROB_TABLE_SIZE =  NUM_COUNT_A * NUM_COUNT_B * NUM_COUNT_AB
                                           * NUM_REQUIRED_B * NUM_REQUIRED_A * NUM_CMC;

#include "mtgdraftbots/generated/prob_table.hpp"

    constexpr auto COLOR_TO_INDEX = frozen::make_unordered_map<char, std::uint8_t>({
        {'W', 0},
        {'U', 1},
        {'B', 2},
        {'R', 3},
        {'G', 4},
        {'w', 0},
        {'u', 1},
        {'b', 2},
        {'r', 3},
        {'g', 4},
    });
    constexpr std::array<std::array<bool, 5>, 32> COLOR_COMBINATIONS{{
        {false, false, false, false, false},
        {true, false, false, false, false}, // Mono colors
        {false, true, false, false, false},
        {false, false, true, false, false},
        {false, false, false, true, false},
        {false, false, false, false, true},
        {true, true, false, false, false}, // Ally colors
        {false, true, true, false, false},
        {false, false, true, true, false},
        {false, false, false, true, true},
        {true, false, false, false, true},
        {true, false, true, false, false}, // Enemy colors
        {false, true, false, true, false},
        {false, false, true, false, true},
        {true, false, false, true, false},
        {false, true, false, false, true},
        {true, true, false, false, true}, // Shards
        {true, true, true, false, false},
        {false, true, true, true, false},
        {false, false, true, true, true},
        {true, false, false, true, true},
        {true, false, true, true, false}, // Wedges
        {false, true, false, true, true},
        {true, false, true, false, true},
        {true, true, false, true, false},
        {false, true, true, false, true},
        {false, true, true, true, true}, // 4-Color
        {true, false, true, true, true},
        {true, true, false, true, true},
        {true, true, true, false, true},
        {true, true, true, true, false},
        {true, true, true, true, true},
    }};

    constexpr std::ptrdiff_t get_color_combination_index(const std::array<bool, 5>& comb) noexcept {
        return std::distance(std::begin(COLOR_COMBINATIONS),
                             std::find(std::begin(COLOR_COMBINATIONS), std::end(COLOR_COMBINATIONS), comb));
    }
}
#endif
