#include "Jce.h"

using namespace NatsuLib;
using namespace YumeBot;
using namespace Jce;

JceStruct::~JceStruct()
{
}

JceInputStream::JceInputStream(natRefPointer<natBinaryReader> reader)
	: m_Reader{ std::move(reader) }
{
	if (m_Reader->GetEndianness() != Environment::Endianness::LittleEndian)
	{
		nat_Throw(JceDecodeException, u8"reader should use little endian."_nv);
	}
}

JceInputStream::~JceInputStream()
{
}

std::pair<HeadData, std::size_t> JceInputStream::ReadHead() const
{
	const auto byte = m_Reader->ReadPod<std::uint8_t>();
	const auto type = static_cast<JceStruct::TypeEnum>(static_cast<std::uint8_t>(byte & 0x0F));
	const auto tag = static_cast<std::uint32_t>((byte & 0xF0) >> 4);

	if (tag != 0x0F)
	{
		return { { tag, type }, 1 };
	}

	return { { m_Reader->ReadPod<std::uint8_t>(), type }, 2 };
}

std::pair<HeadData, std::size_t> JceInputStream::PeekHead() const
{
	const auto underlyingStream = m_Reader->GetUnderlyingStream();
	const auto pos = underlyingStream->GetPosition();
	const auto head = ReadHead();
	underlyingStream->SetPositionFromBegin(pos);
	return head;
}

void JceInputStream::Skip(nLen len)
{
	m_Reader->Skip(len);
}

void JceInputStream::SkipToStructEnd()
{
	while (true)
	{
		const auto [head, headSize] = ReadHead();
		SkipField(head.Type);
		if (head.Type == JceStruct::TypeEnum::StructEnd)
		{
			return;
		}
	}
}

void JceInputStream::SkipField()
{
	const auto [head, headSize] = ReadHead();
	SkipField(head.Type);
}

void JceInputStream::SkipField(JceStruct::TypeEnum type)
{
	switch (type)
	{
	case JceStruct::TypeEnum::Byte:
		Skip(1);
		break;
	case JceStruct::TypeEnum::Short:
		Skip(2);
		break;
	case JceStruct::TypeEnum::Int:
		Skip(4);
		break;
	case JceStruct::TypeEnum::Long:
		Skip(8);
		break;
	case JceStruct::TypeEnum::Float:
		Skip(4);
		break;
	case JceStruct::TypeEnum::Double:
		Skip(8);
		break;
	case JceStruct::TypeEnum::String1:
		Skip(m_Reader->ReadPod<std::uint8_t>());
		break;
	case JceStruct::TypeEnum::String4:
		Skip(m_Reader->ReadPod<std::uint32_t>());
		break;
	case JceStruct::TypeEnum::Map:
	{
		int size;
		if (!Read(0, size))
		{
			nat_Throw(JceDecodeException, u8"Read size failed.");
		}
		for (std::size_t i = 0, iterationTime = static_cast<std::size_t>(size) * 2; i < iterationTime; ++i)
		{
			SkipField();
		}
		break;
	}
	case JceStruct::TypeEnum::List:
	{
		int size;
		if (!Read(0, size))
		{
			nat_Throw(JceDecodeException, u8"Read size failed.");
		}
		for (std::size_t i = 0, iterationTime = static_cast<std::size_t>(size); i < iterationTime; ++i)
		{
			SkipField();
		}
		break;
	}
	case JceStruct::TypeEnum::StructBegin:
		SkipToStructEnd();
		break;
	case JceStruct::TypeEnum::StructEnd:
	case JceStruct::TypeEnum::ZeroTag:
		break;
	case JceStruct::TypeEnum::SimpleList:
	{
		const auto [head, headSize] = ReadHead();
		if (head.Type != JceStruct::TypeEnum::Byte)
		{
			nat_Throw(JceDecodeException, u8"Type mismatch, expected 0, got {0}", static_cast<std::uint32_t>(head.Type))
			;
		}
		std::uint8_t size;
		if (!Read(0, size))
		{
			nat_Throw(JceDecodeException, u8"Read size failed.");
		}
		Skip(size);
		break;
	}
	default:
		nat_Throw(JceDecodeException, u8"Invalid type ({0}).", static_cast<std::uint32_t>(type));
	}
}

