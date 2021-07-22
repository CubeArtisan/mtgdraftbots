#ifndef MTGDRAFTBOTS_ORACLES_HPP
#define MTGDRAFTBOTS_ORACLES_HPP

#include <map>
#include <memory>
#include <string>

#include "mtgdraftbots/types.hpp"
#include "mtgdraftbots/details/cardvalues.hpp"
#include "mtgdraftbots/details/simd.hpp"

namespace mtgdraftbots {
    struct OracleResult {
        std::string title;
        std::string tooltip;
        float weight;
        float value;
        std::vector<float> per_card;
    };

    namespace details {

        constexpr std::size_t WEIGHT_X_DIM = 3;
        constexpr std::size_t WEIGHT_Y_DIM = 15;
        using Weights = std::array<std::array<float, WEIGHT_Y_DIM>, WEIGHT_X_DIM>;

        inline static std::map<std::string, Weights, std::less<>> weights_map;

        using OracleScore = std::vector<std::array<float, NUM_LAND_COMBS>>; // Score for each option.
        using OracleScores = std::vector<OracleScore>; // Score for each card in each option.

        struct OracleMultiResult : Weighted<OracleScore> {
            std::string title;
            std::string tooltip;
            OracleScores per_card;
        };

        struct Oracle {
            constexpr auto calculate_weight(const Weights& weights, const std::pair<Weighted<Coord>, Weighted<Coord>>& weighted_coords) const noexcept -> float {
                const auto& [coord1, coord2] = weighted_coords;
                const auto& [coord1x, coord1y] = coord1.value;
                const auto& [coord2x, coord2y] = coord2.value;
                return coord1.weight * coord2.weight * weights[coord1x][coord1y]
                    + coord1.weight * (1 - coord2.weight) * weights[coord1x][coord2y]
                    + (1 - coord1.weight) * coord2.weight * weights[coord2x][coord1y]
                    + (1 - coord1.weight) * (1 - coord2.weight) * weights[coord2x][coord2y];
            };

            inline auto calculate_weight(const std::pair<Weighted<Coord>, Weighted<Coord>>& weighted_coords) const noexcept -> float {
                auto iter = weights_map.find(title);
                if (iter != weights_map.end()) return calculate_weight(iter->second, weighted_coords);
                else return 0.0;
            }

            inline OracleMultiResult calculate_result(const BotState& bot_state) const noexcept {
                float weight = calculate_weight(bot_state.weighted_coords);
                OracleScores per_card = calculate_values(bot_state);
                std::size_t max_count = 1;
                for (const auto& option : per_card) max_count = std::max(max_count, option.size());
                OracleScore per_option;
                per_option.reserve(per_card.size());
                for (const auto& option_scores : per_card) {
                    std::array<float, NUM_LAND_COMBS> score{ 0.f };
                    for (const auto& option_score : option_scores) score += option_score;
                    score /= static_cast<float>(max_count);
                    per_option.push_back(score);
                }
                return {
                    { weight, std::move(per_option) },
                    std::string(title),
                    std::string(tooltip),
                    std::move(per_card),
                };
            }

            constexpr Oracle(std::string_view name_, std::string_view tooltip_) noexcept : title(name_), tooltip(tooltip_) { }
            virtual ~Oracle() = default;

            std::string_view title;
            std::string_view tooltip;

        protected:
            virtual OracleScores calculate_values(const BotState& bot_state) const noexcept = 0;
        };

        namespace oracles {
            struct RatingOracle : public Oracle {
                constexpr RatingOracle() noexcept : Oracle{ "Rating", "The rating based on the current land combination." } { }

            protected:
                inline OracleScores calculate_values(const BotState& bot_state) const noexcept override {
                    OracleScores result;
                    result.reserve(bot_state.options.size());
                    for (const auto& option : bot_state.options) {
                        OracleScore score_for_option;
                        score_for_option.reserve(option.size());
                        for (const auto card : option) {
                            const std::size_t idx = bot_state.cards_in_pack[card];
                            const float rating = bot_state.cards.get().ratings[idx];
                            std::array<float, NUM_LAND_COMBS> score_for_card;
                            for (std::size_t i = 0; i < 8; i++) {
                                score_for_card[i] = rating * bot_state.land_combs.first[idx][i];
                            }
                            score_for_option.push_back(score_for_card);
                        }
                    }
                    return result;
                }
            };

            struct PickSynergyOracle : public Oracle {
                constexpr PickSynergyOracle() noexcept : Oracle{ "Pick Synergy", "A score of how well this card synergizes with the current picks." } { }

