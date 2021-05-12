#ifndef MTGDRAFTBOTS_H
#define MTGDRAFTBOTS_H

#include <array>
#include <bit>
#include <bits/c++config.h>
#include <cstring>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "pcg_random.hpp"

#include "generated/constants.h"

// Need this since our optimizations don't work in constexpr without bit_cast.
#ifdef HAVE_BITCAST
#define CONSTEXPR contexpr
#else
#define CONSTEXPR inline
#endif

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

        CONSTEXPR auto calculate_pick_from_options(const std::vector<Option>& options) const -> BotScore;
    };

    struct OracleResult {
        std::string title;
        std::string tooltip;
        float weight;
        float value;
        std::vector<float> per_card;
    };

    namespace internal {
        enum struct Mask : std::uint8_t {
            ON = 0xFF, OFF = 0x00
        };
        using LandsMask = std::array<Mask, 32>;

#ifdef HAVE_BITCAST
        constexpr auto sum_masked(const LandMask& mask, const Lands& lands) -> std::uint8_t {
            std::uint64_t result_64 = 0;
            for (std::uint8_t i = 0; i < 4; i++) {
                std::uint64_t sub_mask  = std::bit_cast<std::array<uint64_t, 4>>(mask)[i];
                std::uint64_t sub_lands = std::bit_cast<std::array<uint64_t, 4>>(lands)[i];
                result_64 += sub_mask & sub_lands;
            }
            auto result_64s = std::bit_cast<std::array<uint32_t, 2>>(result_64);
            auto result_32 = std::bit_cast<std::array<uint16_t, 2>>(result_64s[0] + result_64s[1]);
            auto result_16 = std::bit_cast<std::array<uint8_t, 2>>(static_cast<uint16_t>(result_32[0] + result_32[1]));
            return result_16[0] + result_16[1];
        }
#else
        // Please forgive the ugliness it's needed without bit_cast to follow aliasing rules.
        inline auto sum_masked(const LandsMask& mask, const Lands& lands) -> std::uint8_t {
            std::array<std::uint64_t, 4> values64;
            std::memcpy(values64.data(), lands.data(), lands.size());
            std::array<std::uint64_t, 4> masks64;
            std::memcpy(masks64.data(), mask.data(), mask.size());
            std::uint64_t result64 = 0;
            for (std::uint8_t i = 0; i < 4; i++) {
                result64 += values64[i] & masks64[i];
            }
            std::array<std::uint32_t, 2> values32;
            std::memcpy(values32.data(), &result64, 2 * sizeof(std::uint32_t));
            std::uint32_t result32 = values32[0] + values32[1];
            std::array<std::uint16_t, 2> values16;
            std::memcpy(values16.data(), &result32, 2 * sizeof(std::uint16_t));
            std::uint16_t result16 = values16[0] + values16[1];
            std::array<std::uint8_t, 2> values8;
            std::memcpy(values8.data(), &result16, 2 * sizeof(std::uint8_t));
            return values8[0] + values8[1];
        }
#endif

        struct Requirement {
            std::array<Mask, 32> lands_mask;
            std::uint8_t count;
        };

        // This doesn't make the code that much faster to template, but makes some things much cleaner.
        template<std::uint8_t>
        struct ManaRequirements;

        template<>
        struct ManaRequirements<0> {
             constexpr auto calculate_probability(const Lands&) const -> float { return 0.f; }
        };

        template<>
        struct ManaRequirements<1> {
            CONSTEXPR auto calculate_probability(const Lands& lands) const -> float {
                using namespace constants;
                const std::uint8_t usable = sum_masked(valid_lands, lands);
                // We convert back to float here from 16 bit fixed point.
                return PROB_TABLE[offset | usable] * PROB_SCALING_FACTOR;
            }
            // TODO: Implement constructor
            ManaRequirements() {}

        private:
            LandsMask valid_lands;
            std::size_t offset; // This includes CMC from construction as well as the count of the requiremnt.
        };

        template<>
        struct ManaRequirements<2> {
            CONSTEXPR auto calculate_probability(const Lands& lands) const -> float {
                using namespace constants;
                const std::size_t usable_a  = sum_masked( valid_lands_a, lands);
                const std::size_t usable_b  = sum_masked( valid_lands_b, lands);
                const std::size_t usable_ab = sum_masked(valid_lands_ab, lands);
                return PROB_TABLE[
                    offset | ((usable_ab << COUNT_DIMS_EXP) | usable_b) << COUNT_DIMS_EXP | usable_a
                ] * PROB_SCALING_FACTOR;
            }

            // TODO: Implement constructor
            ManaRequirements() {}
        private:
            LandsMask valid_lands_a;
            LandsMask valid_lands_b;
            LandsMask valid_lands_ab;
            std::size_t offset; // This contains CMC
        };

        template<uint8_t n> requires (n > 2)
        struct ManaRequirements<n> {
            CONSTEXPR auto calculate_probability(const Lands& lands) const -> float {
                float result = 1;
                for (const ManaRequirements<1>& sub_requirement : sub_requirements) {
                    result *= sub_requirement.calculate_probability(lands);
                }
                return result;
            }

            // TODO: Implement constructor
            ManaRequirements<n>() { }

        private:
            std::array<ManaRequirements<1>, n + 1> sub_requirements;
        };

        struct CardCost {
            CONSTEXPR auto calculate_probability(const Lands& lands) const -> float {
                return std::visit([&lands](const auto& requirement) { return requirement.calculate_probability(lands); },
                                  inner_requirement);
            }

            constexpr CardCost(std::uint8_t cmc, std::array<std::string_view, 11> symbols) {}

            std::size_t cost_index;

        private:
            std::variant<ManaRequirements<0>, ManaRequirements<1>, ManaRequirements<2>,
                         ManaRequirements<3>, ManaRequirements<4>, ManaRequirements<5>> inner_requirement;
        };

        struct CardValues {
            constexpr CardValues(const std::string_view name)
                : index(constants::CARD_INDICES.at(name)),
                  embedding(constants::CARD_EMBEDDINGS[index]),
                  rating(constants::CARD_RATINGS[index]),
                  cost(constants::CARD_CMCS[index], constants::CARD_COST_SYMBOLS[index])
            {}
            std::size_t index;
            constants::Embedding embedding;
            float rating;
            CardCost cost;
        };

        struct BotState {
            BotState(const DrafterState& drafter_state) {}

            std::vector<CardValues> picked;
            std::vector<CardValues> seen;
            std::vector<CardValues> cards_in_pack;
            std::vector<CardValues> basics;
            std::pair<Coord, Coord> coords;
            std::pair<float, float> coord_weights;
            pcg32 rng;
        };

        namespace oracles {
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
                [](const BotScore& bot_score) -> OracleResult { return {}; },
                constants::RATING_WEIGHTS,
            };

            constexpr Oracle pick_synergy_oracle{
                // TODO: Implement.
                [](const BotScore& bot_score) -> OracleResult { return {}; },
                constants::PICK_SYNERGY_WEIGHTS,
            };

            constexpr Oracle internal_synergy_oracle{
                // TODO: Implement.
                [](const BotScore& bot_score) -> OracleResult { return {}; },
                constants::INTERNAL_SYNERGY_WEIGHTS,
            };

            constexpr Oracle colors_oracle{
                // TODO: Implement.
                [](const BotScore& bot_score) -> OracleResult { return {}; },
                constants::COLORS_WEIGHTS,
            };

            constexpr Oracle openness_oracle{
                // TODO: Implement.
                [](const BotScore& bot_score) -> OracleResult { return {}; },
                constants::OPENNESS_WEIGHTS,
            };

            constexpr std::array ORACLES = {
                rating_oracle,
                pick_synergy_oracle,
                internal_synergy_oracle,
                colors_oracle,
                openness_oracle,
            };
        }
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
        inline auto find_transitions(const Lands& lands, const Lands& availableLands, pcg32& rng) -> std::vector<Transition> {
            // TODO: Implement.
            return {};
        };

        constexpr auto get_available_lands(const std::vector<CardValues>& pool, const std::vector<CardValues>& option,
                                           const std::vector<CardValues>& basics) -> Lands {
            // TODO: Implement.
            return {};
        }

        constexpr auto choose_random_lands(const Lands& availableLands, pcg32& rng) -> Lands {
            // TODO: Implement.
            return {};
        }

        // TODO: If find_transitions can be made constexpr so should this.
        inline auto calculate_score(const BotState& bot_state, BotScore& bot_score) -> BotScore& {
            // TODO: Implement.
            return bot_score;
        }
    }

    CONSTEXPR auto DrafterState::calculate_pick_from_options(const std::vector<Option>& options) const -> BotScore {
        using namespace internal;
        BotState bot_state{*this};
        pcg32 rng{seed};
        BotScore result;
        for (const auto& option_indices : options) {
            std::vector<CardValues> option;
            option.reserve(option_indices.size());
            for (const auto& index : option_indices) option.push_back(bot_state.cards_in_pack[index]);
            const Lands available_lands = get_available_lands(bot_state.picked, option, bot_state.basics);
            const Lands initial_lands = choose_random_lands(available_lands, rng);
            BotScore next_score;
            calculate_score(bot_state, next_score);
            BotScore prev_score;
            prev_score.score = next_score.score - 1;
            while (next_score.score > prev_score.score) {
                prev_score = next_score;
                for (const auto& [increase, decrease] : find_transitions(prev_score.lands, available_lands, rng)) {
                }
            }
        }
        // TODO: Implement
        return result;
    }
}
#endif
