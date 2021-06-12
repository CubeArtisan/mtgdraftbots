#ifndef MTGDRAFTBOTS_DETAILS_TYPES_HPP
#define MTGDRAFTBOTS_DETAILS_TYPES_HPP

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mtgdraftbots {
    using Lands = std::array<std::uint8_t, 32>;
    using Coord = std::pair<float, float>;
    using Option = std::vector<std::uint8_t>;

    struct BotScore;

    struct DrafterState {
        std::vector<std::string> picked;
        std::vector<std::string> seen;
        std::vector<std::string> cards_in_pack;
        std::vector<std::string> basics;
        std::uint8_t pack_num;
        std::uint8_t num_packs;
        std::uint8_t pick_num;
        std::uint8_t num_picks;
        std::uint32_t seed;

        inline auto calculate_pick_from_options(const std::vector<Option>& options) const -> BotScore;
    };

    struct OracleResult {
        std::string title;
        std::string tooltip;
        float weight;
        float value;
        std::vector<float> per_card;
    };

    constexpr size_t EMBEDDING_SIZE = 64;
    using Embedding = std::array<float, 64>;

    struct CardDetails {
        std::string name;
        std::vector<std::string> cost_symbols;
        Embedding embedding;
        float rating;
        std::uint8_t cmc;
        std::optional<std::array<bool, 5>> produces;
    };

    constexpr std::size_t WEIGHT_X_DIM = 3;
    constexpr std::size_t WEIGHT_Y_DIM = 15;
    using Weights = std::array<std::array<float, WEIGHT_Y_DIM>, WEIGHT_X_DIM>;
}
#endif
