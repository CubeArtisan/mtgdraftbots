#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <execution>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <locale>
#include <map>
#include <numeric>
#include <optional>
#include <queue>
#include <random>
#include <ranges>
#include <set>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <fmt/compile.h>
#include <fmt/core.h>
#include <frozen/map.h>
#include <moodycamel/concurrentqueue.h>
#include <moodycamel/blockingconcurrentqueue.h>
#include <simdjson.h>

#include "mtgdraftbots/details/cardcost.hpp"
#include "mtgdraftbots/details/constants.hpp"
#include "mtgdraftbots/details/types.hpp"

using namespace std::string_view_literals;

constexpr auto FETCH_LANDS = frozen::make_map<std::string_view, std::array<bool, 5>>({
    {"arid mesa"sv,            {true, false, false, true, false}},
    {"bad river"sv,            {false, true, true, false, false}},
    {"bloodstained mire"sv,    {false, false, true, true, false}},
    {"flood plain"sv,          {true, true, false, false, false}},
    {"flooded strand"sv,       {true, true, false, false, false}},
    {"grasslands"sv,           {true, false, false, false, true}},
    {"marsh flats"sv,          {true, false, true, false, false}},
    {"misty rainforest"sv,     {false, true, false, false, true}},
    {"mountain valley"sv,      {false, false, false, true, true}},
    {"polluted delta"sv,       {false, true, true, false, false}},
    {"rocky tar pit"sv,        {false, false, true, true, false}},
    {"scalding tarn"sv,        {false, true, false, true, false}},
    {"windswept heath"sv,      {true, false, false, false, true}},
    {"verdant catacombs"sv,    {false, false, true, false, true}},
    {"wooded foothills"sv,     {false, false, false, true, true}},
    {"prismatic vista"sv,      {true, true, true, true, true}},
    {"fabled passage"sv,       {true, true, true, true, true}},
    {"terramorphic expanse"sv, {true, true, true, true, true}},
    {"evolving wilds"sv,       {true, true, true, true, true}},
});

std::map<std::string, mtgdraftbots::CardDetails> load_card_details(const std::string& carddb_filename, simdjson::ondemand::parser& parser) {
    simdjson::padded_string carddb_json = simdjson::padded_string::load(carddb_filename);
    std::map<std::string, mtgdraftbots::CardDetails> result;
    simdjson::ondemand::document json_doc = parser.iterate(carddb_json);
    for (auto field : json_doc.get_object()) {
        simdjson::ondemand::object card = field.value();
        std::string_view name_lower;
        if (card["name_lower"].get(name_lower)) {
            std::cerr << "Card did not have a valid name_lower property." << std::endl;
            continue;
        }
        std::vector<std::string> cost_symbols;
        simdjson::ondemand::array parsed_cost;
        if (!card["parsed_cost"].get(parsed_cost)) {
            for (std::string_view symbol : parsed_cost) cost_symbols.emplace_back(symbol);
        }
        /* mtgdraftbots::Embedding embedding; */
        /* std::size_t index = 0; */
        /* simdjson::ondemand::array embedding; */
        /* if (!card["embedding"].get(embedding)) { */
        /*     for (double x : card["embedding"]) embedding[index++] = x; */
        /* } */
        std::optional<std::array<bool, 5>> produces = std::nullopt;
        std::string_view type;
        if (!card["type"].get(type) && type.find("Land") != std::string_view::npos) {
            auto fetch_iter = FETCH_LANDS.find(name_lower);
            if (fetch_iter != FETCH_LANDS.end()) {
                produces = fetch_iter->second;
            } else {
                simdjson::ondemand::array produces_json;
                if (!card["color_identity"].get(produces_json)) {
                    produces = {false};
                    for (std::string_view symbol : produces_json) {
                        auto iter = mtgdraftbots::constants::COLOR_TO_INDEX.find(symbol[0]);
                        if (iter != mtgdraftbots::constants::COLOR_TO_INDEX.end()) (*produces)[iter->second] = true;
                    }
                } else {
                    std::cout << name_lower << " does not produce any mana." << std::endl;
                }
            }
        }
        /* double elo = 1200; */
        /* if (card["elo"].get(elo)) elo = 1200; */
        std::size_t cmc = 0;
        if (card["cmc"].get(cmc)) cmc = 0;
        result.try_emplace(std::string{name_lower},
                           std::string{name_lower}, cost_symbols, mtgdraftbots::Embedding{0.f},
                           1200.f, static_cast<std::uint8_t>(cmc), produces);
    }
    std::cout << "Loaded all card details." << std::endl;
    return result;
};

