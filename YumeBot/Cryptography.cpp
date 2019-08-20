#include "Cryptography.h"
#include "Utility.h"
#include <Cafe/Io/Streams/MemoryStream.h>
#include <Cafe/Misc/Scope.h>
#include <cstring>
#include <openssl/ecdh.h>
#include <openssl/md5.h>
#include <openssl/objects.h>
#include <random>

#undef min
#undef max

using namespace Cafe::Encoding::StringLiterals;
using namespace YumeBot::Cryptography;

namespace
{
	constexpr std::uint32_t TeaDelta = 0x9E3779B9;
	constexpr std::size_t TeaIterationTimes = 16;
	constexpr std::uint32_t TeaDecryptInitSum = TeaDelta << 4;

	void Encrypt(gsl::span<const std::uint32_t, 2> const& input,
	             gsl::span<std::uint32_t, 2> const& output,
	             gsl::span<const std::uint32_t, 4> const& key)
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

	void Decrypt(gsl::span<const std::uint32_t, 2> const& input,
	             gsl::span<std::uint32_t, 2> const& output,
	             gsl::span<const std::uint32_t, 4> const& key)
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
} // namespace

std::array<std::uint32_t, 4> Tea::FormatKey(gsl::span<const std::byte> const& key)
{
	if (key.empty())
	{
		CAFE_THROW(CryptoException, u8"key is empty."_sv);
	}

	std::array<std::uint32_t, 4> result{ 0x20202020, 0x20202020, 0x20202020, 0x20202020 };
	std::memcpy(result.data(), key.data(), std::min<std::size_t>(key.size(), 16));
	return result;
}

std::size_t Tea::Encrypt(gsl::span<const std::byte> const& input,
                         gsl::span<std::byte> const& output,
                         gsl::span<const std::uint32_t, 4> const& key)
{
	Cafe::Io::ExternalMemoryOutputStream stream{ output, Cafe::Io::ErrorOnOutOfRange };
	return Encrypt(input, &stream, key);
}

std::size_t Tea::Decrypt(gsl::span<const std::byte> const& input,
                         gsl::span<std::byte> const& output,
                         gsl::span<const std::uint32_t, 4> const& key)
{
	Cafe::Io::ExternalMemoryOutputStream stream{ output, Cafe::Io::ErrorOnOutOfRange };
	return Decrypt(input, &stream, key);
}

std::size_t Tea::Encrypt(gsl::span<const std::byte> const& input,
                         Cafe::Io::OutputStream* outputStream,
                         gsl::span<const std::uint32_t, 4> const& key)
{
	const auto totalProcessSize = CalculateOutputSize(input.size());
	assert(totalProcessSize % TeaProcessUnitSize == 0);

	const auto paddingSize = totalProcessSize - input.size();
	const auto frontPaddingSize = paddingSize - 7;

	std::random_device randomDevice;
	std::default_random_engine randomEngine{ randomDevice() };
	std::uniform_int_distribution<std::uint32_t> dist{ 0x00, 0xFF };

	std::uint32_t inputBuffer[2];
	std::uint32_t outputBuffer[2];

	auto inputPtr = input.data();

	for (std::size_t processedLength = 0; processedLength < totalProcessSize;
	     processedLength += TeaProcessUnitSize)
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
				const auto paddingEndPointer =
				    std::next(writePointer, std::min((sizeof inputBuffer) / sizeof(std::byte),
				                                     frontPaddingSize - processedLength));

				if (processedLength == 0)
				{
					*writePointer++ =
					    static_cast<std::byte>((dist(randomEngine) & 0xF8) | (paddingSize - 10));
				}

				std::generate(writePointer, paddingEndPointer,
				              [&] { return static_cast<std::byte>(dist(randomEngine)); });

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
		outputStream->WriteBytes(gsl::as_bytes(gsl::make_span(outputBuffer)));
	}

	return totalProcessSize;
}

