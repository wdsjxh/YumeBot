#pragma once
#include <Cafe/Encoding/CodePage/UTF-8.h>

namespace YumeBot
{
	enum class LocaleId
	{
		EN_US = 1033,
		ZH_CN = 2052,
		ZH_HK = 1028,
	};

	constexpr LocaleId UsingLocaleId = LocaleId::ZH_CN;

	static_assert(
	    std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559,
	    "Jce assumed float and double fulfill the requirements of IEEE 754(IEC 559) standard.");

	using UsingString = Cafe::Encoding::String<Cafe::Encoding::CodePage::Utf8>;
	using UsingStringView = Cafe::Encoding::StringView<Cafe::Encoding::CodePage::Utf8>;
} // namespace YumeBot
