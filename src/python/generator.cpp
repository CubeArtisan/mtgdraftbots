#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <moodycamel/blockingconcurrentqueue.h>
#include <pcg_random.hpp>
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

using namespace py = pybind11;

constexpr std::size_t MAX_IN_PACK = 24;
constexpr std::size_t MAX_SEEN = 400;
constexpr std::size_t MAX_PICKED = 48;
constexpr std::size_t NUM_LAND_COMBS = 8;

struct PyPick {
    // We manipulate it so the first card is always the one chosen to simplify the model's loss calculation.
    static constexpr std::int32_t chosen_card = 0;

    std::array<std::int32_t, MAX_IN_PACK> in_pack{0};
    std::array<std::int32_t, MAX_SEEN> seen{0};
    float num_seen{0.f};
    std::array<std::int32_t, MAX_PICKED> picked{0};
    float num_picked{0.f};
    std::array<std::array<std::int32_t, 2>, 4> coords{{{0, 0}}};
    std::array<float, 4> coord_weights{0.f};
    std::array<std::array<float, MAX_SEEN>, NUM_LAND_COMBS> seen_probs{{{0.f}}};
    std::array<std::array<float, MAX_PICKED>, NUM_LAND_COMBS> picked_probs{{{0.f}}};
    std::array<std::array<float, MAX_IN_PACK>, NUM_LAND_COMBS> in_pack_probs{{{0.f}}};
};

template <std::size_t picks_per_batch>
struct PyPickBatch {
    using python_type = std::tuple<py::array_t<std::int32_t>, py::array_t<std::int32_t>,
                                   py::array_t<float>, py::array_t<std::int32_t>, py::array_t<float>,
                                   py::array_t<std::int32_t>, py::array_t<float>, py::array_t<float>,
                                   py::array_t<float>, py::array_t<float>>;

    // We manipulate it so the first card is always the one chosen to simplify the model's loss calculation.
    static constexpr std::array<std::int32_t, picks_per_batch> chosen_card{0};

    std::array<std::array<std::int32_t, MAX_IN_PACK>, picks_per_batch> in_pack{0};
    std::array<std::array<std::int32_t, MAX_SEEN>, picks_per_batch> seen{0};
    std::array<float, picks_per_batch> num_seen{0.f};
    std::array<std::array<std::int32_t, MAX_PICKED>, picks_per_batch> picked{0};
    std::array<float, picks_per_batch> num_picked{0.f};
    std::array<std::array<std::array<std::int32_t, 2>, 4>, picks_per_batch> coords{{{0, 0}}};
    std::array<std::array<float, 4>, picks_per_batch> coord_weights{0.f};
    std::array<std::array<std::array<float, MAX_SEEN>, NUM_LAND_COMBS>, picks_per_batch> seen_probs{{{0.f}}};
    std::array<std::array<std::array<float, MAX_PICKED>, NUM_LAND_COMBS>, picks_per_batch> picked_probs{{{0.f}}};
    std::array<std::array<std::array<float, MAX_IN_PACK>, NUM_LAND_COMBS>, picks_per_batch> in_pack_probs{{{0.f}}};

    static constexpr std::array<std::size_t, 2> in_pack_shape{picks_per_batch, MAX_IN_PACK};
    static constexpr std::array<std::size_t, 2> seen_shape{picks_per_batch, MAX_SEEN};
    static constexpr std::array<std::size_t, 1> num_seen_shape{picks_per_batch};
    static constexpr std::array<std::size_t, 2> picked_shape{picks_per_batch, MAX_PICKED};
    static constexpr std::array<std::size_t, 1> num_picked_shape{picks_per_batch};
    static constexpr std::array<std::size_t, 3> coords_shape{picks_per_batch, 4, 2};
    static constexpr std::array<std::size_t, 2> coord_weights_shape{picks_per_batch, 4};
    static constexpr std::array<std::size_t, 3> seen_probs_shape{picks_per_batch, NUM_LAND_COMBS, MAX_SEEN};
    static constexpr std::array<std::size_t, 3> picked_probs_shape{picks_per_batch, NUM_LAND_COMBS, MAX_PICKED};
    static constexpr std::array<std::size_t, 3> in_pack_probs_shape{picks_per_batch, NUM_LAND_COMBS, MAX_IN_PACK};

    constexpr std::size_t size() const noexcept { return picks_per_batch; }

    explicit operator python_type() const noexcept {
        return {
            py::array_t<std::int32_t>(in_pack_shape, in_pack.data()),
            py::array_t<std::int32_t>(seen_shape, seen.data()),
            py::array_t<float>(num_seen_shape, num_seen.data()),
            py::array_t<std::int32_t>(picked_shape, picked.data()),
            py::array_t<float>(num_picked_shape, num_picked.data()),
            py::array_t<std::int32_t>(coords_shape, coords.data()),
            py::array_t<float>(coord_weights_shape, coord_weights.data()),
            py::array_t<float>(seen_probs_shape, seen_probs.data()),
            py::array_t<float>(picked_probs_shape, picked_probs.data()),
            py::array_t<float>(in_pack_probs_shape, in_pack_probs.data())
        };
    }
};