std::vector<std::string> load_card_to_int(const std::string& card_to_int_filename, simdjson::ondemand::parser& parser) {
    simdjson::padded_string card_to_int_json = simdjson::padded_string::load(card_to_int_filename);
    std::vector<std::string> result;
    result.emplace_back("placeholder_for_ml_model");
    for (auto field : parser.iterate(card_to_int_json)) result.emplace_back(field.value()["name_lower"].get_string().value());
    std::cout << "Loaded int_to_card mapping." << std::endl;
    return result;
}

constexpr size_t MAX_IN_PACK = 24;
constexpr size_t MAX_SEEN = 400;
constexpr size_t MAX_PICKED = 48;
constexpr size_t NUM_LAND_COMBS = 8;

template<typename T, std::size_t N>
struct FixedVector : public std::array<T, N> {
    using iterator = typename std::array<T, N>::iterator;
    using const_iterator = typename std::array<T, N>::const_iterator;

    constexpr T& front() & noexcept { return (*this)[0]; };
    constexpr const T& front() const & noexcept { return (*this)[0]; };
    constexpr T&& front() && noexcept { return std::move((*this)[0]); };

    constexpr void push_back(const T& value) & noexcept(noexcept(std::declval<T&> = std::declval<const T&>())) {
        (*this)[current_size++] = value;
    }
    constexpr void push_back(T&& value) & noexcept(noexcept(std::declval<T&>() = std::declval<T&&>())) {
        (*this)[current_size++] = std::move(value);
    }

    constexpr void pop_back() & noexcept { current_size--; }

    constexpr iterator end() & noexcept {
        return std::begin(*this) + current_size;
    }
    constexpr const_iterator end() const & noexcept {
        return std::begin(*this) + current_size;
    }

    constexpr std::size_t size() const noexcept { return current_size; }

private:
    std::size_t current_size{0};
};

template<typename Rng>
mtgdraftbots::Lands get_random_lands(const mtgdraftbots::Lands& available_lands, Rng& rng) noexcept {
    std::uniform_int_distribution<std::size_t> land_dist(1, 31);
    mtgdraftbots::Lands result{0};
    for (std::size_t i=0; i < 17; i++) {
        std::size_t ind = land_dist(rng);
        while (available_lands[ind] <= result[ind]) ind = land_dist(rng);
        result[ind]++;
    }
    return result;
}

struct Pick {
    // We really only have the precision of a uint8_t so might as well use fixed point.
    using CardProbabilities = std::array<std::uint8_t, NUM_LAND_COMBS>;
    // We manipulate it so the first card is always the one chosen to simplify the model's loss calculation.
    static constexpr std::uint16_t chosen_card = 0;
    std::array<std::array<std::uint8_t, 2>, 4> coords{{{0u, 0u}}};
    std::array<float, 4> coord_weights{0.f};
    std::uint16_t num_in_pack{0};
    std::uint16_t num_picked{0};
    std::uint16_t num_seen{0};
    std::array<std::uint16_t, MAX_IN_PACK> in_pack{0};
    std::array<CardProbabilities, MAX_IN_PACK> in_pack_probs{{{0}}};
    std::array<std::uint16_t, MAX_PICKED> picked{0};
    std::array<CardProbabilities, MAX_PICKED> picked_probs{{{0}}};
    std::array<std::uint16_t, MAX_SEEN> seen{0};
    std::array<CardProbabilities, MAX_SEEN> seen_probs{{{0}}};

