#pragma once
#include <natBinary.h>
#include <optional>
#include "Utility.h"

namespace YumeBot::Jce
{
	DeclareException(JceException, NatsuLib::natException, u8"YumeBot::Jce::JceException");
	DeclareException(JceDecodeException, JceException, u8"YumeBot::Jce::JceDecodeException");
	DeclareException(JceEncodeException, JceException, u8"YumeBot::Jce::JceEncodeException");

	class JceStruct
		: public NatsuLib::natRefObj
	{
	public:
#define JCE_FIELD_TYPE(OP)\
	OP(Byte, 0x00, std::uint8_t)\
	OP(Short, 0x01, std::int16_t)\
	OP(Int, 0x02, std::int32_t)\
	OP(Long, 0x03, std::int64_t)\
	OP(Float, 0x04, float)\
	OP(Double, 0x05, double)\
	OP(String1, 0x06, std::string)\
	OP(String4, 0x07, std::string)\
	OP(Map, 0x08, std::unordered_map)\
	OP(List, 0x09, std::vector)\
	OP(StructBegin, 0x0A, NatsuLib::natRefPointer)\
	OP(StructEnd, 0x0B, void)\
	OP(ZeroTag, 0x0C, (std::integral_constant<std::size_t, 0>))\
	OP(SimpleList, 0x0D, std::vector<std::uint8_t>)

		enum class TypeEnum : std::uint8_t
		{
#define ENUM_OP(name, code, type) name = code,
			JCE_FIELD_TYPE(ENUM_OP)
#undef ENUM_OP
		};

		static constexpr std::size_t MaxStringLength = 0x06400000;

		~JceStruct();

		virtual std::string_view GetJceStructName() const noexcept = 0;
		virtual TypeEnum GetJceStructType() const noexcept = 0;
	};

	template <typename T>
	struct TlvDeserializer;

	template <typename T>
	struct TlvSerializer;

	struct NoOp
	{
		template <typename T>
		struct Apply
			: Utility::ResultType<T>
		{
		};
	};

	struct IsOptional
	{
		template <typename T>
		struct Apply
			: Utility::ResultType<std::optional<T>>
		{
		};
	};

	template <typename... Args>
	struct TemplateArgs
	{
		template <template <typename...> class Template>
		struct Apply
			: Utility::ResultType<Template<Args...>>
		{
		};
	};

	template <JceStruct::TypeEnum Type, typename AttributeSet>
	struct FieldTypeBuilder;

#define FIELD_TYPE_BUILDER_OP(name, code, type) \
	template <typename... Attributes>\
	struct FieldTypeBuilder<JceStruct::TypeEnum::name, std::tuple<Attributes...>>\
		: decltype(Utility::RecursiveApply<type, Attributes...>())\
	{\
	};

	JCE_FIELD_TYPE(FIELD_TYPE_BUILDER_OP)

#undef FIELD_TYPE_BUILDER_OP

	class JceInputStream
		: NatsuLib::noncopyable
	{
	public:
		struct HeadData
		{
			std::uint32_t Tag;
			std::uint8_t Type;
		};

		explicit JceInputStream(NatsuLib::natRefPointer<NatsuLib::natBinaryReader> reader);
		~JceInputStream();

		HeadData ReadHead() const
		{
			const auto byte = m_Reader->ReadPod<std::uint8_t>();
			const auto type = static_cast<std::uint8_t>(byte & 0x0F);
			auto tag = static_cast<std::uint32_t>((byte & 0xF0) >> 4);
			if (tag == 0x0F)
			{
				tag = m_Reader->ReadPod<std::uint8_t>();
			}

			return { tag, type };
		}

		HeadData PeekHead() const
		{
			const auto underlyingStream = m_Reader->GetUnderlyingStream();
			const auto pos = underlyingStream->GetPosition();
			const auto head = ReadHead();
			underlyingStream->SetPositionFromBegin(pos);
			return head;
		}

		void Skip(nLen len) const
		{
			m_Reader->Skip(len);
		}

		template <JceStruct::TypeEnum Type, typename T>
		bool Read(std::size_t tag, T& value, std::nullptr_t = nullptr)
		{
			return Reader<Type, T>::DoRead(*this, tag, value);
		}

		template <JceStruct::TypeEnum Type, typename T, typename U>
		void Read(std::size_t tag, T& value, U&& defaultValue)
		{
			if (!Reader<Type, T>::DoRead(*this, tag, value))
			{
				value = std::forward<U>(defaultValue);
			}
		}

	private:
		NatsuLib::natRefPointer<NatsuLib::natBinaryReader> m_Reader;

		template <JceStruct::TypeEnum Type, typename T, typename = void>
		struct Reader;

		template <JceStruct::TypeEnum Type, typename T>
		struct Reader<Type, T, std::enable_if_t<std::is_pod_v<T>>>
		{
			static bool DoRead(JceInputStream& self, std::size_t tag, T& value)
			{
				const auto head = self.ReadHead();
				if (static_cast<JceStruct::TypeEnum>(head.Type) != Type)
				{
					nat_Throw(JceDecodeException, u8"Type mismatch.");
				}

				self.m_Reader->ReadPod(value);
				return true;
			}
		};
	};

