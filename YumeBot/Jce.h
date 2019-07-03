#pragma once

#include "Misc.h"
#include "Utility.h"
#include <Cafe/Encoding/CodePage/UTF-8.h>
#include <Cafe/ErrorHandling/ErrorHandling.h>
#include <Cafe/Io/StreamHelpers/BinaryReader.h>
#include <Cafe/Io/StreamHelpers/BinaryWriter.h>
#include <Cafe/Misc/Scope.h>
#include <Cafe/TextUtils/Format.h>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace YumeBot::Jce
{
	CAFE_DEFINE_GENERAL_EXCEPTION(JceException);
	CAFE_DEFINE_GENERAL_EXCEPTION(JceDecodeException, JceException);
	CAFE_DEFINE_GENERAL_EXCEPTION(JceEncodeException, JceException);

	// Jce 中 Byte 是有符号的，此处表示为无符号，使用时需注意
	// String 均不包含结尾空字符且编码不明，猜测为 GB2312，需注意
	class JceStruct
	{
	public:
#define JCE_FIELD_TYPE(OP)                                                                         \
	OP(Byte, 0x00, std::uint8_t)                                                                     \
	OP(Short, 0x01, std::int16_t)                                                                    \
	OP(Int, 0x02, std::int32_t)                                                                      \
	OP(Long, 0x03, std::int64_t)                                                                     \
	OP(Float, 0x04, float)                                                                           \
	OP(Double, 0x05, double)                                                                         \
	OP(String1, 0x06, UsingString)                                                                   \
	OP(String4, 0x07, UsingString)                                                                   \
	OP(Map, 0x08, std::unordered_map)                                                                \
	OP(List, 0x09, std::vector)                                                                      \
	OP(StructBegin, 0x0A, std::shared_ptr)                                                           \
	OP(StructEnd, 0x0B, void)                                                                        \
	OP(ZeroTag, 0x0C, CAFE_ID(std::integral_constant<std::size_t, 0>))                               \
	OP(SimpleList, 0x0D, std::vector<std::byte>)

		enum class TypeEnum : std::uint8_t
		{
#define ENUM_OP(name, code, type) name = code,
			JCE_FIELD_TYPE(ENUM_OP)
#undef ENUM_OP
		};

		static constexpr UsingStringView GetTypeString(TypeEnum type) noexcept
		{
			using namespace Cafe::Encoding::StringLiterals;
			switch (type)
			{
#define STRINGIFY_OP(name, code, type)                                                             \
	case TypeEnum::name:                                                                             \
		return CAFE_UTF8_SV(#name);
				JCE_FIELD_TYPE(STRINGIFY_OP)
#undef STRINGIFY_OP
			default:
				return CAFE_UTF8_SV("UnknownType");
			}
		}

		static constexpr std::size_t MaxStringLength = 0x06400000;

		virtual ~JceStruct();

		[[nodiscard]] virtual UsingStringView GetJceStructName() const noexcept = 0;
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
	} // namespace Detail

	struct HeadData
	{
		std::uint32_t Tag;
		JceStruct::TypeEnum Type;
	};

	class JceInputStream
	{
	public:
		explicit JceInputStream(Cafe::Io::InputStream* stream);

		[[nodiscard]] Cafe::Io::BinaryReader& GetReader() noexcept;

		std::pair<HeadData, std::size_t> ReadHead() const;
		std::pair<HeadData, std::size_t> PeekHead() const;

		void Skip(std::size_t len);
		void SkipToStructEnd();
		void SkipField();
		void SkipField(JceStruct::TypeEnum type);
		[[nodiscard]] bool SkipToTag(std::uint32_t tag);

		///	@brief	以指定的 tag 读取值，读取可能失败
		///	@param	tag		指定 tag
		///	@param	value	要写入的值
		///	@return	读取是否成功
		///	@remark 对于 JceStruct，若传入的引用指针为 const
		///限定的，则直接就地修改，否则将总是会创建新的实例并写入
		///         这是由于新的 JceStruct
		///         总是默认将引用指针初始化为空，而实际中未必总是需要新的实例引发的问题 若传入
		///         JceStruct 派生的实例，也将就地修改
		template <typename T>
		[[nodiscard]] bool Read(std::uint32_t tag, T& value, Detail::NoneType = Detail::None)
		{
			const auto underlyingStream =
			    dynamic_cast<Cafe::Io::SeekableStreamBase*>(m_Reader.GetStream());
			assert(underlyingStream);
			const auto currentPos = underlyingStream->GetPosition();
			CAFE_SCOPE_FAIL
			{
				underlyingStream->SeekFromBegin(currentPos);
			};

			return doRead(tag, value);
		}

		///	@brief	以指定的 tag 读取值，若失败会用默认值赋值
		///	@param	tag				指定的 tag
		///	@param	value			要写入的值
		///	@param	defaultValue	失败时写入的默认值
		///	@remark	返回 std::true_type 是为了代码生成时不需要写额外代码
		template <typename T, typename U>
		std::enable_if_t<std::is_assignable_v<T&, U&&>, std::true_type> Read(std::uint32_t tag,
		                                                                     T& value, U&& defaultValue)
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
		Cafe::Io::BinaryReader m_Reader;

		bool doRead(std::uint32_t tag, std::uint8_t& value);
		bool doRead(std::uint32_t tag, std::byte& value);
		bool doRead(std::uint32_t tag, std::int16_t& value);
		bool doRead(std::uint32_t tag, std::int32_t& value);
		bool doRead(std::uint32_t tag, std::int64_t& value);
		bool doRead(std::uint32_t tag, float& value);
		bool doRead(std::uint32_t tag, double& value);
		bool doRead(std::uint32_t tag, UsingString& value);

		template <typename Key, typename Value>
		bool doRead(std::uint32_t tag, std::unordered_map<Key, Value>& value)
		{
			if (SkipToTag(tag))
			{
				const auto [head, headSize] = ReadHead();
				switch (head.Type)
				{
				case JceStruct::TypeEnum::Map:
				{
					std::int32_t size;
					if (!Read(0, size))
					{
						CAFE_THROW(JceDecodeException, CAFE_UTF8_SV("Read size failed."));
					}
					if (size < 0)
					{
						CAFE_THROW(JceDecodeException,
						           Cafe::TextUtils::FormatString(CAFE_UTF8_SV("Invalid size(${0})."), size));
					}

					// 为了异常安全，构造临时 map 而不是就地修改
					std::unordered_map<Key, Value> tmpMap;

					for (std::size_t i = 0; i < static_cast<std::size_t>(size); ++i)
					{
						Key entryKey;
						if (!Read(0, entryKey))
						{
							CAFE_THROW(JceDecodeException, CAFE_UTF8_SV("Read key failed."));
						}
						Value entryValue;
						if (!Read(1, entryValue))
						{
							CAFE_THROW(JceDecodeException, CAFE_UTF8_SV("Read value failed."));
						}
						tmpMap.emplace(std::move(entryKey), std::move(entryValue));
					}

					value = std::move(tmpMap);
					break;
				}
				default:
					CAFE_THROW(JceDecodeException, Cafe::TextUtils::FormatString(
					                                   CAFE_UTF8_SV("Type mismatch, got unexpected ${0}."),
					                                   static_cast<std::uint32_t>(head.Type)));
				}

				return true;
			}

			return false;
		}

		bool doRead(std::uint32_t tag, gsl::span<std::uint8_t> const& value);
		bool doRead(std::uint32_t tag, gsl::span<std::byte> const& value);

		template <typename T>
		bool doRead(std::uint32_t tag, std::vector<T>& value)
		{
			if (SkipToTag(tag))
			{
				const auto [head, headSize] = ReadHead();
				switch (head.Type)
				{
				case JceStruct::TypeEnum::List:
				{
					std::int32_t size;
					if (!Read(0, size))
					{
						CAFE_THROW(JceDecodeException, CAFE_UTF8_SV("Read size failed."));
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
								CAFE_THROW(JceDecodeException, CAFE_UTF8_SV("Read element failed."));
							}
							tmpList.emplace_back(std::move(elemValue));
						}
					}

					value = std::move(tmpList);

					break;
				}
				case JceStruct::TypeEnum::SimpleList:
					if constexpr (std::is_same_v<T, std::uint8_t> || std::is_same_v<T, std::byte>)
					{
						const auto [sizeField, sizeFieldSize] = ReadHead();
						if (sizeField.Type != JceStruct::TypeEnum::Byte)
						{
							CAFE_THROW(
							    JceDecodeException,
							    Cafe::TextUtils::FormatString(CAFE_UTF8_SV("Type mismatch, got unexpected ${0}."),
							                                  static_cast<std::uint32_t>(sizeField.Type)));
						}

						std::uint8_t size;
						if (!Read(0, size))
						{
							CAFE_THROW(JceDecodeException, CAFE_UTF8_SV("Read size failed."));
						}

						std::vector<std::byte> tmpList(static_cast<std::size_t>(size));
						m_Reader.GetStream()->ReadBytes(gsl::make_span(tmpList.data(), size));

						value = std::move(tmpList);

						break;
					}
					else
					{
						[[fallthrough]];
					}
				default:
					CAFE_THROW(JceDecodeException, Cafe::TextUtils::FormatString(
					                                   CAFE_UTF8_SV("Type mismatch, got unexpected ${0}."),
					                                   static_cast<std::uint32_t>(head.Type)));
				}
				return true;
			}

			return false;
		}

		template <typename T>
		std::enable_if_t<std::is_base_of_v<JceStruct, T>, bool> doRead(std::uint32_t tag,
		                                                               std::shared_ptr<T>& value)
		{
			if (SkipToTag(tag))
			{
				const auto [head, headSize] = ReadHead();
				if (head.Type != JceStruct::TypeEnum::StructBegin)
				{
					CAFE_THROW(JceDecodeException, Cafe::TextUtils::FormatString(
					                                   CAFE_UTF8_SV("Type mismatch, got unexpected ${0}."),
					                                   static_cast<std::uint32_t>(head.Type)));
				}

				const auto newValue = std::make_shared<T>();
				JceDeserializer<T>::Deserialize(*this, *newValue);
				value = newValue;

				SkipToStructEnd();

				return true;
			}

			return false;
		}

		template <typename T>
		std::enable_if_t<std::is_base_of_v<JceStruct, T>, bool> doRead(std::uint32_t tag,
		                                                               std::shared_ptr<T> const& value)
		{
			if (SkipToTag(tag))
			{
				const auto [head, headSize] = ReadHead();
				if (head.Type != JceStruct::TypeEnum::StructBegin)
				{
					CAFE_THROW(JceDecodeException, CAFE_UTF8_SV("Type mismatch, got unexpected {0}."),
					           static_cast<std::uint32_t>(head.Type));
				}

				JceDeserializer<T>::Deserialize(*this, *value);

				SkipToStructEnd();

				return true;
			}

			return false;
		}

		template <typename T>
		std::enable_if_t<std::is_base_of_v<JceStruct, T>, bool> doRead(std::uint32_t tag, T& value)
		{
			if (SkipToTag(tag))
			{
				const auto [head, headSize] = ReadHead();
				if (head.Type != JceStruct::TypeEnum::StructBegin)
				{
					CAFE_THROW(JceDecodeException, Cafe::TextUtils::FormatString(
					                                   CAFE_UTF8_SV("Type mismatch, got unexpected ${0}."),
					                                   static_cast<std::uint32_t>(head.Type)));
				}

				JceDeserializer<T>::Deserialize(*this, value);

				SkipToStructEnd();

				return true;
			}

			return false;
		}
	};

	class JceOutputStream
	{
	public:
		explicit JceOutputStream(Cafe::Io::OutputStream* stream);

		[[nodiscard]] Cafe::Io::BinaryWriter& GetWriter() noexcept;

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
		void Write(std::uint32_t tag, std::shared_ptr<T> const& value)
		{
			if (value)
			{
				doWrite(tag, value);
			}
		}

	private:
		Cafe::Io::BinaryWriter m_Writer;

		void doWrite(std::uint32_t tag, std::uint8_t value);
		void doWrite(std::uint32_t tag, std::byte value);
		void doWrite(std::uint32_t tag, std::int16_t value);
		void doWrite(std::uint32_t tag, std::int32_t value);
		void doWrite(std::uint32_t tag, std::int64_t value);
		void doWrite(std::uint32_t tag, float value);
		void doWrite(std::uint32_t tag, double value);
		void doWrite(std::uint32_t tag, UsingStringView const& value);
		void doWrite(std::uint32_t tag, UsingString const& value);

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

		void doWrite(std::uint32_t tag, gsl::span<const std::uint8_t> const& value);
		void doWrite(std::uint32_t tag, gsl::span<const std::byte> const& value);
		void doWrite(std::uint32_t tag, std::vector<std::byte> const& value);

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
		std::enable_if_t<std::is_base_of_v<JceStruct, T>> doWrite(std::uint32_t tag, T const& value)
		{
			WriteHead({ tag, JceStruct::TypeEnum::StructBegin });
			JceSerializer<T>::Serialize(*this, value);
			WriteHead({ 0, JceStruct::TypeEnum::StructEnd });
		}

		template <typename T>
		std::enable_if_t<std::is_base_of_v<JceStruct, T>> doWrite(std::uint32_t tag,
		                                                          std::shared_ptr<T> const& value)
		{
			if (!value)
			{
				CAFE_THROW(JceEncodeException, CAFE_UTF8_SV("value is nullptr."));
			}

			doWrite(tag, *value);
		}
	};

	struct NoOp
	{
		template <typename T>
		struct Apply : Utility::ResultType<T>
		{
		};
	};

	struct IsOptional
	{
		template <typename T>
		struct Apply
		    : Utility::ResultType<std::conditional_t<Utility::IsTemplateOf<T, std::shared_ptr>::value,
		                                             T, std::optional<T>>>
		{
		};
	};

	template <typename... Args>
	struct TemplateArgs
	{
		template <template <typename...> class Template>
		struct Apply : Utility::ResultType<Template<Args...>>
		{
		};
	};

	template <JceStruct::TypeEnum Type, typename... Attributes>
	struct FieldTypeBuilder;

