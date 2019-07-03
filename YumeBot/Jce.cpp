#include "Jce.h"

using namespace Cafe::Encoding::StringLiterals;
using namespace YumeBot;
using namespace Jce;

JceStruct::~JceStruct()
{
}

JceInputStream::JceInputStream(Cafe::Io::InputStream* stream)
    : m_Reader{ stream, std::endian::little }
{
}

Cafe::Io::BinaryReader& JceInputStream::GetReader() noexcept
{
	return m_Reader;
}

std::pair<HeadData, std::size_t> JceInputStream::ReadHead() const
{
	const auto byte = m_Reader.Read<std::uint8_t>();
	if (!byte)
	{
		CAFE_THROW(JceDecodeException, u8"Read failed."_sv);
	}
	const auto byteValue = *byte;
	const auto type = static_cast<JceStruct::TypeEnum>(static_cast<std::uint8_t>(byteValue & 0x0F));
	const auto tag = static_cast<std::uint32_t>((byteValue & 0xF0) >> 4);

	if (tag != 0x0F)
	{
		return { { tag, type }, 1 };
	}

	return { { *m_Reader.Read<std::uint8_t>(), type }, 2 };
}

std::pair<HeadData, std::size_t> JceInputStream::PeekHead() const
{
	const auto underlyingStream = dynamic_cast<Cafe::Io::SeekableStreamBase*>(m_Reader.GetStream());
	assert(underlyingStream);
	const auto pos = underlyingStream->GetPosition();
	const auto head = ReadHead();
	underlyingStream->SeekFromBegin(pos);
	return head;
}

void JceInputStream::Skip(std::size_t len)
{
	m_Reader.GetStream()->Skip(len);
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
		Skip(*m_Reader.Read<std::uint8_t>());
		break;
	case JceStruct::TypeEnum::String4:
		Skip(*m_Reader.Read<std::uint32_t>());
		break;
	case JceStruct::TypeEnum::Map:
	{
		int size;
		if (!Read(0, size))
		{
			CAFE_THROW(JceDecodeException, u8"Read size failed."_sv);
		}
		for (std::size_t i = 0, iterationTime = static_cast<std::size_t>(size) * 2; i < iterationTime;
		     ++i)
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
			CAFE_THROW(JceDecodeException, u8"Read size failed."_sv);
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
			CAFE_THROW(JceDecodeException,
			           Cafe::TextUtils::FormatString(u8"Type mismatch, expected 0, got ${0}"_sv,
			                                         static_cast<std::uint32_t>(head.Type)));
		}
		std::uint8_t size;
		if (!Read(0, size))
		{
			CAFE_THROW(JceDecodeException, u8"Read size failed."_sv);
		}
		Skip(size);
		break;
	}
	default:
		CAFE_THROW(JceDecodeException, Cafe::TextUtils::FormatString(u8"Invalid type (${0})."_sv,
		                                                             static_cast<std::uint32_t>(type)));
	}
}

bool JceInputStream::SkipToTag(std::uint32_t tag)
try
{
	HeadData head; // NOLINT
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
			value = *m_Reader.Read<std::uint8_t>();
			break;
		case JceStruct::TypeEnum::ZeroTag:
			value = 0;
			break;
		default:
			CAFE_THROW(JceDecodeException,
			           Cafe::TextUtils::FormatString(u8"Type mismatch, got unexpected ${0}."_sv,
			                                         static_cast<std::uint32_t>(head.Type)));
		}

		return true;
	}

	return false;
}

bool JceInputStream::doRead(std::uint32_t tag, std::byte& value)
{
	std::uint8_t v;
	if (!doRead(tag, v))
	{
		return false;
	}

	value = static_cast<std::byte>(v);
	return true;
}

