#ifndef MTGDRAFTBOTS_DETAILS_TYPES_HPP
#define MTGDRAFTBOTS_DETAILS_TYPES_HPP

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "mtgdraftbots/details/simd.hpp"

namespace mtgdraftbots {
    using Coord = std::pair<std::uint8_t, std::uint8_t>;
    using Option = std::vector<unsigned int>;

    struct BotResult;

    struct DrafterState {
        std::vector<unsigned int> picked;
        std::vector<unsigned int> seen;
        std::vector<unsigned int> cards_in_pack;
        std::vector<unsigned int> basics;
        std::vector<std::string> card_oracle_ids;
        unsigned int pack_num;
        unsigned int num_packs;
        unsigned int pick_num;
        unsigned int num_picks;
        unsigned int seed;
    };

    namespace details {
        struct CardValues;

        static inline Embedding embedding_bias{ 0 };

        constexpr std::size_t NUM_LAND_COMBS = 8;

        struct BotState : public DrafterState {
            std::vector<Option> options;
            std::pair<std::vector<std::array<float, NUM_LAND_COMBS>>, std::array<Lands, NUM_LAND_COMBS>> land_combs;
            std::pair<Weighted<Coord>, Weighted<Coord>> weighted_coords;
            std::reference_wrapper<const CardValues> cards;
            std::array<Embedding, NUM_LAND_COMBS> pool_embeddings{ embedding_bias };

            constexpr void calculate_embeddings() & noexcept;
        };

        struct CardDetails {
            std::string name;
            std::vector<std::string> cost_symbols;
            Embedding embedding;
            std::optional<std::array<bool, 5>> produces;
            std::uint8_t cmc;
            float rating;
        };
    }
}
#endif