    constexpr mtgdraftbots::Lands get_available_lands(
            const std::vector<std::optional<mtgdraftbots::CardDetails>>& card_details) const noexcept {
        mtgdraftbots::Lands result{0, 17, 17, 17, 17, 17, 0};
        for (std::uint16_t i=0; i < num_picked; i++) {
            if (card_details[picked[i]]->produces) {
                result[mtgdraftbots::constants::get_color_combination_index(card_details[picked[i]]->produces.value())]++;
            }
        }
        return result;
    }

    template <typename Rng>
    void generate_probs(const std::vector<std::optional<mtgdraftbots::CardDetails>>& card_details,
                        const std::vector<std::optional<mtgdraftbots::internal::CardCost>>& card_costs,
                        Rng& rng) noexcept {
        using ScoreValue = std::tuple<float, std::array<float, MAX_IN_PACK>,
                                      std::array<float, MAX_PICKED>, std::array<float, MAX_SEEN>,
                                      mtgdraftbots::Lands>;
        const mtgdraftbots::Lands available_lands = get_available_lands(card_details);
        std::array<mtgdraftbots::internal::CardCost, MAX_IN_PACK> in_pack_costs{};
        std::array<mtgdraftbots::internal::CardCost, MAX_PICKED> picked_costs{};
        std::array<mtgdraftbots::internal::CardCost, MAX_SEEN> seen_costs{};
        for (std::uint16_t i=0; i < num_in_pack; i++) {
#ifndef NDEBUG
            if (!card_costs[in_pack[i]]) std::cerr << "Cost is null for card in pack." << std::endl;
#endif
            in_pack_costs[i] = *card_costs[in_pack[i]];
        }
        const auto in_pack_begin = std::begin(in_pack_costs);
        const auto in_pack_end = in_pack_begin + num_in_pack;
        for (std::uint16_t i=0; i < num_picked; i++) {
#ifndef NDEBUG
            if (!card_costs[picked[i]]) std::cerr << "Cost is null for picked card." << std::endl;
#endif
            picked_costs[i] = *card_costs[picked[i]];
        }
        const auto picked_begin = std::begin(picked_costs);
        const auto picked_end = picked_begin + num_picked;
        for (std::uint16_t i=0; i < num_seen; i++) {
#ifndef NDEBUG
            if (!card_costs[seen[i]]) std::cerr << "Cost is null for seen card." << std::endl;
#endif
            seen_costs[i] = *card_costs[seen[i]];
        }
        const auto seen_begin = std::begin(seen_costs);
        const auto seen_end = seen_begin + num_seen;
        for (std::size_t i=0; i < NUM_LAND_COMBS; i++) {
            ScoreValue prev_score{-1.f, {0.f}, {0.f}, {0.f}, get_random_lands(available_lands, rng)};
            ScoreValue current_score{0.f, {0.f}, {0.f}, {0.f}, std::get<mtgdraftbots::Lands>(prev_score)};
            while (std::get<float>(prev_score) < std::get<float>(current_score)) {
                prev_score = current_score;
                for (std::uint8_t increase=1; increase < 32; increase++) {
                    if (std::get<mtgdraftbots::Lands>(prev_score)[increase] >= available_lands[increase]) continue;
                    for (std::uint8_t decrease=1; decrease < 32; decrease++) {
                        if (decrease == increase || std::get<mtgdraftbots::Lands>(prev_score)[decrease] == 0) continue;
                        ScoreValue new_score = prev_score;
                        mtgdraftbots::Lands& new_lands = std::get<mtgdraftbots::Lands>(new_score);
                        new_lands[increase]++;
                        new_lands[decrease]--;
                        const auto transformation = [&new_lands](const mtgdraftbots::internal::CardCost& cost){ return cost.calculate_probability(new_lands); };
                        std::transform(in_pack_begin, in_pack_end, std::begin(std::get<1>(new_score)), transformation);
                        std::transform(picked_begin, picked_end, std::begin(std::get<2>(new_score)), transformation);
                        std::transform(seen_begin, seen_end, std::begin(std::get<3>(new_score)), transformation);
                        float max_in_pack_prob = std::reduce(std::execution::unseq, std::begin(std::get<1>(new_score)),
                                                             std::begin(std::get<1>(new_score)) + num_in_pack,
                                                             0.f, [](auto a, auto b){ return std::max(a, b); });
                        float sum_picked_probs = std::reduce(std::execution::unseq, std::begin(std::get<2>(new_score)),
                                                             std::begin(std::get<2>(new_score)) + num_picked);
                        float mean_seen_probs = std::reduce(std::execution::unseq, std::begin(std::get<3>(new_score)),
                                                            std::begin(std::get<3>(new_score)) + num_seen) / num_seen;
                        std::get<float>(new_score) = sum_picked_probs + 3 * mean_seen_probs + 5 * max_in_pack_prob;
                        if (std::get<float>(new_score) > std::get<float>(current_score)) current_score = new_score;
                    }
                }
            }
            for (std::size_t j=0; j < num_in_pack; j++) in_pack_probs[j][i] = std::get<1>(current_score)[j] * std::numeric_limits<typename CardProbabilities::value_type>::max();
            for (std::size_t j=0; j < num_picked; j++) picked_probs[j][i] = std::get<2>(current_score)[j] * std::numeric_limits<typename CardProbabilities::value_type>::max();
            for (std::size_t j=0; j < num_seen; j++) seen_probs[j][i] = std::get<3>(current_score)[j] * std::numeric_limits<typename CardProbabilities::value_type>::max();
        }
    }
};

