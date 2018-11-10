#pragma once
#include "Jce.h"
#include <natStringUtil.h>

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
		constexpr ImplicitConvertibleIdentityType<Utility::RemoveCvRef<T>> ImplicitConvertibleIdentity{};

		constexpr nStrView GetName(ImplicitConvertibleIdentityType<std::uint8_t>) noexcept
		{
			using namespace NatsuLib::StringLiterals;
			return u8"char"_nv;
		}

		constexpr nStrView GetName(ImplicitConvertibleIdentityType<std::int16_t>) noexcept
		{
			using namespace NatsuLib::StringLiterals;
			return u8"short"_nv;
		}

		constexpr nStrView GetName(ImplicitConvertibleIdentityType<std::int32_t>) noexcept
		{
			using namespace NatsuLib::StringLiterals;
			return u8"int32"_nv;
		}

		constexpr nStrView GetName(ImplicitConvertibleIdentityType<std::int64_t>) noexcept
		{
			using namespace NatsuLib::StringLiterals;
			return u8"int64"_nv;
		}

		constexpr nStrView GetName(ImplicitConvertibleIdentityType<float>) noexcept
		{
			using namespace NatsuLib::StringLiterals;
			return u8"float"_nv;
		}

		constexpr nStrView GetName(ImplicitConvertibleIdentityType<double>) noexcept
		{
			using namespace NatsuLib::StringLiterals;
			return u8"double"_nv;
		}

		constexpr nStrView GetName(ImplicitConvertibleIdentityType<std::string>) noexcept
		{
			using namespace NatsuLib::StringLiterals;
			return u8"string"_nv;
		}

		nStrView GetName(NatsuLib::natRefPointer<JceStruct> const& value) noexcept;

#define JCE_STRUCT(name, alias) \
		constexpr nStrView GetName(ImplicitConvertibleIdentityType<NatsuLib::natRefPointer<name>>) noexcept\
		{\
			using namespace NatsuLib::StringLiterals;\
			return u8 ## alias ## _nv;\
		}

#include "JceStructDef.h"

		template <typename T>
		nString GetName(std::vector<T> const& value)
		{
			using namespace NatsuLib::StringLiterals;

			if constexpr (std::is_same_v<JceStruct, T>)
			{
				if (value.empty())
				{
					return u8"list<?>"_ns;
				}

				return NatsuLib::natUtil::FormatString(u8"list<{0}>"_nv, GetName(value.front()));
			}
			else
			{
				return NatsuLib::natUtil::FormatString(u8"list<{0}>"_nv, GetName(ImplicitConvertibleIdentity<T>));
			}
		}

		template <typename Key, typename Value>
		nString GetName(std::unordered_map<Key, Value> const& value)
		{
			using namespace NatsuLib::StringLiterals;

			if constexpr (std::is_same_v<JceStruct, Key> || std::is_same_v<JceStruct, Value>)
			{
				if (value.empty())
				{
					return u8"map<?,?>"_ns;
				}

				const auto& front = *value.cbegin();
				return NatsuLib::natUtil::FormatString(u8"map<{0},{1}>"_nv, GetName(front.first), GetName(front.second));
			}
			else
			{
				return NatsuLib::natUtil::FormatString(u8"map<{0},{1}>"_nv, GetName(ImplicitConvertibleIdentity<Key>), GetName(ImplicitConvertibleIdentity<Value>));
			}
		}
	}

	class OldUniAttribute
	{
	public:
		template <typename T>
		void Put(nString const& name, T const& value)
		{
			const auto memoryStream = NatsuLib::make_ref<NatsuLib::natMemoryStream>(0, false, true, true);
			JceOutputStream out{ NatsuLib::make_ref<NatsuLib::natBinaryWriter>(memoryStream) };
			out.Write(0, value);
			memoryStream->SetPositionFromBegin(0);
			m_Data[name][Detail::GetName(value)].assign(reinterpret_cast<const std::uint8_t*>(memoryStream->GetInternalBuffer()),
									  reinterpret_cast<const std::uint8_t*>(memoryStream->GetInternalBuffer() + memoryStream->GetSize()));
		}

		template <typename T>
		T Get(nString const& name) const
		{
			using namespace NatsuLib;

			const auto iter = m_Data.find(name);
			if (iter == m_Data.cend())
			{
				nat_Throw(natErrException, NatErr_NotFound, u8"No such key(\"{0}\")."_nv, name);
			}

			constexpr auto fieldTypeName = Detail::GetName(Detail::ImplicitConvertibleIdentity<T>);
			const auto fieldIter = iter->second.find(fieldTypeName);
			if (fieldIter == iter->second.cend())
			{
				nat_Throw(natErrException, NatErr_NotFound, u8"No such field of type({0})."_nv, fieldTypeName);
			}

			JceInputStream in{ make_ref<natBinaryReader>(make_ref<natExternMemoryStream>(fieldIter->second.data(), fieldIter->second.size(), true)) };

			T result;
			if (!in.Read(0, result))
			{
				nat_Throw(natErrException, NatErr_InvalidArg, u8"Field of type {0} from key \"{1}\" is corrupted."_nv);
			}

			return result;
		}

		bool Remove(nString const& name);

		void Encode(NatsuLib::natRefPointer<NatsuLib::natBinaryWriter> const& writer) const;
		void Decode(NatsuLib::natRefPointer<NatsuLib::natBinaryReader> const& reader);

	private:
		std::unordered_map<nString, std::unordered_map<nString, std::vector<std::uint8_t>>> m_Data;
	};
}
