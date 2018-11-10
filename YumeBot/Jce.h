#pragma once
#include <natBinary.h>
#include <optional>
#include <string_view>
#include "Utility.h"

namespace YumeBot::Jce
{
	DeclareException(JceException, NatsuLib::natException, u8"YumeBot::Jce::JceException");
	DeclareException(JceDecodeException, JceException, u8"YumeBot::Jce::JceDecodeException");
	DeclareException(JceEncodeException, JceException, u8"YumeBot::Jce::JceEncodeException");

	static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559, "Jce assumed float and double fulfill the requirements of IEEE 754(IEC 559) standard.");
	static_assert(nString::UsingStringType == NatsuLib::StringType::Utf8, "Encoding should be utf-8.");

	// Jce 中 Byte 是有符号的，此处表示为无符号，使用时需注意
	class JceStruct
		: public NatsuLib::natRefObj
	{
	public:
#define JCE_FIELD_TYPE(OP) \
	OP(Byte, 0x00, std::uint8_t)\
	OP(Short, 0x01, std::int16_t)\
	OP(Int, 0x02, std::int32_t)\
	OP(Long, 0x03, std::int64_t)\
	OP(Float, 0x04, float)\
	OP(Double, 0x05, double)\
	OP(String1, 0x06, nString)\
	OP(String4, 0x07, nString)\
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

		static constexpr nStrView GetTypeString(TypeEnum type) noexcept
		{
			using namespace NatsuLib::StringLiterals;
			switch (type)
			{
#define STRINGIFY_OP(name, code, type) \
			case TypeEnum::name:\
				return u8 ## #name ## _nv;
			JCE_FIELD_TYPE(STRINGIFY_OP)
#undef STRINGIFY_OP
			default:
				return u8"UnknownType"_nv;
			}
		}

		static constexpr std::size_t MaxStringLength = 0x06400000;

		~JceStruct();

		[[nodiscard]] virtual nStrView GetJceStructName() const noexcept = 0;
	};

	template <typename T>
	struct JceDeserializer;

	template <typename T>
	struct JceSerializer;

	namespace Detail
	{
		struct NoneType
		{
		};

		constexpr NoneType None{};
	}

	struct HeadData
	{
		std::uint32_t Tag;
		JceStruct::TypeEnum Type;
	};

	class JceInputStream
		: NatsuLib::noncopyable
	{
	public:
		explicit JceInputStream(NatsuLib::natRefPointer<NatsuLib::natBinaryReader> reader);
		~JceInputStream();

		std::pair<HeadData, std::size_t> ReadHead() const;
		std::pair<HeadData, std::size_t> PeekHead() const;

		void Skip(nLen len);
		void SkipToStructEnd();
		void SkipField();
		void SkipField(JceStruct::TypeEnum type);
		bool SkipToTag(std::uint32_t tag);

		template <typename T>
		[[nodiscard]] bool Read(std::uint32_t tag, T& value, Detail::NoneType = Detail::None)
		{
			return doRead(tag, value);
		}

		template <typename T, typename U>
		std::enable_if_t<std::is_assignable_v<T&, U&&>, std::true_type> Read(std::uint32_t tag, T& value, U&& defaultValue)
		{
			bool readSucceed;
			if constexpr (Utility::IsTemplateOf<T, std::optional>::value)
			{
				value.emplace();
				readSucceed = Read(tag, value.value());
			}
			else
			{
				readSucceed = Read(tag, value);
			}

			if (!readSucceed)
			{
				value = std::forward<U>(defaultValue);
			}

			return {};
		}

	private:
		NatsuLib::natRefPointer<NatsuLib::natBinaryReader> m_Reader;

		bool doRead(std::uint32_t tag, std::uint8_t& value);
		bool doRead(std::uint32_t tag, std::int16_t& value);
		bool doRead(std::uint32_t tag, std::int32_t& value);
		bool doRead(std::uint32_t tag, std::int64_t& value);
		bool doRead(std::uint32_t tag, float& value);
		bool doRead(std::uint32_t tag, double& value);
		bool doRead(std::uint32_t tag, nString& value);

		template <typename Key, typename Value>
		bool doRead(std::uint32_t tag, std::unordered_map<Key, Value>& value)
		{
			if (SkipToTag(tag))
			{
				const auto[head, headSize] = ReadHead();
				switch (head.Type)
				{
				case JceStruct::TypeEnum::Map:
				{
					std::int32_t size;
					if (!Read(0, size))
					{
						nat_Throw(JceDecodeException, u8"Read size failed.");
					}
					if (size < 0)
					{
						nat_Throw(JceDecodeException, u8"Invalid size({0}).", size);
					}

					// 为了异常安全，构造临时 map 而不是就地修改
					std::unordered_map<Key, Value> tmpMap;

					for (std::size_t i = 0; i < static_cast<std::size_t>(size); ++i)
					{
						Key entryKey;
						if (!Read(0, entryKey))
						{
							nat_Throw(JceDecodeException, u8"Read key failed.");
						}
						Value entryValue;
						if (!Read(1, entryValue))
						{
							nat_Throw(JceDecodeException, u8"Read value failed.");
						}
						tmpMap.emplace(std::move(entryKey), std::move(entryValue));
					}

					value = std::move(tmpMap);
					break;
				}
				default:
					nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}.", static_cast<std::uint32_t>(head.Type));
				}

				return true;
			}

			return false;
		}

		template <typename T>
		bool doRead(std::uint32_t tag, std::vector<T>& value)
		{
			if (SkipToTag(tag))
			{
				const auto[head, headSize] = ReadHead();
				switch (head.Type)
				{
				case JceStruct::TypeEnum::List:
				{
					std::int32_t size;
					if (!Read(0, size))
					{
						nat_Throw(JceDecodeException, u8"Read size failed.");
					}

					// 为了异常安全，构造临时 vector 而不是就地修改
					std::vector<T> tmpList;
					tmpList.reserve(size);

					if (size > 0)
					{
						for (std::size_t i = 0; i < static_cast<std::size_t>(size); ++i)
						{
							T elemValue;
							if (!Read(0, elemValue))
							{
								nat_Throw(JceDecodeException, u8"Read element failed.");
							}
							tmpList.emplace_back(std::move(elemValue));
						}
					}

					value = std::move(tmpList);

					break;
				}
				case JceStruct::TypeEnum::SimpleList:
					if constexpr (std::is_same_v<T, std::uint8_t>)
					{
						const auto[sizeField, sizeFieldSize] = ReadHead();
						if (sizeField.Type != JceStruct::TypeEnum::Byte)
						{
							nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}.", static_cast<std::uint32_t>(sizeField.Type));
						}

						std::uint8_t size;
						if (!Read(0, size))
						{
							nat_Throw(JceDecodeException, u8"Read size failed.");
						}

						std::vector<std::uint8_t> tmpList(static_cast<std::size_t>(size));
						m_Reader->GetUnderlyingStream()->ReadBytes(tmpList.data(), size);

						value = std::move(tmpList);

						break;
					}
					else
					{
						[[fallthrough]];
					}
				default:
					nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}.", static_cast<std::uint32_t>(head.Type));
				}
				return true;
			}

			return false;
		}

		template <typename T>
		bool doRead(std::uint32_t tag, NatsuLib::natRefPointer<T>& value)
		{
			if (SkipToTag(tag))
			{
				const auto[head, headSize] = ReadHead();
				if (head.Type != JceStruct::TypeEnum::StructBegin)
				{
					nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}.", static_cast<std::uint32_t>(head.Type));
				}

				value = JceDeserializer<T>::Deserialize(*this);

				SkipToStructEnd();

				return true;
			}

			return false;
		}
	};

	class JceOutputStream
		: NatsuLib::noncopyable
	{
	public:
		explicit JceOutputStream(NatsuLib::natRefPointer<NatsuLib::natBinaryWriter> writer);
		~JceOutputStream();

		[[nodiscard]] NatsuLib::natRefPointer<NatsuLib::natBinaryWriter> GetWriter() const noexcept;

		void WriteHead(HeadData head);

		template <typename T>
		void Write(std::uint32_t tag, T const& value)
		{
			doWrite(tag, value);
		}

		template <typename T>
		void Write(std::uint32_t tag, std::optional<T> const& value)
		{
			if (value.has_value())
			{
				Write(tag, value.value());
			}
		}

		template <typename T>
		void Write(std::uint32_t tag, NatsuLib::natRefPointer<T> const& value)
		{
			if (value)
			{
				doWrite(tag, value);
			}
		}

	private:
		NatsuLib::natRefPointer<NatsuLib::natBinaryWriter> m_Writer;

		void doWrite(std::uint32_t tag, std::uint8_t value);
		void doWrite(std::uint32_t tag, std::int16_t value);
		void doWrite(std::uint32_t tag, std::int32_t value);
		void doWrite(std::uint32_t tag, std::int64_t value);
		void doWrite(std::uint32_t tag, float value);
		void doWrite(std::uint32_t tag, double value);
		void doWrite(std::uint32_t tag, nStrView const& value);
		void doWrite(std::uint32_t tag, nString const& value);

		template <typename Key, typename Value>
		void doWrite(std::uint32_t tag, std::unordered_map<Key, Value> const& value)
		{
			WriteHead({ tag, JceStruct::TypeEnum::Map });
			Write(0, static_cast<std::int32_t>(value.size()));
			for (const auto& item : value)
			{
				Write(0, item.first);
				Write(1, item.second);
			}
		}

		void doWrite(std::uint32_t tag, std::vector<std::uint8_t> const& value);

		template <typename T>
		void doWrite(std::uint32_t tag, std::vector<T> const& value)
		{
			WriteHead({ tag, JceStruct::TypeEnum::List });
			Write(0, static_cast<std::int32_t>(value.size()));
			for (const auto& item : value)
			{
				Write(0, item);
			}
		}

		template <typename T>
		void doWrite(std::uint32_t tag, NatsuLib::natRefPointer<T> const& value)
		{
			WriteHead({ tag, JceStruct::TypeEnum::StructBegin });
			JceSerializer<T>::Serialize(*this, value);
			WriteHead({ 0, JceStruct::TypeEnum::StructEnd });
		}
	};

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
			: Utility::ResultType<std::conditional_t<Utility::IsTemplateOf<T, NatsuLib::natRefPointer>::value, T, std::optional<T>>>
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