bool JceInputStream::SkipToTag(std::uint32_t tag)
try
{
	HeadData head;  // NOLINT
	while (true)
	{
		std::size_t headSize;
		std::tie(head, headSize) = PeekHead();
		if (head.Type == JceStruct::TypeEnum::StructEnd)
		{
			return false;
		}
		if (tag <= head.Tag)
		{
			break;
		}
		Skip(headSize);
		SkipField(head.Type);
	}
	return head.Tag == tag;
}
catch (JceDecodeException&)
{
	return false;
}

bool JceInputStream::doRead(std::uint32_t tag, std::uint8_t& value)
{
	if (SkipToTag(tag))
	{
		const auto [head, headSize] = ReadHead();
		switch (head.Type)
		{
		case JceStruct::TypeEnum::Byte:
			m_Reader->ReadPod(value);
			break;
		case JceStruct::TypeEnum::ZeroTag:
			value = 0;
			break;
		default:
			nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}.", static_cast<std::uint32_t>(head.Type))
			;
		}

		return true;
	}

	return false;
}

bool JceInputStream::doRead(std::uint32_t tag, std::int16_t& value)
{
	if (SkipToTag(tag))
	{
		const auto [head, headSize] = ReadHead();
		switch (head.Type)
		{
		case JceStruct::TypeEnum::Byte:
			value = m_Reader->ReadPod<std::int8_t>();
			break;
		case JceStruct::TypeEnum::Short:
			m_Reader->ReadPod(value);
			break;
		case JceStruct::TypeEnum::ZeroTag:
			value = 0;
			break;
		default:
			nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}.", static_cast<std::uint32_t>(head.Type))
			;
		}

		return true;
	}

	return false;
}

bool JceInputStream::doRead(std::uint32_t tag, std::int32_t& value)
{
	if (SkipToTag(tag))
	{
		const auto [head, headSize] = ReadHead();
		switch (head.Type)
		{
		case JceStruct::TypeEnum::Byte:
			value = m_Reader->ReadPod<std::int8_t>();
			break;
		case JceStruct::TypeEnum::Short:
			value = m_Reader->ReadPod<std::int16_t>();
			break;
		case JceStruct::TypeEnum::Int:
			m_Reader->ReadPod(value);
			break;
		case JceStruct::TypeEnum::ZeroTag:
			value = 0;
			break;
		default:
			nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}.", static_cast<std::uint32_t>(head.Type))
			;
		}

		return true;
	}

	return false;
}

bool JceInputStream::doRead(std::uint32_t tag, std::int64_t& value)
{
	if (SkipToTag(tag))
	{
		const auto [head, headSize] = ReadHead();
		switch (head.Type)
		{
		case JceStruct::TypeEnum::Byte:
			value = m_Reader->ReadPod<std::int8_t>();
			break;
		case JceStruct::TypeEnum::Short:
			value = m_Reader->ReadPod<std::int16_t>();
			break;
		case JceStruct::TypeEnum::Int:
			value = m_Reader->ReadPod<std::int32_t>();
			break;
		case JceStruct::TypeEnum::Long:
			m_Reader->ReadPod(value);
			break;
		case JceStruct::TypeEnum::ZeroTag:
			value = 0;
			break;
		default:
			nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected got {0}", static_cast<std::uint32_t>(head.
				Type));
		}

		return true;
	}

	return false;
}

bool JceInputStream::doRead(std::uint32_t tag, float& value)
{
	if (SkipToTag(tag))
	{
		const auto [head, headSize] = ReadHead();
		switch (head.Type)
		{
		case JceStruct::TypeEnum::Float:
			m_Reader->ReadPod(value);
			break;
		case JceStruct::TypeEnum::ZeroTag:
			value = 0;
			break;
		default:
			nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}.", static_cast<std::uint32_t>(head.Type))
			;
		}

		return true;
	}

	return false;
}

