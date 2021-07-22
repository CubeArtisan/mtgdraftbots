#ifndef MTGDRAFTBOTS_DETAILS_CARDCOST_HPP
#define MTGDRAFTBOTS_DETAILS_CARDCOST_HPP

#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <variant>
#include <vector>

#include <mpark/variant.hpp>
#ifdef USE_VECTORCLASS
#include <vectorclass.h>
#endif

#include "mtgdraftbots/details/simd.hpp"
#include "mtgdraftbots/types.hpp"
#include "mtgdraftbots/details/constants.hpp"

namespace mtgdraftbots::details {
    constexpr std::array<LandsMask, 32> MASK_BY_COMB_INDEX = ([]() {
        using namespace constants;
        std::array<LandsMask, 32> result{ {{Mask::OFF}} };
        for (std::size_t i = 0; i < 32; i++) {
            for (std::size_t j = 0; j < 32; j++) {
                for (std::size_t k = 0; k < 5; k++) {
                    if (COLOR_COMBINATIONS[i][k] & COLOR_COMBINATIONS[j][k]) {
                        result[i][j] = Mask::ON;
                        break;
                    }
                }
            }
        }
        return result;
        })();

    // This doesn't make the code faster to template, but makes some things cleaner.
    template<std::uint8_t>
    struct ManaRequirements;

    template<>
    struct ManaRequirements<0> {
         constexpr auto calculate_probability(const Lands&) const noexcept -> float { return 1.f; }

         constexpr bool operator==(const ManaRequirements&) const noexcept { return true; }
    };

    template<>
    struct ManaRequirements<1> {
        constexpr auto calculate_probability(const Lands& lands) const noexcept -> float {
            using namespace constants;
            const std::uint8_t usable = sum_masked(valid_lands, lands);
            // We convert back to float here from 16 bit fixed point.
            return constants::PROB_TABLE[offset | usable];
        }

        constexpr ManaRequirements(std::size_t combIndex, std::size_t devotionCount,
                                   std::size_t cmc) noexcept
                : valid_lands{MASK_BY_COMB_INDEX[combIndex]},
                  offset(((std::min(cmc,constants::NUM_CMC - 1) * constants::NUM_REQUIRED_A
                                  + std::min(devotionCount,constants::NUM_REQUIRED_A - 1)) * constants::NUM_REQUIRED_B)
                         << (3 * constants::COUNT_DIMS_EXP))
        { }

        constexpr ManaRequirements(const LandsMask& mask, std::size_t devotionCount, std::size_t cmc) noexcept
                : valid_lands{mask},
                  offset(((std::min(cmc,constants::NUM_CMC - 1) * constants::NUM_REQUIRED_A
                                  + std::min(devotionCount,constants::NUM_REQUIRED_A - 1)) * constants::NUM_REQUIRED_B)
                         << (3 * constants::COUNT_DIMS_EXP))
        { }


        constexpr ManaRequirements() noexcept = default;
        constexpr ManaRequirements(const ManaRequirements&) noexcept = default;
        constexpr ManaRequirements& operator=(const ManaRequirements&) noexcept = default;
        constexpr ManaRequirements(ManaRequirements&&) noexcept = default;
        constexpr ManaRequirements& operator=(ManaRequirements&&) noexcept = default;
        constexpr bool operator==(const ManaRequirements& other) const noexcept = default;

        template<std::uint8_t n>
        friend struct ManaRequirements;

    private:
        LandsMask valid_lands{Mask::OFF};
        std::size_t offset{0}; // This includes CMC from construction as well as the count of the requiremnt.
    };

    template<>
    struct ManaRequirements<2> {
        constexpr auto calculate_probability(const Lands& lands) const noexcept -> float {
            using namespace constants;
            const std::uint32_t usable_a  = sum_masked( valid_lands_a, lands);
            const std::uint32_t usable_b  = sum_masked( valid_lands_b, lands);
            const std::uint32_t usable_ab = sum_masked(valid_lands_ab, lands);
            return PROB_TABLE[
                offset | (usable_ab << (2 * COUNT_DIMS_EXP)) | (usable_b << COUNT_DIMS_EXP) | usable_a
            ];
        }

