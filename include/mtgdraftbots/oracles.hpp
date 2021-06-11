#ifndef MTGDRAFTBOTS_ORACLES_HPP
#define MTGDRAFTBOTS_ORACLES_HPP

#include <map>
#include <string>

#include "mtgdraftbots/details/types.hpp"
#include "mtgdraftbots/details/constants.hpp"

namespace mtgdraftbots::oracles {
    using OracleFunction = OracleResult (*) (const BotScore& bot_score);
    struct Oracle {
        auto calculate_weight(const std::map<std::string, Weights, std::less<>>& weights_map,
                              const std::pair<Coord, Coord>& coords,
                              const std::pair<float, float>& coord_weights) const noexcept -> float {
            auto iter = weights_map.find(name);
            if (iter != weights_map.end()) return calculate_weight(iter->second, coords, coord_weights);
            else return 0.0;
        }

        constexpr auto calculate_weight(const Weights& weights,
                                        const std::pair<Coord, Coord>& coords,
                                        const std::pair<float, float>& coord_weights) const noexcept -> float {
            const auto& [coord1, coord2] = coords;
            const auto& [coord1x, coord1y] = coord1;
            const auto& [coord2x, coord2y] = coord2;
            const auto& [coord1Weight, coord2Weight] = coord_weights;
            return coord1Weight * coord2Weight * weights[coord1x][coord1y]
                 + coord1Weight * (1 - coord2Weight) * weights[coord1x][coord2y]
                 + (1 - coord1Weight) * coord2Weight * weights[coord2x][coord1y]
                 + (1 - coord1Weight) * (1 - coord2Weight) * weights[coord2x][coord2y];
        };

        OracleFunction calculate_value;
        std::string_view name;
        std::string_view tooltip;
    };

    constexpr Oracle rating_oracle{
        // TODO: Implement.
        [](const BotScore& bot_score) -> OracleResult {
            return {};
        },
        "Rating",
        "The rating based on the Elo and current color commitments.",
    };

    constexpr Oracle pick_synergy_oracle{
        // TODO: Implement.
        [](const BotScore& bot_score) -> OracleResult {
            return {};
        },
        "Pick Synergy",
        "A score of how well this card synergizes with the current picks.",
    };

    constexpr Oracle internal_synergy_oracle{
        // TODO: Implement.
        [](const BotScore& bot_score) -> OracleResult {
            return {};
        },
        "Internal Synergy",
        "A score of how well current picks in these colors synergize with each other.",
    };

    constexpr Oracle colors_oracle{
        // TODO: Implement.
        [](const BotScore& bot_score) -> OracleResult {
            return {};
        },
        "Colors",
        "A score of how well these colors fit in with the current picks.",
    };

    constexpr Oracle external_synergy_oracle{
        // TODO: Implement.
        [](const BotScore& bot_score) -> OracleResult {
            return {};
        },
        "External Synergy",
        "A score of how cards picked so far synergize with the other cards in these colors that have been seen so far.",
    };

    constexpr Oracle openness_oracle{
        // TODO: Implement.
        [](const BotScore& bot_score) -> OracleResult {
            return {};
        },
        "Openness",
        "A score of how many and how good the card we have seen in these colors.",
    };

    constexpr std::array ORACLES{
        rating_oracle,
        pick_synergy_oracle,
        internal_synergy_oracle,
        external_synergy_oracle,
        colors_oracle,
        openness_oracle,
    };
}
#endif
