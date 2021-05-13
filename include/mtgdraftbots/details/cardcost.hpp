#ifndef MTGDRAFTBOTS_DETAILS_CARDCOST_HPP
#define MTGDRAFTBOTS_DETAILS_CARDCOST_HPP

#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <mpark/variant.hpp>

#include "mtgdraftbots/details/types.hpp"
#include "mtgdraftbots/generated/constants.h"

namespace mtgdraftbots::internal {
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
    constexpr auto sum_masked(const LandsMask& mask, const Lands& lands) -> std::uint8_t {
        if (std::is_constant_evaluated()) {
            std::uint8_t result = 0;
            for (std::size_t i=0; i < lands.size(); i++) {
                result += static_cast<std::uint8_t>(mask[i]) & lands[i];
            }
            return result;
        } else {
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
        constexpr auto calculate_probability(const Lands& lands) const -> float {
            using namespace constants;
            const std::uint8_t usable = sum_masked(valid_lands, lands);
            // We convert back to float here from 16 bit fixed point.
            return constants::PROB_TABLE[offset | usable] * PROB_SCALING_FACTOR;
        }
        // TODO: Implement constructor
        ManaRequirements() {}

    private:
        LandsMask valid_lands;
        std::size_t offset; // This includes CMC from construction as well as the count of the requiremnt.
    };

    template<>
    struct ManaRequirements<2> {
        constexpr auto calculate_probability(const Lands& lands) const -> float {
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
        constexpr auto calculate_probability(const Lands& lands) const -> float {
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
        constexpr auto calculate_probability(const Lands& lands) const -> float {
            return visit([&lands](const auto& requirement) { return requirement.calculate_probability(lands); },
                              inner_requirement);
        }

        constexpr CardCost(std::uint8_t cmc, std::array<std::string_view, 11> symbols) {}

        std::size_t cost_index;

    private:
        mpark::variant<ManaRequirements<0>, ManaRequirements<1>, ManaRequirements<2>,
                       ManaRequirements<3>, ManaRequirements<4>, ManaRequirements<5>> inner_requirement;
    };
}

#endif
