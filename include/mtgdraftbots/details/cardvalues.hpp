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
        // These can be either names or oracle_ids depending on the lookup table. card_lookups defaults to oracle_ids.
        inline CardValues(const std::vector<std::string>& card_names, const std::map<std::string, CardValue> lookup) {
            ratings.reserve(card_names.size());
            embeddings.reserve(card_names.size());
            costs.reserve(card_names.size());
            for (const std::string& name : card_names) {
                auto iter = lookup.find(name);
                if (iter != lookup.end()) {
                    ratings.push_back(iter->second.rating);
                    embeddings.push_back(iter->second.embedding);
                    costs.push_back(iter->second.cost);
                    produces.push_back(iter->second.produces);
                }
                else {
                    ratings.push_back(0.5f);
                    embeddings.push_back({ 0 });
                    costs.push_back({});
                    produces.push_back(32);
                }
            }
        }

        inline explicit CardValues(const std::vector<std::string>& card_names) 
            : CardValues(card_names, card_lookups)
        { }

        CardValues() = default;

        void push_back(CardValue value) {
            ratings.push_back(value.rating);
            embeddings.push_back(value.embedding);
            costs.push_back(value.cost);
            produces.push_back(value.produces);
        }

        std::size_t size() const {
            return ratings.size();
        }

        std::vector<float> ratings;
        std::vector<Embedding> embeddings;
        std::vector<CardCost> costs;
        std::vector<std::uint8_t> produces;
    };
}
#endif