bool JceInputStream::doRead(std::uint32_t tag, double& value)
{
	if (SkipToTag(tag))
	{
		const auto [head, headSize] = ReadHead();
		switch (head.Type)
		{
		case JceStruct::TypeEnum::Float:
			value = m_Reader->ReadPod<float>();
			break;
		case JceStruct::TypeEnum::Double:
			m_Reader->ReadPod(value);
			break;
		case JceStruct::TypeEnum::ZeroTag:
			value = 0;
			break;
		default:
			nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}.", static_cast<std::uint32_t>(head.Type))
			;
		}

		return true;
	}

	return false;
}

bool JceInputStream::doRead(std::uint32_t tag, nString& value)
{
	if (SkipToTag(tag))
	{
		const auto [head, headSize] = ReadHead();
		std::size_t strSize;
		switch (head.Type)
		{
		case JceStruct::TypeEnum::String1:
			strSize = m_Reader->ReadPod<std::uint8_t>();
			break;
		case JceStruct::TypeEnum::String4:
			strSize = m_Reader->ReadPod<std::uint32_t>();
			if (strSize > JceStruct::MaxStringLength)
			{
				nat_Throw(JceDecodeException, u8"String too long, {0} sizes requested.", strSize);
			}
			break;
		default:
			nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}.", static_cast<std::uint32_t>(head.Type))
			;
		}

		// 为了异常安全，构造临时字符串而不是就地修改
		nString tmpString(0, strSize);
		m_Reader->GetUnderlyingStream()->ReadBytes(reinterpret_cast<nData>(tmpString.data()), strSize);
		value = std::move(tmpString);

		return true;
	}

	return false;
}

bool JceInputStream::doRead(std::uint32_t tag, gsl::span<std::uint8_t> const& value)
{
	if (SkipToTag(tag))
	{
		const auto[sizeField, sizeFieldSize] = ReadHead();
		if (sizeField.Type != JceStruct::TypeEnum::Byte)
		{
			nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}."_nv, static_cast<std::uint32_t>(sizeField.Type));
		}

		std::uint8_t size;
		if (!Read(0, size))
		{
			nat_Throw(JceDecodeException, u8"Read size failed."_nv);
		}

		if (static_cast<std::size_t>(value.size()) < size)
		{
			nat_Throw(JceDecodeException, u8"Span is not big enough."_nv);
		}

		m_Reader->GetUnderlyingStream()->ReadBytes(value.data(), size);

		return true;
	}

	return false;
}

JceOutputStream::JceOutputStream(natRefPointer<natBinaryWriter> writer)
	: m_Writer{ std::move(writer) }
{
	if (m_Writer->GetEndianness() != Environment::Endianness::LittleEndian)
	{
		nat_Throw(JceDecodeException, u8"writer should use little endian."_nv);
	}
}

JceOutputStream::~JceOutputStream()
{
}

natRefPointer<natBinaryWriter> JceOutputStream::GetWriter() const noexcept
{
	return m_Writer;
}

void JceOutputStream::WriteHead(HeadData head)
{
	if (head.Tag < 15)
	{
		m_Writer->WritePod(static_cast<std::uint8_t>((head.Tag << 4) | static_cast<std::uint8_t>(head.Type)));
	}
	else if (head.Tag < 256)
	{
		m_Writer->WritePod(static_cast<std::uint8_t>(static_cast<std::uint8_t>(head.Type) | 0xF0));
		m_Writer->WritePod(static_cast<std::uint8_t>(head.Tag));
	}
	else
	{
		nat_Throw(JceEncodeException, u8"Tag is too big({0}).", head.Tag);
	}
}

void JceOutputStream::doWrite(std::uint32_t tag, std::uint8_t value)
{
	if (!value)
	{
		WriteHead({ tag, JceStruct::TypeEnum::ZeroTag });
	}
	else
	{
		WriteHead({ tag, JceStruct::TypeEnum::Byte });
		m_Writer->WritePod(value);
	}
}

void JceOutputStream::doWrite(std::uint32_t tag, std::int16_t value)
{
	if (Utility::InRangeOf<std::int8_t>(value))
	{
		Write(tag, static_cast<std::uint8_t>(value));
	}
	else
	{
		WriteHead({ tag, JceStruct::TypeEnum::Short });
		m_Writer->WritePod(value);
	}
}

