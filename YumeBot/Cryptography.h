#pragma once

#include <gsl/span>
#include <natException.h>
#include <natString.h>

#include "Utility.h"
#include <charconv>

namespace YumeBot::Cryptography
{
	DeclareException(CryptoException, NatsuLib::natException, u8"YumeBot::Cryptography::CryptoException");

	namespace Tea
	{
		constexpr std::size_t TeaProcessUnitSize = 8;

		constexpr std::size_t CalculateOutputSize(std::size_t inputSize)
		{
			return Utility::AlignTo(inputSize + 10, TeaProcessUnitSize);
		}

		std::array<std::uint32_t, 4> FormatKey(gsl::span<const std::byte> const& key);

		std::size_t Encrypt(gsl::span<const std::byte> const& input, gsl::span<std::byte> const& output, gsl::span<const std::uint32_t, 4> const& key);
		std::size_t Decrypt(gsl::span<const std::byte> const& input, gsl::span<std::byte> const& output, gsl::span<const std::uint32_t, 4> const& key);
	}

	namespace Md5
	{
		void Calculate(gsl::span<const std::byte> const& input, gsl::span<std::byte, 16> const& output);

		template <typename Receiver>
		decltype(auto) Md5ToHexString(gsl::span<const std::byte, 16> const& md5, Receiver&& receiver)
		{
			char result[32]{ '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0' };

			for (std::size_t i = 0; i < 32; i += 2)
			{
				const auto value = static_cast<std::uint8_t>(md5[i / 2]);
				const auto toCharResult = std::to_chars(result + i + (value <= 0x0F), result + i + 2, value, 16);
				assert(toCharResult.ec == std::errc{});
			}

			return std::invoke(std::forward<Receiver>(receiver), std::cbegin(result), std::cend(result));
		}
	}
}
