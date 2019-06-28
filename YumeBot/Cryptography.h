#pragma once

#include <Cafe/ErrorHandling/ErrorHandling.h>
#include <gsl/span>

#include "Utility.h"
#include <charconv>

namespace YumeBot::Cryptography
{
	CAFE_DEFINE_GENERAL_EXCEPTION(CryptoException);

	namespace Tea
	{
		constexpr std::size_t TeaProcessUnitSize = 8;

		constexpr std::size_t CalculateOutputSize(std::size_t inputSize)
		{
			return Utility::AlignTo(inputSize + 10, TeaProcessUnitSize);
		}

		std::array<std::uint32_t, 4> FormatKey(gsl::span<const std::byte> const& key);

		std::size_t Encrypt(gsl::span<const std::byte> const& input, gsl::span<std::byte> const& output,
		                    gsl::span<const std::uint32_t, 4> const& key);
		std::size_t Decrypt(gsl::span<const std::byte> const& input, gsl::span<std::byte> const& output,
		                    gsl::span<const std::uint32_t, 4> const& key);
	} // namespace Tea

	namespace Md5
	{
		void Calculate(gsl::span<const std::byte> const& input, gsl::span<std::byte, 16> const& output);

		template <typename Receiver>
		decltype(auto) Md5ToHexString(gsl::span<const std::byte, 16> const& md5, Receiver&& receiver)
		{
#if __cpp_char8_t >= 201811L
			char8_t
#else
			char
#endif
			    result[32]{ '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0',
				              '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0',
				              '0', '0', '0', '0', '0', '0', '0', '0', '0', '0' };

			for (std::size_t i = 0; i < 32; i += 2)
			{
				const auto value = static_cast<std::uint8_t>(md5[i / 2]);
				const auto toCharResult =
				    std::to_chars(reinterpret_cast<char*>(result + i + (value <= 0x0F)),
				                  reinterpret_cast<char*>(result + i + 2), value, 16);
				assert(toCharResult.ec == std::errc{});
			}

			return std::invoke(std::forward<Receiver>(receiver),
			                   Cafe::Encoding::StringView<Cafe::Encoding::CodePage::Utf8>{ result });
		}
	} // namespace Md5
} // namespace YumeBot::Cryptography