void JceOutputStream::doWrite(std::uint32_t tag, std::int32_t value)
{
	if (Utility::InRangeOf<std::int16_t>(value))
	{
		Write(tag, static_cast<std::int16_t>(value));
	}
	else
	{
		WriteHead({ tag, JceStruct::TypeEnum::Int });
		m_Writer->WritePod(value);
	}
}

void JceOutputStream::doWrite(std::uint32_t tag, std::int64_t value)
{
	if (Utility::InRangeOf<std::int32_t>(value))
	{
		Write(tag, static_cast<std::int32_t>(value));
	}
	else
	{
		WriteHead({ tag, JceStruct::TypeEnum::Long });
		m_Writer->WritePod(value);
	}
}

void JceOutputStream::doWrite(std::uint32_t tag, float value)
{
	WriteHead({ tag, JceStruct::TypeEnum::Float });
	m_Writer->WritePod(value);
}

void JceOutputStream::doWrite(std::uint32_t tag, double value)
{
	WriteHead({ tag, JceStruct::TypeEnum::Double });
	m_Writer->WritePod(value);
}

void JceOutputStream::doWrite(std::uint32_t tag, nStrView const& value)
{
	const auto strSize = value.size();
	if (strSize <= std::numeric_limits<std::uint8_t>::max())
	{
		WriteHead({ tag, JceStruct::TypeEnum::String1 });
		m_Writer->WritePod(static_cast<std::uint8_t>(strSize));
	}
	else
	{
		if (strSize > std::numeric_limits<std::uint32_t>::max())
		{
			nat_Throw(JceDecodeException, u8"String is too long({0} bytes).", strSize);
		}

		WriteHead({ tag, JceStruct::TypeEnum::String4 });
		m_Writer->WritePod(static_cast<std::uint32_t>(strSize));
	}
	m_Writer->GetUnderlyingStream()->WriteBytes(reinterpret_cast<ncData>(value.data()), strSize);
}

void JceOutputStream::doWrite(std::uint32_t tag, nString const& value)
{
	doWrite(tag, value.GetView());
}

void JceOutputStream::doWrite(std::uint32_t tag, gsl::span<const std::uint8_t> const& value)
{
	WriteHead({ tag, JceStruct::TypeEnum::SimpleList });
	WriteHead({ 0, JceStruct::TypeEnum::Byte });
	const auto size = value.size();
	Write(0, static_cast<std::int32_t>(size));
	m_Writer->GetUnderlyingStream()->WriteBytes(reinterpret_cast<ncData>(value.data()), size);
}

void JceOutputStream::doWrite(std::uint32_t tag, std::vector<std::uint8_t> const& value)
{
	doWrite(tag, gsl::make_span(value));
}

namespace
{
	template <template <typename> class Trait, typename T, typename Tuple>
	constexpr void ReinitializeIf(T& obj, Tuple&& args)
		noexcept((!Trait<T>::value && !std::tuple_size_v<std::remove_reference_t<Tuple>>) || (std::is_nothrow_destructible_v<T> &&
																							  noexcept(Utility::InitializeWithTuple(obj, std::forward<Tuple>(args)))))
	{
		if constexpr (Trait<T>::value || std::tuple_size_v<std::remove_reference_t<Tuple>>)
		{
			obj.T::~T();
			Utility::InitializeWithTuple(obj, std::forward<Tuple>(args));
		}
	}
}

#define JCE_STRUCT(name, alias) \
	name::name()\
	{

#define NO_OP Detail::None

#define DEFAULT_INITIALIZER(...) std::tuple(__VA_ARGS__)

#define FIELD(name, tag, type, ...) \
		::ReinitializeIf<Utility::ConcatTrait<std::is_class, std::negation>::Result>(m_##name, Utility::ReturnFirst<Utility::ConcatTrait<\
						Utility::ConcatTrait<Utility::RemoveCvRef, Utility::BindTrait<std::is_same,\
						Detail::NoneType>::Result>::Result, std::negation>::Result, std::tuple<>>(__VA_ARGS__));

#define END_JCE_STRUCT(name) \
	}

#include "JceStructDef.h"

#define JCE_STRUCT(name, alias) \
	name::~name()\
	{\
	}\
	\
	nStrView name::GetJceStructName() const noexcept\
	{\
		return u8 ## alias ## _nv;\
	}

#include "JceStructDef.h"