        constexpr ManaRequirements(std::size_t combAIndex, std::size_t devotionACount,
                                   std::size_t combBIndex, std::size_t devotionBCount,
                                   std::size_t cmc) noexcept 
                : valid_lands_a(MASK_BY_COMB_INDEX[combAIndex] & ~MASK_BY_COMB_INDEX[combBIndex]),
                  valid_lands_b(MASK_BY_COMB_INDEX[combBIndex] & ~MASK_BY_COMB_INDEX[combAIndex]),
                  valid_lands_ab(MASK_BY_COMB_INDEX[combAIndex] | MASK_BY_COMB_INDEX[combBIndex]),
                  offset(((std::min(cmc,constants::NUM_CMC - 1) * constants::NUM_REQUIRED_A 
                                  + std::min(devotionACount,constants::NUM_REQUIRED_A - 1)) * constants::NUM_REQUIRED_B
                              + std::min(devotionBCount,constants::NUM_REQUIRED_B - 1)) << (3 * constants::COUNT_DIMS_EXP)) {
            if (devotionACount < devotionBCount) {
                *this = ManaRequirements(combBIndex, devotionBCount, combAIndex, devotionACount, cmc);
            }
        }

        constexpr ManaRequirements() noexcept = default;
        constexpr ManaRequirements(const ManaRequirements&) noexcept = default;
        constexpr ManaRequirements& operator=(const ManaRequirements&) noexcept = default;
        constexpr ManaRequirements(ManaRequirements&&) noexcept = default;
        constexpr ManaRequirements& operator=(ManaRequirements&&) noexcept = default;
        constexpr bool operator==(const ManaRequirements& other) const noexcept = default;

    private:
        LandsMask valid_lands_a{Mask::OFF};
        LandsMask valid_lands_b{Mask::OFF};
        LandsMask valid_lands_ab{Mask::OFF};
        std::size_t offset{0}; // This contains CMC
    };

    template<uint8_t n> requires (n > 2)
    struct ManaRequirements<n> {
        constexpr auto calculate_probability(const Lands& lands) const noexcept -> float {
            float result = 1;
            for (const ManaRequirements<1>& sub_requirement : sub_requirements) {
                result *= sub_requirement.calculate_probability(lands);
            }
            return result;
        }

        constexpr ManaRequirements(const std::array<std::pair<std::size_t, std::size_t>, n>& devotions,
                                   std::size_t cmc) noexcept {
            LandsMask combined_mask{Mask::OFF};
            std::size_t total_devotion = 0;
            for (size_t i=0; i < n; i++) {
                const auto [comb_index, devotion_count] = devotions[i];
                total_devotion += devotion_count;
                sub_requirements[i] = ManaRequirements<1>(comb_index, devotion_count, cmc);
                combined_mask = combined_mask | sub_requirements[i].valid_lands;
            }
            sub_requirements[n] = ManaRequirements<1>(combined_mask, total_devotion, cmc);
        }
        constexpr ManaRequirements() noexcept = default;
        constexpr ManaRequirements(const ManaRequirements&) noexcept = default;
        constexpr ManaRequirements& operator=(const ManaRequirements&) noexcept = default;
        constexpr ManaRequirements(ManaRequirements&&) noexcept = default;
        constexpr ManaRequirements& operator=(ManaRequirements&&) noexcept = default;
        constexpr bool operator==(const ManaRequirements& other) const noexcept = default;

    private:
        std::array<ManaRequirements<1>, n + 1> sub_requirements;
    };

    using RequirementVariant = mpark::variant<ManaRequirements<0>, ManaRequirements<1>,
                                              ManaRequirements<2>, ManaRequirements<3>,
                                              ManaRequirements<4>, ManaRequirements<5>>;