std::size_t Tea::Decrypt(gsl::span<const std::byte> const& input,
                         Cafe::Io::OutputStream* outputStream,
                         gsl::span<const std::uint32_t, 4> const& key)
{
	const auto inputSize = static_cast<std::size_t>(input.size());
	if (inputSize % TeaProcessUnitSize != 0 || inputSize < 16)
	{
		CAFE_THROW(CryptoException, u8"Invalid input data."_sv);
	}

	std::uint32_t inputBuffer[2];
	std::uint32_t outputBuffer[2];
	std::uint32_t lastInputBuffer[2];

	std::size_t frontPaddingSize;

	for (std::size_t processedLength = 0; processedLength < inputSize;
	     processedLength += TeaProcessUnitSize)
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
			const auto outputBufferPtr = reinterpret_cast<std::uint8_t*>(outputBuffer);
			const std::size_t paddingSize = (outputBufferPtr[0] & 7u) + 10u;
			frontPaddingSize = paddingSize - 7;
			if (frontPaddingSize < TeaProcessUnitSize)
			{
				const auto dataSize = TeaProcessUnitSize - frontPaddingSize;
				outputStream->WriteBytes(
				    gsl::as_bytes(gsl::make_span(outputBufferPtr + frontPaddingSize, dataSize)));
			}
		}
		else if (processedLength == inputSize - TeaProcessUnitSize)
		{
			outputStream->WriteByte(reinterpret_cast<std::byte*>(outputBuffer)[0]);
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

			outputStream->WriteBytes(gsl::as_bytes(gsl::make_span(outputBufferPtr, outputSize)));
		}
	}

	return inputSize - frontPaddingSize - 7;
}

void Md5::Calculate(gsl::span<const std::byte> const& input, gsl::span<std::byte, 16> const& output)
{
	MD5(reinterpret_cast<const unsigned char*>(input.data()), input.size(),
	    reinterpret_cast<unsigned char*>(output.data()));
}

namespace
{
	// 对应 QQ 5.X
	constexpr unsigned char S_PUB_KEY[] = { 0x04, 0x92, 0x8D, 0x88, 0x50, 0x67, 0x30, 0x88, 0xB3,
		                                      0x43, 0x26, 0x4E, 0x0C, 0x6B, 0xAC, 0xB8, 0x49, 0x6D,
		                                      0x69, 0x77, 0x99, 0xF3, 0x72, 0x11, 0xDE, 0xB2, 0x5B,
		                                      0xB7, 0x39, 0x06, 0xCB, 0x08, 0x9F, 0xEA, 0x96, 0x39,
		                                      0xB4, 0xE0, 0x26, 0x04, 0x98, 0xB5, 0x1A, 0x99, 0x2D,
		                                      0x50, 0x81, 0x3D, 0xA8 };
} // namespace

void Ecdh::GenerateKeyPair(gsl::span<std::byte, 25> const& pubKey,
                           gsl::span<std::byte, 16> const& shareKey)
{
	const auto key = EC_KEY_new_by_curve_name(NID_secp192k1);
	if (!key)
	{
		CAFE_THROW(CryptoException, u8"EC_KEY_new_by_curve_name failed"_sv);
	}

	CAFE_SCOPE_EXIT
	{
		EC_KEY_free(key);
	};

	if (!EC_KEY_generate_key(key))
	{
		CAFE_THROW(CryptoException, u8"EC_KEY_generate_key failed"_sv);
	}

	const auto point1 = EC_KEY_get0_public_key(key);
	if (!point1)
	{
		CAFE_THROW(CryptoException, u8"EC_KEY_get0_public_key failed"_sv);
	}

	const auto group1 = EC_KEY_get0_group(key);
	if (!group1)
	{
		CAFE_THROW(CryptoException, u8"EC_KEY_get0_group failed"_sv);
	}

	unsigned char resultPubKey[25];
	const auto octSize = EC_POINT_point2oct(group1, point1, POINT_CONVERSION_COMPRESSED, resultPubKey,
	                                        sizeof resultPubKey, nullptr);
	if (octSize != sizeof resultPubKey)
	{
		CAFE_THROW(CryptoException, u8"EC_POINT_point2oct failed"_sv);
	}

	const auto group2 = EC_KEY_get0_group(key);
	if (!group2)
	{
		CAFE_THROW(CryptoException, u8"EC_KEY_get0_group failed"_sv);
	}

	const auto point2 = EC_POINT_new(group2);
	if (!point2)
	{
		CAFE_THROW(CryptoException, u8"EC_POINT_new failed"_sv);
	}

	CAFE_SCOPE_EXIT
	{
		EC_POINT_free(point2);
	};

	const auto pointSize = EC_POINT_oct2point(group2, point2, S_PUB_KEY, sizeof S_PUB_KEY, nullptr);
	if (!pointSize)
	{
		CAFE_THROW(CryptoException, u8"EC_POINT_oct2point failed"_sv);
	}

	unsigned char resultShareKey[25];
	const auto result = ECDH_compute_key(resultShareKey, sizeof resultShareKey, point2, key, nullptr);
	if (result != sizeof resultShareKey)
	{
		CAFE_THROW(CryptoException, u8"ECDH_compute_key failed"_sv);
	}

	unsigned char resultShareKeyMd5[16];
	MD5(resultShareKey, result, resultShareKeyMd5);

	std::memcpy(pubKey.data(), resultPubKey, 25);
	std::memcpy(shareKey.data(), resultShareKeyMd5, 16);
}