struct random_seed_seq {
    using result_type = std::random_device::result_type;

    template<typename It>
    void generate(It begin, It end) {
        for (; begin != end; ++begin) {
            *begin = device();
        }
    }

    static random_seed_seq& get_instance() {
        static thread_local random_seed_seq result;
        return result;
    }

private:
    std::random_device device;
};

template<std::size_t N>
bool load_array_indices(std::array<std::uint16_t, N>& indices, std::uint16_t& num_indices,
                        simdjson::ondemand::array json_indices,
                        const std::vector<std::optional<mtgdraftbots::CardDetails>>& card_details) {
    num_indices = 0;
    std::uint64_t card_index64{0};
    for (auto json_index : json_indices) {
        if (num_indices >= N || json_index.get(card_index64)) {
#ifndef NDEBUG
            std::cerr << "Array (" << N << ") got too large(" << num_indices << ") or had an invalid index." << std::endl;
#endif
            return false;
        }
        const std::uint16_t card_index = static_cast<std::uint16_t>(card_index64 + 1);
        if (card_index >= card_details.size() || !card_details[card_index]) {
#ifndef NDEBUG
            std::cerr << "There weren't card details for card_index " << card_index << '.' << std::endl;
#endif
            return false;
        }
        indices[num_indices++] = card_index;
    }
    return true;
}

template<typename Integral, typename JsonType>
bool load_int(Integral& dest, JsonType json_value) {
    std::uint64_t result{0};
    if (json_value.get(result)) {
#ifndef NDEBUG
        std::cerr << "Invalid value was read for integer type." << std::endl;
#endif
        return false;
    }
    dest = static_cast<Integral>(result);
    return true;
}