	class JceOutputStream
		: NatsuLib::noncopyable
	{
	public:
		explicit JceOutputStream(NatsuLib::natRefPointer<NatsuLib::natBinaryWriter> writer);
		~JceOutputStream();

		template <JceStruct::TypeEnum Type, typename T>
		void Write(std::size_t tag, T const& value)
		{
			Writer<Type, T>::DoWrite(*this, tag, value);
		}

		template <JceStruct::TypeEnum Type, typename T>
		void Write(std::size_t tag, std::optional<T> const& value)
		{
			if (value.has_value())
			{
				Write(tag, value.value());
			}
		}

	private:
		NatsuLib::natRefPointer<NatsuLib::natBinaryWriter> m_Writer;

		template <JceStruct::TypeEnum Type, typename T, typename = void>
		struct Writer;

		template <JceStruct::TypeEnum Type, typename T>
		struct Writer<Type, T, std::enable_if_t<std::is_pod_v<T>>>
		{
			static void DoWrite(JceOutputStream& self, std::size_t tag, T const& value)
			{
				
			}
		};
	};

	enum class Code
	{
#define TLV_CODE(name, code, ...) name = code,
#include "TlvCodeDef.h"
	};

#define NO_OP NoOp

#define IS_OPTIONAL(defaultValue) IsOptional

#define TEMPLATE_ARGUMENT(...) TemplateArgs<__VA_ARGS__>

#define FIELD(name, tag, type, ...) \
	public:\
		typename FieldTypeBuilder<JceStruct::TypeEnum::type, std::tuple<__VA_ARGS__>>::Type m_##name;\
		\
	public:\
		const auto& Get##name() const noexcept\
		{\
			return m_##name;\
		}\
		\
		auto& Get##name() noexcept\
		{\
			return m_##name;\
		}\
		\
		template <typename T>\
		void Set##name(T&& arg)\
		{\
			m_##name = std::forward<T>(arg);\
		}\
		\
		static constexpr std::size_t Get##name##Tag() noexcept\
		{\
			return tag;\
		}

#define TLV_CODE(name, code, ...) \
	class name\
		: public NatsuLib::natRefObjImpl<name, JceStruct>\
	{\
	public:\
		~name();\
		\
		std::string_view GetJceStructName() const noexcept override;\
		TypeEnum GetJceStructType() const noexcept override;\
		\
		__VA_ARGS__\
	};

#include "TlvCodeDef.h"

#define NO_OP nullptr

#define IS_OPTIONAL(defaultValue) defaultValue

#define FIELD(name, tag, type, ...) \
	{\
		using FieldType = Utility::RemoveCvRef<decltype(ret->Get##name())>;\
		stream.Read<JceStruct::TypeEnum::type>(tag, ret->Get##name(),\
			Utility::ReturnFirst<Utility::ConcatTrait<Utility::BindTrait<std::is_same, std::nullptr_t>::template Result, std::negation>::template Result, std::nullptr_t>(__VA_ARGS__));\
	}

#define TLV_CODE(name, code, ...) \
	template <>\
	struct TlvDeserializer<name>\
	{\
		static NatsuLib::natRefPointer<name> Deserialize(JceInputStream& stream)\
		{\
			auto ret = NatsuLib::make_ref<name>();\
			__VA_ARGS__\
			return ret;\
		}\
	};

#include "TlvCodeDef.h"

#define FIELD(name, tag, type, ...) stream->Write<JceStruct::TypeEnum::type>(tag, value->Get##name());

#define TLV_CODE(name, code, ...) \
	template <>\
	struct TlvSerializer<name>\
	{\
		static void Serialize(JceOutputStream& stream, NatsuLib::natRefPointer<name> const& value)\
		{\
			__VA_ARGS__\
		}\
	};

#include "TlvCodeDef.h"

}
