#pragma once
#include <Cafe/Io/StreamHelpers/BinaryWriter.h>

namespace YumeBot::Tlv
{
	class TlvBuilder
	{
	public:
		explicit TlvBuilder(Cafe::Io::BinaryWriter writer)
			: m_Writer{ std::move(writer) }
		{
		}



	private:
		Cafe::Io::BinaryWriter m_Writer;
	};
}
