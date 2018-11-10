#include "Wup.h"

using namespace NatsuLib;
using namespace YumeBot;
using namespace Jce;
using namespace Wup;

nStrView Wup::Detail::GetName(natRefPointer<JceStruct> const& value) noexcept
{
	return value->GetJceStructName();
}

bool OldUniAttribute::Remove(nString const& name)
{
	return !!m_Data.erase(name);
}

void OldUniAttribute::Encode(natRefPointer<natBinaryWriter> const& writer) const
{
	JceOutputStream output{ writer };
	output.Write(0, m_Data);
}

void OldUniAttribute::Decode(natRefPointer<natBinaryReader> const& reader)
{
	JceInputStream input{ reader };
	m_Data.clear();
	if (!input.Read(0, m_Data))
	{
		nat_Throw(natErrException, NatErr_InternalErr, u8"Data is corrupted"_nv);
	}
}
