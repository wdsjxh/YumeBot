#pragma once
#include <Cafe/Encoding/CodePage/UTF-8.h>

namespace YumeBot
{
	constexpr std::uint32_t DefaultAppId = 537039093;
	/// @remark 使用短信登录时为 3
	constexpr std::uint32_t DefualtSigSrc = 1;
	constexpr std::uint32_t DefaultBitmap = 0x7F7C;
	constexpr std::uint32_t DefaultGetSig = 0x10400;
	constexpr std::uint32_t DefaultGetSig1 = 0x1E1060;

	constexpr UsingStringView DefaultDomains[]{ CAFE_UTF8_SV("game.qq.com") };

	struct IpV4Addr
	{
		std::uint8_t Content[4]{};

		constexpr bool IsUnspecified() const noexcept
		{
			return std::all_of(std::begin(Content), std::end(Content),
			                   [](std::uint8_t value) { return value == 0; });
		}
	};

	enum class LocaleIdEnum
	{
		EN_US = 1033,
		ZH_CN = 2052,
		ZH_HK = 1028,
	};

	constexpr LocaleIdEnum UsingLocaleId = LocaleIdEnum::ZH_CN;

	enum class ConnectionTypeEnum
	{
		Mobile = 0,
		Wifi = 1,
		MobileMMS = 2,
		MobileSUPL = 3,
		MobileDUN = 4,
		MobileHIPRI = 5,
		WiMAX = 6,
		Bluetooth = 7,
		Dummy = 8,
		Ethernet = 9,
		Vpn = 17
	};

	constexpr auto DefaultApkVersion = CAFE_UTF8_SV("5.0.0");

	constexpr std::uint16_t DefaultClientVersion = 8001;

	// 参考 Docs/研究.md，是签名的 MD5
	constexpr std::uint8_t Signature[]{ 0xa6, 0xb7, 0x45, 0xbf, 0x24, 0xa2, 0xc2, 0x77,
		                                  0x52, 0x77, 0x16, 0xf6, 0xf3, 0x6e, 0xb6, 0x8d };

	// GMT 2014/7/21 8:08:42
	constexpr std::time_t BuildTime = 1405930122;

	constexpr auto OsType = CAFE_UTF8_SV("android");
	constexpr auto DefaultOsVersion = CAFE_UTF8_SV("5.0.2");

	constexpr auto SdkVersion = CAFE_UTF8_SV("5.2.2.98");

	static_assert(
	    std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559,
	    "Jce assumed float and double fulfill the requirements of IEEE 754(IEC 559) standard.");

	using UsingString = Cafe::Encoding::String<Cafe::Encoding::CodePage::Utf8>;
	using UsingStringView = Cafe::Encoding::StringView<Cafe::Encoding::CodePage::Utf8>;
} // namespace YumeBot