template<typename JsonType>
bool load_chosen_card(std::array<std::uint16_t, MAX_IN_PACK>& in_pack, std::uint16_t& num_in_pack,
                      JsonType json_chosen_card) {
    // Remove duplicates from the pack since they aren't helpful for training.
    std::sort(std::begin(in_pack), std::begin(in_pack) + num_in_pack);
    auto iter = std::unique(std::begin(in_pack), std::begin(in_pack) + num_in_pack);
    const std::size_t new_num_in_pack = std::distance(std::begin(in_pack), iter);
    std::fill(iter, std::begin(in_pack) + num_in_pack, 0);
    num_in_pack = new_num_in_pack;

    std::uint16_t chosen_card_index{0};
    if (num_in_pack <= 1 || !load_int(chosen_card_index, json_chosen_card)) {
#ifndef NDEBUG
        std::cerr << "Number of unique cards in pack was too low or failed to load the chosen_card value." << std::endl;
#endif
        return false;
    }
    chosen_card_index++;
    for (std::uint8_t i=0; i < num_in_pack; i++) {
        if (chosen_card_index == in_pack[i]) {
            // We swap it to the front to simplify the model's loss computation.
            if (i != 0) std::swap(in_pack[0], in_pack[i]);
            return true;
        }
    }
#ifndef NDEBUG
    std::cerr << "Chosen card " << chosen_card_index << " did not appear in the pack." << std::endl;
#endif
    return false;
}

constexpr void calculate_coord_info(std::array<std::array<std::uint8_t, 2>, 4>& coords,
                                    std::array<float, 4>& coord_weights,
                                    std::uint8_t pack_num, std::uint8_t num_packs,
                                    std::uint8_t pick_num, std::uint8_t num_picks) noexcept {
    const float pack_float = (static_cast<float>(mtgdraftbots::WEIGHT_X_DIM) * pack_num) / num_packs;
    const std::uint8_t pack_0 = static_cast<std::size_t>(pack_float);
    const std::uint8_t pack_1 = std::min(static_cast<std::uint8_t>(mtgdraftbots::WEIGHT_X_DIM - 1), static_cast<std::uint8_t>(pack_0 + 1));
    const float pack_frac = pack_float - pack_0;
    const float pick_float = (static_cast<float>(mtgdraftbots::WEIGHT_Y_DIM) * pick_num) / num_picks;
    const std::uint8_t pick_0 = static_cast<std::size_t>(pick_float);
    const std::uint8_t pick_1 = std::min(static_cast<std::uint8_t>(mtgdraftbots::WEIGHT_Y_DIM - 1), static_cast<std::uint8_t>(pick_0 + 1));
    const float pick_frac = pick_float - pick_0;
    coords = {{{pack_0, pick_0}, {pack_0, pick_1}, {pack_1, pick_0}, {pack_1, pick_1}}};
    coord_weights = {(1 - pack_frac) * (1 - pick_frac), (1 - pack_frac) * pick_frac,
                     pack_frac * (1 - pick_frac), pack_frac * pick_frac};
}

template<typename JsonType>
bool load_coord_info(std::array<std::array<std::uint8_t, 2>, 4>& coords,
                     std::array<float, 4>& coord_weights, JsonType pack_num_json,
                     JsonType num_packs_json, JsonType pick_num_json, JsonType num_picks_json) {
    std::uint8_t pack_num;
    std::uint8_t num_packs;
    std::uint8_t pick_num;
    std::uint8_t num_picks;
    if (!load_int(pack_num, pack_num_json) || !load_int(num_packs, num_packs_json)
        || !load_int(pick_num, pick_num_json) || !load_int(num_picks, num_picks_json)
        || pack_num >= num_packs || pick_num >= num_picks) {
#ifndef NDEBUG
        std::cerr << "Loaded coordinates were invalid." << std::endl;
#endif
        return false;
    }
    calculate_coord_info(coords, coord_weights, pack_num, num_packs, pick_num, num_picks);
    return true;
}

