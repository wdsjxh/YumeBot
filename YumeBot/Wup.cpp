#include "Wup.h"

using namespace Cafe;
using namespace Io;
using namespace ErrorHandling;
using namespace Encoding;
using namespace StringLiterals;

using namespace YumeBot;
using namespace Jce;
using namespace Wup;

UsingStringView Wup::Detail::GetName(std::shared_ptr<JceStruct> const& value) noexcept
{
	return value->GetJceStructName();
}

bool OldUniAttribute::Remove(UsingString const& name)
{
	return !!m_Data.erase(name);
}

void OldUniAttribute::Encode(BinaryWriter const& writer) const
{
	JceOutputStream output{ writer };
	output.Write(0, m_Data);
}

void OldUniAttribute::Decode(BinaryReader const& reader)
{
	JceInputStream input{ reader };
	m_Data.clear();
	if (!input.Read(0, m_Data))
	{
		CAFE_THROW(CafeException, u8"Data is corrupted"_sv);
	}
}

UniPacket::UniPacket() : m_OldRespIRet{}
{
}

void UniPacket::Encode(BinaryWriter const& writer)
{
	MemoryStream tmpBuffer;
	m_UniAttribute.Encode(Cafe::Io::BinaryWriter{ &tmpBuffer });
	const auto buffer = tmpBuffer.GetInternalStorage();
	m_RequestPacket.GetsBuffer().assign(buffer.data(), buffer.data() + buffer.size());

	const auto underlyingStream = dynamic_cast<SeekableStreamBase*>(writer.GetStream());
	assert(underlyingStream);
	const auto sizePos = underlyingStream->GetPosition();
	// 占位 4 字节以便之后返回写入长度信息
	writer.Write(std::uint32_t{});

	JceOutputStream os{ writer };
	os.Write(0, m_RequestPacket);
	const auto endPos = underlyingStream->GetPosition();
	const auto length = endPos - sizePos;
	underlyingStream->SeekFromBegin(sizePos);
	writer.Write(static_cast<std::int32_t>(length));
	underlyingStream->SeekFromBegin(endPos);
}

void UniPacket::Decode(BinaryReader const& reader)
{
	const auto underlyingStream = dynamic_cast<SeekableStreamBase*>(reader.GetStream());
	assert(underlyingStream);
	underlyingStream->Seek(SeekOrigin::Current, 4);
	JceInputStream is{ reader };
	if (!is.Read(0, m_RequestPacket))
	{
		CAFE_THROW(CafeException, u8"Read RequestPacket failed."_sv);
	}

	const auto& buffer = m_RequestPacket.GetsBuffer();
	ExternalMemoryInputStream stream{ gsl::as_bytes(gsl::make_span(buffer.data(), buffer.size())) };
	m_UniAttribute.Decode(Cafe::Io::BinaryReader{ &stream });
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
	MemoryStream memoryStream;
	m_UniAttribute.Encode(Cafe::Io::BinaryWriter{ &memoryStream });

	os.Write(1, m_RequestPacket.GetiVersion());
	os.Write(2, m_RequestPacket.GetcPacketType());
	os.Write(3, m_RequestPacket.GetiRequestId());
	os.Write(4, m_RequestPacket.GetiMessageType());
	os.Write(5, m_OldRespIRet);
	const auto internalStorage = memoryStream.GetInternalStorage();
	os.Write(6, internalStorage);
	os.Write(7, m_RequestPacket.Getstatus());
}