            protected:
                inline OracleScores calculate_values(const BotState& bot_state) const noexcept override {
                    OracleScores result;
                    result.reserve(bot_state.options.size());
                    for (const auto& option : bot_state.options) {
                        OracleScore score_for_option;
                        score_for_option.reserve(option.size());
                        for (const auto card : option) {
                            const std::size_t idx = bot_state.cards_in_pack[card];
                            const Embedding& card_embed = bot_state.cards.get().embeddings[idx];
                            const float norm = card_embed * card_embed;
                            if (norm <= 0.f) {
                                score_for_option.push_back({ 0 });
                            }
                            else {
                                std::array<float, NUM_LAND_COMBS> score_for_card;
                                for (std::size_t i = 0; i < 8; i++) {
                                    score_for_card[i] = bot_state.land_combs.first[idx][i] * (card_embed * bot_state.pool_embeddings[i] / std::sqrt(norm) + 1.f) / 2.f;
                                }
                                score_for_option.push_back(score_for_card);
                            }
                        }
                    }
                    return result;
                }
            };

            struct InternalSynergyOracle : public Oracle {
                constexpr InternalSynergyOracle() noexcept : Oracle{ "Internal Synergy", "A score of how well current picks in these colors synergize with each other." } { }

            protected:
                inline OracleScores calculate_values(const BotState& bot_state) const noexcept override {
                    std::array<float, NUM_LAND_COMBS> scores{ 0.f };
                    for (const auto idx : bot_state.picked) {
                        const Embedding& card_embed = bot_state.cards.get().embeddings[idx];
                        float norm = card_embed * card_embed;
                        if (norm <= 0.f) {
                            scores += 0.5f * bot_state.land_combs.first[idx];
                        }
                        else {
                            for (std::size_t i = 0; i < NUM_LAND_COMBS; i++) {
                                scores[i] += bot_state.land_combs.first[idx][i] * (card_embed * bot_state.pool_embeddings[i] / std::sqrt(norm) + 1.f) / 2.f;
                            }
                        }
                    }
                    scores /= static_cast<float>(bot_state.picked.size());
                    return OracleScores(bot_state.options.size(), OracleScore(1, scores));
                }
            };

            struct ColorsOracle : public Oracle {
                constexpr ColorsOracle() noexcept : Oracle{ "Colors", "A score of how well these colors fit in with the picks so far." } { }

            protected:
                inline OracleScores calculate_values(const BotState& bot_state) const noexcept override {
                    std::array<float, NUM_LAND_COMBS> scores{ 0.f };
                    for (const auto idx : bot_state.picked) {
                        const float rating = bot_state.cards.get().ratings[idx];
                        scores += rating * bot_state.land_combs.first[idx];
                    }
                    scores /= static_cast<float>(bot_state.picked.size());
                    return OracleScores(bot_state.options.size(), OracleScore(1, scores));
                }
            };

            struct ExternalSynergyOracle : public Oracle {
                constexpr ExternalSynergyOracle() noexcept : Oracle{ "External Synergy", "A score of how cards picked so far synergize with the other cards in these colors that have been seen so far." } { }

            protected:
                inline OracleScores calculate_values(const BotState& bot_state) const noexcept override {
                    std::array<float, NUM_LAND_COMBS> scores{ 0.f };
                    for (const auto idx : bot_state.seen) {
                        const Embedding& card_embed = bot_state.cards.get().embeddings[idx];
                        float norm = card_embed * card_embed;
                        if (norm <= 0.f) {
                            scores += 0.5f * bot_state.land_combs.first[idx];
                        }
                        else {
                            for (std::size_t i = 0; i < NUM_LAND_COMBS; i++) {
                                scores[i] += bot_state.land_combs.first[idx][i] * (card_embed * bot_state.pool_embeddings[i] / std::sqrt(norm) + 1.f) / 2.f;
                            }
                        }
                    }
                    scores /= static_cast<float>(bot_state.seen.size());
                    return OracleScores(bot_state.options.size(), OracleScore(1, scores));
                }
            };

            struct OpennessOracle : public Oracle {
                constexpr OpennessOracle() noexcept : Oracle{ "Openness", "A score of how many and how good the card we have seen in these colors." } { }

            protected:
                inline OracleScores calculate_values(const BotState& bot_state) const noexcept override {
                    std::array<float, NUM_LAND_COMBS> scores{ 0.f };
                    for (const auto idx : bot_state.seen) {
                        const float rating = bot_state.cards.get().ratings[idx];
                        scores += rating * bot_state.land_combs.first[idx];
                    }
                    scores /= static_cast<float>(bot_state.seen.size());
                    return OracleScores(bot_state.options.size(), OracleScore(1, scores));
                }
            };
        }

        const std::array<std::unique_ptr<const Oracle>, 6> ORACLES{
            std::make_unique<oracles::RatingOracle>(),
            std::make_unique<oracles::PickSynergyOracle>(),
            std::make_unique<oracles::ColorsOracle>(),
            std::make_unique<oracles::InternalSynergyOracle>(),
            std::make_unique<oracles::OpennessOracle>(),
            std::make_unique<oracles::ExternalSynergyOracle>(),
        };
    }
}
#endif