#define FIELD_TYPE_BUILDER_OP(name, code, type)                                                    \
	template <typename... Attributes>                                                                \
	struct FieldTypeBuilder<JceStruct::TypeEnum::name, Attributes...>                                \
	    : decltype(Utility::RecursiveApply<type, Attributes...>())                                   \
	{                                                                                                \
	};

	JCE_FIELD_TYPE(FIELD_TYPE_BUILDER_OP)

#undef FIELD_TYPE_BUILDER_OP

#define NO_OP NoOp

#define IS_OPTIONAL(defaultValue) IsOptional

#define TEMPLATE_ARGUMENT(...) TemplateArgs<__VA_ARGS__>

#define FIELD(name, tag, type, ...)                                                                \
private:                                                                                           \
	typename FieldTypeBuilder<JceStruct::TypeEnum::type __VA_OPT__(, ) __VA_ARGS__>::Type m_##name;  \
                                                                                                   \
public:                                                                                            \
	const auto& Get##name() const noexcept                                                           \
	{                                                                                                \
		return m_##name;                                                                               \
	}                                                                                                \
                                                                                                   \
	auto& Get##name() noexcept                                                                       \
	{                                                                                                \
		return m_##name;                                                                               \
	}                                                                                                \
                                                                                                   \
	template <typename T>                                                                            \
	void Set##name(T&& arg)                                                                          \
	{                                                                                                \
		m_##name = std::forward<T>(arg);                                                               \
	}                                                                                                \
                                                                                                   \
	static constexpr std::size_t Get##name##Tag() noexcept                                           \
	{                                                                                                \
		return tag;                                                                                    \
	}

