#pragma once
#include "Jce.h"
#include <Cafe/Io/Streams/MemoryStream.h>
#include <Cafe/TextUtils/Format.h>

namespace YumeBot::Jce::Wup
{
	namespace Detail
	{
		template <typename T>
		struct ImplicitConvertibleIdentityType
		{
			using Type = T;

			constexpr ImplicitConvertibleIdentityType() = default;

			template <typename U, std::enable_if_t<std::is_same_v<T, U>, int> = 0>
			constexpr ImplicitConvertibleIdentityType(U const&) noexcept
			{
			}
		};

		template <typename T>
		constexpr ImplicitConvertibleIdentityType<Utility::RemoveCvRef<T>>
		    ImplicitConvertibleIdentity{};

		constexpr UsingStringView GetName(ImplicitConvertibleIdentityType<std::uint8_t>) noexcept
		{
			return CAFE_UTF8_SV("char");
		}

		constexpr UsingStringView GetName(ImplicitConvertibleIdentityType<std::int16_t>) noexcept
		{
			return CAFE_UTF8_SV("short");
		}

		constexpr UsingStringView GetName(ImplicitConvertibleIdentityType<std::int32_t>) noexcept
		{
			return CAFE_UTF8_SV("int32");
		}

		constexpr UsingStringView GetName(ImplicitConvertibleIdentityType<std::int64_t>) noexcept
		{
			return CAFE_UTF8_SV("int64");
		}

		constexpr UsingStringView GetName(ImplicitConvertibleIdentityType<float>) noexcept
		{
			return CAFE_UTF8_SV("float");
		}

		constexpr UsingStringView GetName(ImplicitConvertibleIdentityType<double>) noexcept
		{
			return CAFE_UTF8_SV("double");
		}

		constexpr UsingStringView GetName(ImplicitConvertibleIdentityType<std::string>) noexcept
		{
			return CAFE_UTF8_SV("string");
		}

		UsingStringView GetName(std::shared_ptr<JceStruct> const& value) noexcept;

#define JCE_STRUCT(name, alias)                                                                    \
	constexpr UsingStringView GetName(                                                               \
	    ImplicitConvertibleIdentityType<std::shared_ptr<name>>) noexcept                             \
	{                                                                                                \
		return CAFE_UTF8_SV(alias);                                                                    \
	}                                                                                                \
                                                                                                   \
	constexpr UsingStringView GetName(std::shared_ptr<name> const&) noexcept                         \
	{                                                                                                \
		return GetName(ImplicitConvertibleIdentity<std::shared_ptr<name>>);                            \
	}

#include "JceStructDef.h"

		template <typename T>
		UsingString GetName(std::vector<T> const& value)
		{
			using namespace Cafe::Encoding::StringLiterals;

			if constexpr (std::is_same_v<JceStruct, T>)
			{
				if (value.empty())
				{
					return u8"list<?>"_s;
				}

				return Cafe::TextUtils::FormatString(CAFE_UTF8_SV("list<${0}>"), GetName(value.front()));
			}
			else
			{
				return Cafe::TextUtils::FormatString(CAFE_UTF8_SV("list<${0}>"),
				                                     GetName(ImplicitConvertibleIdentity<T>));
			}
		}

		template <typename Key, typename Value>
		UsingString GetName(std::unordered_map<Key, Value> const& value)
		{
			using namespace Cafe::Encoding::StringLiterals;

			if constexpr (std::is_same_v<JceStruct, Key> || std::is_same_v<JceStruct, Value>)
			{
				if (value.empty())
				{
					return u8"map<?,?>"_s;
				}

				const auto& front = *value.cbegin();
				return Cafe::TextUtils::FormatString(CAFE_UTF8_SV("map<${0},${1}>"), GetName(front.first),
				                                     GetName(front.second));
			}
			else
			{
				return Cafe::TextUtils::FormatString(CAFE_UTF8_SV("map<${0},${1}>"),
				                                     GetName(ImplicitConvertibleIdentity<Key>),
				                                     GetName(ImplicitConvertibleIdentity<Value>));
			}
		}
	} // namespace Detail

	class OldUniAttribute
	{
	public:
		template <typename T>
		void Put(UsingString const& name, T const& value)
		{
			Cafe::Io::MemoryStream memoryStream;
			JceOutputStream out{ Cafe::Io::BinaryWriter{ &memoryStream } };
			out.Write(0, value);
			memoryStream.SeekFromBegin(0);
			const auto internalStorage = memoryStream.GetInternalStorage();
			m_Data[name][Detail::GetName(value)].assign(internalStorage.data(),
			                                            internalStorage.data() + internalStorage.size());
		}

		template <typename T>
		bool Get(UsingString const& name, T& result) const
		{
			const auto iter = m_Data.find(name);
			if (iter == m_Data.cend())
			{
				CAFE_THROW(Cafe::ErrorHandling::CafeException,
				           Cafe::TextUtils::FormatString(CAFE_UTF8_SV("No such key(\"${0}\")."), name));
			}

			constexpr auto fieldTypeName = Detail::GetName(Detail::ImplicitConvertibleIdentity<T>);
			const auto fieldIter = iter->second.find(fieldTypeName);
			if (fieldIter == iter->second.cend())
			{
				CAFE_THROW(Cafe::ErrorHandling::CafeException,
				           Cafe::TextUtils::FormatString(CAFE_UTF8_SV("No such field of type(${0})."),
				                                         fieldTypeName));
			}

			Cafe::Io::ExternalMemoryInputStream stream{ gsl::as_bytes(
				  gsl::make_span(fieldIter->second.data(), fieldIter->second.size())) };
			JceInputStream in{ Cafe::Io::BinaryReader{ &stream } };

			return in.Read(0, result);
		}

		bool Remove(UsingString const& name);

		void Encode(Cafe::Io::BinaryWriter const& writer) const;
		void Decode(Cafe::Io::BinaryReader const& reader);

	private:
		std::unordered_map<UsingString, std::unordered_map<UsingString, std::vector<std::byte>>> m_Data;
	};

	class UniPacket
	{
	public:
		UniPacket();

		void Encode(Cafe::Io::BinaryWriter const& writer);
		void Decode(Cafe::Io::BinaryReader const& reader);

		UniPacket CreateResponse();
		void CreateOldRespEncode(JceOutputStream& os);

		RequestPacket& GetRequestPacket() noexcept
		{
			return m_RequestPacket;
		}

		OldUniAttribute& GetAttribute() noexcept
		{
			return m_UniAttribute;
		}

		std::int32_t GetOldRespIRet() const noexcept
		{
			return m_OldRespIRet;
		}

		void SetOldRespIRet(std::int32_t value) noexcept
		{
			m_OldRespIRet = value;
		}

	private:
		RequestPacket m_RequestPacket;
		OldUniAttribute m_UniAttribute;
		std::int32_t m_OldRespIRet;
	};
} // namespace YumeBot::Jce::Wup
