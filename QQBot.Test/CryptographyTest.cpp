#include "pch.h"
#include <catch.hpp>

using namespace QQBot;

TEST_CASE("Cryptography", "[Utility][Cryptography]")
{
	using namespace Cryptography;

	SECTION("Tea")
	{
		using namespace Tea;

		constexpr const char text[] = "123456789123456789";
		constexpr const char key[] = "00000000000000000000000000000000";

		char result[CalculateOutputSize(std::size(text))]{};
		const auto formattedKey = FormatKey(Utility::ToByteSpan(key));
		const auto resultSize = Encrypt(Utility::ToByteSpan(text), Utility::ToByteSpan(result), formattedKey);
		char decryptResult[std::size(result)]{};
		const auto decryptResultSize = Decrypt(Utility::ToByteSpan(result), Utility::ToByteSpan(decryptResult), formattedKey);

		REQUIRE(resultSize == Cryptography::Tea::CalculateOutputSize(std::size(text)));
		REQUIRE(decryptResultSize == std::size(text));
		REQUIRE(std::memcmp(decryptResult, text, std::size(text)) == 0);
	}

	SECTION("Md5")
	{
		using namespace Md5;

		constexpr const char test[] = "test";

		std::byte result[16];
		Calculate(Utility::ToByteSpan(test).subspan(0, 4), result);

		constexpr const nByte expectedResult[] = "\x09\x8f\x6b\xcd\x46\x21\xd3\x73\xca\xde\x4e\x83\x26\x27\xb4\xf6";
		REQUIRE(std::memcmp(result, expectedResult, std::size(result)) == 0);
	}
}