#define JCE_STRUCT(name, alias)                                                                    \
	class name : public JceStruct                                                                    \
	{                                                                                                \
	public:                                                                                          \
		name();                                                                                        \
		~name();                                                                                       \
                                                                                                   \
		UsingStringView GetJceStructName() const noexcept override;

#define END_JCE_STRUCT(name)                                                                       \
	}                                                                                                \
	;

#include "JceStructDef.h"

#define NO_OP Detail::None

#define IS_OPTIONAL(defaultValue) defaultValue

// 读取 optional 的时候不会返回 false
#define FIELD(name, tag, type, ...)                                                                \
	{                                                                                                \
		using FieldType =                                                                              \
		    typename Utility::MayRemoveTemplate<Utility::RemoveCvRef<decltype(value.Get##name())>,     \
		                                        std::optional>::Type;                                  \
		if (!stream.Read(                                                                              \
		        tag, value.Get##name(),                                                                \
		        Utility::ReturnFirst<                                                                  \
		            Utility::ConcatTrait<                                                              \
		                Utility::ConcatTrait<                                                          \
		                    Utility::RemoveCvRef,                                                      \
		                    Utility::BindTrait<std::is_same, Detail::NoneType>::Result>::Result,       \
		                std::negation>::Result,                                                        \
		            Detail::NoneType>(__VA_ARGS__)))                                                   \
		{                                                                                              \
			CAFE_THROW(JceDecodeException,                                                               \
			           CAFE_UTF8_SV("Deserializing failed : Failed to read field \"" #name               \
			                        "\" which is not optional."));                                       \
		}                                                                                              \
	}

#define JCE_STRUCT(name, alias)                                                                    \
	template <>                                                                                      \
	struct JceDeserializer<name>                                                                     \
	{                                                                                                \
		static void Deserialize(JceInputStream& stream, name& value)                                   \
		{

#define END_JCE_STRUCT(name)                                                                       \
	}                                                                                                \
	}                                                                                                \
	;

#include "JceStructDef.h"

#define FIELD(name, tag, type, ...) stream.Write(tag, value.Get##name());

#define JCE_STRUCT(name, alias)                                                                    \
	template <>                                                                                      \
	struct JceSerializer<name>                                                                       \
	{                                                                                                \
		static void Serialize(JceOutputStream& stream, name const& value)                              \
		{

#define END_JCE_STRUCT(name)                                                                       \
	}                                                                                                \
	}                                                                                                \
	;

#include "JceStructDef.h"

} // namespace YumeBot::Jce
