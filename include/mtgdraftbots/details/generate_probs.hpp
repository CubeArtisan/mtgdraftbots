#ifndef MTGDRAFTBOTS_GENERATE_PROBS_H
#define MTGDRAFTBOTS_GENERATE_PROBS_H

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <random>

#include "mtgdraftbots/types.hpp"
#include "mtgdraftbots/details/cardvalues.hpp"

namespace mtgdraftbots::details {
    template<std::uint_fast32_t N, std::enable_if_t<(N == 4), std::nullptr_t> = nullptr>
    constexpr std::uint_fast32_t rand_lcg(std::uint_fast32_t x_prev) noexcept {
        return x_prev * 1664525UL + 1013904223UL;
    }

    template<std::uint_fast32_t N, std::enable_if_t<(N == 8), std::nullptr_t> = nullptr>
    constexpr std::uint_fast32_t rand_lcg(std::uint_fast32_t x_prev) noexcept {
        return x_prev * 2862933555777941757ULL + 3037000493ULL;
    }
    struct Rand {
        std::uint_fast32_t state;
        constexpr std::uint_fast32_t operator( )() noexcept {
            state = rand_lcg<sizeof(std::uint_fast32_t)>(state);
            return state & 0b1'1111;
        }
    };

    struct Transition {
        std::uint8_t increase_color;
        std::uint8_t decrease_color;
    };

    struct transitions_range {
        template<typename Func>
        constexpr void operator()(Func func) const {
            for (std::uint8_t inc = 0; inc < current_lands.size(); inc++) {
                if (current_lands[inc] < available_lands[inc]) {
                    for (std::uint8_t dec = 0; dec < current_lands.size(); dec++) {
                        if (current_lands[dec] > 0 && inc != dec) func(Transition{ inc, dec });
                    }
                }
            }
        }
        Lands current_lands;
        Lands available_lands;
    };

    constexpr auto get_available_lands(const DrafterState& drafter_state, const CardValues& cards) -> Lands {
        Lands result{ 0 };
        for (std::size_t picked_index : drafter_state.picked) {
            if (cards.produces[picked_index] < 32) {
                result[cards.produces[picked_index]]++;
            }
        }
        for (std::size_t basic_index : drafter_state.basics) {
            if (cards.produces[basic_index] < 32) {
                result[cards.produces[basic_index]] += 17;
            }
        }
        return result;
    }

    constexpr Lands get_random_lands(const Lands& available_lands, Rand& rng) {
        std::uint8_t total_available = 0;
        for (std::uint8_t x : available_lands) total_available += x;
        if (total_available < 17) return available_lands;
        Lands result{ 0 };
        for (std::size_t i = 0; i < 17; i++) {
            std::size_t ind = rng();
            while (static_cast<std::size_t>(available_lands[ind]) <= static_cast<std::size_t>(result[ind])) {
                ind = rng();
            }
            result[ind]++;
        }
        return result;
    }

    std::pair<std::vector<std::array<float, NUM_LAND_COMBS>>, std::array<Lands, NUM_LAND_COMBS>> generate_probs(
            const DrafterState& drafter_state, const CardValues& cards) {
        Rand rng{drafter_state.seed};
        std::vector<std::array<float, NUM_LAND_COMBS>> result(drafter_state.card_oracle_ids.size());
        using ScoreValue = std::tuple<float, std::vector<float>, Lands>;
        std::array<Lands, NUM_LAND_COMBS> result_lands;
        std::array<std::array<std::uint8_t, 5>, NUM_LAND_COMBS> found_values{ {{ 0 }} };
        constexpr std::array<LandsMask, 5> masks{
            MASK_BY_COMB_INDEX[1],
            MASK_BY_COMB_INDEX[2],
            MASK_BY_COMB_INDEX[3],
            MASK_BY_COMB_INDEX[4],
            MASK_BY_COMB_INDEX[5],
        };
        const mtgdraftbots::Lands available_lands = get_available_lands(drafter_state, cards);
        for (std::size_t i = 0; i < NUM_LAND_COMBS; i++) {
            ScoreValue prev_score{ -1.f, std::vector<float>(drafter_state.card_oracle_ids.size(), 0.f), get_random_lands(available_lands, rng) };
            ScoreValue current_score{ 0.f, std::vector<float>(drafter_state.card_oracle_ids.size(), 0.f), std::get<Lands>(prev_score) };
            while (std::get<float>(prev_score) < std::get<float>(current_score)) {
                prev_score = current_score;
                for (std::uint8_t increase = 1; increase < 32; increase++) {
                    if (std::get<mtgdraftbots::Lands>(prev_score)[increase] >= available_lands[increase]) continue;
                    for (std::uint8_t decrease = 1; decrease < 32; decrease++) {
                        if (decrease == increase || std::get<mtgdraftbots::Lands>(prev_score)[decrease] == 0) continue;
                        ScoreValue new_score = prev_score;
                        mtgdraftbots::Lands& new_lands = std::get<mtgdraftbots::Lands>(new_score);
                        std::transform(std::begin(masks), std::end(masks), std::begin(found_values[i]),
                            [&](const mtgdraftbots::details::LandsMask& mask) { return mtgdraftbots::details::sum_masked(mask, new_lands); });
                        bool too_close = false;
                        for (std::size_t j = 0; j < i; j++) {
                            std::int8_t difference = 0;
                            for (std::size_t k = 0; k < found_values[i].size(); k++) {
                                difference += std::abs(found_values[j][k] - found_values[i][k]);
                            }
                            if (difference < 5) {
                                too_close = true;
                                break;
                            }
                        }
                        if (too_close) continue;
                        new_lands[increase]++;
                        new_lands[decrease]--;
                        const auto transformation = [&new_lands](const mtgdraftbots::details::CardCost& cost) { return cost.calculate_probability(new_lands); };
                        std::transform(cards.costs.begin(), cards.costs.end(), std::begin(std::get<1>(new_score)), transformation);
                        float max_in_pack_prob = std::reduce(std::begin(drafter_state.cards_in_pack),
                            std::end(drafter_state.cards_in_pack),
                            0.f, [&](auto a, auto b) { return std::max(a, std::get<1>(new_score)[b]); });
                        float min_in_pack_prob = std::reduce(std::begin(drafter_state.cards_in_pack),
                            std::end(drafter_state.cards_in_pack),
                            1.f, [&](auto a, auto b) { return std::min(a, std::get<1>(new_score)[b]); });
                        float sum_picked_prob = std::reduce( std::begin(drafter_state.picked),
                            std::end(drafter_state.picked),
                            0.f, [&](auto a, auto b) { return a + std::get<1>(new_score)[b]; });
                        float mean_seen_prob = std::reduce(std::begin(drafter_state.seen),
                            std::end(drafter_state.seen),
                            0.f, [&](auto a, auto b) { return a + std::get<1>(new_score)[b]; }) / drafter_state.seen.size();
                        std::get<float>(new_score) = sum_picked_prob + 3 * mean_seen_prob + 5 * max_in_pack_prob + 2 * min_in_pack_prob;
                        if (std::get<float>(new_score) > std::get<float>(current_score)) current_score = new_score;
                    }
                }
            }
            for (std::size_t j = 0; j < drafter_state.card_oracle_ids.size(); j++) {
                result[j][i] = std::get<1>(current_score)[j];
            }
            result_lands[i] = std::get<Lands>(current_score);
        }
        return { std::move(result), result_lands };
    }
}
#endif