#ifndef MTGDRAFTBOTS_ORACLES_HPP
#define MTGDRAFTBOTS_ORACLES_HPP

#include "mtgdraftbots/details/types.hpp"
#include "mtgdraftbots/generated/constants.h"

namespace mtgdraftbots::oracles {
    using OracleFunction = OracleResult (*) (const BotScore& bot_score);
    struct Oracle {
        constexpr auto calculate_weight(const std::pair<Coord, Coord>& coords,
                                        const std::pair<float, float> coord_weights) const -> float {
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
        constants::Weights weights;
    };

    constexpr Oracle rating_oracle{
        // TODO: Implement.
        [](const BotScore& bot_score) -> OracleResult {
            return {};
        },
        constants::RATING_WEIGHTS,
    };

    constexpr Oracle pick_synergy_oracle{
        // TODO: Implement.
        [](const BotScore& bot_score) -> OracleResult {
            return {};
        },
        constants::PICK_SYNERGY_WEIGHTS,
    };

    constexpr Oracle internal_synergy_oracle{
        // TODO: Implement.
        [](const BotScore& bot_score) -> OracleResult {
            return {};
        },
        constants::INTERNAL_SYNERGY_WEIGHTS,
    };

    constexpr Oracle colors_oracle{
        // TODO: Implement.
        [](const BotScore& bot_score) -> OracleResult {
            return {};
        },
        constants::COLORS_WEIGHTS,
    };

    constexpr Oracle openness_oracle{
        // TODO: Implement.
        [](const BotScore& bot_score) -> OracleResult {
            return {};
        },
        constants::OPENNESS_WEIGHTS,
    };

    constexpr std::array ORACLES{
        rating_oracle,
        pick_synergy_oracle,
        internal_synergy_oracle,
        colors_oracle,
        openness_oracle,
    };
}
#endif