void process_files_worker(const std::vector<std::optional<mtgdraftbots::CardDetails>>& card_details,
                          const std::vector<std::optional<mtgdraftbots::internal::CardCost>>& card_costs,
                          const std::set<std::string, std::less<>>& valid_deckids,
                          moodycamel::ConcurrentQueue<std::string>& files_to_process,
                          moodycamel::BlockingConcurrentQueue<Pick>& processed_picks,
                          const moodycamel::ProducerToken& files_to_process_producer) {
    moodycamel::ProducerToken processed_picks_producer(processed_picks);
    std::mt19937_64 rng(random_seed_seq::get_instance());
    std::string current_filename;
    simdjson::ondemand::parser parser;
    static std::atomic<std::size_t> num_finished_files = 0;
    static std::atomic<std::size_t> num_valid = 0;
    static std::atomic<std::size_t> num_picks = 0;
    static std::atomic<std::size_t> num_drafts = 0;
    while (files_to_process.try_dequeue_from_producer(files_to_process_producer, current_filename)){
        simdjson::padded_string drafts_file_json = simdjson::padded_string::load(current_filename);
        std::size_t num_picks_in_file = 0;
        std::size_t num_drafts_in_file = 0;
        std::size_t num_valid_in_file = 0;
        for (simdjson::ondemand::object draft_json : parser.iterate(drafts_file_json)) {
            std::string_view deckid;
            if (draft_json["deckid"].get(deckid)) {
#ifndef NDEBUG
                std::cerr << "Draft did not have a deckid." << std::endl;
#endif
                continue;
            }
            if (valid_deckids.find(deckid) == valid_deckids.end()) {
#ifndef NDEBUG
                std::cerr << "Draft's deckid was not in the set of valid deckids." << std::endl;
#endif
                continue;
            }
            num_drafts_in_file++;
            for (simdjson::ondemand::object pick_json : draft_json["picks"]) {
                num_picks_in_file++;
                Pick current_pick;
                if (!load_array_indices(current_pick.seen, current_pick.num_seen, pick_json["seen"], card_details)
                    || !load_array_indices(current_pick.picked, current_pick.num_picked, pick_json["picked"], card_details)
                    || !load_array_indices(current_pick.in_pack, current_pick.num_in_pack, pick_json["cardsInPack"], card_details)
                    || !load_chosen_card(current_pick.in_pack, current_pick.num_in_pack, pick_json["chosenCard"])
                    || !load_coord_info(current_pick.coords, current_pick.coord_weights,
                                        pick_json["pack"], pick_json["packs"], pick_json["pick"],
                                        pick_json["packSize"])) continue;
                current_pick.generate_probs(card_details, card_costs, rng);
                num_valid_in_file++;
                processed_picks.enqueue(processed_picks_producer, current_pick);
            }
        }
        const std::size_t new_num_valid = (num_valid += num_valid_in_file);
        const std::size_t new_num_picks = (num_picks += num_picks_in_file);
        const std::size_t new_num_drafts = (num_drafts += num_drafts_in_file);
        fmt::print(
            FMT_STRING("Finished file: {: >3d} which had {: >10L} out of {: >10L}({: >5.2L}%) usable picks from {: >7L} drafts.\n\tThis puts it to {: >10L} out of {: >10L}({: >5.2L}%) usable picks from {: >7L} drafts.\n"),
            (++num_finished_files), num_valid_in_file, num_picks_in_file, (100.f * num_valid_in_file) / num_picks_in_file,
            num_drafts_in_file, new_num_valid, new_num_picks, (100.f * new_num_valid) / new_num_picks, new_num_drafts
        );
    }
}

constexpr std::int64_t TIMEOUT_USECS = 500'000; // 500 milliseconds
constexpr std::size_t MIN_FILE_SIZE = 128 * 1024 * 1024; // 128 MB

template<typename T> requires std::is_trivially_copyable_v<T>
std::size_t write_value(std::ofstream& file_stream, const T& value) {
    std::size_t size = sizeof(T);
    if (size == 0) return 0;
    file_stream.write(reinterpret_cast<const char*>(&value), size);
    return size;
}

template<typename T> requires std::ranges::contiguous_range<T> and std::is_trivially_copyable_v<typename T::value_type>
std::size_t write_range(std::ofstream& file_stream, const T& value, std::uint16_t num_values) {
    std::size_t size = sizeof(typename T::value_type) * num_values;
    if (size == 0) return 0;
    file_stream.write(reinterpret_cast<const char*>(value.data()), size);
    return size;
}