    template <typename Container>
    constexpr auto get_requirement(std::uint8_t cmc, const Container& symbols) noexcept -> RequirementVariant {
        using namespace constants;
        std::array<std::uint8_t, 32> devotion_index{255};
        std::uint8_t devotion_count{0};
        std::array<std::pair<std::uint8_t, std::uint8_t>, 5> devotions;
        for (const auto& symbol : symbols) {
            std::array<bool, 5> found_colors{false};
            bool found_any = false;
            for (const char c : symbol) {
                if (c == '2' || c == 'P' || c == 'p') {
                    found_any = false;
                    break;
                } else {
                    auto iter = COLOR_TO_INDEX.find(c);
                    if (iter != COLOR_TO_INDEX.end()) {
                        found_colors[iter->second] = true;
                        found_any = true;
                    }
                }
            }
            if (found_any) {
                // We know this isn't end since COLOR_COMBINATIONS contains every possibility.
                // We also know it isn't zero since at least 1 is true.
                std::size_t index = std::distance(
                    std::begin(COLOR_COMBINATIONS),
                    std::find(std::begin(COLOR_COMBINATIONS), std::end(COLOR_COMBINATIONS), found_colors)
                );
                if (devotion_index[index] != 255) {
                    devotions[devotion_count].first = index;
                    devotions[devotion_count].second = 1;
                    devotion_index[index] = devotion_count;
                    devotion_count++;
                } else {
                    devotions[devotion_index[index]].second++;
                }
            }
        }
        switch (devotion_count) {
        case 1:
            return ManaRequirements<1>{devotions[0].first, devotions[0].second, cmc};
        case 2:
            return ManaRequirements<2>{devotions[0].first, devotions[0].second,
                                       devotions[1].first, devotions[1].second,
                                       cmc};
        case 3:
            return ManaRequirements<3>{std::array<std::pair<std::size_t, std::size_t>, 3>{{
                                        {devotions[0].first, devotions[0].second},
                                        {devotions[1].first, devotions[1].second},
                                        {devotions[2].first, devotions[2].second},
                                       }}, cmc};
        case 4:
            return ManaRequirements<4>{std::array<std::pair<std::size_t, std::size_t>, 4>{{
                                        {devotions[0].first, devotions[0].second},
                                        {devotions[1].first, devotions[1].second},
                                        {devotions[2].first, devotions[2].second},
                                        {devotions[3].first, devotions[3].second},
                                       }}, cmc};
        case 5:
            return ManaRequirements<5>{std::array<std::pair<std::size_t, std::size_t>, 5>{{
                                        {devotions[0].first, devotions[0].second},
                                        {devotions[1].first, devotions[1].second},
                                        {devotions[2].first, devotions[2].second},
                                        {devotions[3].first, devotions[3].second},
                                        {devotions[4].first, devotions[4].second},
                                       }}, cmc};
        }
        return ManaRequirements<0>();
    }

    struct CardCost : public RequirementVariant {
        constexpr auto calculate_probability(const Lands& lands) const -> float {
            return mpark::visit([&lands](const auto& requirement) { return requirement.calculate_probability(lands); },
                                *this);
        }

        constexpr CardCost() noexcept
                : RequirementVariant(ManaRequirements<0>{})
        { }

        constexpr CardCost(const CardDetails& card) noexcept
                : RequirementVariant(get_requirement(card.cmc, card.cost_symbols))
        { }

        template<typename Container>
        constexpr CardCost(std::uint8_t cmc, const Container& symbols) noexcept
                : RequirementVariant(get_requirement(cmc, symbols))
        { }

        using RequirementVariant::RequirementVariant;
        using RequirementVariant::operator=;

        constexpr bool operator==(const RequirementVariant& other) const noexcept {
            if (this->index() != other.index()) return false;
            if (this->index() == std::variant_npos) return true;
            return mpark::visit([](const auto& req1, const auto& req2) {
                if constexpr (std::is_same_v<decltype(req1), decltype(req2)>) return req1 == req2;
                else return false;
            }, *this, other);
        }
    };
}

#endif