template <std::size_t picks_per_batch>
struct DraftPickGenerator {
    using result_type = typename PyPickBatch<picks_per_batch>::python_type;
    static constexpr std::size_t batch_size = picks_per_batch;

    DraftPickGenerator(std::size_t num_readers, std::size_t num_shufflers, std::size_t num_batchers,
                       std::size_t shuffle_buffer_length, std::size_t seed, std::string folder_path)
            : num_reader_threads{num_readers}, num_shuffler_threads{num_shufflers},
              num_batch_threads{num_batchers}, shuffle_buffer_size{shuffle_buffer_length},
              initial_seed{seed}, files_to_read_producer{files_to_read},
              main_rng{initial_seed, num_shufflers} {
        for (const auto& path_data : std::filesystem::directory_iterator(folder_path)) {
            draft_filenames.push_back(path_data.path());
        }
        length = 0;
    }

    DraftPickGenerator& enter() {
        py::gil_scoped_release release;
        if (exit_threads) {
            exit_threads = false;
            std::size_t thread_number = 0;
            for (std::size_t i=0; i < num_reader_threads; i++) {
                reader_threads.emplace_back([this](){ this->read_worker(); });
            }
            for (std::size_t i=0; i < num_shuffler_threads; i++) {
                shuffler_threads.emplace_back([this, j=thread_number++](){ this->shuffle_worker(pcg32(this->initial_seed, j)); });
            }
            for (std::size_t i=0; i < num_batch_threads; i++) {
                batch_threads.emplace_back([this](){ this->batch_worker(); });
            }
        }
        queue_new_epoch();
        return *this;
    }

    bool exit(py::object, py::object, py::object) {
        exit_threads = true;
        for (auto& worker : reader_threads) worker.join();
        for (auto& worker : shuffler_threads) worker.join();
        for (auto& worker : batch_threads) worker.join();
    }

    std::size_t size() const noexcept { return length; }

    DraftPickGenerator& queue_new_epoch() {
        std::shuffle(std::begin(draft_filenames), std::end(draft_filenames), main_rng);
        files_to_read.enqueue_bulk(files_to_read_producer, std::begin(draft_filenames), draft_filenames.size());
        return *this;
    }

    result_type next() {
        std::unique_ptr<PyPickBatch<picks_per_batch>> batched;
        loaded_batches.wait_dequeue(loaded_batches_consumer, batched);
        return static_cast<result_type>(*batched);
    }