bool JceInputStream::doRead(std::uint32_t tag, std::int16_t& value)
{
	if (SkipToTag(tag))
	{
		const auto [head, headSize] = ReadHead();
		switch (head.Type)
		{
		case JceStruct::TypeEnum::Byte:
			value = *m_Reader.Read<std::uint8_t>();
			break;
		case JceStruct::TypeEnum::Short:
			value = *m_Reader.Read<std::int16_t>();
			break;
		case JceStruct::TypeEnum::ZeroTag:
			value = 0;
			break;
		default:
			CAFE_THROW(JceDecodeException,
			           Cafe::TextUtils::FormatString(u8"Type mismatch, got unexpected ${0}."_sv,
			                                         static_cast<std::uint32_t>(head.Type)));
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
			value = *m_Reader.Read<std::uint8_t>();
			break;
		case JceStruct::TypeEnum::Short:
			value = *m_Reader.Read<std::int16_t>();
			break;
		case JceStruct::TypeEnum::Int:
			value = *m_Reader.Read<std::int32_t>();
			break;
		case JceStruct::TypeEnum::ZeroTag:
			value = 0;
			break;
		default:
			CAFE_THROW(JceDecodeException,
			           Cafe::TextUtils::FormatString(u8"Type mismatch, got unexpected ${0}."_sv,
			                                         static_cast<std::uint32_t>(head.Type)));
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
			value = *m_Reader.Read<std::uint8_t>();
			break;
		case JceStruct::TypeEnum::Short:
			value = *m_Reader.Read<std::int16_t>();
			break;
		case JceStruct::TypeEnum::Int:
			value = *m_Reader.Read<std::int32_t>();
			break;
		case JceStruct::TypeEnum::Long:
			value = *m_Reader.Read<std::int64_t>();
			break;
		case JceStruct::TypeEnum::ZeroTag:
			value = 0;
			break;
		default:
			CAFE_THROW(JceDecodeException,
			           Cafe::TextUtils::FormatString(u8"Type mismatch, got unexpected ${0}"_sv,
			                                         static_cast<std::uint32_t>(head.Type)));
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
			value = *m_Reader.Read<float>();
			break;
		case JceStruct::TypeEnum::ZeroTag:
			value = 0;
			break;
		default:
			CAFE_THROW(JceDecodeException,
			           Cafe::TextUtils::FormatString(u8"Type mismatch, got unexpected ${0}."_sv,
			                                         static_cast<std::uint32_t>(head.Type)));
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
			value = *m_Reader.Read<float>();
			break;
		case JceStruct::TypeEnum::Double:
			value = *m_Reader.Read<double>();
			break;
		case JceStruct::TypeEnum::ZeroTag:
			value = 0;
			break;
		default:
			CAFE_THROW(JceDecodeException,
			           Cafe::TextUtils::FormatString(u8"Type mismatch, got unexpected ${0}."_sv,
			                                         static_cast<std::uint32_t>(head.Type)));
		}

		return true;
	}

	return false;
}

bool JceInputStream::doRead(std::uint32_t tag, UsingString& value)
{
	if (SkipToTag(tag))
	{
		const auto [head, headSize] = ReadHead();
		std::size_t strSize;
		switch (head.Type)
		{
		case JceStruct::TypeEnum::String1:
			strSize = *m_Reader.Read<std::uint8_t>();
			break;
		case JceStruct::TypeEnum::String4:
			strSize = *m_Reader.Read<std::uint32_t>();
			if (strSize > JceStruct::MaxStringLength)
			{
				CAFE_THROW(JceDecodeException, Cafe::TextUtils::FormatString(
				                                   u8"String too long, ${0} sizes requested."_sv, strSize));
			}
			break;
		default:
			CAFE_THROW(JceDecodeException,
			           Cafe::TextUtils::FormatString(u8"Type mismatch, got unexpected ${0}."_sv,
			                                         static_cast<std::uint32_t>(head.Type)));
		}

		// 为了异常安全，构造临时字符串而不是就地修改
		UsingString tmpString;
		tmpString.Resize(strSize + 1);
		m_Reader.GetStream()->ReadBytes(
		    gsl::as_writeable_bytes(gsl::make_span(tmpString.GetData(), strSize)));
		value = std::move(tmpString);

		return true;
	}

	return false;
}

bool JceInputStream::doRead(std::uint32_t tag, gsl::span<std::uint8_t> const& value)
{
	return doRead(tag, gsl::as_writeable_bytes(value));
}

bool JceInputStream::doRead(std::uint32_t tag, gsl::span<std::byte> const& value)
{
	if (SkipToTag(tag))
	{
		const auto [sizeField, sizeFieldSize] = ReadHead();
		if (sizeField.Type != JceStruct::TypeEnum::Byte)
		{
			CAFE_THROW(JceDecodeException,
			           Cafe::TextUtils::FormatString(u8"Type mismatch, got unexpected ${0}."_sv,
			                                         static_cast<std::uint32_t>(sizeField.Type)));
		}

		std::uint8_t size;
		if (!Read(0, size))
		{
			CAFE_THROW(JceDecodeException, u8"Read size failed."_sv);
		}

		if (static_cast<std::size_t>(value.size()) < size)
		{
			CAFE_THROW(JceDecodeException, u8"Span is not big enough."_sv);
		}

		m_Reader.GetStream()->ReadBytes(value);

		return true;
	}

	return false;
}

JceOutputStream::JceOutputStream(Cafe::Io::OutputStream* stream)
    : m_Writer{ stream, std::endian::little }
{
}

Cafe::Io::BinaryWriter& JceOutputStream::GetWriter() noexcept
{
	return m_Writer;
}

void JceOutputStream::WriteHead(HeadData head)
{
	if (head.Tag < 15)
	{
		m_Writer.Write(
		    static_cast<std::uint8_t>((head.Tag << 4) | static_cast<std::uint8_t>(head.Type)));
	}
	else if (head.Tag < 256)
	{
		m_Writer.Write(static_cast<std::uint8_t>(static_cast<std::uint8_t>(head.Type) | 0xF0));
		m_Writer.Write(static_cast<std::uint8_t>(head.Tag));
	}
	else
	{
		CAFE_THROW(JceEncodeException,
		           Cafe::TextUtils::FormatString(u8"Tag is too big(${0})."_sv, head.Tag));
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
		m_Writer.Write(value);
	}
}