void save_picks(std::stop_token stop_tkn,
                moodycamel::BlockingConcurrentQueue<Pick>& processed_picks,
                std::atomic<std::size_t>& file_count,
                std::string_view destination_format_string) {
    moodycamel::ConsumerToken processed_picks_consumer(processed_picks);
    Pick current_pick;
    std::ofstream current_file(fmt::format(destination_format_string, ++file_count), std::ios::binary);
    std::size_t current_file_size{0};
    while(!stop_tkn.stop_requested()) {
        if (processed_picks.wait_dequeue_timed(processed_picks_consumer, current_pick, TIMEOUT_USECS)) {
            if (current_file_size > MIN_FILE_SIZE) {
                current_file.flush();
                current_file = std::ofstream(fmt::format(destination_format_string, ++file_count));
                current_file_size = 0;
            }
            std::size_t pick_record_size = 0;
            pick_record_size += write_value(current_file, current_pick.coords);
            pick_record_size += write_value(current_file, current_pick.coord_weights);
            pick_record_size += write_value(current_file, current_pick.num_in_pack);
            pick_record_size += write_value(current_file, current_pick.num_picked);
            pick_record_size += write_value(current_file, current_pick.num_seen);
            pick_record_size += write_range(current_file, current_pick.in_pack, current_pick.num_in_pack);
            pick_record_size += write_range(current_file, current_pick.in_pack_probs, current_pick.num_in_pack);
            pick_record_size += write_range(current_file, current_pick.picked, current_pick.num_picked);
            pick_record_size += write_range(current_file, current_pick.picked_probs, current_pick.num_picked);
            pick_record_size += write_range(current_file, current_pick.seen, current_pick.num_seen);
            pick_record_size += write_range(current_file, current_pick.seen_probs, current_pick.num_seen);
            current_file_size += pick_record_size;
        }
    }
}

std::set<std::string, std::less<>> filter_invalid_deckids(simdjson::ondemand::parser& parser,
        std::size_t num_files, moodycamel::BlockingConcurrentQueue<simdjson::padded_string>& file_contents) {
    fmt::print("Started collecting valid deckids.\n");
    std::map<std::string, std::pair<std::string, std::string>> earliest_by_draftid;
    moodycamel::ConsumerToken file_contents_consumer(file_contents);
    simdjson::padded_string drafts_file_json;
    std::size_t seen_drafts = 0;
    for (std::size_t i=0; i < num_files; i++) {
        file_contents.wait_dequeue(file_contents_consumer, drafts_file_json);
        for (simdjson::ondemand::object draft_json : parser.iterate(drafts_file_json)) {
            seen_drafts++;
            std::string_view date;
            std::string_view deckid;
            std::string_view draftid;
            if (!draft_json["date"].get(date) && !draft_json["deckid"].get(deckid) && !draft_json["draftid"].get(draftid)) {
                auto [iter, inserted] = earliest_by_draftid.try_emplace(std::string{draftid}, std::piecewise_construct,
                                                                        std::forward_as_tuple(date), std::forward_as_tuple(deckid));
                if (!inserted) {
                    auto comparison = iter->second.first <=> date;
                    if (std::is_gt(comparison)) iter->second = {std::piecewise_construct, std::forward_as_tuple(date), std::forward_as_tuple(deckid)};
                    else if (std::is_eq(comparison) && iter->second.second == deckid) {
                        std::cerr << "Multiple decks compared equal with deckid " << deckid << std::endl;
                    }
                }
            } else {
                std::cerr << "Draft did not have one of date, deckid, or draftid." << std::endl;
            }
        }
    }
    auto transformed = earliest_by_draftid | std::views::transform([](const auto& kv_pair) { return kv_pair.second.second; });
    fmt::print(FMT_STRING("Got the set of valid deckids with {:L} valid drafts out of {:L} seen.\n"),
               earliest_by_draftid.size(), seen_drafts);
    return {std::begin(transformed), std::end(transformed)};
}

