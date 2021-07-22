#ifndef MTGDRAFTBOTS_H
#define MTGDRAFTBOTS_H

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include <frozen/string.h>

#include "mtgdraftbots/oracles.hpp"
#include "mtgdraftbots/types.hpp"
#include "mtgdraftbots/details/cardcost.hpp"
#include "mtgdraftbots/details/cardvalues.hpp"
#include "mtgdraftbots/details/constants.hpp"
#include "mtgdraftbots/details/generate_probs.hpp"

namespace mtgdraftbots {
    struct BotScore {
        float score;
        std::array<OracleResult, details::ORACLES.size()> oracle_results;
        Lands lands;
    };

    struct BotResult : public DrafterState {
        std::vector<Option> options;
        std::vector<BotScore> scores;
        unsigned int chosen_option;
    };

    namespace details {
        constexpr void BotState::calculate_embeddings() & noexcept {
            for (std::size_t i = 0; i < NUM_LAND_COMBS; i++) {
                for (std::size_t idx : picked) {
                    pool_embeddings[i] += land_combs.first[idx][i] * cards.get().embeddings[idx];
                }
                l2_normalize(pool_embeddings[i]);
            }
        }
    };

    inline auto calculate_pick_from_options(const DrafterState& drafter_state, const std::vector<Option>& options) -> BotResult {
        using namespace mtgdraftbots::details;
        const float packFloat = details::WEIGHT_Y_DIM * static_cast<float>(drafter_state.pack_num) / drafter_state.num_packs;
        const float pickFloat = details::WEIGHT_X_DIM * static_cast<float>(drafter_state.pick_num) / drafter_state.num_picks;
        const std::size_t packLower = static_cast<std::size_t>(packFloat);
        const std::size_t pickLower = static_cast<std::size_t>(pickFloat);
        const std::size_t packUpper = std::min(packLower + 1, details::WEIGHT_Y_DIM - 1);
        const std::size_t pickUpper = std::min(pickLower + 1, details::WEIGHT_X_DIM - 1);
        BotResult result{ drafter_state, options };
        details::CardValues cards(drafter_state.card_oracle_ids);
        details::BotState bot_state{
            drafter_state,
            options,
            details::generate_probs(drafter_state, cards),
            { {packFloat - packLower, {pickLower, packLower}}, {pickFloat - pickLower, { pickUpper, packUpper } }},
            std::cref(cards),
        };
        bot_state.calculate_embeddings();
        std::vector<details::OracleMultiResult> oracle_results;
        oracle_results.reserve(details::ORACLES.size());
        result.scores.reserve(options.size());
        std::transform(std::begin(details::ORACLES), std::end(details::ORACLES), std::back_inserter(oracle_results),
                       [&](const auto& oracle) { return oracle->calculate_result(bot_state); });
        for (std::size_t i = 0; i < options.size(); i++) {
            std::array<float, details::NUM_LAND_COMBS> scores = { 0.f };
            for (const auto& oracle_result : oracle_results) scores += oracle_result.weight * oracle_result.value[i];
            std::size_t best_index = 0;
            float best_score = scores[0];
            for (std::size_t j = 1; j < NUM_LAND_COMBS; j++) {
                if (scores[j] > best_score) {
                    best_index = j;
                    best_score = scores[j];
                }
            }
            float total_weight = 0.f;
            for (const auto& oracle_result : oracle_results) total_weight += oracle_result.weight;
            std::array<OracleResult, details::ORACLES.size()> best_oracle_results;
            for (std::size_t j = 0; j < best_oracle_results.size(); j++) {
                std::vector<float> per_card;
                per_card.reserve(oracle_results[j].per_card.size());
                for (const auto& scores : oracle_results[j].per_card[i]) per_card.push_back(scores[best_index]);
                best_oracle_results[j] = OracleResult{
                    oracle_results[j].title,
                    oracle_results[j].tooltip,
                    oracle_results[j].weight / total_weight,
                    oracle_results[j].value[i][best_index],
                    std::move(per_card),
                };
            }
            result.scores.push_back({ best_score, best_oracle_results, bot_state.land_combs.second[best_index] });
        }
        std::size_t best_option = 0;
        std::size_t best_result = -1;
        for (std::size_t i = 0; i < result.scores.size(); i++) {
            if (result.scores[i].score > best_result) {
                best_option = i;
                best_result = result.scores[i].score;
            }
        }
        result.chosen_option = best_option;
        return result;
    }

    inline void initialize_draftbots(const std::vector<char>& buffer) {
        const char* cur_pos = buffer.data();
        const char* const end_pos = cur_pos + buffer.size();
        for (std::size_t i = 0; i < details::embedding_bias.size(); i++) {
            details::embedding_bias[i] = *reinterpret_cast<const float*>(cur_pos);
            cur_pos += sizeof(float);
        }
        std::size_t num_oracles = *reinterpret_cast<const std::size_t*>(cur_pos);
        cur_pos += sizeof(std::size_t);
        for (std::size_t i = 0; i < num_oracles; i++) {
            details::Weights weights;
            for (std::size_t x = 0; x < details::WEIGHT_X_DIM; x++) {
                for (std::size_t y = 0; y < details::WEIGHT_Y_DIM; y++) {
                    weights[x][y] = *reinterpret_cast<const float*>(cur_pos);
                    cur_pos += sizeof(float);
                }
            }
            std::size_t length = std::strlen(cur_pos);
            std::string title(cur_pos, cur_pos + length);
            cur_pos += length + 1;
            details::weights_map.insert({ title, weights });
        }
        std::size_t num_cards = *reinterpret_cast<const std::size_t*>(cur_pos);
        cur_pos += sizeof(std::size_t);
        for (std::size_t i = 0; i < num_cards; i++) {
            float rating = *reinterpret_cast<const float*>(cur_pos);
            cur_pos += sizeof(float);
            details::Embedding embedding;
            for (std::size_t j = 0; j < embedding.size(); j++) {
                embedding[j] = *reinterpret_cast<const float*>(cur_pos);
                cur_pos += sizeof(float);
            }
			std::uint8_t produces_index = *reinterpret_cast<const std::uint8_t>(cur_pos);
			cur_pos += sizeof(std::uint8_t);
            std::optional<details::Colors> produces = std::nullopt;
			if (produces_index < 32) {
				produces = details::COLOR_COMBINATIONS[produces_index];
			}
            std::uint8_t cmc = *reinterpret_cast<const std::uint8_t*>(cur_pos);
            cur_pos += sizeof(std::uint8_t);
            std::uint8_t num_symbols = *reinterpret_cast<const std::uint8_t*>(cur_pos);
            cur_pos += sizeof(std::uint8_t);
            std::vector<std::string> symbols;
            symbols.reserve(num_symbols);
            for (std::size_t j = 0; j < num_symbols; j++) {
                std::size_t length = std::strlen(cur_pos);
                symbols.emplace_back(cur_pos, cur_pos + length);
                cur_pos += length + 1;
            }
            details::CardCost cost(cmc, symbols);
            std::size_t length = std::strlen(cur_pos);
            std::string oracle_id(cur_pos, cur_pos + length);
            cur_pos += length + 1;
            details::card_lookups.insert({ oracle_id, details::CardValue{ rating, embedding, cost, produces } });
        }
    }
}
#endif
