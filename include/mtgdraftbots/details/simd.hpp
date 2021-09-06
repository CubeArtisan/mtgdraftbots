#ifndef MTGDRAFTBOTS_SIMD_HPP
#define MTGDRAFTBOTS_SIMD_HPP

#include <array>
#include <bit>
#include <cmath>
#include <concepts>

#ifdef USE_VECTORCLASS
#include <vectorclass.h>
#endif

namespace mtgdraftbots {
    using Lands = std::array<unsigned char, 32>;

    namespace details {
        template <typename T>
        struct Weighted {
            float weight;
            T value;
        };

        constexpr std::size_t EMBEDDING_SIZE = 32;
        using Embedding = std::array<float, EMBEDDING_SIZE>;

        enum struct Mask : unsigned char {
            ON = 0xFF, OFF = 0x00
        };
        using LandsMask = std::array<Mask, 32>;

        template<typename To, typename From>
        constexpr auto bit_cast(const From& from) noexcept -> To {
#ifdef __clang__
            return __builtin_bit_cast(To, from);
#else
            return std::bit_cast<To>(from);
#endif
        }

#ifdef USE_VECTORCLASS
        inline auto vcl_sum_masked(const LandsMask& mask, const Lands& lands) -> std::uint32_t {
            vcl::Vec32uc maskVec;
            maskVec.load(mask.data());
            vcl::Vec32uc landsVec;
            landsVec.load(lands.data());
            return vcl::horizontal_add_x(maskVec & landsVec);
        }

        inline auto vcl_operator_or(const LandsMask& mask1, const LandsMask& mask2) -> LandsMask {
            LandsMask result;
            vcl::Vec32uc mask1Vec;
            mask1Vec.load(mask1.data());
            vcl::Vec32uc mask2Vec;
            mask2Vec.load(mask2.data());
            (mask1Vec | mask2Vec).store(result.data());
            return result;
        }

        inline auto vcl_operator_and(const LandsMask& mask1, const LandsMask& mask2) -> LandsMask {
            LandsMask result;
            vcl::Vec32uc mask1Vec;
            mask1Vec.load(mask1.data());
            vcl::Vec32uc mask2Vec;
            mask2Vec.load(mask2.data());
            (mask1Vec & mask2Vec).store(result.data());
            return result;
        }

        inline auto vcl_operator_not(const LandsMask& mask) -> LandsMask {
            LandsMask result;
            vcl::Vec32uc maskVec;
            maskVec.load(mask.data());
            (~maskVec).store(result.data());
            return result;
        }
#endif

        constexpr auto sum_masked(const LandsMask& mask, const Lands& lands) -> unsigned char {
#ifdef USE_VECTORCLASS
            if (!std::is_constant_evaluated()) {
                return static_cast<unsigned char>(vcl_sum_masked(mask, lands));
            }
#endif
            std::uint64_t result_64 = 0;
            for (unsigned char i = 0; i < 4; i++) {
                std::uint64_t sub_mask = details::bit_cast<std::array<std::uint64_t, 4>>(mask)[i];
                std::uint64_t sub_lands = details::bit_cast<std::array<std::uint64_t, 4>>(lands)[i];
                result_64 += sub_mask & sub_lands;
            }
            auto result_64s = details::bit_cast<std::array<std::uint32_t, 2>>(result_64);
            auto result_32 = details::bit_cast<std::array<std::uint16_t, 2>>(result_64s[0] + result_64s[1]);
            auto result_16 = details::bit_cast<std::array<unsigned char, 2>>(static_cast<std::uint16_t>(result_32[0] + result_32[1]));
            return static_cast<unsigned char>(result_16[0] + result_16[1]);
        }

        constexpr auto operator|(const LandsMask& mask1, const LandsMask& mask2) -> LandsMask {
#ifdef USE_VECTORCLASS
            if (!std::is_constant_evaluated()) {
                return vcl_operator_or(mask1, mask2);
            }
#endif
            std::array<std::uint64_t, 4> result;
            for (unsigned char i = 0; i < 4; i++) {
                result[i] = details::bit_cast<std::array<std::uint64_t, 4>>(mask1)[i]
                    | details::bit_cast<std::array<std::uint64_t, 4>>(mask2)[i];
            }
            return details::bit_cast<LandsMask>(result);
        }

        constexpr auto operator&(const LandsMask& mask1, const LandsMask& mask2) -> LandsMask {
#ifdef USE_VECTORCLASS
            if (!std::is_constant_evaluated()) {
                return vcl_operator_and(mask1, mask2);
            }
#endif
            std::array<std::uint64_t, 4> result;
            for (unsigned char i = 0; i < 4; i++) {
                result[i] = details::bit_cast<std::array<std::uint64_t, 4>>(mask1)[i]
                    & details::bit_cast<std::array<std::uint64_t, 4>>(mask2)[i];
            }
            return details::bit_cast<LandsMask>(result);
        }

        constexpr auto operator~(const LandsMask& mask) -> LandsMask {
#ifdef USE_VECTORCLASS
            if (!std::is_constant_evaluated()) {
                return vcl_operator_not(mask);
            }
#endif
            std::array<std::uint64_t, 4> result;
            for (unsigned char i = 0; i < 4; i++) {
                result[i] = ~details::bit_cast<std::array<std::uint64_t, 4>>(mask)[i];
            }
            return details::bit_cast<LandsMask>(result);
        }

        template<typename Container>
        constexpr auto operator+=(Container& emb1, const Container& emb2) noexcept -> Container& {
            for (std::size_t i = 0; i < emb1.size(); i++) emb1[i] += emb2[i];
            return emb1;
        }

        template<typename Container>
        constexpr auto operator+=(Container& emb1, typename Container::value_type value) noexcept -> Container& {
            for (std::size_t i = 0; i < emb1.size(); i++) emb1[i] += value;
            return emb1;
        }

        template<typename Container>
        constexpr auto operator+=(Container& emb,
                                  Weighted<Container> weighted_emb) noexcept -> Container& {
            for (std::size_t i = 0; i < emb.size(); i++) emb[i] += weighted_emb.weight * weighted_emb.value[i];
            return emb;
        }
        
        template <typename Numeric, typename Container> requires std::same_as<Numeric, typename Container::value_type>
        constexpr auto operator*(Numeric weight, const Container& value) noexcept -> Weighted<Container> {
            return { weight, value };
        }

        template <typename Container>
        constexpr auto operator*(const Container& emb1, const Container& emb2) noexcept -> typename Container::value_type {
            typename Container::value_type result = 0.f;
            for (std::size_t i = 0; i < emb1.size(); i++) result += emb1[i] * emb2[i];
            return result;
        }

        template <typename Container>
        constexpr auto operator/=(Container& emb, typename Container::value_type value) noexcept -> Container& {
            for (std::size_t i = 0; i < emb.size(); i++) emb[i] /= value;
            return emb;
        }

        template <typename Container>
        constexpr auto l2_normalize(Container& emb) noexcept -> Container& {
            auto norm = emb * emb;
            if (norm <= static_cast<decltype(norm)>(0)) emb = { static_cast<decltype(norm)>(0) };
            else emb /= std::sqrt(norm);
            return emb;
        }
    }
}
#endif