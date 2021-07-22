#ifndef MTGDRAFTBOTS_CARDVALUES_HPP
#define MTGDRAFTBOTS_CARDVALUES_HPP

#include <optional>

#include "mtgdraftbots/types.hpp"
#include "mtgdraftbots/details/cardcost.hpp"

namespace mtgdraftbots::details {
    using Colors = std::array<bool, 5>;

    struct CardValue {
        float rating;
        Embedding embedding;
        CardCost cost;
        std::uint8_t produces;
    };

    inline static std::map<std::string, CardValue> card_lookups;

    struct CardValues {
        inline explicit CardValues(const std::vector<std::string>& card_names) {
            ratings.reserve(card_names.size());
            embeddings.reserve(card_names.size());
            costs.reserve(card_names.size());
            for (const std::string& name : card_names) {
                auto iter = card_lookups.find(name);
                if (iter != card_lookups.end()) {
                    ratings.push_back(iter->second.rating);
                    embeddings.push_back(iter->second.embedding);
                    costs.push_back(iter->second.cost);
                    produces.push_back(iter->second.produces);
                }
                else {
                    ratings.push_back(0.1f);
                    embeddings.push_back({ 0 });
                    costs.push_back({});
                    produces.push_back(32);
                }
            }
        }

        std::vector<float> ratings;
        std::vector<Embedding> embeddings;
        std::vector<CardCost> costs;
        std::vector<std::uint8_t> produces;
    };
}
#endif