#include "pch.h"
#include <catch.hpp>

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
typename MayAddConst<T, std::byte>::Type (&ToBytes(T (&arr)[N]))[sizeof(T) * N / sizeof(std::byte)]
{
	return reinterpret_cast<typename MayAddConst<T, std::byte>::Type(&)[sizeof(T) * N / sizeof(std::byte)]>(arr);
}

TEST_CASE("Cryptography", "[Utility][Cryptography]")
{
	constexpr const char text[] = "123456789123456789";
	constexpr const char key[] = "00000000000000000000000000000000";

	char result[QQBot::Cryptography::Tea::CalculateOutputSize(std::size(text))]{};
	const auto resultSize = QQBot::Cryptography::Tea::Encrypt(ToBytes(text), ToBytes(result), ToBytes(key));
	char decryptResult[std::size(result)]{};
	const auto decryptResultSize = QQBot::Cryptography::Tea::Decrypt(ToBytes(result), ToBytes(decryptResult), ToBytes(key));

	REQUIRE(resultSize == QQBot::Cryptography::Tea::CalculateOutputSize(std::size(text)));
	REQUIRE(decryptResultSize == std::size(text));
	REQUIRE(std::memcmp(decryptResult, text, std::size(text)) == 0);
}
