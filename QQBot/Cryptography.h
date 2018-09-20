#pragma once

#include <gsl/span>
#include <natException.h>
#include <natString.h>

#include "Utility.h"

namespace QQBot::Cryptography
{
	DeclareException(CryptoException, NatsuLib::natException, u8"QQBot::Cryptography::CryptoException");

	namespace Tea
	{
		constexpr std::size_t TeaProcessUnitSize = 8;

		constexpr std::size_t CalculateOutputSize(std::size_t inputSize)
		{
			return Utility::AlignTo(inputSize + 10, TeaProcessUnitSize);
		}

		std::size_t Encrypt(gsl::span<const std::byte> const& input, gsl::span<std::byte> const& output, gsl::span<const std::byte> const& key);
		std::size_t Decrypt(gsl::span<const std::byte> const& input, gsl::span<std::byte> const& output, gsl::span<const std::byte> const& key);
	}
}
