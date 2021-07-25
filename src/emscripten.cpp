#include <string>
#include <vector>

#include <emscripten/wget.h>
#include <emscripten/bind.h>

#include "mtgdraftbots/mtgdraftbots.hpp"

using namespace emscripten;
using namespace mtgdraftbots;

// Bindings for std::vector
namespace emscripten {
	namespace internal {

		template <typename T, typename Allocator>
		struct BindingType<std::vector<T, Allocator>> {
			using ValBinding = BindingType<val>;
			using WireType = ValBinding::WireType;

			static WireType toWireType(const std::vector<T, Allocator>& vec) {
				return ValBinding::toWireType(val::array(vec));
			}

			static std::vector<T, Allocator> fromWireType(WireType value) {
				return vecFromJSArray<T>(ValBinding::fromWireType(value));
			}
		};

		template <typename T>
		struct TypeID<T,
			typename std::enable_if_t<std::is_same<
			typename Canonicalized<T>::type,
			std::vector<typename Canonicalized<T>::type::value_type,
			typename Canonicalized<T>::type::allocator_type>>::value>> {
			static constexpr TYPEID get() { return TypeID<val>::get(); }
		};

	}  // namespace internal
}  // namespace emscripten

void pass_data_to_initialize(void*, void* data, int len) {
	std::vector<char> file_buffer(len, '\0');
	std::memcpy(file_buffer.data(), data, file_buffer.size());
	initialize_draftbots(file_buffer);
};

void initialize_error(void*) {
	std::cerr << "Error initializing" << std::endl;
}

std::vector<std::string> initialize_with_data(std::string data, int len) {
	std::vector<char> file_buffer(len);
	std::memcpy(file_buffer.data(), data.data(), file_buffer.size());
	initialize_draftbots(file_buffer);
	std::vector<std::string> oracle_ids;
	oracle_ids.reserve(details::card_lookups.size());
	for (const auto pair : details::card_lookups) {
		oracle_ids.push_back(pair.first);
	}
	return oracle_ids;
};

EMSCRIPTEN_BINDINGS(mtgdraftbots) {
	// There's sadly no default way to do this.
	value_array<Lands>("Lands")
		.element(emscripten::index<0>())
		.element(emscripten::index<1>())
		.element(emscripten::index<2>())
		.element(emscripten::index<3>())
		.element(emscripten::index<4>())
		.element(emscripten::index<5>())
		.element(emscripten::index<6>())
		.element(emscripten::index<7>())
		.element(emscripten::index<8>())
		.element(emscripten::index<9>())
		.element(emscripten::index<10>())
		.element(emscripten::index<11>())
		.element(emscripten::index<12>())
		.element(emscripten::index<13>())
		.element(emscripten::index<14>())
		.element(emscripten::index<15>())
		.element(emscripten::index<16>())
		.element(emscripten::index<17>())
		.element(emscripten::index<18>())
		.element(emscripten::index<19>())
		.element(emscripten::index<20>())
		.element(emscripten::index<21>())
		.element(emscripten::index<22>())
		.element(emscripten::index<23>())
		.element(emscripten::index<24>())
		.element(emscripten::index<25>())
		.element(emscripten::index<26>())
		.element(emscripten::index<27>())
		.element(emscripten::index<28>())
		.element(emscripten::index<29>())
		.element(emscripten::index<30>())
		.element(emscripten::index<31>());

	value_object<DrafterState>("DrafterState")
		.field("picked", &DrafterState::picked)
		.field("seen", &DrafterState::seen)
		.field("cardsInPack", &DrafterState::cards_in_pack)
		.field("basics", &DrafterState::basics)
		.field("cardOracleIds", &DrafterState::card_oracle_ids)
		.field("packNum", &DrafterState::pack_num)
		.field("numPacks", &DrafterState::num_packs)
		.field("pickNum", &DrafterState::pick_num)
		.field("numPicks", &DrafterState::num_picks)
		.field("seed", &DrafterState::seed);
	value_object<OracleResult>("OracleResult")
		.field("title", &OracleResult::title)
		.field("tooltip", &OracleResult::tooltip)
		.field("weight", &OracleResult::weight)
		.field("value", &OracleResult::value)
		.field("per_card", &OracleResult::per_card);
	value_object<BotScore>("BotScore")
		.field("score", &BotScore::score)
		.field("oracleResults", &BotScore::oracle_results)
		.field("lands", &BotScore::lands);
	value_object<BotResult>("BotState")
		.field("picked", &BotResult::picked)
		.field("seen", &BotResult::seen)
		.field("cardsInPack", &BotResult::cards_in_pack)
		.field("basics", &BotResult::basics)
		.field("cardOracleIds", &BotResult::card_oracle_ids)
		.field("packNum", &BotResult::pack_num)
		.field("numPacks", &BotResult::num_packs)
		.field("pickNum", &BotResult::pick_num)
		.field("numPicks", &BotResult::num_picks)
		.field("seed", &BotResult::seed)
		.field("options", &BotResult::options)
		.field("chosenOption", &BotResult::chosen_option)
		.field("recognized", &BotResult::recognized)
		.field("scores", &BotResult::scores);
	function("calculatePickFromOptions", &calculate_pick_from_options);
	function("initializeDraftbots", &initialize_with_data);
	function("testRecognized", &test_recognized);
}