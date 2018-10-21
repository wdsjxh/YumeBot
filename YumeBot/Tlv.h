#pragma once
#include <natBinary.h>

namespace YumeBot::Tlv
{
	class TlvBuilder
	{
	public:
		explicit TlvBuilder(NatsuLib::natRefPointer<NatsuLib::natBinaryWriter> writer)
			: m_Writer{ std::move(writer) }
		{
		}



	private:
		NatsuLib::natRefPointer<NatsuLib::natBinaryWriter> m_Writer;
	};
}