void JceOutputStream::doWrite(std::uint32_t tag, std::byte value)
{
	doWrite(tag, static_cast<std::uint8_t>(value));
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
		m_Writer.Write(value);
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
		m_Writer.Write(value);
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
		m_Writer.Write(value);
	}
}

void JceOutputStream::doWrite(std::uint32_t tag, float value)
{
	WriteHead({ tag, JceStruct::TypeEnum::Float });
	m_Writer.Write(value);
}

void JceOutputStream::doWrite(std::uint32_t tag, double value)
{
	WriteHead({ tag, JceStruct::TypeEnum::Double });
	m_Writer.Write(value);
}

void JceOutputStream::doWrite(std::uint32_t tag, UsingStringView const& value)
{
	const auto valueToWrite = value.Trim();
	const auto strSize = valueToWrite.size();
	if (strSize <= std::numeric_limits<std::uint8_t>::max())
	{
		WriteHead({ tag, JceStruct::TypeEnum::String1 });
		m_Writer.Write(static_cast<std::uint8_t>(strSize));
	}
	else
	{
		if (strSize > std::numeric_limits<std::uint32_t>::max())
		{
			CAFE_THROW(JceDecodeException,
			           Cafe::TextUtils::FormatString(u8"String is too long(${0} bytes)."_sv, strSize));
		}

		WriteHead({ tag, JceStruct::TypeEnum::String4 });
		m_Writer.Write(static_cast<std::uint32_t>(strSize));
	}
	m_Writer.GetStream()->WriteBytes(gsl::as_bytes(gsl::make_span(valueToWrite.GetData(), strSize)));
}

void JceOutputStream::doWrite(std::uint32_t tag, UsingString const& value)
{
	doWrite(tag, value.GetView());
}

void JceOutputStream::doWrite(std::uint32_t tag, gsl::span<const std::uint8_t> const& value)
{
	doWrite(tag, gsl::as_bytes(value));
}

void JceOutputStream::doWrite(std::uint32_t tag, gsl::span<const std::byte> const& value)
{
	WriteHead({ tag, JceStruct::TypeEnum::SimpleList });
	WriteHead({ 0, JceStruct::TypeEnum::Byte });
	const auto size = value.size();
	Write(0, static_cast<std::int32_t>(size));
	m_Writer.GetStream()->WriteBytes(value);
}

void JceOutputStream::doWrite(std::uint32_t tag, std::vector<std::byte> const& value)
{
	doWrite(tag, gsl::make_span(value));
}

namespace
{
	template <template <typename> class Trait, typename T, typename Tuple>
	constexpr void ReinitializeIf(T& obj, Tuple&& args) noexcept(
	    (!Trait<T>::value && !std::tuple_size_v<std::remove_reference_t<Tuple>>) ||
	    (std::is_nothrow_destructible_v<T> &&
	     noexcept(Utility::InitializeWithTuple(obj, std::forward<Tuple>(args)))))
	{
		if constexpr (Trait<T>::value || std::tuple_size_v<std::remove_reference_t<Tuple>>)
		{
			obj.T::~T();
			Utility::InitializeWithTuple(obj, std::forward<Tuple>(args));
		}
	}
} // namespace

#define JCE_STRUCT(name, alias)                                                                    \
	name::name()                                                                                     \
	{

#define NO_OP Detail::None

#define DEFAULT_INITIALIZER(...) std::tuple(__VA_ARGS__)

#define FIELD(name, tag, type, ...)                                                                \
	{                                                                                                \
		using FieldType =                                                                              \
		    typename Utility::MayRemoveTemplate<Utility::RemoveCvRef<decltype(m_##name)>,              \
		                                        std::optional>::Type;                                  \
		::ReinitializeIf<Utility::ConcatTrait<std::is_class, std::negation>::Result>(                  \
		    m_##name, Utility::ReturnFirst<                                                            \
		                  Utility::ConcatTrait<                                                        \
		                      Utility::ConcatTrait<                                                    \
		                          Utility::RemoveCvRef,                                                \
		                          Utility::BindTrait<std::is_same, Detail::NoneType>::Result>::Result, \
		                      std::negation>::Result,                                                  \
		                  std::tuple<>>(__VA_ARGS__));                                                 \
	}

#define END_JCE_STRUCT(name) }

#include "JceStructDef.h"

#define JCE_STRUCT(name, alias)                                                                    \
	name::~name()                                                                                    \
	{                                                                                                \
	}                                                                                                \
                                                                                                   \
	UsingStringView name::GetJceStructName() const noexcept                                          \
	{                                                                                                \
		return u8##alias##_sv;                                                                         \
	}

#include "JceStructDef.h"