constexpr std::size_t NUM_FILE_WRITERS = 16;

int main() {
    std::locale::global(std::locale("en_US.UTF-8"));
    simdjson::ondemand::parser parser;
    const std::map<std::string, mtgdraftbots::CardDetails> card_details_by_name = load_card_details("data/maps/carddb.json", parser);
    const std::vector<std::string> card_to_int = load_card_to_int("data/maps/int_to_card.json", parser);
    const std::vector<std::optional<mtgdraftbots::CardDetails>> card_details =
        ([&]() -> std::vector<std::optional<mtgdraftbots::CardDetails>> {
            auto transformed = card_to_int
                | std::views::transform([&](const std::string& name) -> std::optional<mtgdraftbots::CardDetails> {
                    auto iter = card_details_by_name.find(name);
                    if (iter == card_details_by_name.end()) return std::nullopt;
                    else return iter->second;
                });
            return {std::begin(transformed), std::end(transformed)};
        })();
    const std::vector<std::optional<mtgdraftbots::internal::CardCost>> card_costs =
        ([&]() -> std::vector<std::optional<mtgdraftbots::internal::CardCost>> {
            auto transformed = card_details
                | std::views::transform([](const auto& cd) -> std::optional<mtgdraftbots::internal::CardCost> {
                    if (cd) return mtgdraftbots::internal::CardCost{*cd};
                    else return std::nullopt;
                });
            return {std::begin(transformed), std::end(transformed)};
        })();
    std::cout << "Created all the card costs." << std::endl;
    std::vector<std::string> draft_filenames;
    for (const auto& path_data : std::filesystem::directory_iterator("data/drafts/")) {
        draft_filenames.push_back(path_data.path());
    }
    moodycamel::BlockingConcurrentQueue<simdjson::padded_string> file_contents;
    moodycamel::ProducerToken file_contents_producer(file_contents);
    std::jthread load_files([&](){
        for (const std::string& filename : draft_filenames) file_contents.enqueue(file_contents_producer, simdjson::padded_string::load(filename));
    });
    std::set<std::string, std::less<>> valid_deckids = filter_invalid_deckids(parser, draft_filenames.size(), file_contents);
    std::mt19937_64 rng(random_seed_seq::get_instance());
    std::shuffle(draft_filenames.begin(), draft_filenames.end(), rng);
    moodycamel::ConcurrentQueue<std::string> files_to_process(draft_filenames.size());
    moodycamel::ProducerToken files_to_process_producer(files_to_process);
    moodycamel::BlockingConcurrentQueue<Pick> processed_picks;
    moodycamel::ConsumerToken processed_picks_consumer(processed_picks);
    files_to_process.enqueue_bulk(files_to_process_producer, std::make_move_iterator(draft_filenames.begin()),
                                  draft_filenames.size());
    std::vector<std::jthread> file_workers;
    file_workers.reserve(std::jthread::hardware_concurrency());
    for (size_t i=0; i < std::jthread::hardware_concurrency(); i++) {
        file_workers.emplace_back([&]() {
            process_files_worker(card_details, card_costs, valid_deckids, files_to_process, processed_picks,
                                 files_to_process_producer);
        });
    }
    std::atomic<std::size_t> file_count{0};
    std::vector<std::jthread> save_workers;
    save_workers.reserve(NUM_FILE_WRITERS);
    for (std::size_t i=0; i < NUM_FILE_WRITERS; i++) {
        save_workers.emplace_back([&](std::stop_token stop_tkn) {
            save_picks(stop_tkn, processed_picks, file_count, "data/parsed_picks/full_uncompressed/{:0>4d}.bin");
        });
    }
    file_workers.clear();
    for (auto& worker : save_workers) worker.request_stop();
    return 0;
}
