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

UniPacket::UniPacket()
	: m_OldRespIRet{}
{
}

void UniPacket::Encode(natRefPointer<natBinaryWriter> const& writer)
{
	const auto tmpBuffer = make_ref<natMemoryStream>(0, false, true, true);
	m_UniAttribute.Encode(make_ref<natBinaryWriter>(tmpBuffer));
	const auto buffer = tmpBuffer->GetInternalBuffer();
	m_RequestPacket.GetsBuffer().assign(buffer, buffer + tmpBuffer->GetSize());

	const auto underlyingStream = writer->GetUnderlyingStream();
	const auto sizePos = underlyingStream->GetPosition();
	underlyingStream->SetPosition(NatSeek::Cur, 4);

	JceOutputStream os{ writer };
	os.Write(0, m_RequestPacket);
	const auto endPos = underlyingStream->GetPosition();
	const auto length = endPos - sizePos;
	underlyingStream->SetPositionFromBegin(sizePos);
	writer->WritePod(static_cast<std::int32_t>(length));
	underlyingStream->SetPositionFromBegin(endPos);
}

void UniPacket::Decode(natRefPointer<natBinaryReader> const& reader)
{
	reader->GetUnderlyingStream()->SetPosition(NatSeek::Cur, 4);
	JceInputStream is{ reader };
	if (!is.Read(0, m_RequestPacket))
	{
		nat_Throw(natErrException, NatErr_InternalErr, u8"Read RequestPacket failed."_nv);
	}

	const auto& buffer = m_RequestPacket.GetsBuffer();
	m_UniAttribute.Decode(make_ref<natBinaryReader>(make_ref<natExternMemoryStream>(buffer.data(), buffer.size(), true)));
}

UniPacket UniPacket::CreateResponse()
{
	UniPacket result;
	result.m_RequestPacket.SetiRequestId(m_RequestPacket.GetiRequestId());
	result.m_RequestPacket.SetsServantName(m_RequestPacket.GetsServantName());
	result.m_RequestPacket.SetsFuncName(m_RequestPacket.GetsFuncName());
	result.m_RequestPacket.SetiVersion(m_RequestPacket.GetiVersion());
	return result;
}

void UniPacket::CreateOldRespEncode(JceOutputStream& os)
{
	const auto memoryStream = make_ref<natMemoryStream>(0, false, true, true);
	const auto tempWriter = make_ref<natBinaryWriter>(memoryStream);
	m_UniAttribute.Encode(tempWriter);

	os.Write(1, m_RequestPacket.GetiVersion());
	os.Write(2, m_RequestPacket.GetcPacketType());
	os.Write(3, m_RequestPacket.GetiRequestId());
	os.Write(4, m_RequestPacket.GetiMessageType());
	os.Write(5, m_OldRespIRet);
	os.Write(6, gsl::make_span(memoryStream->GetInternalBuffer(), memoryStream->GetSize()));
	os.Write(7, m_RequestPacket.Getstatus());
}
