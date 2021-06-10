#ifndef MTGDRAFTBOTS_H
#define MTGDRAFTBOTS_H

#include <array>
#include <bit>
#include <bits/c++config.h>
#include <cstring>
#include <cstdint>
#include <frozen/string.h>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include <Eigen/Dense>

#include "mtgdraftbots/oracles.hpp"
#include "mtgdraftbots/details/cardcost.hpp"
#include "mtgdraftbots/details/types.hpp"
#include "mtgdraftbots/generated/constants.h"

namespace mtgdraftbots {

    namespace internal {

        struct CardValues {
            // Can this be constexpr?
            inline explicit CardValues(const Eigen::ArrayXi indices_)
                : indices(indices_),
                  embeddings(indices.rows(), 64),
                  ratings(indices.rows())
            {
                costs.reserve(indices.rows());
                for (std::size_t i=0; i < indices.rows(); i++) {
                    embeddings.row(i) = Eigen::Matrix<float, 1, mtgdraftbots::constants::EMBEDDING_SIZE>(
                        mtgdraftbots::constants::CARD_EMBEDDINGS[indices[i]].data()
                    );
                    ratings[i] = constants::CARD_RATINGS[indices[i]];
                    costs.push_back({mtgdraftbots::constants::CARD_CMCS[indices[i]],
                                     mtgdraftbots::constants::CARD_COST_SYMBOLS[indices[i]]});
                }
            }
            Eigen::ArrayXi indices;
            Eigen::Matrix<float, Eigen::Dynamic, mtgdraftbots::constants::EMBEDDING_SIZE> embeddings;
            Eigen::Matrix<float, Eigen::Dynamic, 1> ratings;
            std::vector<CardCost> costs;
        };

        template <typename Rng>
        struct BotState {
            CardValues picked;
            CardValues seen;
            CardValues cards_in_pack;
            CardValues basics;
            std::pair<Coord, Coord> coords;
            std::pair<float, float> coord_weights;
            mutable Rng rng;
        };
    }

    struct BotScore {
        DrafterState drafter_state;
        float score;
        std::array<OracleResult, oracles::ORACLES.size()> oracle_results;
        float total_nonland_prob;
        Option option;
        std::string colors;
        std::map<std::string, int> lands;
        std::vector<float> probabilities;
    };

    namespace internal {
        struct Transition {
            std::uint8_t increase_color;
            std::uint8_t decrease_color;
        };

        // TODO: Can this be constexpr?
        template<typename Rng>
        inline auto find_transitions(const Lands& lands, const Lands& availableLands, Rng& rng) -> std::vector<Transition> {
            // TODO: Implement.
            return {};
        };

        constexpr auto get_available_lands(const CardValues& pool, const CardValues& option,
                                           const CardValues& basics) -> Lands {
            // TODO: Implement.
            return {};
        }

        template<typename Rng>
        constexpr auto choose_random_lands(const Lands& availableLands, Rng& rng) -> Lands {
            // TODO: Implement.
            return {};
        }

        // TODO: If find_transitions can be made constexpr so should this.
        template <typename Rng>
        inline auto calculate_score(const BotState<Rng>& bot_state, BotScore& bot_score) -> BotScore& {
            // TODO: Implement.
            return bot_score;
        }

        inline Eigen::ArrayXi get_card_indices(const std::vector<std::string>& names) {
            std::vector<int> picked_indices;
            picked_indices.reserve(names.size());
            for (size_t i=0; i < names.size(); i++) {
                const auto iter = mtgdraftbots::constants::CARD_INDICES.find(frozen::string(names[i]));
                if (iter != mtgdraftbots::constants::CARD_INDICES.end()) {
                    picked_indices.push_back(iter->second);
                }
            }
            Eigen::ArrayXi result(picked_indices.size());
            for (size_t i=0; i < picked_indices.size(); i++) {
                result[i] = picked_indices[i];
            }
            return result;
        }
    }

    // TODO: Make this constexpr
    inline auto DrafterState::calculate_pick_from_options(const std::vector<Option>& options) const -> BotScore {
        using namespace internal;
        const float packFloat = constants::WEIGHT_Y_DIM * static_cast<float>(pack_num) / num_packs;
        const float pickFloat = constants::WEIGHT_X_DIM * static_cast<float>(pick_num) / num_picks;
        const std::size_t packLower = static_cast<std::size_t>(packFloat);
        const std::size_t pickLower = static_cast<std::size_t>(pickFloat);
        const std::size_t packUpper = std::min(packLower + 1, constants::WEIGHT_Y_DIM - 1);
        const std::size_t pickUpper = std::min(pickLower + 1, constants::WEIGHT_X_DIM - 1);
        BotState<std::mt19937_64> bot_state{
            CardValues{internal::get_card_indices(picked)},
            CardValues{internal::get_card_indices(seen)},
            CardValues{internal::get_card_indices(cards_in_pack)},
            CardValues{internal::get_card_indices(basics)},
            {{pickLower, packLower}, {pickUpper, packUpper}},
            {packFloat - packLower, pickFloat - pickLower},
            std::mt19937_64{seed}
        };
        BotScore result;
        for (const auto& option_indices : options) {
            Eigen::ArrayXi option_card_indices(option_indices.size());
            for (size_t i = 0; i < option_indices.size(); i++) {
                option_card_indices[i] = bot_state.cards_in_pack.indices[option_indices[i]];
            }
            CardValues option(option_card_indices);
            const Lands available_lands = get_available_lands(bot_state.picked, option, bot_state.basics);
            const Lands initial_lands = choose_random_lands(available_lands, bot_state.rng);
            BotScore next_score;
            calculate_score(bot_state, next_score);
            BotScore prev_score;
            prev_score.score = next_score.score - 1;
            while (next_score.score > prev_score.score) {
                prev_score = next_score;
                for (const auto& [increase, decrease] : find_transitions(prev_score.lands, available_lands, bot_state.rng)) {
                    // TODO: Implement
                }
            }
        }
        return result;
    }
}
#endif
