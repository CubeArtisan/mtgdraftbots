#ifndef MTGDRAFTBOTS_H
#define MTGDRAFTBOTS_H

#include <array>
#include <bit>
#include <bits/c++config.h>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace mtgdraftbots {
    namespace internal {
        enum struct Mask : std::uint8_t {
            ON = 0xFF, OFF = 0x00
        };
        using Embedding = std::array<float, 64>;
        using Lands = std::array<std::uint8_t, 32>;
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

        constexpr std::uint8_t count_usable(const LandsMask& mask, const Lands& lands) {
            std::uint32_t result = 0;
            for (uint8_t i = 0; i < 8; i++) {
                std::uint32_t sub_mask = std::bit_cast<std::array<uint32_t, 8>>(mask)[i]
            std::uint32_t  =

        }

        struct Requirement {
            std::array<Mask, 32> lands_mask;
            std::uint8_t count;
        };

        // This doesn't make the code that much faster, but makes some things much cleaner.
        template<std::uint8_t>
        struct ManaRequirements;

        template<>
        struct ManaRequirements<0> {
             constexpr float calculate_probability(const Lands&) const { return 0; }
        };

        template<>
        struct ManaRequirements<1> {
            constexpr float calculate_probability(const Lands& lands) const {
                const std::uint8_t usable = count_usable(valid_lands, lands);
                // We convert back to float here from 16 bit fixed point.
                return PROB_TABLE[offset | usable] / static_cast<float>(0xFFFF);
            }

        private:
            LandsMask valid_lands;
            std::size_t offset; // This includes CMC from construction as well as the count of the requiremnt.
        };

        template<>
        struct ManaRequirements<2> {
            constexpr float calculate_probability(const Lands& lands) const {
                const std::size_t usable_a  = count_usable( valid_lands_a, lands);
                const std::size_t usable_b  = count_usable( valid_lands_b, lands);
                const std::size_t usable_ab = count_usable(valid_lands_ab, lands);
                return PROB_TABLE[
                    offset | ((usable_ab << COUNT_DIMS_EXP) | usable_b) << COUNT_DIMS_EXP | usable_a
                ] / static_cast<float>(0xFFFF);
            }

        private:
            LandsMask valid_lands_a;
            LandsMask valid_lands_b;
            LandsMask valid_lands_ab;
            std::size_t offset;
        };

        template<uint8_t n> requires (n > 2)
        struct ManaRequirements<n> {
            constexpr float calculate_probability(const Lands& lands) const {
                float result = 1;
                for (const ManaRequirements<1>& sub_requirement : sub_requirements) {
                    result *= sub_requirement.calculate_probability(lands);
                }
                return result;
            }

        private:
            std::array<ManaRequirements<1>, n + 1> sub_requirements;
        };

        struct CardCost {
            constexpr float calculate_probability(const Lands& lands) const {
                return std::visit([&lands](const auto& requirement) { return requirement.calculate_probability(lands); },
                                  inner_requirement);
            }

        private:
            std::variant<ManaRequirements<0>, ManaRequirements<1>, ManaRequirements<2>,
                         ManaRequirements<3>, ManaRequirements<4>, ManaRequirements<5>> inner_requirement;
        };

        struct CardValues {
            Embedding embedding;
            float rating;
            CardCost cost;
        };
    }
}
#endif
