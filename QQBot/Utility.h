#pragma once
#include <cstddef>

namespace QQBot::Utility
{
	constexpr std::size_t AlignTo(std::size_t num, std::size_t alignment)
	{
		return num + alignment - 1 & ~(alignment - 1);
	}
}
