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
			JceStruct::TypeEnum Type;
		};

		explicit JceInputStream(NatsuLib::natRefPointer<NatsuLib::natBinaryReader> reader);
		~JceInputStream();

		std::pair<HeadData, std::size_t> ReadHead() const
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

		std::pair<HeadData, std::size_t> PeekHead() const
		{
			const auto underlyingStream = m_Reader->GetUnderlyingStream();
			const auto pos = underlyingStream->GetPosition();
			const auto head = ReadHead();
			underlyingStream->SetPositionFromBegin(pos);
			return head;
		}

		void Skip(nLen len)
		{
			m_Reader->Skip(len);
		}

		void SkipToStructEnd()
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

		void SkipField()
		{
			const auto [head, headSize] = ReadHead();
			SkipField(head.Type);
		}

		void SkipField(JceStruct::TypeEnum type)
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
					nat_Throw(JceDecodeException, u8"Type mismatch, expected 0, got {0}", static_cast<std::uint32_t>(head.Type));
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

		bool SkipToTag(std::uint32_t tag)
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

		template <typename T>
		bool Read(std::size_t tag, T& value, std::nullptr_t = nullptr)
		{
			return Reader<T>::DoRead(*this, tag, value);
		}

		template <typename T, typename U>
		std::enable_if_t<std::is_assignable_v<T&, U&&>, std::true_type> Read(std::size_t tag, T& value, U&& defaultValue)
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

		template <typename T, typename = void>
		struct Reader;

		template <>
		struct Reader<std::uint8_t>
		{
			static bool DoRead(JceInputStream& self, std::size_t tag, std::uint8_t& value)
			{
				if (self.SkipToTag(tag))
				{
					const auto [head, headSize] = self.ReadHead();
					switch (head.Type)
					{
					case JceStruct::TypeEnum::Byte:
						self.m_Reader->ReadPod(value);
						break;
					case JceStruct::TypeEnum::ZeroTag:
						value = 0;
						break;
					default:
						nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}", static_cast<std::uint32_t>(head.Type));
					}

					return true;
				}

				return false;
			}
		};

		template <>
		struct Reader<std::int16_t>
		{
			static bool DoRead(JceInputStream& self, std::size_t tag, std::int16_t& value)
			{
				if (self.SkipToTag(tag))
				{
					const auto [head, headSize] = self.ReadHead();
					switch (head.Type)
					{
					case JceStruct::TypeEnum::Byte:
						value = self.m_Reader->ReadPod<std::uint8_t>();
						break;
					case JceStruct::TypeEnum::Short:
						self.m_Reader->ReadPod(value);
						break;
					case JceStruct::TypeEnum::ZeroTag:
						value = 0;
						break;
					default:
						nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}", static_cast<std::uint32_t>(head.Type));
					}

					return true;
				}

				return false;
			}
		};

		template <>
		struct Reader<std::int32_t>
		{
			static bool DoRead(JceInputStream& self, std::size_t tag, std::int32_t& value)
			{
				if (self.SkipToTag(tag))
				{
					const auto [head, headSize] = self.ReadHead();
					switch (head.Type)
					{
					case JceStruct::TypeEnum::Byte:
						value = self.m_Reader->ReadPod<std::uint8_t>();
						break;
					case JceStruct::TypeEnum::Short:
						value = self.m_Reader->ReadPod<std::int16_t>();
						break;
					case JceStruct::TypeEnum::Int:
						self.m_Reader->ReadPod(value);
						break;
					case JceStruct::TypeEnum::ZeroTag:
						value = 0;
						break;
					default:
						nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}", static_cast<std::uint32_t>(head.Type));
					}

					return true;
				}

				return false;
			}
		};

		template <>
		struct Reader<std::int64_t>
		{
			static bool DoRead(JceInputStream& self, std::size_t tag, std::int64_t& value)
			{
				if (self.SkipToTag(tag))
				{
					const auto[head, headSize] = self.ReadHead();
					switch (head.Type)
					{
					case JceStruct::TypeEnum::Byte:
						value = self.m_Reader->ReadPod<std::uint8_t>();
						break;
					case JceStruct::TypeEnum::Short:
						value = self.m_Reader->ReadPod<std::int16_t>();
						break;
					case JceStruct::TypeEnum::Int:
						value = self.m_Reader->ReadPod<std::int32_t>();
						break;
					case JceStruct::TypeEnum::Long:
						self.m_Reader->ReadPod(value);
						break;
					case JceStruct::TypeEnum::ZeroTag:
						value = 0;
						break;
					default:
						nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected got {0}", static_cast<std::uint32_t>(head.Type));
					}

					return true;
				}

				return false;
			}
		};

		template <>
		struct Reader<float>
		{
			static bool DoRead(JceInputStream& self, std::size_t tag, float& value)
			{
				if (self.SkipToTag(tag))
				{
					const auto[head, headSize] = self.ReadHead();
					switch (head.Type)
					{
					case JceStruct::TypeEnum::Float:
						self.m_Reader->ReadPod(value);
						break;
					case JceStruct::TypeEnum::ZeroTag:
						value = 0;
						break;
					default:
						nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}", static_cast<std::uint32_t>(head.Type));
					}

					return true;
				}

				return false;
			}
		};

		template <>
		struct Reader<double>
		{
			static bool DoRead(JceInputStream& self, std::size_t tag, double& value)
			{
				if (self.SkipToTag(tag))
				{
					const auto[head, headSize] = self.ReadHead();
					switch (head.Type)
					{
					case JceStruct::TypeEnum::Float:
						value = self.m_Reader->ReadPod<float>();
						break;
					case JceStruct::TypeEnum::Double:
						self.m_Reader->ReadPod(value);
						break;
					case JceStruct::TypeEnum::ZeroTag:
						value = 0;
						break;
					default:
						nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}", static_cast<std::uint32_t>(head.Type));
					}

					return true;
				}

				return false;
			}
		};

		template <>
		struct Reader<std::string>
		{
			static bool DoRead(JceInputStream& self, std::size_t tag, std::string& value)
			{
				if (self.SkipToTag(tag))
				{
					const auto [head, headSize] = self.ReadHead();
					std::size_t strSize;
					switch (head.Type)
					{
					case JceStruct::TypeEnum::String1:
						strSize = self.m_Reader->ReadPod<std::uint8_t>();
						break;
					case JceStruct::TypeEnum::String4:
						strSize = self.m_Reader->ReadPod<std::uint32_t>();
						if (strSize > JceStruct::MaxStringLength)
						{
							nat_Throw(JceDecodeException, u8"String too long, {0} sizes requested.", strSize);
						}
						break;
					default:
						nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}", static_cast<std::uint32_t>(head.Type));
					}

					// 为了异常安全，构造临时字符串而不是就地修改
					std::string tmpString(strSize, 0);
					self.m_Reader->GetUnderlyingStream()->ReadBytes(reinterpret_cast<nData>(tmpString.data()), strSize);
					value = std::move(tmpString);

					return true;
				}

				return false;
			}
		};

		template <typename Key, typename Value>
		struct Reader<std::unordered_map<Key, Value>>
		{
			static bool DoRead(JceInputStream& self, std::size_t tag, std::unordered_map<Key, Value>& value)
			{
				if (self.SkipToTag(tag))
				{
					const auto [head, headSize] = self.ReadHead();
					switch (head.Type)
					{
					case JceStruct::TypeEnum::Map:
					{
						std::int32_t size;
						if (!self.Read(0, size))
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
							if (!self.Read(0, entryKey))
							{
								nat_Throw(JceDecodeException, u8"Read key failed.");
							}
							Value entryValue;
							if (!self.Read(1, entryValue))
							{
								nat_Throw(JceDecodeException, u8"Read value failed.");
							}
							tmpMap.emplace(std::move(entryKey), std::move(entryValue));
						}

						value = std::move(tmpMap);
						break;
					}
					default:
						nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}", static_cast<std::uint32_t>(head.Type));
					}

					return true;
				}

				return false;
			}
		};

		template <typename T>
		struct Reader<std::vector<T>>
		{
			static bool DoRead(JceInputStream& self, std::size_t tag, std::vector<T>& value)
			{
				if (self.SkipToTag(tag))
				{
					const auto[head, headSize] = self.ReadHead();
					switch (head.Type)
					{
					case JceStruct::TypeEnum::List:
					{
						std::int32_t size;
						if (!self.Read(0, size))
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
								if (!self.Read(0, elemValue))
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
							const auto[sizeField, sizeFieldSize] = self.ReadHead();
							if (sizeField.Type != JceStruct::TypeEnum::Byte)
							{
								nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}", static_cast<std::uint32_t>(sizeField.Type));
							}

							std::uint8_t size;
							if (!self.Read(0, size))
							{
								nat_Throw(JceDecodeException, u8"Read size failed.");
							}

							std::vector<std::uint8_t> tmpList(static_cast<std::size_t>(size));
							self.m_Reader->GetUnderlyingStream()->ReadBytes(tmpList.data(), size);

							value = std::move(tmpList);

							break;
						}
						else
						{
							[[fallthrough]];
						}
					default:
						nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}", static_cast<std::uint32_t>(head.Type));
					}
					return true;
				}

				return false;
			}
		};

		template <typename T>
		struct Reader<T, std::enable_if_t<std::is_base_of_v<JceStruct, T>>>
		{
			static bool DoRead(JceInputStream& self, std::size_t tag, T& value)
			{
				if (self.SkipToTag(tag))
				{
					const auto[head, headSize] = self.ReadHead();
					if (head.Type != JceStruct::TypeEnum::StructBegin)
					{
						nat_Throw(JceDecodeException, u8"Type mismatch, got unexpected {0}", static_cast<std::uint32_t>(head.Type));
					}

					value = TlvDeserializer<T>::Deserialize(self);

					self.SkipToStructEnd();

					return true;
				}

				return false;
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

	// 读取 optional 的时候不会返回 false
#define FIELD(name, tag, type, ...) \
	{\
		using FieldType = typename Utility::MayRemoveTemplate<Utility::RemoveCvRef<decltype(ret->Get##name())>, std::optional>::Type;\
		if (!stream.Read(tag, ret->Get##name(),\
			Utility::ReturnFirst<Utility::ConcatTrait<Utility::BindTrait<std::is_same, std::nullptr_t>::template Result, std::negation>::template Result, std::nullptr_t>(__VA_ARGS__)))\
		{\
			nat_Throw(JceDecodeException, u8"Deserializing failed : Failed to read field \"" #name "\" which is not optional.");\
		}\
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
