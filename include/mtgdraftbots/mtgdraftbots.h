#ifndef MTGDRAFTBOTS_H
#define MTGDRAFTBOTS_H

#include <array>
#include <bit>
#include <bits/c++config.h>
#include <cstring>
#include <cstdint>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include <Eigen/Eigen>

#include "mtgdraftbots/oracles.hpp"
#include "mtgdraftbots/details/cardcost.hpp"
#include "mtgdraftbots/details/types.hpp"
#include "mtgdraftbots/generated/constants.h"

namespace mtgdraftbots {

    namespace internal {

        struct CardValues {
            // Can this be constexpr?
            inline CardValues(const Eigen::ArrayXi indices_)
                : indices(indices_),
                  embeddings(indices.rows()),
                  ratings(indices.rows())
            {
                costs.reserve(indices.rows());
                for (std::size_t i=0; i < indices.rows(); i++) {
                    embeddings.row(i) = Eigen::Matrix<float, 1, mtgdraftbots::constants::EMBEDDING_SIZE>(
                        mtgdraftbots::constants::CARD_EMBEDDINGS[indices[i]]
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
            inline BotState(const DrafterState& drafter_state, Rng&& _rng) : rng(_rng) {}

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
        std::array<OracleResult, internal::oracles::ORACLES.size()> oracle_results;
        float total_nonland_prob;
        Option option;
        std::array<bool, 5> colors;
        Lands lands;
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
    }

    // TODO: Make this constexpr
    inline auto DrafterState::calculate_pick_from_options(const std::vector<Option>& options) const -> BotScore {
        using namespace internal;
        BotState bot_state{*this, std::mt19937_64{seed}};
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
                }
            }
        }
        // TODO: Implement
        return result;
    }
}
#endif
