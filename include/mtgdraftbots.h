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
#include <variant>
#include <vector>

namespace mtgdraftbots {
    using Lands = std::array<std::uint8_t, 32>;
    using Coord = std::pair<float, float>;
    using Option = std::vector<std::uint8_t>;

    struct BotScore;

    struct DrafterState {
        // TODO: Make all these vectors small_vector to avoid dynamic allocations as much as possible.
        std::vector<std::uint16_t> picked;
        std::vector<std::uint16_t> seen;
        std::vector<std::uint16_t> cards_in_pack;
        std::vector<std::uint16_t> basics;
        std::vector<std::string> card_names;
        std::pair<Coord, Coord> coords;
        std::pair<float, float> coord_weights;
        std::uint32_t seed;

#ifdef HAVE_BITCAST
        constexpr
#else // We need this to prevent ODR errors that we'd much rather just avoid with constexpr
        inline
#endif
        auto calculate_pick_from_options(const std::vector<Option>& options) const -> BotScore;
    };

    struct OracleResult {
        std::string title;
        std::string tooltip;
        float weight;
        float value;
        std::vector<float> per_card;
    };

    namespace internal::oracles {
        constexpr std::size_t WEIGHT_X_DIM = 15;
        constexpr std::size_t WEIGHT_Y_DIM = 3;
        using Weights = std::array<std::array<float, WEIGHT_Y_DIM>, WEIGHT_X_DIM>;
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

            Weights weights;
        };

        // TODO: Move into separate constants file.
        constexpr Weights RATING_WEIGHTS{};
        constexpr Weights PICK_SYNERGY_WEIGHTS{};
        constexpr Weights INTERNAL_SYNERGY_WEIGHTS{};
        constexpr Weights COLORS_WEIGHTS{};
        constexpr Weights OPENNESS_WEIGHTS{};

        constexpr Oracle rating_oracle{
            // TODO: Implement.
            [](const BotScore& bot_score) -> OracleResult { return {}; },
            RATING_WEIGHTS,
        };

        constexpr Oracle pick_synergy_oracle{
            // TODO: Implement.
            [](const BotScore& bot_score) -> OracleResult { return {}; },
            PICK_SYNERGY_WEIGHTS,
        };

        constexpr Oracle internal_synergy_oracle{
            // TODO: Implement.
            [](const BotScore& bot_score) -> OracleResult { return {}; },
            INTERNAL_SYNERGY_WEIGHTS,
        };

        constexpr Oracle colors_oracle{
            // TODO: Implement.
            [](const BotScore& bot_score) -> OracleResult { return {}; },
            COLORS_WEIGHTS,
        };

        constexpr Oracle openness_oracle{
            // TODO: Implement.
            [](const BotScore& bot_score) -> OracleResult { return {}; },
            OPENNESS_WEIGHTS,
        };

        constexpr std::array ORACLES = {
            rating_oracle,
            pick_synergy_oracle,
            internal_synergy_oracle,
            colors_oracle,
            openness_oracle,
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
        enum struct Mask : std::uint8_t {
            ON = 0xFF, OFF = 0x00
        };
        using Embedding = std::array<float, 64>;
        using LandsMask = std::array<Mask, 32>;
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
#ifdef HAVE_BITCAST
            constexpr
#else
            inline
#endif
            auto calculate_probability(const Lands& lands) const -> float {
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
#ifdef HAVE_BITCAST
            constexpr
#else
            inline
#endif
            auto calculate_probability(const Lands& lands) const -> float {
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
#ifdef HAVE_BITCAST
            constexpr
#else
            inline
#endif
            auto calculate_probability(const Lands& lands) const -> float {
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
#ifdef HAVE_BITCAST
            constexpr
#else
            inline
#endif
            auto calculate_probability(const Lands& lands) const -> float {
                return std::visit([&lands](const auto& requirement) { return requirement.calculate_probability(lands); },
                                  inner_requirement);
            }

            std::size_t cost_index;

        private:
            std::variant<ManaRequirements<0>, ManaRequirements<1>, ManaRequirements<2>,
                         ManaRequirements<3>, ManaRequirements<4>, ManaRequirements<5>> inner_requirement;
        };

        struct CardValues {
            Embedding embedding;
            float rating;
            CardCost cost;
            size_t card_index;
            // I'd like these to be in a separate object so we don't have to carry them everywhere.
            std::optional<std::uint8_t> produces;
            std::string name;
        };

        struct Transition {
            std::uint8_t increase_color;
            std::uint8_t decrease_color;
        };

        // TODO: Can this be constexpr?
        template <typename Generator>
        inline auto find_transitions(const Lands& lands, const Lands& availableLands, Generator& rng) -> std::vector<Transition> {
            // TODO: Implement.
            return {};
        };

        constexpr auto get_available_lands(const std::vector<CardValues>& pool, const std::vector<CardValues>& option,
                                           const std::vector<CardValues>& basics) -> Lands {
            // TODO: Implement.
            return {};
        }

        template <typename Generator>
        constexpr auto choose_random_lands(const Lands& availableLands, Generator& rng) -> Lands {
            // TODO: Implement.
            return {};
        }

        // TODO: If find_transitions can be made constexpr so should this.
        inline auto calculate_score(BotScore& bot_score) -> BotScore& {
            // TODO: Implement.
            return bot_score;
        }
    }

#ifdef HAVE_BITCAST
    constexpr
#else // We need this to prevent ODR errors that we'd much rather just avoid with constexpr
    inline
#endif
    auto DrafterState::calculate_pick_from_options(const std::vector<Option>& options) const -> BotScore {
        // TODO: Implement
        return {};
    }
}
#endif
