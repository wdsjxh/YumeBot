#pragma once
#include <cstddef>
#include <gsl/span>
#include <type_traits>

namespace QQBot::Utility
{
	constexpr std::size_t AlignTo(std::size_t num, std::size_t alignment)
	{
		return num + alignment - 1 & ~(alignment - 1);
	}

	template <typename T, typename U, bool = std::is_const_v<T>>
	struct MayAddConst
	{
		using Type = std::add_const_t<U>;
	};

	template <typename T, typename U>
	struct MayAddConst<T, U, false>
	{
		using Type = U;
	};

	template <typename T, std::size_t N>
	typename MayAddConst<T, std::byte>::Type(&ToByteArray(T(&arr)[N]) noexcept)[sizeof(T) * N / sizeof(std::byte)]
	{
		return reinterpret_cast<typename MayAddConst<T, std::byte>::Type(&)[sizeof(T) * N / sizeof(std::byte)]>(arr);
	}

	template <typename T, std::size_t N>
	gsl::span<typename MayAddConst<T, std::byte>::Type, sizeof(T) * N / sizeof(std::byte)> ToByteSpan(T(&arr)[N]) noexcept
	{
		return ToByteArray(arr);
	}
}