#define NO_OP NoOp

#define IS_OPTIONAL(defaultValue) IsOptional

#define TEMPLATE_ARGUMENT(...) TemplateArgs<__VA_ARGS__>

#define FIELD(name, tag, type, ...) \
	private:\
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

#define JCE_STRUCT(name, alias) \
	class name\
		: public NatsuLib::natRefObjImpl<name, JceStruct>\
	{\
	public:\
		~name();\
		\
		nStrView GetJceStructName() const noexcept override;

#define END_JCE_STRUCT(name) \
	};

#include "JceStructDef.h"


#define NO_OP Detail::None

#define IS_OPTIONAL(defaultValue) defaultValue

// 读取 optional 的时候不会返回 false
#define FIELD(name, tag, type, ...) \
			{\
				using FieldType = typename Utility::MayRemoveTemplate<Utility::RemoveCvRef<decltype(ret->Get##name())>, std::optional>::Type;\
				if (!stream.Read(tag, ret->Get##name(),\
					Utility::ReturnFirst<Utility::ConcatTrait<Utility::ConcatTrait<Utility::RemoveCvRef, Utility::BindTrait<std::is_same,\
						Detail::NoneType>::Result>::Result, std::negation>::Result, Detail::NoneType>(__VA_ARGS__)))\
				{\
					nat_Throw(JceDecodeException, u8"Deserializing failed : Failed to read field \"" #name "\" which is not optional.");\
				}\
			}

#define JCE_STRUCT(name, alias) \
	template <>\
	struct JceDeserializer<name>\
	{\
		static NatsuLib::natRefPointer<name> Deserialize(JceInputStream& stream)\
		{\
			auto ret = NatsuLib::make_ref<name>();

#define END_JCE_STRUCT(name) \
			return ret;\
		}\
	};

#include "JceStructDef.h"


#define FIELD(name, tag, type, ...) stream.Write(tag, value->Get##name());

#define JCE_STRUCT(name, alias) \
	template <>\
	struct JceSerializer<name>\
	{\
		static void Serialize(JceOutputStream& stream, NatsuLib::natRefPointer<name> const& value)\
		{

#define END_JCE_STRUCT(name) \
		}\
	};

#include "JceStructDef.h"

}
