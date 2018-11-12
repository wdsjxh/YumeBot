#include "Cryptography.h"
#include "Utility.h"
#include <random>
#include <openssl/md5.h>

#undef min
#undef max

using namespace NatsuLib;
using namespace YumeBot::Cryptography;

namespace
{
	constexpr std::uint32_t TeaDelta = 0x9E3779B9;
	constexpr std::size_t TeaIterationTimes = 16;
	constexpr std::uint32_t TeaDecryptInitSum = TeaDelta << 4;

	void Encrypt(gsl::span<const std::uint32_t, 2> const& input, gsl::span<std::uint32_t, 2> const& output, gsl::span<const std::uint32_t, 4> const& key)
	{
		auto left = input[0], right = input[1];
		const auto a = key[0], b = key[1], c = key[2], d = key[3];

		std::uint32_t sum = 0;
		for (std::size_t i = 0; i < TeaIterationTimes; ++i)
		{
			sum += TeaDelta;
			left += ((right << 4) + a) ^ (right + sum) ^ ((right >> 5) + b);
			right += ((left << 4) + c) ^ (left + sum) ^ ((left >> 5) + d);
		}

		output[0] = left;
		output[1] = right;
	}

	void Decrypt(gsl::span<const std::uint32_t, 2> const& input, gsl::span<std::uint32_t, 2> const& output, gsl::span<const std::uint32_t, 4> const& key)
	{
		auto left = input[0], right = input[1];
		const auto a = key[0], b = key[1], c = key[2], d = key[3];

		auto sum = TeaDecryptInitSum;
		for (std::size_t i = 0; i < TeaIterationTimes; i++)
		{
			right -= ((left << 4) + c) ^ (left + sum) ^ ((left >> 5) + d);
			left -= ((right << 4) + a) ^ (right + sum) ^ ((right >> 5) + b);
			sum -= TeaDelta;
		}

		output[0] = left;
		output[1] = right;
	}
}

std::array<std::uint32_t, 4> Tea::FormatKey(gsl::span<const std::byte> const& key)
{
	if (key.empty())
	{
		nat_Throw(CryptoException, u8"key is empty."_nv);
	}

	std::array<std::uint32_t, 4> result{ 0x20202020, 0x20202020, 0x20202020, 0x20202020 };
	std::memcpy(result.data(), key.data(), std::min<std::size_t>(key.size(), 16));
	return result;
}

///	@see https://baike.baidu.com/item/TEA%E5%8A%A0%E5%AF%86%E7%AE%97%E6%B3%95
std::size_t Tea::Encrypt(gsl::span<const std::byte> const& input, gsl::span<std::byte> const& output, gsl::span<const std::uint32_t, 4> const& key)
{
	const auto totalProcessSize = CalculateOutputSize(input.size());
	assert(totalProcessSize % TeaProcessUnitSize == 0);
	if (static_cast<std::size_t>(output.size()) < totalProcessSize)
	{
		nat_Throw(CryptoException, u8"No enough space to output."_nv);
	}
	const auto paddingSize = totalProcessSize - input.size();
	const auto frontPaddingSize = paddingSize - 7;

	std::random_device randomDevice;
	std::default_random_engine randomEngine{ randomDevice() };
	const std::uniform_int_distribution<nuInt> dist{ 0x00, 0xFF };

	std::uint32_t inputBuffer[2];
	std::uint32_t outputBuffer[2];

	auto inputPtr = input.data();

	for (std::size_t processedLength = 0; processedLength < totalProcessSize; processedLength += TeaProcessUnitSize)
	{
		if (processedLength == totalProcessSize - TeaProcessUnitSize)
		{
			reinterpret_cast<std::byte&>(inputBuffer[0]) = *inputPtr++;
			std::memset(reinterpret_cast<std::byte*>(inputBuffer) + 1, 0, 7);
		}
		else
		{
			auto writePointer = reinterpret_cast<std::byte*>(inputBuffer);
			const auto writeEndPointer = reinterpret_cast<std::byte*>(std::end(inputBuffer));

			if (processedLength < frontPaddingSize)
			{
				const auto paddingEndPointer = std::next(writePointer, std::min((sizeof inputBuffer) / sizeof(std::byte), frontPaddingSize - processedLength));

				if (processedLength == 0)
				{
					*writePointer++ = static_cast<std::byte>((dist(randomEngine) & 0xF8) | (paddingSize - 10));
				}

				std::generate(writePointer, paddingEndPointer, [&]
				{
					return static_cast<std::byte>(dist(randomEngine));
				});

				writePointer = paddingEndPointer;
			}

			const auto readSize = std::distance(writePointer, writeEndPointer);
			std::memcpy(writePointer, inputPtr, readSize);
			inputPtr += readSize;
		}

		if (processedLength > 0)
		{
			const auto inputBytePtr = reinterpret_cast<std::byte*>(inputBuffer);
			const auto outputBytePtr = reinterpret_cast<std::byte*>(outputBuffer);
			for (std::size_t i = 0; i < (sizeof inputBuffer) / sizeof(std::byte); ++i)
			{
				inputBytePtr[i] ^= outputBytePtr[i];
			}
		}

		::Encrypt(inputBuffer, outputBuffer, key);
		std::memcpy(&output[processedLength], outputBuffer, TeaProcessUnitSize);
	}

	return totalProcessSize;
}