    result_type getitem(std::size_t) { return next(); }

private:
    void read_worker() {
        moodycamel::ConsumerToken files_to_read_consumer(files_to_read);
        moodycamel::ProducerToken loaded_picks_producer(loaded_picks);
        std::string cur_filename;
        std::vector<char> loaded_file_buffer;
        while (!exit_threads) {
            if (files_to_read.wait_dequeue_timed(files_to_read_consumer, cur_filename, 100'000)) {
                std::ifstream picks_file(cur_filename, std::ios::binary | std::ios::ate);
                auto file_size = picks_file.tellg();
                loaded_file_buffer.clear();
                loaded_file_buffer.resize(file_size);
                picks_file.seekg(0);
                picks_file.read(loaded_file_buffer.data(), file_size);
                char* current_pos = loaded_file_buffer.data();
                char* end_pos = loaded_file_buffer.data() + loaded_file_buffer.size();
                while (current_pos < end_pos) {
                    if (exit_threads) return;
                    PyPick loaded_data;
                    for (std::size_t i=0; i < 4; i++) {
                        for (std::size_t j=0; j < 2; j++) {
                            loaded_data.coords[i][j] = *reinterpret_cast<std::uint8_t*>(current_pos++);
                        }
                    }
                    for (std::size_t i=0; i < 4; i++) {
                        loaded_data.coord_weights[i] = *reinterpret_cast<std::uint8_t*>(current_pos++);
                    }
                    std::uint16_t num_in_pack = *reinterpret_cast<std::uint16_t*>(current_pos);
                    current_pos += 2;
                    loaded_data.num_picked = *reinterpret_cast<std::uint16_t*>(current_pos);
                    current_pos += 2;
                    loaded_data.num_seen = *reinterpret_cast<std::uint16_t*>(current_pos);
                    current_pos += 2;
                    for (std::size_t i=0; i < num_in_pack; i++) {
                        loaded_data.in_pack[i] = *reinterpret_cast<std::uint16_t*>(current_pos);
                        current_pos += 2;
                    }
                    for (std::size_t i=0; i < num_in_pack; i++) {
                        for (std::size_t j=0; j < NUM_LAND_COMBS; j++) {
                            loaded_data.in_pack_probs[j][i] = (*reinterpret_cast<std::uint8_t*>(current_pos++))
                                / static_cast<float>(std::numeric_limits<std::uint8_t>::max());
                        }
                    }
                    for (std::size_t i=0; i < loaded_data.num_picked; i++) {
                        loaded_data.picked[i] = *reinterpret_cast<std::uint16_t*>(current_pos);
                        current_pos += 2;
                    }
                    for (std::size_t i=0; i < loaded_data.num_picked; i++) {
                        for (std::size_t j=0; j < NUM_LAND_COMBS; j++) {
                            loaded_data.picked_probs[j][i] = (*reinterpret_cast<std::uint8_t*>(current_pos++))
                                / static_cast<float>(std::numeric_limits<std::uint8_t>::max());
                        }
                    }
                    for (std::size_t i=0; i < loaded_data.num_seen; i++) {
                        loaded_data.seen[i] = *reinterpret_cast<std::uint16_t*>(current_pos);
                        current_pos += 2;
                    }
                    for (std::size_t i=0; i < loaded_data.num_seen; i++) {
                        for (std::size_t j=0; j < NUM_LAND_COMBS; j++) {
                            loaded_data.seen_probs[j][i] = (*reinterpret_cast<std::uint8_t*>(current_pos++))
                                / static_cast<float>(std::numeric_limits<std::uint8_t>::max());
                        }
                    }
                    loaded_picks.enqueue(loaded_picks_producer, loaded_data);
                }
            }
        }
    }

    void shuffle_worker(pcg32 rng) {
        moodycamel::ConsumerToken loaded_picks_consumer(loaded_picks);
        moodycamel::ProducerToken shuffled_picks_producer(shuffled_picks);
        std::vector<PyPick> shuffle_buffer;
        shuffle_buffer.reserve(shuffle_buffer_size);
        std::uniform_int_distribution<std::size_t> index_selector(0, shuffle_buffer_size - 1);
        while (shuffle_buffer.size() < shuffle_buffer_size && !exit_threads) {
            PyPick loaded_pick;
            if (loaded_picks.wait_dequeue_timed(loaded_picks_consumer, loaded_pick, 100'000)) {
                shuffle_buffer.push_back(loaded_pick);
            }
        }
        while (!exit_threads) {
            std::size_t index = index_selector(rng);
            shuffled_picks.enqueue(shuffled_picks_producer, shuffle_buffer[index]);
            while (!loaded_picks.wait_dequeue_timed(loaded_picks_consumer, shuffle_buffer[index], 100'000)
                   && !exit_threads) { }
        }
    }

    void batch_worker() {
        moodycamel::ConsumerToken shuffled_picks_consumer(shuffled_picks);
        moodycamel::ProducerToken loaded_batches_producer(loaded_batches);
        PyPick loaded;
        while (!exit_threads) {
            auto batch = std::make_unique<PyPickBatch<picks_per_batch>>();
            for (std::size_t i=0; i < picks_per_batch; i++) {
                while (!shuffled_picks.wait_dequeue_timed(shuffled_picks_consumer, loaded, 100'000)
                       && !exit_threads) { }
                if (exit_threads) return;
                batch->in_pack[i] = loaded.in_pack;
                batch->seen[i] = loaded.seen;
                batch->num_seen[i] = loaded.num_seen;
                batch->picked[i] = loaded.picked;
                batch->num_picked[i] = loaded.num_picked;
                batch->coords[i] = loaded.coords;
                batch->coord_weights[i] = loaded.coord_weights;
                batch->seen_probs[i] = loaded.seen_probs;
                batch->picked_probs[i] = loaded.picked_probs;
                batch->in_pack_probs[i] = loaded.in_pack_probs;
                loaded_batches.enqueue(loaded_batches_producer, std::move(batch));
            }
        }
    }

    std::size_t initial_seed;
    std::size_t num_reader_threads;
    std::size_t num_shuffler_threads;
    std::size_t num_batch_threads;
    std::size_t shuffle_buffer_size;

    std::vector<std::string> draft_filenames;
    std::size_t length;

    moodycamel::BlockingConcurrentQueue<std::string> files_to_read;
    moodycamel::BlockingConcurrentQueue<PyPick> loaded_picks;
    moodycamel::BlockingConcurrentQueue<PyPick> shuffled_picks;
    moodycamel::BlockingConcurrentQueue<std::unique_ptr<PyPickBatch<picks_per_batch>>> loaded_batches;
    moodycamel::ProducerToken files_to_read_producer;
    moodycamel::ConsumerToken loaded_batches_consumer;

    std::atomic<bool> exit_threads{true};
    std::vector<std::thread> reader_threads;
    std::vector<std::thread> shuffler_threads;
    std::vector<std::thread> batch_threads;

    pcg32 main_rng;
};

PYBIND11_MODULE(generator, m) {
    using namespace pybind11::literals;
    using DraftPickGenerator512 = DraftPickGenerator<512, 65536>;
    py::class_<DraftPickGenerator512>(m, "DraftPickGenerator512")
        .def(py::init<std::size_t, std::size_t, std::size_t, std::size_t, std::string>())
        .def("__enter__", &DraftPickGenerator512::enter)
        .def("__exit__", &DraftPickGenerator512::exit)
        .def("__len__", &DraftPickGenerator512::size)
        .def("__getitem__", &DraftPickGenerator512::getitem)
        .def("__next__", &DraftPickGenerator512::next)
        .def("__iter__", &DraftPickGenerator512::queue_new_epoch)
        .def("on_epoch_end", &DraftPickGenerator512::queue_new_epoch)
}