std::size_t Tea::Decrypt(gsl::span<const std::byte> const& input, gsl::span<std::byte> const& output, gsl::span<const std::uint32_t, 4> const& key)
{
	const auto inputSize = static_cast<std::size_t>(input.size());
	if (inputSize % TeaProcessUnitSize != 0 || inputSize < 16)
	{
		nat_Throw(CryptoException, u8"Invalid input data."_nv);
	}

	std::uint32_t inputBuffer[2];
	std::uint32_t outputBuffer[2];
	std::uint32_t lastInputBuffer[2];

	std::size_t frontPaddingSize;
	auto outputPtr = output.data();

	for (std::size_t processedLength = 0; processedLength < inputSize; processedLength += TeaProcessUnitSize)
	{
		std::memcpy(inputBuffer, &input[processedLength], TeaProcessUnitSize);

		::Decrypt(inputBuffer, outputBuffer, key);

		if (processedLength > 0)
		{
			const auto inputBytePtr = reinterpret_cast<std::byte*>(lastInputBuffer);
			const auto outputBytePtr = reinterpret_cast<std::byte*>(outputBuffer);
			for (std::size_t i = 0; i < (sizeof inputBuffer) / sizeof(std::byte); ++i)
			{
				outputBytePtr[i] ^= inputBytePtr[i];
			}
		}

		std::memcpy(lastInputBuffer, inputBuffer, sizeof lastInputBuffer);

		if (processedLength == 0)
		{
			const auto outputBufferPtr = reinterpret_cast<nByte*>(outputBuffer);
			const std::size_t paddingSize = (outputBufferPtr[0] & 7u) + 10u;
			if (static_cast<std::size_t>(output.size()) < inputSize - paddingSize)
			{
				nat_Throw(CryptoException, u8"No enough space to output."_nv);
			}
			frontPaddingSize = paddingSize - 7;
			if (frontPaddingSize < TeaProcessUnitSize)
			{
				const auto dataSize = TeaProcessUnitSize - frontPaddingSize;
				std::memcpy(outputPtr, outputBufferPtr + frontPaddingSize, dataSize);
				outputPtr += dataSize;
			}
		}
		else if (processedLength == inputSize - TeaProcessUnitSize)
		{
			*outputPtr = reinterpret_cast<std::byte*>(outputBuffer)[0];
		}
		else
		{
			if (processedLength + TeaProcessUnitSize <= frontPaddingSize)
			{
				continue;
			}

			auto outputSize = TeaProcessUnitSize;
			auto outputBufferPtr = reinterpret_cast<std::byte*>(outputBuffer);
			if (processedLength < frontPaddingSize)
			{
				outputSize = processedLength + TeaProcessUnitSize - frontPaddingSize;
				outputBufferPtr += TeaProcessUnitSize - outputSize;
			}

			std::memcpy(outputPtr, outputBufferPtr, outputSize);
			outputPtr += outputSize;
		}
	}

	return inputSize - frontPaddingSize - 7;
}

void Md5::Calculate(gsl::span<const std::byte> const& input, gsl::span<std::byte, 16> const& output)
{
	MD5(reinterpret_cast<const unsigned char*>(input.data()), input.size(), reinterpret_cast<unsigned char*>(output.data()));
}
